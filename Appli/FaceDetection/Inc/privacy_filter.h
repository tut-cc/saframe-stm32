#ifndef PRIVACY_FILTER_H
#define PRIVACY_FILTER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRIVACY_MAX_DETECTIONS  (10U)
#define PRIVACY_PROXY_CLASS_COUNT (3U)

typedef enum
{
  PRIVACY_PROXY_FACE = 0,
  PRIVACY_PROXY_DOCUMENT = 1,
  PRIVACY_PROXY_LOGO = 2,
} PrivacyProxyClass;

typedef enum
{
  PRIVACY_MODE_MASK = 0,
  PRIVACY_MODE_MOSAIC = 1,
} PrivacyMode;

typedef enum
{
  PRIVACY_FRAME_FREE = 0,
  PRIVACY_FRAME_DISPLAYED,
  PRIVACY_FRAME_CAPTURING,
  PRIVACY_FRAME_CAPTURED,
  PRIVACY_FRAME_INFERENCE,
  PRIVACY_FRAME_PROCESSED,
  PRIVACY_FRAME_DROPPED,
} PrivacyFrameState;

typedef struct
{
  int16_t x;
  int16_t y;
  uint16_t width;
  uint16_t height;
  uint8_t class_index;
} PrivacyRoi;

typedef struct
{
  uint32_t frame_number;
  uint32_t buffer_index;
  PrivacyFrameState state;
  PrivacyMode applied_mode;
  uint32_t deadline_started_cycles;
  uint32_t queued_at_cycles;
  uint32_t capture_us;
  uint32_t capture_queue_wait_us;
  uint32_t nn_copy_us;
  uint32_t buffer_wait_us;
  uint32_t inference_wait_us;
  uint32_t render_wait_us;
  uint32_t inference_us;
  uint32_t postprocess_us;
  uint32_t vision_us;
  uint32_t cache_invalidate_us;
  uint32_t filter_us;
  uint32_t cache_clean_us;
  uint32_t render_us;
  uint32_t total_us;
  uint32_t ltdc_us;
  uint32_t vblank_us;
  uint32_t published_frames;
  uint32_t dropped_deadline;
  uint32_t consecutive_drops;
  uint32_t detection_count;
  uint32_t class_detection_count[PRIVACY_PROXY_CLASS_COUNT];
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
                                   uint32_t deadline_cycles);
const char *PrivacyFilter_ModeName(PrivacyMode mode);
bool PrivacyProxyClass_IsValid(int32_t class_index);
const char *PrivacyProxyClass_InternalName(uint32_t class_index);
const char *PrivacyProxyClass_DisplayName(uint32_t class_index);

#ifdef __cplusplus
}
#endif

#endif /* PRIVACY_FILTER_H */
