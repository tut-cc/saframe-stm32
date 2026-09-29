#include "usb_webcam.h"

#include <assert.h>
#include <stddef.h>

#include "stm32n6xx_hal.h"
#include <tk/tkernel.h>
#include "uvcl.h"

#define USB_WEBCAM_BUFFER_COUNT (2U)
#define USB_WEBCAM_SERVICE_PRIORITY (8)
#define USB_WEBCAM_SERVICE_STACK_SIZE (8U * 1024U)
#define USB_WEBCAM_ENCODE_TIMEOUT_MS (100U)

__attribute__((section(".psram_bss")))
__attribute__((aligned(32)))
static uint8_t webcam_mcu_buffer[USB_WEBCAM_MCU_SIZE];

__attribute__((section(".psram_bss")))
__attribute__((aligned(32)))
static uint8_t webcam_buffers[USB_WEBCAM_BUFFER_COUNT][USB_WEBCAM_JPEG_MAX_SIZE];

static JPEG_HandleTypeDef webcam_jpeg;
static uint32_t webcam_jpeg_length;
static uint32_t webcam_jpeg_chunks;

static volatile bool webcam_buffer_busy[USB_WEBCAM_BUFFER_COUNT];
static volatile bool webcam_streaming;
static volatile uint32_t webcam_submitted_frames;
static volatile uint32_t webcam_dropped_frames;
static volatile uint32_t webcam_last_jpeg_bytes;
static volatile uint32_t webcam_maximum_convert_us;
static volatile uint32_t webcam_maximum_encode_us;
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

static void webcam_jpeg_init(void)
{
  webcam_jpeg.Instance = JPEG;
  assert(HAL_JPEG_Init(&webcam_jpeg) == HAL_OK);

  const JPEG_ConfTypeDef configuration = {
    .ColorSpace = JPEG_YCBCR_COLORSPACE,
    .ChromaSubsampling = JPEG_422_SUBSAMPLING,
    .ImageHeight = USB_WEBCAM_HEIGHT,
    .ImageWidth = USB_WEBCAM_WIDTH,
    .ImageQuality = USB_WEBCAM_JPEG_QUALITY,
  };
  assert(HAL_JPEG_ConfigEncoding(&webcam_jpeg, &configuration) == HAL_OK);
}

/* Returns the encoded length, or 0 when the frame did not fit. */
static uint32_t webcam_jpeg_encode(uint8_t *destination)
{
  webcam_jpeg_length = 0U;
  webcam_jpeg_chunks = 0U;
  if (HAL_JPEG_Encode(&webcam_jpeg, webcam_mcu_buffer, USB_WEBCAM_MCU_SIZE,
                      destination, USB_WEBCAM_JPEG_MAX_SIZE,
                      USB_WEBCAM_ENCODE_TIMEOUT_MS) != HAL_OK)
  {
    /* A failed polling encode leaves the core running and the pause state
     * set, so reset the peripheral before the next frame. */
    assert(HAL_JPEG_DeInit(&webcam_jpeg) == HAL_OK);
    webcam_jpeg_init();
    return 0U;
  }
  /* A full output buffer is reported as an extra chunk and then reused. */
  if (webcam_jpeg_chunks != 1U || webcam_jpeg_length >= USB_WEBCAM_JPEG_MAX_SIZE)
  {
    return 0U;
  }
  return webcam_jpeg_length;
}

/* The whole frame is passed as one input buffer. Without pausing here the
 * HAL would feed the same MCUs again once they are consumed. */
void HAL_JPEG_GetDataCallback(JPEG_HandleTypeDef *hjpeg, uint32_t NbDecodedData)
{
  (void)NbDecodedData;
  (void)HAL_JPEG_Pause(hjpeg, JPEG_PAUSE_RESUME_INPUT);
}

void HAL_JPEG_DataReadyCallback(JPEG_HandleTypeDef *hjpeg, uint8_t *pDataOut,
                                uint32_t OutDataLength)
{
  (void)hjpeg;
  (void)pDataOut;
  webcam_jpeg_length = OutDataLength;
  webcam_jpeg_chunks++;
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

  webcam_jpeg_init();

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

bool UsbWebcam_SubmitRgb565(const uint16_t *source,
                            uint32_t source_width,
                            uint32_t source_height)
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
    return false;
  }

  const uint32_t convert_started = DWT->CYCCNT;
  UsbWebcam_ConvertRgb565ToYcbcr422Mcu(source, source_width, source_height,
                                       webcam_mcu_buffer, USB_WEBCAM_WIDTH,
                                       USB_WEBCAM_HEIGHT);
  const uint32_t encode_started = DWT->CYCCNT;
  const uint32_t length = webcam_jpeg_encode(webcam_buffers[index]);
  const uint32_t encode_finished = DWT->CYCCNT;

  const uint32_t convert_us = webcam_cycles_to_us(encode_started - convert_started);
  const uint32_t encode_us = webcam_cycles_to_us(encode_finished - encode_started);
  if (convert_us > webcam_maximum_convert_us)
  {
    webcam_maximum_convert_us = convert_us;
  }
  if (encode_us > webcam_maximum_encode_us)
  {
    webcam_maximum_encode_us = encode_us;
  }

  if (length == 0U)
  {
    webcam_buffer_busy[index] = false;
    webcam_dropped_frames++;
    return false;
  }
  webcam_last_jpeg_bytes = length;

  SCB_CleanDCache_by_Addr(webcam_buffers[index], (int32_t)length);
  if (UVCL_ShowFrame(webcam_buffers[index], (int)length) != 0)
  {
    webcam_buffer_busy[index] = false;
    webcam_dropped_frames++;
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

uint32_t UsbWebcam_MaximumConvertUs(void)
{
  return webcam_maximum_convert_us;
}

uint32_t UsbWebcam_MaximumEncodeUs(void)
{
  return webcam_maximum_encode_us;
}

bool UsbWebcam_IsStreaming(void)
{
  return webcam_streaming;
}

void HAL_JPEG_MspInit(JPEG_HandleTypeDef *hjpeg)
{
  assert(hjpeg->Instance == JPEG);

  __HAL_RCC_JPEG_FORCE_RESET();
  __HAL_RCC_JPEG_RELEASE_RESET();
  __HAL_RCC_JPEG_CLK_ENABLE();
  __HAL_RCC_JPEG_CLK_SLEEP_ENABLE();
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
