 /**
 ******************************************************************************
 * @file    cmw_camera_conf.h
 * @author  GPM Application Team
 *
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef CMW_CAMERA_CONF_H
#define CMW_CAMERA_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#if defined (STM32N657xx)
#include "stm32n6xx_hal.h"
#ifdef USE_STM32N6570_NUCLEO_REV_B01
#include "stm32n6xx_nucleo_bus.h"
#else
#include "stm32n6570_discovery_bus.h"
#endif
#else
#error Add header files for your specific board
#endif


/* ########################## Module Selection ############################## */
/**
  * @brief This is the list of modules to be used in the HAL driver
  */
#define USE_IMX335_SENSOR

/* The IMX335 occasionally NACKs a register write while streaming; retry it
 * instead of failing the ISP background process. */
int32_t CameraPipeline_SensorWriteReg16(uint16_t DevAddr, uint16_t Reg,
                                        uint8_t *pData, uint16_t Length);
#define CMW_I2C_WRITEREG16 CameraPipeline_SensorWriteReg16

#ifdef __cplusplus
}
#endif

#endif /* CMW_CAMERA_CONF_H */
