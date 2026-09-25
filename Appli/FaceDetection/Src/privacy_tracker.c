#include "privacy_tracker.h"

#include <stddef.h>
#include <string.h>

#define PRIVACY_ROI_MARGIN_X_PC             (30U)
#define PRIVACY_ROI_MARGIN_Y_PC             (40U)
#define PRIVACY_HELD_MARGIN_STEP_MS         (250U)
#define PRIVACY_HELD_MARGIN_STEP_PC         (10U)
#define PRIVACY_HELD_MARGIN_X_MAX_PC        (60U)
#define PRIVACY_HELD_MARGIN_Y_MAX_PC        (70U)

static float roi_iou(const PrivacyRoi *a, const PrivacyRoi *b)
{
  const int32_t ax1 = (int32_t)a->x + (int32_t)a->width;
  const int32_t ay1 = (int32_t)a->y + (int32_t)a->height;
  const int32_t bx1 = (int32_t)b->x + (int32_t)b->width;
  const int32_t by1 = (int32_t)b->y + (int32_t)b->height;
  const int32_t ix0 = (a->x > b->x) ? a->x : b->x;
  const int32_t iy0 = (a->y > b->y) ? a->y : b->y;
  const int32_t ix1 = (ax1 < bx1) ? ax1 : bx1;
  const int32_t iy1 = (ay1 < by1) ? ay1 : by1;

  if ((ix1 <= ix0) || (iy1 <= iy0))
  {
    return 0.0f;
  }

  const uint32_t intersection =
      (uint32_t)(ix1 - ix0) * (uint32_t)(iy1 - iy0);
  const uint32_t union_area =
      ((uint32_t)a->width * a->height) +
      ((uint32_t)b->width * b->height) - intersection;
  return union_area == 0U ? 0.0f : (float)intersection / (float)union_area;
}

static int16_t smooth_position(int16_t old_value, int16_t new_value)
{
  const int32_t weighted = ((int32_t)old_value * 2) +
                           ((int32_t)new_value * 3);
  return (int16_t)((weighted + 2) / 5);
}

static uint16_t smooth_size(uint16_t old_value, uint16_t new_value)
{
  const uint32_t weighted = ((uint32_t)old_value * 2U) +
                            ((uint32_t)new_value * 3U);
  return (uint16_t)((weighted + 2U) / 5U);
}

static PrivacyRoi smooth_roi(const PrivacyRoi *old_roi,
                             const PrivacyRoi *new_roi)
{
  const PrivacyRoi smoothed = {
    .x = smooth_position(old_roi->x, new_roi->x),
    .y = smooth_position(old_roi->y, new_roi->y),
    .width = smooth_size(old_roi->width, new_roi->width),
    .height = smooth_size(old_roi->height, new_roi->height),
  };
  return smoothed;
}

static PrivacyRoi expand_and_clip(const PrivacyRoi *roi,
                                  uint32_t age_ms,
                                  uint32_t frame_width,
                                  uint32_t frame_height)
{
  uint32_t margin_x_pc = PRIVACY_ROI_MARGIN_X_PC;
  uint32_t margin_y_pc = PRIVACY_ROI_MARGIN_Y_PC;
  if (age_ms > 0U)
  {
    const uint32_t steps = age_ms / PRIVACY_HELD_MARGIN_STEP_MS;
    margin_x_pc += steps * PRIVACY_HELD_MARGIN_STEP_PC;
    margin_y_pc += steps * PRIVACY_HELD_MARGIN_STEP_PC;
    if (margin_x_pc > PRIVACY_HELD_MARGIN_X_MAX_PC)
    {
      margin_x_pc = PRIVACY_HELD_MARGIN_X_MAX_PC;
    }
    if (margin_y_pc > PRIVACY_HELD_MARGIN_Y_MAX_PC)
    {
      margin_y_pc = PRIVACY_HELD_MARGIN_Y_MAX_PC;
    }
  }

  const int32_t margin_x = ((int32_t)roi->width * (int32_t)margin_x_pc) / 100;
  const int32_t margin_y = ((int32_t)roi->height * (int32_t)margin_y_pc) / 100;
  int32_t x0 = (int32_t)roi->x - margin_x;
  int32_t y0 = (int32_t)roi->y - margin_y;
  int32_t x1 = (int32_t)roi->x + (int32_t)roi->width + margin_x;
  int32_t y1 = (int32_t)roi->y + (int32_t)roi->height + margin_y;

  if (x0 < 0)
  {
    x0 = 0;
  }
  if (y0 < 0)
  {
    y0 = 0;
  }
  if (x1 > (int32_t)frame_width)
  {
    x1 = (int32_t)frame_width;
  }
  if (y1 > (int32_t)frame_height)
  {
    y1 = (int32_t)frame_height;
  }

  PrivacyRoi expanded = {0};
  if ((x1 > x0) && (y1 > y0))
  {
    expanded.x = (int16_t)x0;
    expanded.y = (int16_t)y0;
    expanded.width = (uint16_t)(x1 - x0);
    expanded.height = (uint16_t)(y1 - y0);
  }
  return expanded;
}

void PrivacyTracker_Init(PrivacyTracker *tracker)
{
  if (tracker != NULL)
  {
    memset(tracker, 0, sizeof(*tracker));
  }
}

void PrivacyTracker_Update(PrivacyTracker *tracker,
                           const PrivacyDetection *detections,
                           uint32_t detection_count,
                           uint32_t now_ms,
                           uint32_t frame_width,
                           uint32_t frame_height,
                           PrivacyTrackerResult *result)
{
  if ((tracker == NULL) || (result == NULL))
  {
    return;
  }

  memset(result, 0, sizeof(*result));
  uint8_t track_matched[PRIVACY_MAX_FACES] = {0};
  uint8_t detection_used[PRIVACY_MAX_FACES] = {0};
  uint32_t detection_order[PRIVACY_MAX_FACES];
  if (detections == NULL)
  {
    detection_count = 0U;
  }
  if (detection_count > PRIVACY_MAX_FACES)
  {
    detection_count = PRIVACY_MAX_FACES;
  }

  for (uint32_t i = 0U; i < PRIVACY_MAX_FACES; i++)
  {
    if (tracker->tracks[i].active != 0U)
    {
      const uint32_t age_ms = now_ms - tracker->tracks[i].last_seen_ms;
      if (age_ms > PRIVACY_TRACK_HOLD_MS)
      {
        tracker->tracks[i].active = 0U;
        result->expired_count++;
      }
    }
  }

  for (uint32_t i = 0U; i < detection_count; i++)
  {
    detection_order[i] = i;
  }
  for (uint32_t i = 1U; i < detection_count; i++)
  {
    const uint32_t item = detection_order[i];
    uint32_t position = i;
    while ((position > 0U) &&
           (detections[detection_order[position - 1U]].confidence <
            detections[item].confidence))
    {
      detection_order[position] = detection_order[position - 1U];
      position--;
    }
    detection_order[position] = item;
  }

  for (uint32_t ordered = 0U; ordered < detection_count; ordered++)
  {
    const uint32_t detection_index = detection_order[ordered];
    const PrivacyDetection *detection = &detections[detection_index];
    if ((detection->confidence < PRIVACY_TRACK_UPDATE_THRESHOLD) ||
        (detection->roi.width == 0U) || (detection->roi.height == 0U))
    {
      continue;
    }

    int32_t best_track = -1;
    float best_iou = PRIVACY_TRACK_MATCH_IOU;
    for (uint32_t track_index = 0U;
         track_index < PRIVACY_MAX_FACES; track_index++)
    {
      if ((tracker->tracks[track_index].active == 0U) ||
          (track_matched[track_index] != 0U))
      {
        continue;
      }
      const float iou = roi_iou(&tracker->tracks[track_index].roi,
                                &detection->roi);
      if (iou >= best_iou)
      {
        best_iou = iou;
        best_track = (int32_t)track_index;
      }
    }

    if (best_track >= 0)
    {
      PrivacyTrack *track = &tracker->tracks[best_track];
      track->roi = smooth_roi(&track->roi, &detection->roi);
      track->last_seen_ms = now_ms;
      track_matched[best_track] = 1U;
      detection_used[detection_index] = 1U;
      result->detected_count++;
    }
  }

  for (uint32_t ordered = 0U; ordered < detection_count; ordered++)
  {
    const uint32_t detection_index = detection_order[ordered];
    const PrivacyDetection *detection = &detections[detection_index];
    if ((detection_used[detection_index] != 0U) ||
        (detection->confidence < PRIVACY_TRACK_ACQUIRE_THRESHOLD) ||
        (detection->roi.width == 0U) || (detection->roi.height == 0U))
    {
      continue;
    }

    for (uint32_t track_index = 0U;
         track_index < PRIVACY_MAX_FACES; track_index++)
    {
      if (tracker->tracks[track_index].active == 0U)
      {
        tracker->tracks[track_index].active = 1U;
        tracker->tracks[track_index].roi = detection->roi;
        tracker->tracks[track_index].last_seen_ms = now_ms;
        track_matched[track_index] = 1U;
        detection_used[detection_index] = 1U;
        result->detected_count++;
        break;
      }
    }
  }

  for (uint32_t track_index = 0U;
       track_index < PRIVACY_MAX_FACES; track_index++)
  {
    const PrivacyTrack *track = &tracker->tracks[track_index];
    if ((track->active == 0U) || (result->roi_count >= PRIVACY_MAX_FACES))
    {
      continue;
    }

    const uint32_t age_ms = now_ms - track->last_seen_ms;
    if (track_matched[track_index] == 0U)
    {
      result->held_count++;
    }
    result->rois[result->roi_count] =
        expand_and_clip(&track->roi, age_ms, frame_width, frame_height);
    if ((result->rois[result->roi_count].width != 0U) &&
        (result->rois[result->roi_count].height != 0U))
    {
      result->roi_count++;
    }
  }
}
