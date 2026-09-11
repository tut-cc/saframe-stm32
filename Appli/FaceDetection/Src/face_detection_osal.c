#include <stdbool.h>
#include <stdint.h>

#include <tk/tkernel.h>
#include "stm32n6xx_hal.h"

static volatile bool kernel_started;

void FaceDetection_OSStarted(void)
{
  kernel_started = true;
}

uint32_t HAL_GetTick(void)
{
  if (!kernel_started)
  {
    return uwTick;
  }

  SYSTIM time;
  if (tk_get_tim(&time) != E_OK)
  {
    return uwTick;
  }

  return time.lo;
}

void HAL_Delay(uint32_t delay)
{
  if (kernel_started)
  {
    if (delay > 0U)
    {
      (void)tk_dly_tsk((RELTIM)delay);
    }
    return;
  }

  uint32_t tick_start = uwTick;
  uint32_t wait = delay;
  if (wait < HAL_MAX_DELAY)
  {
    wait += (uint32_t)uwTickFreq;
  }
  while ((uwTick - tick_start) < wait)
  {
  }
}
