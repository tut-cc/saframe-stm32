#include "usb_webcam_venc.h"

#include <assert.h>
#include <stddef.h>

#include "stm32n6xx_hal.h"
#include "stm32n6xx_ll_venc.h"
#include <tk/tkernel.h>
#include <tm/tmonitor.h>
#include "ewl.h"
#include "ewl_impl.h"
#include "jpegencapi.h"

/* The JPEG encoder only allocates its instance from EWL, so a small pool is
 * enough. Nothing in it is ever read by the VENC itself. */
#define VENC_POOL_SIZE (64U * 1024U)
#define VENC_WAIT_TIMEOUT_MS (100)
#define VENC_IRQ_PRIORITY (6U)

__attribute__((section(".psram_bss")))
__attribute__((aligned(32)))
static uint8_t venc_pool[VENC_POOL_SIZE];
static uint32_t venc_pool_used;

static ID venc_irq_sem_id;
static JpegEncInst venc_jpeg;
static JpegEncCfg venc_config;
static int32_t venc_last_error;

/* EWL memory: a bump allocator over venc_pool. The encoder instance lives for
 * the whole run, so nothing is ever returned. */
void *EWLmalloc(u32 n)
{
  const uint32_t size = ALIGNED_SIZE(n);
  if (size > VENC_POOL_SIZE - venc_pool_used)
  {
    return NULL;
  }
  void *block = &venc_pool[venc_pool_used];
  venc_pool_used += size;
  return block;
}

void EWLfree(void *p)
{
  (void)p;
}

/* The JPEG path never hands linear memory to the hardware. Refusing it keeps
 * a cached buffer from ever being shared with the VENC by accident. */
i32 EWLMallocLinear(const void *inst, u32 size, EWLLinearMem_t *info)
{
  (void)inst;
  (void)size;
  (void)info;
  return EWL_ERROR;
}

void EWLFreeLinear(const void *inst, EWLLinearMem_t *info)
{
  (void)inst;
  (void)info;
}

void EWLPoolChoiceCb(uint8_t **pool_ptr, size_t *size)
{
  *pool_ptr = NULL;
  *size = 0U;
}

void EWLPoolReleaseCb(uint8_t **pool_ptr)
{
  (void)pool_ptr;
}

i32 EWLWaitHwRdy(const void *inst, u32 *slicesReady)
{
  if (inst == NULL)
  {
    return EWL_HW_WAIT_ERROR;
  }
  if (tk_wai_sem(venc_irq_sem_id, 1, VENC_WAIT_TIMEOUT_MS) != E_OK)
  {
    return EWL_HW_WAIT_TIMEOUT;
  }
  if (slicesReady != NULL)
  {
    *slicesReady = (VENC_REG(21UL) >> 16) & 0xFFUL;
  }
  return EWL_HW_WAIT_OK;
}

void UsbWebcamVenc_IRQHandler(void)
{
  /* Clears the slice/IRQ bits and leaves the frame status for the encoder. */
  VENC_IRQHandler();
  (void)tk_sig_sem(venc_irq_sem_id, 1);
}

void UsbWebcamVenc_Init(uint32_t width, uint32_t height, uint32_t quality_level)
{
  const T_CSEM irq_sem_config = {
    .sematr = TA_TFIFO,
    .isemcnt = 0,
    .maxsem = 1,
  };
  venc_irq_sem_id = tk_cre_sem(&irq_sem_config);
  assert(venc_irq_sem_id > 0);

  /* SystemInit turns the SYSCFG clock off again, and reading SYSCFG without
   * its clock stalls the bus. VENCRAM must be owned by the encoder, not
   * mapped as system RAM. */
  tm_putstring((UB *)"VENC: initializing.\n");
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  const uint32_t vencram_control = SYSCFG->VENCRAMCR;
  tm_printf((UB *)"VENC: VENCRAMCR=0x%08x.\n", vencram_control);
  assert(!LL_VENC_IS_VENCRAM_SYSTEM_ACCESSIBLE());

  LL_VENC_Init();
  __HAL_RCC_VENC_FORCE_RESET();
  __HAL_RCC_VENC_RELEASE_RESET();
  /* Keep the encoder clocked while the CPU sleeps during NPU inference. */
  __HAL_RCC_VENC_CLK_SLEEP_ENABLE();
  __HAL_RCC_VENCRAM_MEM_CLK_SLEEP_ENABLE();
  tm_printf((UB *)"VENC: clocked; ASIC ID=0x%08x.\n", EWLReadAsicID());
  HAL_NVIC_SetPriority(VENC_IRQn, VENC_IRQ_PRIORITY, 0U);
  HAL_NVIC_EnableIRQ(VENC_IRQn);

  venc_config.inputWidth = width;
  venc_config.inputHeight = height;
  venc_config.codingWidth = width;
  venc_config.codingHeight = height;
  venc_config.xOffset = 0U;
  venc_config.yOffset = 0U;
  venc_config.restartInterval = 0U;
  venc_config.qLevel = quality_level;
  venc_config.frameType = JPEGENC_RGB565;
  venc_config.colorConversion.type = JPEGENC_RGBTOYUV_BT601;
  venc_config.rotation = JPEGENC_ROTATE_0;
  venc_config.codingType = JPEGENC_WHOLE_FRAME;
  venc_config.codingMode = JPEGENC_420_MODE;
  venc_config.unitsType = JPEGENC_NO_UNITS;
  venc_config.markerType = JPEGENC_SINGLE_MARKER;
  venc_config.xDensity = 1U;
  venc_config.yDensity = 1U;

  venc_last_error = JpegEncInit(&venc_config, &venc_jpeg);
  assert(venc_last_error == JPEGENC_OK);
  venc_last_error = JpegEncSetPictureSize(venc_jpeg, &venc_config);
  assert(venc_last_error == JPEGENC_OK);
  tm_printf((UB *)"VENC: JPEG %ux%u q%u ready; pool %u/%u bytes.\n",
            width, height, quality_level, venc_pool_used, VENC_POOL_SIZE);
}

uint32_t UsbWebcamVenc_Encode(const uint16_t *frame, uint8_t *destination,
                              uint32_t destination_size)
{
  /* Drop a completion left over from an encode that timed out. */
  while (tk_wai_sem(venc_irq_sem_id, 1, TMO_POL) == E_OK)
  {
  }

  JpegEncIn input = {0};
  input.frameHeader = 1U;
  input.busLum = (size_t)frame;
  input.pLum = (const u8 *)frame;
  input.pOutBuf = destination;
  input.busOutBuf = (size_t)destination;
  input.outBufSize = destination_size;

  JpegEncOut output = {0};
  JpegEncRet ret;
  do
  {
    ret = JpegEncEncode(venc_jpeg, &input, &output, NULL, NULL);
  } while (ret == JPEGENC_RESTART_INTERVAL);

  if (ret != JPEGENC_FRAME_READY)
  {
    venc_last_error = ret;
    return 0U;
  }

  /* The header is written by the CPU and the scan by the VENC. PSRAM is
   * write-through in the default memory map (the MPU is not enabled), so the
   * header already reached memory and dropping stale lines is enough. */
  SCB_InvalidateDCache_by_Addr(destination, (int32_t)output.jfifSize);
  return output.jfifSize;
}

uint32_t UsbWebcamVenc_PoolUsedBytes(void)
{
  return venc_pool_used;
}

int32_t UsbWebcamVenc_LastError(void)
{
  return venc_last_error;
}
