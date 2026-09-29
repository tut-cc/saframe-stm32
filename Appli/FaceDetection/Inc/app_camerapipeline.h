 /**
 ******************************************************************************
 * @file    app_camerapipeline.h
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
#ifndef APP_CAMERAPIPELINE
#define APP_CAMERAPIPELINE

#include <stdint.h>

#define SCREEN_HEIGHT (480)
#define SCREEN_WIDTH  (800)

/* Sanitized frame shared by the LCD and the USB webcam (Pipe 1 output). */
#define PRIVACY_FRAME_WIDTH  (1280U)
#define PRIVACY_FRAME_HEIGHT (720U)

/* Where the Pipe 1 frame sits inside the centered square seen by the NN, in
 * sensor pixels. The frame spans the full square width, so only a vertical
 * offset is needed to map NN coordinates onto it. */
typedef struct
{
  uint32_t square_size;
  uint32_t y_in_square;
  uint32_t height_in_square;
} CameraPipeline_FrameWindow;

void CameraPipeline_Init(uint32_t *frame_width, uint32_t *frame_height, uint32_t *pitch_nn);
CameraPipeline_FrameWindow CameraPipeline_GetFrameWindow(void);
void CameraPipeline_DeInit(void);
void CameraPipeline_Start(void);
void CameraPipeline_DisplayPipe_Start(uint8_t *display_pipe_dst, uint32_t cam_mode);
void CameraPipeline_DisplayPipe_Stop(void);
void CameraPipeline_NNPipe_Start(uint8_t *nn_pipe_dst, uint32_t cam_mode);
void CameraPipeline_IspUpdate(void);

#endif