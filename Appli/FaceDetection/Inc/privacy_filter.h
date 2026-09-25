#ifndef PRIVACY_FILTER_H
#define PRIVACY_FILTER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRIVACY_MAX_FACES  (10U)

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
  PrivacyMode applied_mode;
  uint32_t deadline_started_cycles;
  uint32_t capture_us;
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
  float max_face_score;
  uint32_t face_candidates;
  uint32_t nms_face_candidates;
  uint32_t detected_faces;
  uint32_t held_faces;
  uint32_t expired_tracks;
  uint8_t input_min[3];
  uint8_t input_max[3];
  uint8_t input_mean[3];
  uint32_t input_hash;
  int8_t cls_output_min;
  int8_t cls_output_max;
  int8_t obj_output_min;
  int8_t obj_output_max;
  uint32_t face_count;
  PrivacyRoi faces[PRIVACY_MAX_FACES];
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

#ifdef __cplusplus
}
#endif

#endif /* PRIVACY_FILTER_H */
