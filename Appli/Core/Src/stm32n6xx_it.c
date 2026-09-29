/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32n6xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32n6xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <tm/tmonitor.h>
#include "face_detection_diagnostics.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

#define APP_FAULT_RECORD_MAGIC (0x53414645UL)

typedef struct
{
  uint32_t magic;
  uint32_t kind;
  uint32_t frame;
  uint32_t stage;
  uint32_t pc;
  uint32_t lr;
  uint32_t sp;
  uint32_t cfsr;
  uint32_t hfsr;
  uint32_t mmfar;
  uint32_t bfar;
  uint32_t line;
  const char *file;
} AppFaultRecord;

__attribute__((section(".noinit")))
static volatile AppFaultRecord app_fault_record;

volatile uint32_t g_app_diagnostic_frame;
volatile AppDiagnosticStage g_app_diagnostic_stage = APP_STAGE_STARTUP;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*
 * Called from the naked fault handlers below, which clear MSPLIM first: on a
 * stack-limit fault (CFSR.STKOF) SP sits at the limit, so this function's own
 * pushes would fault again. If stacking itself overflowed, the frame at
 * 'stack' is incomplete and pc/lr are not meaningful; rely on cfsr/sp/stage.
 */
__attribute__((noreturn))
void AppDiagnostics_RecordFault(uint32_t *stack, uint32_t kind)
{
  __disable_irq();
  app_fault_record.magic = APP_FAULT_RECORD_MAGIC;
  app_fault_record.kind = kind;
  app_fault_record.frame = g_app_diagnostic_frame;
  app_fault_record.stage = (uint32_t)g_app_diagnostic_stage;
  app_fault_record.pc = stack[6];
  app_fault_record.lr = stack[5];
  app_fault_record.sp = (uint32_t)stack;
  app_fault_record.cfsr = SCB->CFSR;
  app_fault_record.hfsr = SCB->HFSR;
  app_fault_record.mmfar = SCB->MMFAR;
  app_fault_record.bfar = SCB->BFAR;
  app_fault_record.line = 0U;
  app_fault_record.file = NULL;
  __DSB();
  while (1)
  {
    __WFI();
  }
}

void AppDiagnostics_RecordAssert(const char *file, uint32_t line)
{
  uint32_t link_register;
  __asm volatile("mov %0, lr" : "=r"(link_register));
  __disable_irq();
  app_fault_record.magic = APP_FAULT_RECORD_MAGIC;
  app_fault_record.kind = APP_FAULT_ASSERT;
  app_fault_record.frame = g_app_diagnostic_frame;
  app_fault_record.stage = (uint32_t)g_app_diagnostic_stage;
  app_fault_record.pc = (uint32_t)__builtin_return_address(0);
  app_fault_record.lr = link_register;
  app_fault_record.sp = (__get_CONTROL() & CONTROL_SPSEL_Msk) != 0U ? __get_PSP() : __get_MSP();
  app_fault_record.cfsr = SCB->CFSR;
  app_fault_record.hfsr = SCB->HFSR;
  app_fault_record.mmfar = SCB->MMFAR;
  app_fault_record.bfar = SCB->BFAR;
  app_fault_record.line = line;
  app_fault_record.file = file;
  __DSB();
  while (1)
  {
    __WFI();
  }
}

static const char *AppDiagnostics_FaultName(uint32_t kind)
{
  switch ((AppFaultKind)kind)
  {
    case APP_FAULT_HARD: return "HardFault";
    case APP_FAULT_MEMMANAGE: return "MemManage";
    case APP_FAULT_BUS: return "BusFault";
    case APP_FAULT_USAGE: return "UsageFault";
    case APP_FAULT_ASSERT: return "assert";
    default: return "unknown";
  }
}

static const char *AppDiagnostics_StageName(uint32_t stage)
{
  switch ((AppDiagnosticStage)stage)
  {
    case APP_STAGE_STARTUP: return "startup";
    case APP_STAGE_CAMERA_CAPTURE: return "camera_capture";
    case APP_STAGE_NPU_INFERENCE: return "npu_inference";
    case APP_STAGE_POSTPROCESS: return "postprocess";
    case APP_STAGE_CACHE_INVALIDATE: return "cache_invalidate";
    case APP_STAGE_PRIVACY_FILTER: return "privacy_filter";
    case APP_STAGE_CACHE_CLEAN: return "cache_clean";
    case APP_STAGE_LTDC_RELOAD: return "ltdc_reload";
    case APP_STAGE_VBLANK_WAIT: return "vblank_wait";
    case APP_STAGE_MONITOR: return "monitor";
    default: return "unknown";
  }
}

void AppDiagnostics_ReportPreviousFault(void)
{
  if (app_fault_record.magic != APP_FAULT_RECORD_MAGIC)
  {
    return;
  }

  tm_printf((UB *)"FAULT: kind=%s frame=%u stage=%s pc=0x%08x lr=0x%08x sp=0x%08x.\n",
            AppDiagnostics_FaultName(app_fault_record.kind), app_fault_record.frame,
            AppDiagnostics_StageName(app_fault_record.stage),
            app_fault_record.pc, app_fault_record.lr, app_fault_record.sp);
  tm_printf((UB *)"FAULT: CFSR=0x%08x HFSR=0x%08x MMFAR=0x%08x BFAR=0x%08x.\n",
            app_fault_record.cfsr, app_fault_record.hfsr,
            app_fault_record.mmfar, app_fault_record.bfar);
  if (app_fault_record.kind == APP_FAULT_ASSERT)
  {
    tm_printf((UB *)"FAULT: assert %s:%u.\n",
              app_fault_record.file != NULL ? app_fault_record.file : "?",
              app_fault_record.line);
  }
  app_fault_record.magic = 0U;
}

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern I2C_HandleTypeDef hi2c1;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
__attribute__((naked)) void HardFault_Handler(void)
{
  __asm volatile("movs r2, #0\n"
                 "msr msplim, r2\n"
                 "tst lr, #4\n"
                 "ite eq\n"
                 "mrseq r0, msp\n"
                 "mrsne r0, psp\n"
                 "movs r1, #1\n"
                 "b AppDiagnostics_RecordFault\n");
}

/**
  * @brief This function handles Memory management fault.
  */
__attribute__((naked)) void MemManage_Handler(void)
{
  __asm volatile("movs r2, #0\n"
                 "msr msplim, r2\n"
                 "tst lr, #4\n"
                 "ite eq\n"
                 "mrseq r0, msp\n"
                 "mrsne r0, psp\n"
                 "movs r1, #2\n"
                 "b AppDiagnostics_RecordFault\n");
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
__attribute__((naked)) void BusFault_Handler(void)
{
  __asm volatile("movs r2, #0\n"
                 "msr msplim, r2\n"
                 "tst lr, #4\n"
                 "ite eq\n"
                 "mrseq r0, msp\n"
                 "mrsne r0, psp\n"
                 "movs r1, #3\n"
                 "b AppDiagnostics_RecordFault\n");
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
__attribute__((naked)) void UsageFault_Handler(void)
{
  __asm volatile("movs r2, #0\n"
                 "msr msplim, r2\n"
                 "tst lr, #4\n"
                 "ite eq\n"
                 "mrseq r0, msp\n"
                 "mrsne r0, psp\n"
                 "movs r1, #4\n"
                 "b AppDiagnostics_RecordFault\n");
}

/**
  * @brief This function handles Secure fault.
  */
void SecureFault_Handler(void)
{
  /* USER CODE BEGIN SecureFault_IRQn 0 */

  /* USER CODE END SecureFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_SecureFault_IRQn 0 */
    /* USER CODE END W1_SecureFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32N6xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32n6xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles ADC1 and ADC2 global interrupt.
  */
void ADC1_2_IRQHandler(void)
{
  /* USER CODE BEGIN ADC1_2_IRQn 0 */

  /* USER CODE END ADC1_2_IRQn 0 */
  HAL_ADC_IRQHandler(&hadc1);
  HAL_ADC_IRQHandler(&hadc2);
  /* USER CODE BEGIN ADC1_2_IRQn 1 */

  /* USER CODE END ADC1_2_IRQn 1 */
}

/**
  * @brief This function handles I2C1 Event interrupt.
  */
void I2C1_EV_IRQHandler(void)
{
  /* USER CODE BEGIN I2C1_EV_IRQn 0 */

  /* USER CODE END I2C1_EV_IRQn 0 */
  HAL_I2C_EV_IRQHandler(&hi2c1);
  /* USER CODE BEGIN I2C1_EV_IRQn 1 */

  /* USER CODE END I2C1_EV_IRQn 1 */
}

/**
  * @brief This function handles I2C1 Error interrupt.
  */
void I2C1_ER_IRQHandler(void)
{
  /* USER CODE BEGIN I2C1_ER_IRQn 0 */

  /* USER CODE END I2C1_ER_IRQn 0 */
  HAL_I2C_ER_IRQHandler(&hi2c1);
  /* USER CODE BEGIN I2C1_ER_IRQn 1 */

  /* USER CODE END I2C1_ER_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
