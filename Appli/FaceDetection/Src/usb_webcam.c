#include "usb_webcam.h"

#include <assert.h>
#include <stddef.h>

#include "stm32n6xx_hal.h"
#include <tk/tkernel.h>
#include <tm/tmonitor.h>
#include "uvcl.h"
#include "usb_webcam_venc.h"

#define USB_WEBCAM_BUFFER_COUNT (2U)
#define USB_WEBCAM_SERVICE_PRIORITY (8)
#define USB_WEBCAM_SERVICE_STACK_SIZE (8U * 1024U)

__attribute__((section(".psram_bss")))
__attribute__((aligned(32)))
static uint8_t webcam_buffers[USB_WEBCAM_BUFFER_COUNT][USB_WEBCAM_JPEG_MAX_SIZE];

static volatile bool webcam_buffer_busy[USB_WEBCAM_BUFFER_COUNT];
static volatile bool webcam_streaming;
static volatile uint32_t webcam_submitted_frames;
static volatile uint32_t webcam_dropped_frames;
static volatile uint32_t webcam_last_jpeg_bytes;
static volatile uint32_t webcam_maximum_encode_us;
static volatile UsbWebcam_DropCounts webcam_drops;
static UVCL_Callbacks_t webcam_callbacks;
static ID webcam_irq_sem_id;

static void webcam_service_task(INT stacd, void *exinf)
{
  (void)stacd;
  (void)exinf;

  for (;;)
  {
    assert(tk_wai_sem(webcam_irq_sem_id, 1, TMO_FEVR) == E_OK);
    UVCL_IRQHandler();
    HAL_NVIC_EnableIRQ(USB1_OTG_HS_IRQn);
  }
}

static const T_CTSK webcam_service_task_config = {
  .itskpri = USB_WEBCAM_SERVICE_PRIORITY,
  .stksz = USB_WEBCAM_SERVICE_STACK_SIZE,
  .task = webcam_service_task,
  .tskatr = TA_HLNG | TA_RNG3,
};

static void webcam_streaming_active(UVCL_Callbacks_t *callbacks,
                                    UVCL_StreamConf_t stream)
{
  (void)callbacks;
  (void)stream;
  webcam_streaming = true;
  webcam_drops.stream_starts++;
}

static void webcam_streaming_inactive(UVCL_Callbacks_t *callbacks)
{
  (void)callbacks;
  webcam_streaming = false;
}

static void webcam_frame_release(UVCL_Callbacks_t *callbacks, void *frame)
{
  (void)callbacks;
  for (uint32_t i = 0U; i < USB_WEBCAM_BUFFER_COUNT; i++)
  {
    if (frame == webcam_buffers[i])
    {
      webcam_buffer_busy[i] = false;
      return;
    }
  }
}

static uint32_t webcam_cycles_to_us(uint32_t cycles)
{
  const uint32_t cycles_per_us = SystemCoreClock / 1000000U;
  assert(cycles_per_us != 0U);
  return cycles / cycles_per_us;
}

void UsbWebcam_Init(void)
{
  const T_CSEM irq_sem_config = {
    .sematr = TA_TFIFO,
    .isemcnt = 0,
    .maxsem = 1,
  };

  webcam_irq_sem_id = tk_cre_sem(&irq_sem_config);
  assert(webcam_irq_sem_id > 0);

  UsbWebcamVenc_Init(USB_WEBCAM_WIDTH, USB_WEBCAM_HEIGHT,
                     USB_WEBCAM_JPEG_QUALITY);

  UVCL_Conf_t configuration = {0};
  configuration.streams[0].payload_type = UVCL_PAYLOAD_JPEG;
  configuration.streams[0].width = (int)USB_WEBCAM_WIDTH;
  configuration.streams[0].height = (int)USB_WEBCAM_HEIGHT;
  configuration.streams[0].fps = (int)USB_WEBCAM_FPS;
  configuration.streams[0].dwMaxVideoFrameSize = USB_WEBCAM_JPEG_MAX_SIZE;
  configuration.streams_nb = 1;
  configuration.is_immediate_mode = 0;

  webcam_callbacks.streaming_active = webcam_streaming_active;
  webcam_callbacks.streaming_inactive = webcam_streaming_inactive;
  webcam_callbacks.frame_release = webcam_frame_release;
  assert(UVCL_Init(USB1_OTG_HS, &configuration, &webcam_callbacks) == 0);

  const ID service_task_id = tk_cre_tsk(&webcam_service_task_config);
  assert(service_task_id > 0);
  assert(tk_sta_tsk(service_task_id, 0) == E_OK);
  HAL_NVIC_EnableIRQ(USB1_OTG_HS_IRQn);
}

void UsbWebcam_IRQHandler(void)
{
  HAL_NVIC_DisableIRQ(USB1_OTG_HS_IRQn);
  (void)tk_sig_sem(webcam_irq_sem_id, 1);
}

void UsbWebcam_VencIRQHandler(void)
{
  UsbWebcamVenc_IRQHandler();
}

bool UsbWebcam_SubmitRgb565(const uint16_t *frame)
{
  if (!webcam_streaming)
  {
    return false;
  }

  uint32_t index;
  for (index = 0U; index < USB_WEBCAM_BUFFER_COUNT; index++)
  {
    if (!webcam_buffer_busy[index])
    {
      webcam_buffer_busy[index] = true;
      break;
    }
  }
  if (index == USB_WEBCAM_BUFFER_COUNT)
  {
    webcam_dropped_frames++;
    webcam_drops.busy++;
    return false;
  }

  const uint32_t encode_started = DWT->CYCCNT;
  const uint32_t length = UsbWebcamVenc_Encode(frame, webcam_buffers[index],
                                               USB_WEBCAM_JPEG_MAX_SIZE);
  const uint32_t encode_us = webcam_cycles_to_us(DWT->CYCCNT - encode_started);
  if (encode_us > webcam_maximum_encode_us)
  {
    webcam_maximum_encode_us = encode_us;
  }

  if (length == 0U)
  {
    webcam_buffer_busy[index] = false;
    webcam_dropped_frames++;
    webcam_drops.encode++;
    webcam_drops.last_encode_error = UsbWebcamVenc_LastError();
    return false;
  }
  webcam_last_jpeg_bytes = length;

  if (UVCL_ShowFrame(webcam_buffers[index], (int)length) != 0)
  {
    webcam_buffer_busy[index] = false;
    webcam_dropped_frames++;
    webcam_drops.show++;
    return false;
  }

  webcam_submitted_frames++;
  return true;
}

uint32_t UsbWebcam_SubmittedFrames(void)
{
  return webcam_submitted_frames;
}

uint32_t UsbWebcam_DroppedFrames(void)
{
  return webcam_dropped_frames;
}

uint32_t UsbWebcam_LastJpegBytes(void)
{
  return webcam_last_jpeg_bytes;
}

uint32_t UsbWebcam_MaximumEncodeUs(void)
{
  return webcam_maximum_encode_us;
}

/* TEMPORARY diagnostic for the VENC bus errors seen on hardware: encodes the
 * same frame into PSRAM and into AXISRAM2 (unused by the NN) and reports how
 * often each fails. The AXISRAM2 JPEG is not cache-coherent and is thrown
 * away; only the error counts matter. */
#define USB_WEBCAM_SELFTEST_RUNS (16U)
#define USB_WEBCAM_SELFTEST_AXISRAM2 ((uint8_t *)0x34100000UL)

static void webcam_selftest_run(const uint16_t *frame, uint8_t *destination,
                                const char *label, const char *target)
{
  uint32_t ok = 0U;
  uint32_t total_us = 0U;
  int32_t last_error = 0;
  for (uint32_t i = 0U; i < USB_WEBCAM_SELFTEST_RUNS; i++)
  {
    const uint32_t started = DWT->CYCCNT;
    const uint32_t length = UsbWebcamVenc_Encode(frame, destination,
                                                 USB_WEBCAM_JPEG_MAX_SIZE);
    total_us += webcam_cycles_to_us(DWT->CYCCNT - started);
    if (length != 0U)
    {
      ok++;
    }
    else
    {
      last_error = UsbWebcamVenc_LastError();
    }
  }
  tm_printf((UB *)"VENC selftest [%s] out=%s: ok=%u/%u last_error=%d avg=%uus.\n",
            label, target, ok, USB_WEBCAM_SELFTEST_RUNS, (int)last_error,
            total_us / USB_WEBCAM_SELFTEST_RUNS);
}

void UsbWebcam_SelfTest(const uint16_t *frame, const char *label)
{
  __HAL_RCC_AXISRAM2_MEM_CLK_ENABLE();
  webcam_selftest_run(frame, webcam_buffers[0], label, "psram");
  webcam_selftest_run(frame, USB_WEBCAM_SELFTEST_AXISRAM2, label, "axisram2");
}

int32_t UsbWebcam_SelfTestEncodeOnce(const uint16_t *frame, bool to_axisram2)
{
  __HAL_RCC_AXISRAM2_MEM_CLK_ENABLE();
  uint8_t *destination = to_axisram2 ? USB_WEBCAM_SELFTEST_AXISRAM2
                                     : webcam_buffers[0];
  if (UsbWebcamVenc_Encode(frame, destination, USB_WEBCAM_JPEG_MAX_SIZE) != 0U)
  {
    return 0;
  }
  return UsbWebcamVenc_LastError();
}

UsbWebcam_DropCounts UsbWebcam_GetDropCounts(void)
{
  UsbWebcam_DropCounts counts;
  counts.stream_starts = webcam_drops.stream_starts;
  counts.busy = webcam_drops.busy;
  counts.encode = webcam_drops.encode;
  counts.show = webcam_drops.show;
  counts.last_encode_error = webcam_drops.last_encode_error;
  return counts;
}

bool UsbWebcam_IsStreaming(void)
{
  return webcam_streaming;
}

void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd)
{
  assert(hpcd->Instance == USB1_OTG_HS);

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_SLEEP_ENABLE();
  HAL_PWREx_EnableVddUSBVMEN();
  while (__HAL_PWR_GET_FLAG(PWR_FLAG_USB33RDY) == 0U)
  {
  }
  HAL_PWREx_EnableVddUSB();

  __HAL_RCC_USB1_OTG_HS_FORCE_RESET();
  __HAL_RCC_USB1_OTG_HS_RELEASE_RESET();
  __HAL_RCC_USB1_OTG_HS_CLK_ENABLE();
  __HAL_RCC_USB1_OTG_HS_CLK_SLEEP_ENABLE();
  USB1_HS_PHYC->USBPHYC_CR &= ~(0x7U << 4U);
  USB1_HS_PHYC->USBPHYC_CR |= (0x2U << 4U);
  __HAL_RCC_USB1_OTG_HS_PHY_CLK_ENABLE();
  __HAL_RCC_USB1_OTG_HS_PHY_CLK_SLEEP_ENABLE();
  HAL_NVIC_SetPriority(USB1_OTG_HS_IRQn, 6U, 0U);
}
