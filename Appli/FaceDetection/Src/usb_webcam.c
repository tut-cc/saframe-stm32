#include "usb_webcam.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "stm32n6xx_hal.h"
#include <tk/tkernel.h>
#include "usb_h264_encoder.h"
#include "uvcl.h"

#define INPUT_COUNT 2U
#define OUTPUT_COUNT 2U
#define ENCODER_PRIORITY 7
#define SERVICE_PRIORITY 8
#define TASK_STACK_SIZE (8U * 1024U)

typedef enum { INPUT_FREE, INPUT_WRITING, INPUT_READY, INPUT_ENCODING } InputState;

__attribute__((section(".psram_bss"), aligned(32)))
static uint16_t inputs[INPUT_COUNT][USB_WEBCAM_WIDTH * USB_WEBCAM_HEIGHT];
__attribute__((section(".uncached_bss"), aligned(32)))
static uint8_t outputs[OUTPUT_COUNT][USB_WEBCAM_H264_MAX_SIZE];
extern uint8_t __uncached_bss_start__;
extern uint8_t __uncached_bss_end__;
static volatile InputState input_state[INPUT_COUNT];
static volatile uint32_t input_generation[INPUT_COUNT];
static volatile uint32_t next_input_generation;
static volatile int32_t latest_input;
static volatile bool output_busy[OUTPUT_COUNT];
static volatile bool streaming, force_idr, prepend_headers;
static volatile uint32_t stream_epoch;
static volatile uint32_t encoded_frames, repeated_frames, encode_dropped;
static volatile uint32_t last_bytes, maximum_encode_us, idr_count;
static UVCL_Callbacks_t callbacks;
static ID usb_irq_sem_id;

static uint32_t lock(void) { uint32_t key = __get_PRIMASK(); __disable_irq(); return key; }
static void unlock(uint32_t key) { __set_PRIMASK(key); }
static uint32_t cycles_to_us(uint32_t cycles)
{
  const uint32_t cycles_per_us = SystemCoreClock / 1000000U;
  assert(cycles_per_us != 0U);
  return cycles / cycles_per_us;
}

static void streaming_active(UVCL_Callbacks_t *cb, UVCL_StreamConf_t stream)
{
  (void)cb; (void)stream;
  uint32_t key = lock();
  stream_epoch++;
  force_idr = true; prepend_headers = true; streaming = true;
  unlock(key);
}
static void streaming_inactive(UVCL_Callbacks_t *cb)
{
  (void)cb;
  uint32_t key = lock();
  stream_epoch++;
  streaming = false;
  for (uint32_t i = 0; i < OUTPUT_COUNT; i++)
  {
    output_busy[i] = false;
  }
  unlock(key);
}
static void frame_release(UVCL_Callbacks_t *cb, void *frame)
{
  (void)cb;
  for (uint32_t i = 0; i < OUTPUT_COUNT; i++)
    if (frame == outputs[i]) { output_busy[i] = false; return; }
}

static void usb_service_task(INT stacd, void *exinf)
{
  (void)stacd; (void)exinf;
  for (;;) {
    assert(tk_wai_sem(usb_irq_sem_id, 1, TMO_FEVR) == E_OK);
    UVCL_IRQHandler();
    HAL_NVIC_EnableIRQ(USB1_OTG_HS_IRQn);
  }
}

static int32_t claim_input(uint32_t *generation)
{
  uint32_t key = lock();
  int32_t index = latest_input;
  if (index >= 0 && input_state[index] == INPUT_READY) {
    input_state[index] = INPUT_ENCODING;
    *generation = input_generation[index];
  } else index = -1;
  unlock(key);
  return index;
}
static void release_input(int32_t index)
{
  uint32_t key = lock();
  input_state[index] = (latest_input == index) ? INPUT_READY : INPUT_FREE;
  unlock(key);
}
static int32_t claim_output(void)
{
  uint32_t key = lock();
  int32_t index = -1;
  for (uint32_t i = 0; i < OUTPUT_COUNT; i++)
    if (!output_busy[i]) { output_busy[i] = true; index = (int32_t)i; break; }
  unlock(key);
  return index;
}

static void encode_once(uint32_t *previous_generation)
{
  uint32_t generation;
  int32_t in = claim_input(&generation);
  if (in < 0) { encode_dropped++; force_idr = true; return; }
  int32_t out = claim_output();
  if (out < 0) {
    release_input(in); encode_dropped++; force_idr = true; return;
  }
  const bool repeated = generation == *previous_generation;
  uint32_t key = lock();
  const bool idr = force_idr;
  const bool headers = prepend_headers;
  const uint32_t encoding_epoch = stream_epoch;
  force_idr = false; prepend_headers = false;
  unlock(key);
  SCB_CleanDCache_by_Addr(inputs[in], (int32_t)sizeof(inputs[in]));
  uint32_t started = DWT->CYCCNT;
  int32_t length = UsbH264Encoder_Encode(inputs[in], outputs[out], sizeof(outputs[out]), idr, headers);
  uint32_t elapsed = cycles_to_us(DWT->CYCCNT - started);
  release_input(in);
  if (elapsed > maximum_encode_us) maximum_encode_us = elapsed;
  if (length <= 0) {
    output_busy[out] = false; encode_dropped++; force_idr = true; prepend_headers = true; return;
  }
  key = lock();
  const bool stream_changed = !streaming || stream_epoch != encoding_epoch;
  if (stream_changed) {
    output_busy[out] = false;
    force_idr = true;
    prepend_headers = true;
  }
  unlock(key);
  if (stream_changed) {
    encode_dropped++;
    return;
  }
  if (UVCL_ShowFrame(outputs[out], length) != 0) {
    output_busy[out] = false; encode_dropped++; force_idr = true; prepend_headers = true; return;
  }
  *previous_generation = generation;
  encoded_frames++; last_bytes = (uint32_t)length;
  if (repeated) repeated_frames++;
  if (UsbH264Encoder_LastWasIdr()) idr_count++;
}

static void encoder_task(INT stacd, void *exinf)
{
  (void)stacd; (void)exinf;
  static const uint8_t cadence[] = {33U, 33U, 34U};
  uint32_t phase = 0, deadline = HAL_GetTick(), previous = UINT32_MAX;
  bool was_streaming = false;
  for (;;) {
    if (!streaming) { was_streaming = false; (void)tk_dly_tsk(10U); continue; }
    if (!was_streaming) { phase = 0; deadline = HAL_GetTick(); previous = UINT32_MAX; was_streaming = true; }
    encode_once(&previous);
    deadline += cadence[phase]; phase = (phase + 1U) % 3U;
    int32_t remaining = (int32_t)(deadline - HAL_GetTick());
    if (remaining > 0) (void)tk_dly_tsk((RELTIM)remaining);
  }
}

static const T_CTSK service_config = {
  .itskpri = SERVICE_PRIORITY, .stksz = TASK_STACK_SIZE,
  .task = usb_service_task, .tskatr = TA_HLNG | TA_RNG3,
};
static const T_CTSK encoder_config = {
  .itskpri = ENCODER_PRIORITY, .stksz = TASK_STACK_SIZE,
  .task = encoder_task, .tskatr = TA_HLNG | TA_RNG3,
};

void UsbWebcam_Init(void)
{
  memset(&__uncached_bss_start__, 0,
         (size_t)(&__uncached_bss_end__ - &__uncached_bss_start__));
  memset(inputs, 0, sizeof(inputs));
  latest_input = 0; input_state[0] = INPUT_READY; input_state[1] = INPUT_FREE;
  const T_CSEM sem = {.sematr = TA_TFIFO, .isemcnt = 0, .maxsem = 1};
  usb_irq_sem_id = tk_cre_sem(&sem); assert(usb_irq_sem_id > 0);
  UsbH264Encoder_Init(USB_WEBCAM_WIDTH, USB_WEBCAM_HEIGHT, USB_WEBCAM_FPS, USB_WEBCAM_BITRATE);
  UVCL_Conf_t config = {0};
  config.streams[0].payload_type = UVCL_PAYLOAD_FB_H264;
  config.streams[0].width = USB_WEBCAM_WIDTH; config.streams[0].height = USB_WEBCAM_HEIGHT;
  config.streams[0].fps = USB_WEBCAM_FPS;
  config.streams[0].dwMaxVideoFrameSize = USB_WEBCAM_H264_MAX_SIZE;
  config.streams_nb = 1; config.is_immediate_mode = 1;
  callbacks.streaming_active = streaming_active;
  callbacks.streaming_inactive = streaming_inactive;
  callbacks.frame_release = frame_release;
  assert(UVCL_Init(USB1_OTG_HS, &config, &callbacks) == 0);
  ID encoder_id = tk_cre_tsk(&encoder_config), service_id = tk_cre_tsk(&service_config);
  assert(encoder_id > 0 && service_id > 0);
  assert(tk_sta_tsk(encoder_id, 0) == E_OK && tk_sta_tsk(service_id, 0) == E_OK);
  HAL_NVIC_EnableIRQ(USB1_OTG_HS_IRQn);
}

void UsbWebcam_IRQHandler(void)
{
  HAL_NVIC_DisableIRQ(USB1_OTG_HS_IRQn); (void)tk_sig_sem(usb_irq_sem_id, 1);
}
void UsbWebcam_Venc_IRQHandler(void) { UsbH264Encoder_IRQHandler(); }

bool UsbWebcam_SubmitRgb565(const uint16_t *source, uint32_t source_width, uint32_t source_height)
{
  int32_t index = -1; uint32_t key = lock();
  for (uint32_t i = 0; i < INPUT_COUNT; i++)
    if (input_state[i] == INPUT_FREE) {
      input_state[i] = INPUT_WRITING; index = (int32_t)i; break;
    }
  if (index < 0)
    for (uint32_t i = 0; i < INPUT_COUNT; i++)
      if (input_state[i] == INPUT_READY) {
        input_state[i] = INPUT_WRITING; index = (int32_t)i; break;
      }
  unlock(key);
  if (index < 0) return false;
  UsbWebcam_CopyRgb565CenterDownsample(source, source_width, source_height,
      inputs[index], USB_WEBCAM_WIDTH, USB_WEBCAM_HEIGHT);
  SCB_CleanDCache_by_Addr(inputs[index], (int32_t)sizeof(inputs[index]));
  key = lock();
  int32_t old = latest_input;
  if (old >= 0 && old != index && input_state[old] == INPUT_READY) input_state[old] = INPUT_FREE;
  input_generation[index] = ++next_input_generation;
  input_state[index] = INPUT_READY; latest_input = index;
  unlock(key);
  return true;
}

uint32_t UsbWebcam_EncodedFrames(void) { return encoded_frames; }
uint32_t UsbWebcam_RepeatedFrames(void) { return repeated_frames; }
uint32_t UsbWebcam_EncodeDroppedFrames(void) { return encode_dropped; }
uint32_t UsbWebcam_LastBytes(void) { return last_bytes; }
uint32_t UsbWebcam_MaximumEncodeUs(void) { return maximum_encode_us; }
uint32_t UsbWebcam_IdrCount(void) { return idr_count; }
bool UsbWebcam_IsStreaming(void) { return streaming; }

void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd)
{
  assert(hpcd->Instance == USB1_OTG_HS);
  __HAL_RCC_PWR_CLK_ENABLE(); __HAL_RCC_PWR_CLK_SLEEP_ENABLE();
  HAL_PWREx_EnableVddUSBVMEN();
  while (__HAL_PWR_GET_FLAG(PWR_FLAG_USB33RDY) == 0U) {}
  HAL_PWREx_EnableVddUSB();
  __HAL_RCC_USB1_OTG_HS_FORCE_RESET(); __HAL_RCC_USB1_OTG_HS_RELEASE_RESET();
  __HAL_RCC_USB1_OTG_HS_CLK_ENABLE(); __HAL_RCC_USB1_OTG_HS_CLK_SLEEP_ENABLE();
  USB1_HS_PHYC->USBPHYC_CR = (USB1_HS_PHYC->USBPHYC_CR & ~(0x7U << 4U)) | (0x2U << 4U);
  __HAL_RCC_USB1_OTG_HS_PHY_CLK_ENABLE(); __HAL_RCC_USB1_OTG_HS_PHY_CLK_SLEEP_ENABLE();
  HAL_NVIC_SetPriority(USB1_OTG_HS_IRQn, 6U, 0U);
}
