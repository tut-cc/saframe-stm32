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

void CameraPipeline_Init(uint32_t *lcd_bg_width, uint32_t *lcd_bg_height, uint32_t *pitch_nn);
void CameraPipeline_DeInit(void);
void CameraPipeline_Start(void);
void CameraPipeline_DisplayPipe_Start(uint8_t *display_pipe_dst, uint32_t cam_mode);
void CameraPipeline_DisplayPipe_Stop(void);
void CameraPipeline_NNPipe_Start(uint8_t *nn_pipe_dst, uint32_t cam_mode);
typedef struct
{
  uint32_t isp_errors;
  int32_t isp_last_error;
  uint32_t sensor_write_retries;
  uint32_t sensor_write_failures;
} CameraPipelineIspDiagnostics;

void CameraPipeline_IspUpdate(void);
void CameraPipeline_GetIspDiagnostics(CameraPipelineIspDiagnostics *diagnostics);
/* Program the destination latched at the next frame start of a running pipe. */
void CameraPipeline_SetPipeAddress(uint32_t pipe, uint8_t *dst);

#endif