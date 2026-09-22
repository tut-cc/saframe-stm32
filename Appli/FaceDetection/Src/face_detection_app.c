 /**
 ******************************************************************************
 * @file    main.c
 * @author  GPM Application Team
 *
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <tk/tkernel.h>
#include <tm/tmonitor.h>
#include "cmw_camera.h"
#include "stm32n6570_discovery_bus.h"
#include "stm32n6570_discovery_lcd.h"
#include "stm32n6570_discovery_xspi.h"
#include "stm32n6570_discovery.h"
#include "stm32_lcd.h"
#include "stm32_lcd_ex.h"
#include "app_postprocess.h"
#include "stai.h"
#include "stai_network.h"
#include "app_camerapipeline.h"
#include "face_detection_app.h"
#include "app_config.h"
#include "crop_img.h"
#include "privacy_filter.h"


#define LCD_FG_WIDTH  SCREEN_WIDTH
#define LCD_FG_HEIGHT SCREEN_HEIGHT
#define LCD_FG_FRAMEBUFFER_SIZE  (LCD_FG_WIDTH * LCD_FG_HEIGHT * 2)

#define NETWORK_WEIGHTS_ADDRESS  (0x70380000UL)
#define CAMERA_FRAME_TIMEOUT_MS  (3000U)
#define PRIVACY_RESULT_READY     (1U << 0)
#define PRIVACY_ROI_MARGIN_PC    (15U)
#define CONTROL_PERIOD_MS        (20U)
#define BUTTON_DEBOUNCE_MS       (200U)

#ifndef APP_GIT_SHA1_STRING
#define APP_GIT_SHA1_STRING "dev"
#endif
#ifndef APP_VERSION_STRING
#define APP_VERSION_STRING "unversioned"
#endif


typedef struct
{
  uint32_t X0;
  uint32_t Y0;
  uint32_t XSize;
  uint32_t YSize;
} Rectangle_TypeDef;

/* Lcd Background area */
Rectangle_TypeDef lcd_bg_area = {
#if ASPECT_RATIO_MODE == ASPECT_RATIO_CROP || ASPECT_RATIO_MODE == ASPECT_RATIO_FIT
  .X0 = (LCD_FG_WIDTH - LCD_FG_HEIGHT) / 2,
#else
  .X0 = 0,
#endif
  .Y0 = 0,
  .XSize = 0,
  .YSize = 0,
};

/* Lcd Foreground area */
Rectangle_TypeDef lcd_fg_area = {
  .X0 = 0,
  .Y0 = 0,
  .XSize = LCD_FG_WIDTH,
  .YSize = LCD_FG_HEIGHT,
};

#if POSTPROCESS_TYPE == POSTPROCESS_FD_BLAZEFACE_UI
  fd_blazeface_pp_static_param_t pp_params;
#elif POSTPROCESS_TYPE == POSTPROCESS_FD_YUNET_UI
  fd_yunet_pp_static_param_t pp_params;
#else
  #error "PostProcessing type not supported"
#endif

stai_ptr nn_in;
BSP_LCD_LayerConfig_t LayerConfig = {0};
void* pp_input;
fd_pp_out_t pp_output;

#define ALIGN_TO_16(value) (((value) + 15) & ~15)

/* When NN input dimensions are not a multiple of 16, the DCMIPP output needs cropping */
#if (STAI_NETWORK_IN_1_WIDTH * STAI_NETWORK_IN_1_CHANNEL) != ALIGN_TO_16(STAI_NETWORK_IN_1_WIDTH * STAI_NETWORK_IN_1_CHANNEL)
#define DCMIPP_NN_NEEDS_CROP 1
#define DCMIPP_OUT_NN_LEN (ALIGN_TO_16(STAI_NETWORK_IN_1_WIDTH * STAI_NETWORK_IN_1_CHANNEL) * STAI_NETWORK_IN_1_HEIGHT)
#define DCMIPP_OUT_NN_BUFF_LEN (DCMIPP_OUT_NN_LEN + 32 - DCMIPP_OUT_NN_LEN%32)

__attribute__ ((aligned (32)))
static uint8_t dcmipp_out_nn[DCMIPP_OUT_NN_BUFF_LEN];
#else
#define DCMIPP_NN_NEEDS_CROP 0
#endif

/* model */
STAI_NETWORK_CONTEXT_DECLARE(network_context, STAI_NETWORK_CONTEXT_SIZE)
/* Lcd Background Buffer */
__attribute__ ((section (".psram_bss")))
__attribute__ ((aligned (32)))
static uint8_t lcd_bg_buffer[800 * 480 * 2];
/* Lcd Foreground Buffer */
__attribute__ ((section (".psram_bss")))
__attribute__ ((aligned (32)))
static uint8_t lcd_fg_buffer[2][LCD_FG_WIDTH * LCD_FG_HEIGHT * 2];
static int lcd_fg_buffer_rd_idx;

static ID privacy_result_flag_id;
static ID privacy_result_mutex_id;
static ID privacy_display_mutex_id;
static PrivacyFrameResult privacy_results[2];
static uint32_t privacy_published_index;
static volatile PrivacyMode privacy_mode = PRIVACY_MODE_MASK;
static volatile uint32_t privacy_render_ms;

static void SystemClock_Config(void);
static void NPURam_enable(void);
static void NPUCache_config(void);
static void Display_Status(const char *message, uint32_t color);
static void LCD_init(void);
static void Security_Config(void);
static void set_clk_sleep_mode(void);
static void IAC_Config(void);
static void NeuralNetwork_init(uint32_t *nn_in_length, stai_ptr *nn_out, stai_size *number_output, int32_t nn_out_len[]);
static void StartPrivacyTasks(void);
static void PublishPrivacyResult(fd_pp_out_t *postprocess,
                                 uint32_t frame_number,
                                 uint32_t inference_ms,
                                 uint32_t vision_ms);
static void PrivacyRenderTask(INT stacd, void *exinf);
static void ControlMonitorTask(INT stacd, void *exinf);

static ID camera_frame_flag_id;

#define CAMERA_FRAME_READY (1U << 0)

static void CSI_InterruptHandler(UINT intno);
static void DCMIPP_InterruptHandler(UINT intno);
static void RegisterApplicationInterrupts(void);
static bool NetworkWeightsValid(void);

extern void NPU0_IRQHandler(void);

static const T_CTSK privacy_render_task_config = {
  .itskpri = 8,
  .stksz = 16 * 1024,
  .task = PrivacyRenderTask,
  .tskatr = TA_HLNG | TA_RNG3,
};

static const T_CTSK control_monitor_task_config = {
  .itskpri = 12,
  .stksz = 8 * 1024,
  .task = ControlMonitorTask,
  .tskatr = TA_HLNG | TA_RNG3,
};


/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
void FaceDetection_Run(void)
{
  T_CFLG camera_frame_flag = {
    .flgatr = TA_TFIFO | TA_WMUL,
    .iflgptn = 0,
  };

  camera_frame_flag_id = tk_cre_flg(&camera_frame_flag);
  assert(camera_frame_flag_id > 0);
  const T_CMTX display_mutex = {
    .mtxatr = TA_INHERIT,
    .ceilpri = 0,
  };
  privacy_display_mutex_id = tk_cre_mtx(&display_mutex);
  assert(privacy_display_mutex_id > 0);
  RegisterApplicationInterrupts();
  tm_putstring((UB *)"FD: application interrupts registered.\n");

  const bool weights_valid = NetworkWeightsValid();
  tm_printf((UB *)"FD: model weights at 0x%08x: %s.\n",
            NETWORK_WEIGHTS_ADDRESS, weights_valid ? "OK" : "MISSING OR INVALID");

  /*** NN Init ****************************************************************/
  uint32_t nn_in_len = 0;
  stai_size number_output = 0;
  stai_ptr nn_out[STAI_NETWORK_OUT_NUM] = {0};
  int32_t nn_out_len[STAI_NETWORK_OUT_NUM] = {0};

  NeuralNetwork_init(&nn_in_len, nn_out, &number_output, nn_out_len);
  tm_putstring((UB *)"FD: neural network initialized.\n");

  /*** Post Processing Init ***************************************************/
  stai_network_info info;
  int ret;

  ret = stai_network_get_info(network_context, &info);
  assert(ret == STAI_SUCCESS);
  app_postprocess_init(&pp_params, &info);

  /*** Camera Init ************************************************************/
  uint32_t pitch_nn = 0;
  CameraPipeline_Init(&lcd_bg_area.XSize, &lcd_bg_area.YSize, &pitch_nn);
  tm_putstring((UB *)"FD: camera pipeline initialized.\n");

  LCD_init();
  tm_putstring((UB *)"FD: LCD foreground layer initialized.\n");

  /* Start LCD Display camera pipe stream */
  CameraPipeline_DisplayPipe_Start(lcd_bg_buffer, CMW_MODE_CONTINUOUS);

  if (!weights_valid)
  {
    Display_Status("ERROR: program network_data.hex", UTIL_LCD_COLOR_RED);
    tm_putstring((UB *)"FD: inference disabled; program Model/network_data.hex to XSPI2.\n");
    while (1)
    {
      CameraPipeline_IspUpdate();
      (void)tk_dly_tsk(100);
    }
  }

  Display_Status("FD: waiting for camera frame", UTIL_LCD_COLOR_YELLOW);
  tm_putstring((UB *)"FD: display started; waiting for NN camera frame.\n");
  StartPrivacyTasks();

  /*** App Loop ***************************************************************/
  uint32_t frame_count = 0;
  while (1)
  {
    CameraPipeline_IspUpdate();

#if DCMIPP_NN_NEEDS_CROP
    /* Start NN camera single capture Snapshot into intermediate buffer */
    CameraPipeline_NNPipe_Start(dcmipp_out_nn, CMW_MODE_SNAPSHOT);
#else
    /* Start NN camera single capture Snapshot directly into NN input */
    CameraPipeline_NNPipe_Start(nn_in, CMW_MODE_SNAPSHOT);
#endif

    UINT frame_pattern;
    ER ercd = tk_wai_flg(camera_frame_flag_id, CAMERA_FRAME_READY,
                         TWF_ORW | TWF_BITCLR, &frame_pattern, CAMERA_FRAME_TIMEOUT_MS);
    if (ercd == E_TMOUT)
    {
      tm_putstring((UB *)"FD: ERROR: timed out waiting for DCMIPP pipe 2.\n");
      Display_Status("ERROR: NN camera timeout", UTIL_LCD_COLOR_RED);
      while (1)
      {
        CameraPipeline_IspUpdate();
        (void)tk_dly_tsk(100);
      }
    }
    assert(ercd == E_OK);

    uint32_t ts[2] = { 0 };

#if DCMIPP_NN_NEEDS_CROP
    /*
     * Crop the image: the DCMIPP hardware requires output dimensions to be
     * multiples of 16, so we crop the padded buffer into the NN input buffer.
     */
    SCB_InvalidateDCache_by_Addr(dcmipp_out_nn, sizeof(dcmipp_out_nn));
    img_crop(dcmipp_out_nn, nn_in, pitch_nn, STAI_NETWORK_IN_1_WIDTH, STAI_NETWORK_IN_1_HEIGHT, STAI_NETWORK_IN_1_CHANNEL);
    SCB_CleanInvalidateDCache_by_Addr(nn_in, nn_in_len);
#endif

    const uint32_t vision_started_at = HAL_GetTick();
    ts[0] = vision_started_at;
    if (frame_count == 0U)
    {
      Display_Status("FD: first inference running", UTIL_LCD_COLOR_GREEN);
      tm_putstring((UB *)"FD: first NN camera frame received; starting inference.\n");
    }
    /* run ATON inference */
    ret = stai_network_run(network_context, STAI_MODE_SYNC);
    assert(ret == 0);
    ts[1] = HAL_GetTick();

    int32_t pp_ret = app_postprocess_run((void **) nn_out, number_output, &pp_output, &pp_params);
    assert(pp_ret == 0);

    frame_count++;
    PublishPrivacyResult(&pp_output, frame_count, ts[1] - ts[0],
                         HAL_GetTick() - vision_started_at);
    /* Discard nn_out region (used by pp_input and pp_outputs variables) to avoid Dcache evictions during nn inference */
    for (int i = 0; i < number_output; i++)
    {
      void *tmp = nn_out[i];
      SCB_InvalidateDCache_by_Addr(tmp, nn_out_len[i]);
    }
  }
}


void FaceDetection_PreHALInit(void)
{
  /* Power on ICACHE */
  MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_ICACTIVE_Msk;

  /* Set back system and CPU clock source to HSI */
  __HAL_RCC_CPUCLK_CONFIG(RCC_CPUCLKSOURCE_HSI);
  __HAL_RCC_SYSCLK_CONFIG(RCC_SYSCLKSOURCE_HSI);
}


void FaceDetection_HardwareInit(void)
{

  SCB_EnableICache();

#if defined(USE_DCACHE)
  /* Power on DCACHE */
  MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_DCACTIVE_Msk;
  SCB_EnableDCache();
#endif

  SystemClock_Config();

  NPURam_enable();

  NPUCache_config();

  /*** External RAM and NOR Flash *********************************************/
  BSP_XSPI_RAM_Init(0);
  BSP_XSPI_RAM_EnableMemoryMappedMode(0);

  BSP_XSPI_NOR_Init_t NOR_Init;
  NOR_Init.InterfaceMode = BSP_XSPI_NOR_OPI_MODE;
  NOR_Init.TransferRate = BSP_XSPI_NOR_DTR_TRANSFER;
  BSP_XSPI_NOR_Init(0, &NOR_Init);
  BSP_XSPI_NOR_EnableMemoryMappedMode(0);

  /* Set all required IPs as secure privileged */
  Security_Config();

  IAC_Config();
  set_clk_sleep_mode();
}

void FaceDetection_CameraFrameCallback(void)
{
  if (camera_frame_flag_id > 0)
  {
    (void)tk_set_flg(camera_frame_flag_id, CAMERA_FRAME_READY);
  }
}

static void CSI_InterruptHandler(UINT intno)
{
  (void)intno;
  HAL_DCMIPP_CSI_IRQHandler(CMW_CAMERA_GetDCMIPPHandle());
}

static void DCMIPP_InterruptHandler(UINT intno)
{
  (void)intno;
  HAL_DCMIPP_IRQHandler(CMW_CAMERA_GetDCMIPPHandle());
}

static void RegisterApplicationInterrupts(void)
{
  const T_DINT csi_interrupt = {
    .intatr = TA_HLNG,
    .inthdr = (FP)CSI_InterruptHandler,
  };
  const T_DINT dcmipp_interrupt = {
    .intatr = TA_HLNG,
    .inthdr = (FP)DCMIPP_InterruptHandler,
  };
  const T_DINT npu_interrupt = {
    .intatr = TA_ASM,
    .inthdr = (FP)NPU0_IRQHandler,
  };

  assert(tk_def_int(CSI_IRQn, &csi_interrupt) == E_OK);
  assert(tk_def_int(DCMIPP_IRQn, &dcmipp_interrupt) == E_OK);
  assert(tk_def_int(NPU0_IRQn, &npu_interrupt) == E_OK);
}

static bool NetworkWeightsValid(void)
{
  /* Signature of the YuNet (yunetn_320_qdq_int8.onnx) network_data.hex,
   * generated via Model/generate-n6-model_STM32N6570-DK.sh. The weights blob
   * is only ~94 KB (vs. ~1.2 MB for ST-YOLOX), so the offsets are closer
   * together and stay within its smaller address span. */
  static const struct
  {
    uint32_t offset;
    uint32_t expected;
  } signature[] = {
    { 0x00000U, 0xFC31952DU },
    { 0x08000U, 0x2117D020U },
    { 0x16000U, 0x0502F34BU },
  };

  for (uint32_t i = 0; i < (sizeof(signature) / sizeof(signature[0])); i++)
  {
    const volatile uint32_t *word =
        (const volatile uint32_t *)(NETWORK_WEIGHTS_ADDRESS + signature[i].offset);
    if (*word != signature[i].expected)
    {
      return false;
    }
  }

  return true;
}

static void NeuralNetwork_init(uint32_t *nn_in_length, stai_ptr *nn_out, stai_size *number_output, int32_t nn_out_len[])
{
  stai_network_info info;
  int ret;

  /* initialize runtime */
  ret = stai_runtime_init();
  assert(ret == STAI_SUCCESS);
  /* init model instance */
  ret = stai_network_init(network_context);
  assert(ret == STAI_SUCCESS);

  ret = stai_network_get_info(network_context, &info);
  assert(ret == STAI_SUCCESS);
  assert(info.n_inputs == 1);
  *number_output = STAI_NETWORK_OUT_NUM;

  /* Get the input buffer size & address */
  *nn_in_length = info.inputs[0].size_bytes;
  ret = stai_network_get_inputs(network_context, &nn_in, (stai_size *)&info.n_inputs);
  assert(ret == STAI_SUCCESS);

  /* Get the output buffers size & address */
  ret = stai_network_get_outputs(network_context, nn_out, number_output);
  assert(ret == STAI_SUCCESS);
  for (int i = 0; i < *number_output; i++)
  {
    nn_out_len[i] = info.outputs[i].size_bytes;
  }
}

static void NPURam_enable(void)
{
  __HAL_RCC_NPU_CLK_ENABLE();
  __HAL_RCC_NPU_FORCE_RESET();
  __HAL_RCC_NPU_RELEASE_RESET();

  /* Enable NPU RAMs (4x448KB) */
  __HAL_RCC_AXISRAM3_MEM_CLK_ENABLE();
  __HAL_RCC_AXISRAM4_MEM_CLK_ENABLE();
  __HAL_RCC_AXISRAM5_MEM_CLK_ENABLE();
  __HAL_RCC_AXISRAM6_MEM_CLK_ENABLE();
  __HAL_RCC_RAMCFG_CLK_ENABLE();
  RAMCFG_HandleTypeDef hramcfg = {0};
  hramcfg.Instance =  RAMCFG_SRAM3_AXI;
  HAL_RAMCFG_EnableAXISRAM(&hramcfg);
  hramcfg.Instance =  RAMCFG_SRAM4_AXI;
  HAL_RAMCFG_EnableAXISRAM(&hramcfg);
  hramcfg.Instance =  RAMCFG_SRAM5_AXI;
  HAL_RAMCFG_EnableAXISRAM(&hramcfg);
  hramcfg.Instance =  RAMCFG_SRAM6_AXI;
  HAL_RAMCFG_EnableAXISRAM(&hramcfg);
}

static void set_clk_sleep_mode(void)
{
  /*** Enable sleep mode support during NPU inference *************************/
  /* Configure peripheral clocks to remain active during sleep mode */
  /* Keep all IP's enabled during WFE so they can wake up CPU. Fine tune
   * this if you want to save maximum power
   */
  __HAL_RCC_XSPI1_CLK_SLEEP_ENABLE();    /* For display frame buffer */
  __HAL_RCC_XSPI2_CLK_SLEEP_ENABLE();    /* For NN weights */
  __HAL_RCC_NPU_CLK_SLEEP_ENABLE();      /* For NN inference */
  __HAL_RCC_CACHEAXI_CLK_SLEEP_ENABLE(); /* For NN inference */
  __HAL_RCC_LTDC_CLK_SLEEP_ENABLE();     /* For display */
  __HAL_RCC_DMA2D_CLK_SLEEP_ENABLE();    /* For display */
  __HAL_RCC_DCMIPP_CLK_SLEEP_ENABLE();   /* For camera configuration retention */
  __HAL_RCC_CSI_CLK_SLEEP_ENABLE();      /* For camera configuration retention */

  __HAL_RCC_FLEXRAM_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_AXISRAM1_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_AXISRAM2_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_AXISRAM3_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_AXISRAM4_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_AXISRAM5_MEM_CLK_SLEEP_ENABLE();
  __HAL_RCC_AXISRAM6_MEM_CLK_SLEEP_ENABLE(); 

}

static void NPUCache_config(void)
{
  npu_cache_enable();
}

static void Security_Config(void)
{
  __HAL_RCC_RIFSC_CLK_ENABLE();
  RIMC_MasterConfig_t RIMC_master = {0};
  RIMC_master.MasterCID = RIF_CID_1;
  RIMC_master.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_NPU, &RIMC_master);
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DMA2D, &RIMC_master);
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DCMIPP, &RIMC_master);
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC1 , &RIMC_master);
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_LTDC2 , &RIMC_master);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_NPU , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DMA2D , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_CSI    , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_DCMIPP , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDC   , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL1 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_LTDCL2 , RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
}

static void IAC_Config(void)
{
/* Configure IAC to trap illegal access events */
  __HAL_RCC_IAC_CLK_ENABLE();
  __HAL_RCC_IAC_FORCE_RESET();
  __HAL_RCC_IAC_RELEASE_RESET();
}

void IAC_IRQHandler(void)
{
  while (1)
  {
  }
}

/* Display functions */
static int clamp_point(int *x, int *y)
{
  int xi = *x;
  int yi = *y;

  if (*x < (int)lcd_bg_area.X0)
    *x = lcd_bg_area.X0;
  if (*y < (int)lcd_bg_area.Y0)
    *y = lcd_bg_area.Y0;
  if (*x >= lcd_bg_area.X0 + lcd_bg_area.XSize)
    *x = lcd_bg_area.X0 + lcd_bg_area.XSize - 1;
  if (*y >= lcd_bg_area.Y0 + lcd_bg_area.YSize)
    *y = lcd_bg_area.Y0 + lcd_bg_area.YSize - 1;

  return (xi != *x) || (yi != *y);
}

static void convert_length(float32_t wi, float32_t hi, int *wo, int *ho)
{
  *wo = lcd_bg_area.XSize * wi;
  *ho = lcd_bg_area.YSize * hi;
}

static void convert_point(float32_t xi, float32_t yi, int *xo, int *yo)
{
  *xo = lcd_bg_area.XSize * xi + lcd_bg_area.X0;
  *yo = lcd_bg_area.YSize * yi + lcd_bg_area.Y0;
}

static PrivacyRoi MakePrivacyRoi(const fd_pp_outBuffer_t *detection)
{
  int center_x, center_y;
  int width, height;

  convert_point(detection->x_center, detection->y_center, &center_x, &center_y);
  convert_length(detection->width, detection->height, &width, &height);

  const int margin_x = (width * PRIVACY_ROI_MARGIN_PC) / 100;
  const int margin_y = (height * PRIVACY_ROI_MARGIN_PC) / 100;
  int x0 = center_x - ((width + 1) / 2) - margin_x;
  int y0 = center_y - ((height + 1) / 2) - margin_y;
  int x1 = center_x + ((width + 1) / 2) + margin_x;
  int y1 = center_y + ((height + 1) / 2) + margin_y;

  clamp_point(&x0, &y0);
  clamp_point(&x1, &y1);

  PrivacyRoi roi = {0};
  if ((x1 > x0) && (y1 > y0))
  {
    roi.x = (int16_t)x0;
    roi.y = (int16_t)y0;
    roi.width = (uint16_t)(x1 - x0 + 1);
    roi.height = (uint16_t)(y1 - y0 + 1);
  }
  return roi;
}

static void PublishPrivacyResult(fd_pp_out_t *postprocess,
                                 uint32_t frame_number,
                                 uint32_t inference_ms,
                                 uint32_t vision_ms)
{
  PrivacyFrameResult result = {
    .frame_number = frame_number,
    .inference_ms = inference_ms,
    .vision_ms = vision_ms,
  };

  result.face_count = (uint32_t)postprocess->nb_detect;
  if (result.face_count > PRIVACY_MAX_FACES)
  {
    result.face_count = PRIVACY_MAX_FACES;
  }
  for (uint32_t i = 0; i < result.face_count; i++)
  {
    result.faces[i] = MakePrivacyRoi(&postprocess->pOutBuff[i]);
  }

  assert(tk_loc_mtx(privacy_result_mutex_id, TMO_FEVR) == E_OK);
  const uint32_t next_index = 1U - privacy_published_index;
  privacy_results[next_index] = result;
  privacy_published_index = next_index;
  assert(tk_unl_mtx(privacy_result_mutex_id) == E_OK);
  assert(tk_set_flg(privacy_result_flag_id, PRIVACY_RESULT_READY) == E_OK);
}

static void Display_Status(const char *message, uint32_t color)
{
  int ret;

  assert(tk_loc_mtx(privacy_display_mutex_id, TMO_FEVR) == E_OK);

  ret = HAL_LTDC_SetAddress_NoReload(&hlcd_ltdc,
                                     (uint32_t)lcd_fg_buffer[lcd_fg_buffer_rd_idx],
                                     LTDC_LAYER_2);
  assert(ret == HAL_OK);

  UTIL_LCD_FillRect(lcd_fg_area.X0, lcd_fg_area.Y0,
                    lcd_fg_area.XSize, lcd_fg_area.YSize, 0x00000000);
  UTIL_LCD_SetTextColor(color);
  UTIL_LCD_SetBackColor(0x80000000);
  UTIL_LCDEx_PrintfAt(0, LINE(10), CENTER_MODE, "%s", message);
  UTIL_LCD_SetBackColor(0);
  UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_WHITE);

  SCB_CleanDCache_by_Addr(lcd_fg_buffer[lcd_fg_buffer_rd_idx],
                          LCD_FG_FRAMEBUFFER_SIZE);
  ret = HAL_LTDC_ReloadLayer(&hlcd_ltdc, LTDC_RELOAD_VERTICAL_BLANKING,
                             LTDC_LAYER_2);
  assert(ret == HAL_OK);
  lcd_fg_buffer_rd_idx = 1 - lcd_fg_buffer_rd_idx;
  assert(tk_unl_mtx(privacy_display_mutex_id) == E_OK);
}

static void PrivacyRenderTask(INT stacd, void *exinf)
{
  (void)stacd;
  (void)exinf;

  while (1)
  {
    UINT pattern;
    assert(tk_wai_flg(privacy_result_flag_id, PRIVACY_RESULT_READY,
                      TWF_ORW | TWF_BITCLR, &pattern, TMO_FEVR) == E_OK);

    PrivacyFrameResult result;
    assert(tk_loc_mtx(privacy_result_mutex_id, TMO_FEVR) == E_OK);
    result = privacy_results[privacy_published_index];
    assert(tk_unl_mtx(privacy_result_mutex_id) == E_OK);

    const uint32_t render_started_at = HAL_GetTick();
    const PrivacyMode mode = privacy_mode;
    assert(tk_loc_mtx(privacy_display_mutex_id, TMO_FEVR) == E_OK);
    PrivacyRenderTarget target = {
      .overlay = (uint16_t *)lcd_fg_buffer[lcd_fg_buffer_rd_idx],
      .overlay_width = LCD_FG_WIDTH,
      .overlay_height = LCD_FG_HEIGHT,
      .background = (const uint16_t *)lcd_bg_buffer,
      .background_width = lcd_bg_area.XSize,
      .background_height = lcd_bg_area.YSize,
      .background_x = lcd_bg_area.X0,
      .background_y = lcd_bg_area.Y0,
    };

    if (mode == PRIVACY_MODE_MOSAIC)
    {
      SCB_InvalidateDCache_by_Addr(lcd_bg_buffer, sizeof(lcd_bg_buffer));
    }
    PrivacyFilter_Render(&result, mode, &target);

    const int ret = HAL_LTDC_SetAddress_NoReload(
        &hlcd_ltdc, (uint32_t)lcd_fg_buffer[lcd_fg_buffer_rd_idx], LTDC_LAYER_2);
    assert(ret == HAL_OK);

    UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_WHITE);
    UTIL_LCD_SetBackColor(0xA0000000);
    UTIL_LCDEx_PrintfAt(0, LINE(1), CENTER_MODE, "%s | Faces %u",
                       PrivacyFilter_ModeName(mode), result.face_count);
    UTIL_LCDEx_PrintfAt(0, LINE(20), CENTER_MODE, "AI %ums | Vision %ums | Draw %ums",
                       result.inference_ms, result.vision_ms, privacy_render_ms);
    UTIL_LCD_SetBackColor(0);

    SCB_CleanDCache_by_Addr(lcd_fg_buffer[lcd_fg_buffer_rd_idx],
                            LCD_FG_FRAMEBUFFER_SIZE);
    assert(HAL_LTDC_ReloadLayer(&hlcd_ltdc, LTDC_RELOAD_VERTICAL_BLANKING,
                                LTDC_LAYER_2) == HAL_OK);
    lcd_fg_buffer_rd_idx = 1 - lcd_fg_buffer_rd_idx;
    privacy_render_ms = HAL_GetTick() - render_started_at;
    assert(tk_unl_mtx(privacy_display_mutex_id) == E_OK);
  }
}

static void ControlMonitorTask(INT stacd, void *exinf)
{
  (void)stacd;
  (void)exinf;

  uint32_t previous_button = 0U;
  uint32_t last_switch_at = HAL_GetTick() - BUTTON_DEBOUNCE_MS;
  uint32_t last_log_at = HAL_GetTick();

  while (1)
  {
    const uint32_t now = HAL_GetTick();
    const uint32_t button = BSP_PB_GetState(BUTTON_USER1);
    if ((button != 0U) && (previous_button == 0U) &&
        ((now - last_switch_at) >= BUTTON_DEBOUNCE_MS))
    {
      privacy_mode = (privacy_mode == PRIVACY_MODE_MASK) ?
                     PRIVACY_MODE_MOSAIC : PRIVACY_MODE_MASK;
      last_switch_at = now;
      (void)tk_set_flg(privacy_result_flag_id, PRIVACY_RESULT_READY);
      tm_printf((UB *)"PRIVACY: mode changed to %s.\n",
                PrivacyFilter_ModeName(privacy_mode));
    }
    previous_button = button;

    if ((now - last_log_at) >= 1000U)
    {
      PrivacyFrameResult result;
      assert(tk_loc_mtx(privacy_result_mutex_id, TMO_FEVR) == E_OK);
      result = privacy_results[privacy_published_index];
      assert(tk_unl_mtx(privacy_result_mutex_id) == E_OK);
      tm_printf((UB *)"FD: frame=%u mode=%s detections=%u ai=%ums vision=%ums draw=%ums.\n",
                result.frame_number, PrivacyFilter_ModeName(privacy_mode),
                result.face_count, result.inference_ms, result.vision_ms,
                privacy_render_ms);
      last_log_at = now;
    }

    (void)tk_dly_tsk(CONTROL_PERIOD_MS);
  }
}

static void StartPrivacyTasks(void)
{
  const T_CFLG result_flag = {
    .flgatr = TA_TFIFO | TA_WMUL,
    .iflgptn = 0,
  };
  const T_CMTX result_mutex = {
    .mtxatr = TA_INHERIT,
    .ceilpri = 0,
  };

  privacy_result_flag_id = tk_cre_flg(&result_flag);
  privacy_result_mutex_id = tk_cre_mtx(&result_mutex);
  assert(privacy_result_flag_id > 0);
  assert(privacy_result_mutex_id > 0);
  assert(BSP_PB_Init(BUTTON_USER1, BUTTON_MODE_GPIO) == BSP_ERROR_NONE);

  const ID render_task_id = tk_cre_tsk(&privacy_render_task_config);
  const ID control_task_id = tk_cre_tsk(&control_monitor_task_config);
  assert(render_task_id > 0);
  assert(control_task_id > 0);
  assert(tk_sta_tsk(render_task_id, 0) == E_OK);
  assert(tk_sta_tsk(control_task_id, 0) == E_OK);

  tm_putstring((UB *)"PRIVACY: render and control tasks started; default mode MASK.\n");
}

static void LCD_init(void)
{
  BSP_LCD_Init(0, LCD_ORIENTATION_LANDSCAPE);

  /* Preview layer Init */
  LayerConfig.X0          = lcd_bg_area.X0;
  LayerConfig.Y0          = lcd_bg_area.Y0;
  LayerConfig.X1          = lcd_bg_area.X0 + lcd_bg_area.XSize;
  LayerConfig.Y1          = lcd_bg_area.Y0 + lcd_bg_area.YSize;
  LayerConfig.PixelFormat = LCD_PIXEL_FORMAT_RGB565;
  LayerConfig.Address     = (uint32_t) lcd_bg_buffer;

  BSP_LCD_ConfigLayer(0, LTDC_LAYER_1, &LayerConfig);

  LayerConfig.X0 = lcd_fg_area.X0;
  LayerConfig.Y0 = lcd_fg_area.Y0;
  LayerConfig.X1 = lcd_fg_area.X0 + lcd_fg_area.XSize;
  LayerConfig.Y1 = lcd_fg_area.Y0 + lcd_fg_area.YSize;
  LayerConfig.PixelFormat = LCD_PIXEL_FORMAT_ARGB4444;
  LayerConfig.Address = (uint32_t) lcd_fg_buffer; /* External XSPI1 PSRAM */

  BSP_LCD_ConfigLayer(0, LTDC_LAYER_2, &LayerConfig);
  UTIL_LCD_SetFuncDriver(&LCD_Driver);
  UTIL_LCD_SetLayer(LTDC_LAYER_2);
  UTIL_LCD_Clear(0x00000000);
  UTIL_LCD_SetFont(&Font20);
  UTIL_LCD_SetTextColor(UTIL_LCD_COLOR_WHITE);
}

/**
  * @brief  DCMIPP Clock Config for DCMIPP.
  * @param  hdcmipp  DCMIPP Handle
  *         Being __weak it can be overwritten by the application
  * @retval HAL_status
  */
HAL_StatusTypeDef MX_DCMIPP_ClockConfig(DCMIPP_HandleTypeDef *hdcmipp)
{
  RCC_PeriphCLKInitTypeDef RCC_PeriphCLKInitStruct = {0};
  HAL_StatusTypeDef ret = HAL_OK;

  RCC_PeriphCLKInitStruct.PeriphClockSelection = RCC_PERIPHCLK_DCMIPP;
  RCC_PeriphCLKInitStruct.DcmippClockSelection = RCC_DCMIPPCLKSOURCE_IC17;
  RCC_PeriphCLKInitStruct.ICSelection[RCC_IC17].ClockSelection = RCC_ICCLKSOURCE_PLL2;
  RCC_PeriphCLKInitStruct.ICSelection[RCC_IC17].ClockDivider = 3;
  ret = HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct);
  if (ret)
  {
    return ret;
  }

  RCC_PeriphCLKInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CSI;
  RCC_PeriphCLKInitStruct.ICSelection[RCC_IC18].ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_PeriphCLKInitStruct.ICSelection[RCC_IC18].ClockDivider = 40;
  ret = HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct);
  if (ret)
  {
    return ret;
  }

  return ret;
}

static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_PeriphCLKInitTypeDef RCC_PeriphCLKInitStruct = {0};

  /* Ensure VDDCORE=0.9V before increasing the system frequency */
  BSP_SMPS_Init(SMPS_VOLTAGE_OVERDRIVE);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_NONE;

  /* PLL1 = 64 x 25 / 2 = 800MHz */
  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL1.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL1.PLLM = 2;
  RCC_OscInitStruct.PLL1.PLLN = 25;
  RCC_OscInitStruct.PLL1.PLLFractional = 0;
  RCC_OscInitStruct.PLL1.PLLP1 = 1;
  RCC_OscInitStruct.PLL1.PLLP2 = 1;

  /* PLL2 = 64 x 125 / 8 = 1000MHz */
  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL2.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL2.PLLM = 8;
  RCC_OscInitStruct.PLL2.PLLFractional = 0;
  RCC_OscInitStruct.PLL2.PLLN = 125;
  RCC_OscInitStruct.PLL2.PLLP1 = 1;
  RCC_OscInitStruct.PLL2.PLLP2 = 1;

  /* PLL3 = (64 x 225 / 8) / (1 * 2) = 900MHz */
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL3.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL3.PLLM = 8;
  RCC_OscInitStruct.PLL3.PLLN = 225;
  RCC_OscInitStruct.PLL3.PLLFractional = 0;
  RCC_OscInitStruct.PLL3.PLLP1 = 1;
  RCC_OscInitStruct.PLL3.PLLP2 = 2;

  /* PLL4 = (64 x 225 / 8) / (6 * 6) = 50 MHz */
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL4.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL4.PLLM = 8;
  RCC_OscInitStruct.PLL4.PLLFractional = 0;
  RCC_OscInitStruct.PLL4.PLLN = 225;
  RCC_OscInitStruct.PLL4.PLLP1 = 6;
  RCC_OscInitStruct.PLL4.PLLP2 = 6;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    while(1);
  }

  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK |
                                 RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 |
                                 RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK4 |
                                 RCC_CLOCKTYPE_PCLK5);

  /* CPU CLock (sysa_ck) = ic1_ck = PLL1 output/ic1_divider = 800 MHz */
  RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_IC2_IC6_IC11;
  RCC_ClkInitStruct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC1Selection.ClockDivider = 1;

  /* AXI Clock (sysb_ck) = ic2_ck = PLL1 output/ic2_divider = 400 MHz */
  RCC_ClkInitStruct.IC2Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC2Selection.ClockDivider = 2;

  /* NPU Clock (sysc_ck) = ic6_ck = PLL2 output/ic6_divider = 1000 MHz */
  RCC_ClkInitStruct.IC6Selection.ClockSelection = RCC_ICCLKSOURCE_PLL2;
  RCC_ClkInitStruct.IC6Selection.ClockDivider = 1;

  /* AXISRAM3/4/5/6 Clock (sysd_ck) = ic11_ck = PLL3 output/ic11_divider = 900 MHz */
  RCC_ClkInitStruct.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL3;
  RCC_ClkInitStruct.IC11Selection.ClockDivider = 1;

  /* HCLK = sysb_ck / HCLK divider = 200 MHz */
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;

  /* PCLKx = HCLK / PCLKx divider = 200 MHz */
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
  RCC_ClkInitStruct.APB5CLKDivider = RCC_APB5_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
  {
    while(1);
  }

  RCC_PeriphCLKInitStruct.PeriphClockSelection = 0;

  /* XSPI1 kernel clock (ck_ker_xspi1) = HCLK = 200MHz */
  RCC_PeriphCLKInitStruct.PeriphClockSelection |= RCC_PERIPHCLK_XSPI1;
  RCC_PeriphCLKInitStruct.Xspi1ClockSelection = RCC_XSPI1CLKSOURCE_HCLK;

  /* XSPI2 kernel clock (ck_ker_xspi1) = HCLK =  200MHz */
  RCC_PeriphCLKInitStruct.PeriphClockSelection |= RCC_PERIPHCLK_XSPI2;
  RCC_PeriphCLKInitStruct.Xspi2ClockSelection = RCC_XSPI2CLKSOURCE_HCLK;

  if (HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct) != HAL_OK)
  {
    while (1);
  }
}

void npu_cache_enable_clocks_and_reset(void)
{
  __HAL_RCC_CACHEAXIRAM_MEM_CLK_ENABLE();
  __HAL_RCC_CACHEAXI_CLK_ENABLE();
  __HAL_RCC_CACHEAXI_FORCE_RESET();
  __HAL_RCC_CACHEAXI_RELEASE_RESET();
}

void npu_cache_disable_clocks_and_reset(void)
{
  __HAL_RCC_CACHEAXIRAM_MEM_CLK_DISABLE();
  __HAL_RCC_CACHEAXI_CLK_DISABLE();
  __HAL_RCC_CACHEAXI_FORCE_RESET();
}
