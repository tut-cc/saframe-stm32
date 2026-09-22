#ifndef FACE_DETECTION_DIAGNOSTICS_H
#define FACE_DETECTION_DIAGNOSTICS_H

#include <stdint.h>

typedef enum
{
  APP_STAGE_STARTUP = 0,
  APP_STAGE_CAMERA_CAPTURE,
  APP_STAGE_NPU_INFERENCE,
  APP_STAGE_POSTPROCESS,
  APP_STAGE_CACHE_INVALIDATE,
  APP_STAGE_PRIVACY_FILTER,
  APP_STAGE_CACHE_CLEAN,
  APP_STAGE_LTDC_RELOAD,
  APP_STAGE_VBLANK_WAIT,
  APP_STAGE_MONITOR,
} AppDiagnosticStage;

typedef enum
{
  APP_FAULT_NONE = 0,
  APP_FAULT_HARD,
  APP_FAULT_MEMMANAGE,
  APP_FAULT_BUS,
  APP_FAULT_USAGE,
  APP_FAULT_ASSERT,
} AppFaultKind;

extern volatile uint32_t g_app_diagnostic_frame;
extern volatile AppDiagnosticStage g_app_diagnostic_stage;

void AppDiagnostics_ReportPreviousFault(void);
__attribute__((noreturn))
void AppDiagnostics_RecordAssert(const char *file, uint32_t line);

#define APP_ASSERT(condition) \
  do \
  { \
    if (!(condition)) \
    { \
      AppDiagnostics_RecordAssert(__FILE__, __LINE__); \
    } \
  } while (0)

#endif /* FACE_DETECTION_DIAGNOSTICS_H */
