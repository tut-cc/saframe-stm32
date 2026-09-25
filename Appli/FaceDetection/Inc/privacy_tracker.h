#ifndef PRIVACY_TRACKER_H
#define PRIVACY_TRACKER_H

#include <stdint.h>

#include "privacy_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRIVACY_TRACK_ACQUIRE_THRESHOLD  (0.35f)
#define PRIVACY_TRACK_UPDATE_THRESHOLD   (0.20f)
#define PRIVACY_TRACK_MATCH_IOU           (0.15f)
#define PRIVACY_TRACK_HOLD_MS             (1000U)

typedef struct
{
  PrivacyRoi roi;
  float confidence;
} PrivacyDetection;

typedef struct
{
  uint8_t active;
  PrivacyRoi roi;
  uint32_t last_seen_ms;
} PrivacyTrack;

typedef struct
{
  PrivacyTrack tracks[PRIVACY_MAX_FACES];
} PrivacyTracker;

typedef struct
{
  uint32_t roi_count;
  PrivacyRoi rois[PRIVACY_MAX_FACES];
  uint32_t detected_count;
  uint32_t held_count;
  uint32_t expired_count;
} PrivacyTrackerResult;

void PrivacyTracker_Init(PrivacyTracker *tracker);
void PrivacyTracker_Update(PrivacyTracker *tracker,
                           const PrivacyDetection *detections,
                           uint32_t detection_count,
                           uint32_t now_ms,
                           uint32_t frame_width,
                           uint32_t frame_height,
                           PrivacyTrackerResult *result);

#ifdef __cplusplus
}
#endif

#endif /* PRIVACY_TRACKER_H */
