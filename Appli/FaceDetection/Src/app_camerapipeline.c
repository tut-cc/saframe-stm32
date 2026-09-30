 /**
 ******************************************************************************
 * @file    app_camerapipeline.c
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
#include "cmw_camera.h"
#include "app_camerapipeline.h"
#include "app_config.h"
#include "crop_img.h"
#include "face_detection_app.h"
#include "face_detection_diagnostics.h"
#include "stai_network.h"

#undef assert
#define assert(condition) APP_ASSERT(condition)

/* Leave the driver use the default resolution */
#define CAMERA_WIDTH 0
#define CAMERA_HEIGHT 0
#define CAMERA_FPS 30

static void DCMIPP_PipeInitDisplay(CMW_CameraInit_t *camConf, uint32_t *bg_width, uint32_t *bg_height)
{
  CMW_Aspect_Ratio_Mode_t aspect_ratio;
  CMW_DCMIPP_Conf_t dcmipp_conf = {0};
  int ret;

  if (ASPECT_RATIO_MODE == ASPECT_RATIO_CROP)
  {
    aspect_ratio = CMW_Aspect_ratio_crop;
  }
  else if (ASPECT_RATIO_MODE == ASPECT_RATIO_FIT)
  {
    aspect_ratio = CMW_Aspect_ratio_fit;
  }
  else if (ASPECT_RATIO_MODE == ASPECT_RATIO_FULLSCREEN)
  {
    aspect_ratio = CMW_Aspect_ratio_fullscreen;
  }

  int lcd_bg_width;
  int lcd_bg_height;

  lcd_bg_height = (camConf->height <= SCREEN_HEIGHT) ? camConf->height : SCREEN_HEIGHT;

#if ASPECT_RATIO_MODE == ASPECT_RATIO_FULLSCREEN
  lcd_bg_width = (((camConf->width*lcd_bg_height)/camConf->height) - ((camConf->width*lcd_bg_height)/camConf->height) % 16);
#else
  lcd_bg_width = (camConf->height <= SCREEN_HEIGHT) ? camConf->height : SCREEN_HEIGHT;
#endif

  *bg_width = lcd_bg_width;
  *bg_height = lcd_bg_height;

  dcmipp_conf.output_width = lcd_bg_width;
  dcmipp_conf.output_height = lcd_bg_height;
  dcmipp_conf.output_format = DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1;
  dcmipp_conf.output_bpp = 2;
  dcmipp_conf.mode = aspect_ratio;
  dcmipp_conf.enable_gamma_conversion = 0;
  uint32_t pitch;
  ret = CMW_CAMERA_SetPipeConfig(DCMIPP_PIPE1, &dcmipp_conf, &pitch);
  assert(ret == HAL_OK);
  assert(dcmipp_conf.output_width * dcmipp_conf.output_bpp == pitch);
}

static void DCMIPP_PipeInitNn(uint32_t *pitch)
{
  CMW_Aspect_Ratio_Mode_t aspect_ratio;
  CMW_DCMIPP_Conf_t dcmipp_conf;
  int ret;

  if (ASPECT_RATIO_MODE == ASPECT_RATIO_CROP)
  {
    aspect_ratio = CMW_Aspect_ratio_crop;
  }
  else if (ASPECT_RATIO_MODE == ASPECT_RATIO_FIT)
  {
    aspect_ratio = CMW_Aspect_ratio_fit;
  }
  else if (ASPECT_RATIO_MODE == ASPECT_RATIO_FULLSCREEN)
  {
    aspect_ratio = CMW_Aspect_ratio_fit;
  }

  dcmipp_conf.output_width = STAI_NETWORK_IN_1_WIDTH;
  dcmipp_conf.output_height = STAI_NETWORK_IN_1_HEIGHT;
  dcmipp_conf.output_format = DCMIPP_PIXEL_PACKER_FORMAT_RGB888_YUV444_1;
  dcmipp_conf.output_bpp = STAI_NETWORK_IN_1_CHANNEL;
  dcmipp_conf.mode = aspect_ratio;
  dcmipp_conf.enable_swap = COLOR_MODE;
  dcmipp_conf.enable_gamma_conversion = 0;
  ret = CMW_CAMERA_SetPipeConfig(DCMIPP_PIPE2, &dcmipp_conf, pitch);
  assert(ret == HAL_OK);
}

/**
* @brief Init the camera and the 2 DCMIPP pipes
* @param lcd_bg_width display width
* @param lcd_bg_height display height
* @param pitch_nn output pitch computed by the CMW
*/
void CameraPipeline_Init(uint32_t *lcd_bg_width, uint32_t *lcd_bg_height, uint32_t *pitch_nn)
{
  int ret;
  CMW_CameraInit_t cam_conf;

  cam_conf.width = CAMERA_WIDTH;
  cam_conf.height = CAMERA_HEIGHT;
  cam_conf.fps = CAMERA_FPS;
  cam_conf.mirror_flip = CAMERA_FLIP;

  ret = CMW_CAMERA_Init(&cam_conf, NULL);
  assert(ret == CMW_ERROR_NONE);
  DCMIPP_PipeInitDisplay(&cam_conf, lcd_bg_width, lcd_bg_height);
  DCMIPP_PipeInitNn(pitch_nn);
}

void CameraPipeline_DeInit(void)
{
  int ret;
  ret = CMW_CAMERA_DeInit();
  assert(ret == CMW_ERROR_NONE);
}

static void StartPipe(uint32_t pipe, uint8_t *dst, uint32_t cam_mode)
{
  /* The HAL start ORs the mode into PxFCTCR.CPTMODE, so a pipe once started in
   * snapshot mode would stay in snapshot mode. Set the mode field explicitly. */
  HAL_StatusTypeDef hal_ret = HAL_DCMIPP_PIPE_SetCaptureMode(
      CMW_CAMERA_GetDCMIPPHandle(), pipe, cam_mode);
  assert(hal_ret == HAL_OK);
  int ret = CMW_CAMERA_Start(pipe, dst, cam_mode);
  assert(ret == CMW_ERROR_NONE);
}

void CameraPipeline_DisplayPipe_Start(uint8_t *display_pipe_dst, uint32_t cam_mode)
{
  StartPipe(DCMIPP_PIPE1, display_pipe_dst, cam_mode);
}

void CameraPipeline_NNPipe_Start(uint8_t *nn_pipe_dst, uint32_t cam_mode)
{
  StartPipe(DCMIPP_PIPE2, nn_pipe_dst, cam_mode);
}

void CameraPipeline_DisplayPipe_Stop()
{
  int ret;
  ret = CMW_CAMERA_Suspend(DCMIPP_PIPE1);
  assert(ret == CMW_ERROR_NONE);
}

void CameraPipeline_IspUpdate(void)
{
  int ret = CMW_ERROR_NONE;
  ret = CMW_CAMERA_Run();
  assert(ret == CMW_ERROR_NONE);
}

void CameraPipeline_SetPipeAddress(uint32_t pipe, uint8_t *dst)
{
  const HAL_StatusTypeDef ret = HAL_DCMIPP_PIPE_SetMemoryAddress(
      CMW_CAMERA_GetDCMIPPHandle(), pipe, DCMIPP_MEMORY_ADDRESS_0, (uint32_t)dst);
  assert(ret == HAL_OK);
}

/**
  * @brief  Vsync event callback
  * @param  pipe DCMIPP pipe starting a new frame
  * @retval None
  */
int CMW_CAMERA_PIPE_VsyncEventCallback(uint32_t pipe)
{
  switch (pipe)
  {
    case DCMIPP_PIPE1 :
    case DCMIPP_PIPE2 :
      FaceDetection_CameraVsyncCallback(pipe);
      break;
  }
  return 0;
}

/**
  * @brief  Frame event callback
  * @param  hdcmipp pointer to the DCMIPP handle
  * @retval None
  */
int CMW_CAMERA_PIPE_FrameEventCallback(uint32_t pipe)
{
  switch (pipe)
  {
    case DCMIPP_PIPE1 :
    case DCMIPP_PIPE2 :
      FaceDetection_CameraFrameCallback(pipe);
      break;
  }
  return 0;
}

void CMW_CAMERA_PIPE_ErrorCallback(uint32_t pipe)
{
  /* FIXME : Need to tune sensor/ipplug so we can remove this implementation */
}
