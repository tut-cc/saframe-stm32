#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "privacy_filter.h"
#include "privacy_tracker.h"
#include "yunet_diagnostics.h"

static void test_mask_and_clear(void)
{
  uint16_t overlay[6U * 5U];
  const PrivacyRenderTarget target = {
    .overlay = overlay,
    .overlay_width = 6U,
    .overlay_height = 5U,
  };
  const PrivacyFrameResult result = {
    .face_count = 1U,
    .faces = {{.x = 2, .y = 1, .width = 3U, .height = 2U}},
  };

  for (uint32_t i = 0; i < 30U; i++)
  {
    overlay[i] = 0xFFFFU;
  }
  PrivacyFilter_Render(&result, PRIVACY_MODE_MASK, &target);

  for (uint32_t y = 0; y < 5U; y++)
  {
    for (uint32_t x = 0; x < 6U; x++)
    {
      const uint16_t expected = ((x >= 2U) && (x < 5U) &&
                                 (y >= 1U) && (y < 3U)) ? 0xF000U : 0U;
      assert(overlay[(y * 6U) + x] == expected);
    }
  }
}

static void test_mosaic_color_and_clipping(void)
{
  uint16_t overlay[4U * 4U] = {0};
  uint16_t background[4U * 4U] = {0};
  background[(2U * 4U) + 2U] = 0x07E0U;

  const PrivacyRenderTarget target = {
    .overlay = overlay,
    .overlay_width = 4U,
    .overlay_height = 4U,
    .background = background,
    .background_width = 4U,
    .background_height = 4U,
  };
  const PrivacyFrameResult result = {
    .face_count = 2U,
    .faces = {
      {.x = 0, .y = 0, .width = 4U, .height = 4U},
      {.x = 3, .y = 3, .width = 20U, .height = 20U},
    },
  };

  PrivacyFilter_Render(&result, PRIVACY_MODE_MOSAIC, &target);
  for (uint32_t i = 0; i < 15U; i++)
  {
    assert(overlay[i] == 0xF0F0U);
  }
  assert(overlay[15] == 0xF000U);
}

static void test_invalid_roi_is_ignored(void)
{
  uint16_t overlay[4U] = {1U, 2U, 3U, 4U};
  const PrivacyRenderTarget target = {
    .overlay = overlay,
    .overlay_width = 2U,
    .overlay_height = 2U,
  };
  const PrivacyFrameResult result = {
    .face_count = 1U,
    .faces = {{.x = -1, .y = 0, .width = 1U, .height = 1U}},
  };

  PrivacyFilter_Render(&result, PRIVACY_MODE_MASK, &target);
  for (uint32_t i = 0; i < 4U; i++)
  {
    assert(overlay[i] == 0U);
  }
}

static void test_rgb565_mask_multiple_rois_and_clipping(void)
{
  uint16_t frame[6U * 4U];
  for (uint32_t i = 0; i < 24U; i++)
  {
    frame[i] = (uint16_t)(i + 1U);
  }
  const PrivacyFrameResult result = {
    .face_count = 2U,
    .faces = {
      {.x = 1, .y = 1, .width = 2U, .height = 2U},
      {.x = 5, .y = 3, .width = 8U, .height = 8U},
    },
  };

  PrivacyFilter_ApplyRgb565(&result, PRIVACY_MODE_MASK, frame, 6U, 4U);
  assert(frame[(1U * 6U) + 1U] == 0U);
  assert(frame[(1U * 6U) + 2U] == 0U);
  assert(frame[(2U * 6U) + 1U] == 0U);
  assert(frame[(2U * 6U) + 2U] == 0U);
  assert(frame[(3U * 6U) + 5U] == 0U);
  assert(frame[0] == 1U);
}

static void test_rgb565_mosaic_and_zero_detections(void)
{
  uint16_t frame[20U * 20U];
  for (uint32_t y = 0; y < 20U; y++)
  {
    for (uint32_t x = 0; x < 20U; x++)
    {
      frame[(y * 20U) + x] = (x < 16U && y < 16U) ?
                             ((x + y) & 1U ? 0xFFFFU : 0x0000U) : 0xF800U;
    }
  }
  PrivacyFrameResult result = {
    .face_count = 1U,
    .faces = {{.x = 0, .y = 0, .width = 20U, .height = 20U}},
  };

  PrivacyFilter_ApplyRgb565(&result, PRIVACY_MODE_MOSAIC, frame, 20U, 20U);
  assert(frame[0] == 0x7BEFU);
  assert(frame[(15U * 20U) + 15U] == 0x7BEFU);
  assert(frame[(16U * 20U) + 16U] == 0xF800U);
  assert(frame[(19U * 20U) + 19U] == 0xF800U);

  const uint16_t unchanged = frame[0];
  result.face_count = 0U;
  PrivacyFilter_ApplyRgb565(&result, PRIVACY_MODE_MASK, frame, 20U, 20U);
  assert(frame[0] == unchanged);
}

static void test_deadline_boundary_and_tick_wrap(void)
{
  assert(PrivacyFrame_IsWithinDeadline(100U, 33099U, 33000U));
  assert(PrivacyFrame_IsWithinDeadline(100U, 33100U, 33000U));
  assert(!PrivacyFrame_IsWithinDeadline(100U, 33101U, 33000U));
  assert(PrivacyFrame_IsWithinDeadline(UINT32_MAX - 10U, 5U, 16U));
}

static void test_yunet_score_diagnostics(void)
{
  YuNetScoreDiagnostics diagnostics;
  YuNetScoreDiagnostics_Reset(&diagnostics);

  const float below = YuNetScoreDiagnostics_Dequantize(
      6, 0.1f, 2, 1, 1.0f, 0);
  const float boundary = YuNetScoreDiagnostics_Dequantize(
      7, 0.1f, 2, 1, 1.0f, 0);
  const float above = YuNetScoreDiagnostics_Dequantize(
      8, 0.1f, 2, 1, 1.0f, 0);

  assert(below > 0.39f && below < 0.41f);
  assert(boundary > 0.49f && boundary < 0.51f);
  YuNetScoreDiagnostics_Observe(&diagnostics, below, 0.5f);
  YuNetScoreDiagnostics_Observe(&diagnostics, boundary, 0.5f);
  YuNetScoreDiagnostics_Observe(&diagnostics, above, 0.5f);
  assert(diagnostics.max_score > 0.59f && diagnostics.max_score < 0.61f);
  assert(diagnostics.candidates_above_threshold == 1U);
}

static PrivacyDetection detection(int16_t x, int16_t y,
                                  uint16_t width, uint16_t height,
                                  float confidence)
{
  const PrivacyDetection value = {
    .roi = {.x = x, .y = y, .width = width, .height = height},
    .confidence = confidence,
  };
  return value;
}

static void test_tracker_thresholds_smoothing_and_hold(void)
{
  PrivacyTracker tracker;
  PrivacyTrackerResult result;
  PrivacyTracker_Init(&tracker);

  PrivacyDetection low = detection(100, 80, 100U, 50U, 0.34f);
  PrivacyTracker_Update(&tracker, &low, 1U, 100U, 400U, 300U, &result);
  assert(result.roi_count == 0U);

  PrivacyDetection acquired = detection(100, 80, 100U, 50U, 0.35f);
  PrivacyTracker_Update(&tracker, &acquired, 1U, 110U, 400U, 300U, &result);
  assert(result.roi_count == 1U);
  assert(result.detected_count == 1U);
  assert(result.held_count == 0U);
  assert(result.rois[0].x == 70);
  assert(result.rois[0].y == 60);
  assert(result.rois[0].width == 160U);
  assert(result.rois[0].height == 90U);

  PrivacyDetection below_update = detection(105, 85, 100U, 50U, 0.19f);
  PrivacyTracker_Update(&tracker, &below_update, 1U, 115U, 400U, 300U,
                        &result);
  assert(result.detected_count == 0U);
  assert(result.held_count == 1U);
  assert(result.rois[0].x == 70);
  assert(result.rois[0].y == 60);

  PrivacyDetection update = detection(110, 90, 100U, 50U, 0.20f);
  PrivacyTracker_Update(&tracker, &update, 1U, 120U, 400U, 300U, &result);
  assert(result.detected_count == 1U);
  assert(result.rois[0].x == 76);
  assert(result.rois[0].y == 66);

  PrivacyTracker_Update(&tracker, NULL, 0U, 369U, 400U, 300U, &result);
  assert(result.held_count == 1U);
  assert(result.rois[0].x == 76);
  PrivacyTracker_Update(&tracker, NULL, 0U, 370U, 400U, 300U, &result);
  assert(result.rois[0].x == 66);
  assert(result.rois[0].y == 61);

  PrivacyTracker_Update(&tracker, NULL, 0U, 1120U, 400U, 300U, &result);
  assert(result.roi_count == 1U);
  assert(result.held_count == 1U);
  assert(result.rois[0].x == 46);
  assert(result.rois[0].y == 51);
  PrivacyTracker_Update(&tracker, NULL, 0U, 1121U, 400U, 300U, &result);
  assert(result.roi_count == 0U);
  assert(result.expired_count == 1U);
}

static void test_tracker_iou_matching_and_one_to_one(void)
{
  PrivacyTracker tracker;
  PrivacyTrackerResult result;
  PrivacyTracker_Init(&tracker);
  PrivacyDetection initial[2] = {
    detection(0, 50, 100U, 100U, 0.80f),
    detection(200, 50, 100U, 100U, 0.70f),
  };
  PrivacyTracker_Update(&tracker, initial, 2U, 0U, 400U, 240U, &result);
  assert(result.roi_count == 2U);

  /* A 73-pixel shift has IoU ~= 0.156 and updates the existing track. */
  PrivacyDetection match = detection(73, 50, 100U, 100U, 0.25f);
  PrivacyTracker_Update(&tracker, &match, 1U, 10U, 400U, 240U, &result);
  assert(result.detected_count == 1U);
  assert(result.held_count == 1U);

  PrivacyTracker_Init(&tracker);
  PrivacyTracker_Update(&tracker, initial, 1U, 0U, 400U, 240U, &result);
  /* A 74-pixel shift has IoU ~= 0.149 and cannot update or create at 0.25. */
  PrivacyDetection no_match = detection(74, 50, 100U, 100U, 0.25f);
  PrivacyTracker_Update(&tracker, &no_match, 1U, 10U, 400U, 240U, &result);
  assert(result.detected_count == 0U);
  assert(result.held_count == 1U);
  assert(result.roi_count == 1U);

  PrivacyTracker_Init(&tracker);
  PrivacyTracker_Update(&tracker, initial, 1U, 0U, 400U, 240U, &result);
  /* The higher-confidence second item must claim the existing track first. */
  PrivacyDetection overlapping[2] = {
    detection(10, 50, 100U, 100U, 0.55f),
    detection(5, 50, 100U, 100U, 0.60f),
  };
  PrivacyTracker_Update(&tracker, overlapping, 2U, 10U, 400U, 240U, &result);
  assert(result.detected_count == 2U);
  assert(result.roi_count == 2U);
  assert(tracker.tracks[0].roi.x == 3);
  assert(tracker.tracks[1].roi.x == 10);
}

static void test_tracker_capacity_clipping_and_tick_wrap(void)
{
  PrivacyTracker tracker;
  PrivacyTrackerResult result;
  PrivacyDetection detections[PRIVACY_MAX_FACES];
  PrivacyTracker_Init(&tracker);
  for (uint32_t i = 0U; i < PRIVACY_MAX_FACES; i++)
  {
    detections[i] = detection((int16_t)(i * 30U), 0, 20U, 20U,
                              0.90f - ((float)i * 0.01f));
  }
  PrivacyTracker_Update(&tracker, detections, PRIVACY_MAX_FACES,
                        UINT32_MAX - 500U, 320U, 240U, &result);
  assert(result.roi_count == PRIVACY_MAX_FACES);
  assert(result.rois[0].x == 0);
  assert(result.rois[0].y == 0);

  PrivacyTracker_Update(&tracker, NULL, 0U, 499U, 320U, 240U, &result);
  assert(result.roi_count == PRIVACY_MAX_FACES);
  assert(result.held_count == PRIVACY_MAX_FACES);
  PrivacyTracker_Update(&tracker, NULL, 0U, 500U, 320U, 240U, &result);
  assert(result.roi_count == 0U);
  assert(result.expired_count == PRIVACY_MAX_FACES);
}

int main(void)
{
  test_mask_and_clear();
  test_mosaic_color_and_clipping();
  test_invalid_roi_is_ignored();
  test_rgb565_mask_multiple_rois_and_clipping();
  test_rgb565_mosaic_and_zero_detections();
  test_deadline_boundary_and_tick_wrap();
  test_yunet_score_diagnostics();
  test_tracker_thresholds_smoothing_and_hold();
  test_tracker_iou_matching_and_one_to_one();
  test_tracker_capacity_clipping_and_tick_wrap();
  puts("privacy_filter_test: PASS");
  return 0;
}
