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

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

/* Leave the driver use the default resolution */
#define CAMERA_WIDTH 0
#define CAMERA_HEIGHT 0
#define CAMERA_FPS 30

#if ASPECT_RATIO_MODE != ASPECT_RATIO_CROP
#error "The Pipe 1 frame window assumes the NN pipe crops a centered square."
#endif

static CameraPipeline_FrameWindow frame_window;

/* Pipe 1 takes a 16:9 window from the vertical middle of the square the NN
 * pipe crops, so every published pixel has been seen by the NN. */
static void DCMIPP_PipeInitDisplay(CMW_CameraInit_t *camConf, uint32_t *frame_width, uint32_t *frame_height)
{
  CMW_DCMIPP_Conf_t dcmipp_conf = {0};
  int ret;

  const uint32_t square_size = MIN(camConf->width, camConf->height);
  const uint32_t square_x = (camConf->width - square_size + 1U) / 2U;
  const uint32_t square_y = (camConf->height - square_size + 1U) / 2U;
  /* Keep offsets and height even so the Bayer phase is preserved. */
  const uint32_t height_in_square =
    ((square_size * PRIVACY_FRAME_HEIGHT / PRIVACY_FRAME_WIDTH) + 1U) & ~1U;
  const uint32_t y_in_square = ((square_size - height_in_square) / 2U) & ~1U;
  assert(height_in_square <= square_size);

  frame_window.square_size = square_size;
  frame_window.y_in_square = y_in_square;
  frame_window.height_in_square = height_in_square;

  *frame_width = PRIVACY_FRAME_WIDTH;
  *frame_height = PRIVACY_FRAME_HEIGHT;

  dcmipp_conf.output_width = PRIVACY_FRAME_WIDTH;
  dcmipp_conf.output_height = PRIVACY_FRAME_HEIGHT;
  dcmipp_conf.output_format = DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1;
  dcmipp_conf.output_bpp = 2;
  dcmipp_conf.mode = CMW_Aspect_ratio_manual_roi;
  dcmipp_conf.manual_conf.width = square_size;
  dcmipp_conf.manual_conf.height = height_in_square;
  dcmipp_conf.manual_conf.offset_x = square_x;
  dcmipp_conf.manual_conf.offset_y = square_y + y_in_square;
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
* @param frame_width sanitized frame width
* @param frame_height sanitized frame height
* @param pitch_nn output pitch computed by the CMW
*/
void CameraPipeline_Init(uint32_t *frame_width, uint32_t *frame_height, uint32_t *pitch_nn)
{
  int ret;
  CMW_CameraInit_t cam_conf;

  cam_conf.width = CAMERA_WIDTH;
  cam_conf.height = CAMERA_HEIGHT;
  cam_conf.fps = CAMERA_FPS;
  cam_conf.mirror_flip = CAMERA_FLIP;

  ret = CMW_CAMERA_Init(&cam_conf, NULL);
  assert(ret == CMW_ERROR_NONE);
  DCMIPP_PipeInitDisplay(&cam_conf, frame_width, frame_height);
  DCMIPP_PipeInitNn(pitch_nn);
}

CameraPipeline_FrameWindow CameraPipeline_GetFrameWindow(void)
{
  assert(frame_window.square_size != 0U);
  return frame_window;
}

void CameraPipeline_DeInit(void)
{
  int ret;
  ret = CMW_CAMERA_DeInit();
  assert(ret == CMW_ERROR_NONE);
}

void CameraPipeline_DisplayPipe_Start(uint8_t *display_pipe_dst, uint32_t cam_mode)
{
  int ret;
  ret = CMW_CAMERA_Start(DCMIPP_PIPE1, display_pipe_dst, cam_mode);
  assert(ret == CMW_ERROR_NONE);
}

void CameraPipeline_NNPipe_Start(uint8_t *nn_pipe_dst, uint32_t cam_mode)
{
  int ret;

  ret = CMW_CAMERA_Start(DCMIPP_PIPE2, nn_pipe_dst, cam_mode);
  assert(ret == CMW_ERROR_NONE);
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
