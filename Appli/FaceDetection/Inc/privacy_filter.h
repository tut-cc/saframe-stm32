#ifndef PRIVACY_FILTER_H
#define PRIVACY_FILTER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRIVACY_MAX_DETECTIONS  (10U)

typedef enum
{
  PRIVACY_MODE_MASK = 0,
  PRIVACY_MODE_MOSAIC = 1,
} PrivacyMode;

typedef enum
{
  PRIVACY_FRAME_WORKING = 0,
  PRIVACY_FRAME_PROCESSED,
  PRIVACY_FRAME_PUBLISHED,
  PRIVACY_FRAME_DROPPED,
} PrivacyFrameState;

typedef struct
{
  int16_t x;
  int16_t y;
  uint16_t width;
  uint16_t height;
} PrivacyRoi;

typedef struct
{
  uint32_t frame_number;
  uint32_t buffer_index;
  PrivacyFrameState state;
  uint32_t deadline_started_at;
  uint32_t capture_ms;
  uint32_t inference_ms;
  uint32_t postprocess_ms;
  uint32_t vision_ms;
  uint32_t render_ms;
  uint32_t total_ms;
  uint32_t detection_count;
  PrivacyRoi detections[PRIVACY_MAX_DETECTIONS];
} PrivacyFrameResult;

typedef struct
{
  uint16_t *overlay;
  uint32_t overlay_width;
  uint32_t overlay_height;
  const uint16_t *background;
  uint32_t background_width;
  uint32_t background_height;
  uint32_t background_x;
  uint32_t background_y;
} PrivacyRenderTarget;

void PrivacyFilter_Clear(const PrivacyRenderTarget *target);
void PrivacyFilter_Render(const PrivacyFrameResult *result,
                          PrivacyMode mode,
                          const PrivacyRenderTarget *target);
void PrivacyFilter_ApplyRgb565(const PrivacyFrameResult *result,
                               PrivacyMode mode,
                               uint16_t *frame,
                               uint32_t width,
                               uint32_t height);
bool PrivacyFrame_IsWithinDeadline(uint32_t started_at,
                                   uint32_t completed_at,
                                   uint32_t deadline_ms);
const char *PrivacyFilter_ModeName(PrivacyMode mode);

#ifdef __cplusplus
}
#endif

#endif /* PRIVACY_FILTER_H */
