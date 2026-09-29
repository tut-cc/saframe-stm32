#include "usb_h264_encoder.h"

#include <assert.h>
#include <string.h>

#include <tk/tkernel.h>

#include "ewl.h"
#include "ewl_impl.h"
#include "h264encapi.h"
#include "stm32n6xx_hal.h"
#include "stm32n6xx_ll_venc.h"

#define VENC_LINEAR_POOL_SIZE (4U * 1024U * 1024U)
#define VENC_SOFTWARE_POOL_SIZE (512U * 1024U)
#define VENC_HEADER_MAX_SIZE (256U)
#define VENC_WAIT_TIMEOUT_MS (100U)
#define VENC_INITIAL_QP (25)

__attribute__((section(".psram_bss"), aligned(32)))
static uint8_t venc_linear_pool[VENC_LINEAR_POOL_SIZE];
__attribute__((section(".psram_bss"), aligned(32)))
static uint8_t venc_software_pool[VENC_SOFTWARE_POOL_SIZE];

static uint8_t *venc_linear_next = venc_linear_pool;
static uint8_t *venc_software_next = venc_software_pool;
__attribute__((section(".uncached_bss"), aligned(32)))
static uint8_t venc_headers[VENC_HEADER_MAX_SIZE];
static size_t venc_headers_length;
static H264EncInst venc_instance;
static uint64_t venc_picture_count;
static uint32_t venc_gop_length;
static ID venc_irq_sem_id;
static bool venc_last_was_idr;

static size_t append_alignment_nal(uint8_t *output, size_t length, size_t capacity)
{
  size_t padding = 8U - (((uintptr_t)output + length) % 8U);
  if (padding == 8U)
  {
    return length;
  }
  if (padding < 6U)
  {
    padding += 8U;
  }
  assert(length + padding <= capacity);
  output[length++] = 0x00U;
  output[length++] = 0x00U;
  output[length++] = 0x00U;
  output[length++] = 0x01U;
  output[length++] = 0x2cU;
  while (padding-- > 5U)
  {
    output[length++] = 0xffU;
  }
  return length;
}

static uintptr_t align_up(uintptr_t value, uintptr_t alignment)
{
  return (value + alignment - 1U) & ~(alignment - 1U);
}

void UsbH264Encoder_Init(uint32_t width, uint32_t height, uint32_t fps,
                         uint32_t bitrate)
{
  const T_CSEM semaphore = {
    .sematr = TA_TFIFO,
    .isemcnt = 0,
    .maxsem = 1,
  };
  venc_irq_sem_id = tk_cre_sem(&semaphore);
  assert(venc_irq_sem_id > 0);

  /* VENCRAM ownership is read through SYSCFG by LL_VENC_Init(). */
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  LL_VENC_Init();
  __HAL_RCC_VENC_FORCE_RESET();
  __HAL_RCC_VENC_RELEASE_RESET();

  H264EncConfig config = {0};
  config.streamType = H264ENC_BYTE_STREAM;
  config.viewMode = H264ENC_BASE_VIEW_SINGLE_BUFFER;
  config.level = H264ENC_LEVEL_3;
  config.width = width;
  config.height = height;
  config.frameRateNum = fps;
  config.frameRateDenom = 1U;
  config.refFrameAmount = 1U;
  assert(H264EncInit(&config, &venc_instance) == H264ENC_OK);

  H264EncPreProcessingCfg preprocessing;
  assert(H264EncGetPreProcessing(venc_instance, &preprocessing) == H264ENC_OK);
  preprocessing.inputType = H264ENC_RGB565;
  preprocessing.origWidth = width;
  preprocessing.origHeight = height;
  assert(H264EncSetPreProcessing(venc_instance, &preprocessing) == H264ENC_OK);

  H264EncCodingCtrl coding;
  assert(H264EncGetCodingCtrl(venc_instance, &coding) == H264ENC_OK);
  /* SPS/PPS are cached below and explicitly prepended on stream (re)sync. */
  coding.idrHeader = 0U;
  assert(H264EncSetCodingCtrl(venc_instance, &coding) == H264ENC_OK);

  H264EncRateCtrl rate;
  assert(H264EncGetRateCtrl(venc_instance, &rate) == H264ENC_OK);
  rate.pictureRc = 1U;
  rate.mbRc = 1U;
  rate.pictureSkip = 0U;
  rate.hrd = 0U;
  rate.qpHdr = VENC_INITIAL_QP;
  rate.qpMin = 10U;
  rate.qpMax = 51U;
  rate.gopLen = fps;
  rate.bitPerSecond = bitrate;
  rate.intraQpDelta = 0;
  assert(H264EncSetRateCtrl(venc_instance, &rate) == H264ENC_OK);

  venc_gop_length = fps;
  venc_picture_count = 0U;

  H264EncIn input = {0};
  H264EncOut output = {0};
  input.pOutBuf = (u32 *)venc_headers;
  input.busOutBuf = (ptr_t)venc_headers;
  input.outBufSize = sizeof(venc_headers);
  assert(H264EncStrmStart(venc_instance, &input, &output) == H264ENC_OK);
  assert(output.streamSize <= sizeof(venc_headers));
  venc_headers_length = append_alignment_nal(venc_headers, output.streamSize,
                                              sizeof(venc_headers));

  HAL_NVIC_SetPriority(VENC_IRQn, 7U, 0U);
  HAL_NVIC_EnableIRQ(VENC_IRQn);
}

int32_t UsbH264Encoder_Encode(const uint16_t *input, uint8_t *output,
                              size_t output_capacity, bool force_idr,
                              bool prepend_headers)
{
  /* Do not let a completion that arrived after a prior timeout satisfy this frame. */
  while (tk_wai_sem(venc_irq_sem_id, 1, TMO_POL) == E_OK)
  {
  }

  size_t prefix_length = 0U;
  if (prepend_headers)
  {
    if (venc_headers_length > output_capacity)
    {
      return -1;
    }
    memcpy(output, venc_headers, venc_headers_length);
    prefix_length = venc_headers_length;
  }

  H264EncIn enc_input = {0};
  H264EncOut enc_output = {0};
  enc_input.busLuma = (ptr_t)input;
  enc_input.pOutBuf = (u32 *)(output + prefix_length);
  enc_input.busOutBuf = (ptr_t)(output + prefix_length);
  enc_input.outBufSize = output_capacity - prefix_length;
  venc_last_was_idr = force_idr || ((venc_picture_count % venc_gop_length) == 0U);
  enc_input.codingType = venc_last_was_idr
                           ? H264ENC_INTRA_FRAME : H264ENC_PREDICTED_FRAME;
  enc_input.timeIncrement = 1U;
  enc_input.ipf = H264ENC_REFERENCE_AND_REFRESH;
  enc_input.ltrf = H264ENC_NO_REFERENCE_NO_REFRESH;
  enc_input.lineBufWrCnt = 0U;
  enc_input.sendAUD = 0U;

  const H264EncRet result = H264EncStrmEncode(venc_instance, &enc_input,
                                               &enc_output, NULL, NULL, NULL);
  if (result != H264ENC_FRAME_READY)
  {
    return -1;
  }
  venc_picture_count++;
  if (enc_output.streamSize > (output_capacity - prefix_length))
  {
    return -1;
  }
  return (int32_t)(prefix_length + enc_output.streamSize);
}

bool UsbH264Encoder_LastWasIdr(void)
{
  return venc_last_was_idr;
}

void UsbH264Encoder_IRQHandler(void)
{
  VENC_IRQHandler();
}

void EWLUserSignalFromISR(void)
{
  if (venc_irq_sem_id > 0)
  {
    (void)tk_sig_sem(venc_irq_sem_id, 1);
  }
}

i32 EWLWaitHwRdy(const void *instance, u32 *slices_ready)
{
  if (instance == NULL || venc_irq_sem_id <= 0)
  {
    return EWL_HW_WAIT_ERROR;
  }
  if (tk_wai_sem(venc_irq_sem_id, 1, VENC_WAIT_TIMEOUT_MS) != E_OK)
  {
    return EWL_HW_WAIT_TIMEOUT;
  }
  if (slices_ready != NULL)
  {
    *slices_ready = 0U;
  }
  return EWL_OK;
}

void *EWLmalloc(u32 size)
{
  const uintptr_t next = align_up((uintptr_t)venc_software_next, 8U);
  const uintptr_t end = next + size;
  assert(end <= (uintptr_t)(venc_software_pool + sizeof(venc_software_pool)));
  venc_software_next = (uint8_t *)end;
  return (void *)next;
}

void *EWLcalloc(u32 count, u32 size)
{
  void *memory = EWLmalloc(count * size);
  memset(memory, 0, count * size);
  return memory;
}

void EWLfree(void *memory)
{
  (void)memory;
}

i32 EWLMallocLinear(const void *instance, u32 size, EWLLinearMem_t *info)
{
  (void)instance;
  const uintptr_t next = align_up((uintptr_t)venc_linear_next, 32U);
  const uintptr_t end = next + size;
  if (end > (uintptr_t)(venc_linear_pool + sizeof(venc_linear_pool)))
  {
    return EWL_ERROR;
  }
  info->size = size;
  info->virtualAddress = (u32 *)next;
  info->busAddress = (ptr_t)next;
  venc_linear_next = (uint8_t *)end;
  return EWL_OK;
}

void EWLFreeLinear(const void *instance, EWLLinearMem_t *info)
{
  (void)instance;
  (void)info;
}
