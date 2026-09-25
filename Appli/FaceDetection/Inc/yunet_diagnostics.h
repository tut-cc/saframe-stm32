#ifndef YUNET_DIAGNOSTICS_H
#define YUNET_DIAGNOSTICS_H

#include <stdint.h>

typedef struct
{
  float max_score;
  uint32_t candidates_above_threshold;
} YuNetScoreDiagnostics;

static inline void YuNetScoreDiagnostics_Reset(YuNetScoreDiagnostics *diagnostics)
{
  diagnostics->max_score = 0.0f;
  diagnostics->candidates_above_threshold = 0U;
}

static inline float YuNetScoreDiagnostics_Dequantize(
    int8_t cls, float cls_scale, int8_t cls_zero_point,
    int8_t objectness, float objectness_scale, int8_t objectness_zero_point)
{
  const float cls_score =
      (float)((int32_t)cls - (int32_t)cls_zero_point) * cls_scale;
  const float objectness_score =
      (float)((int32_t)objectness - (int32_t)objectness_zero_point) *
      objectness_scale;
  return cls_score * objectness_score;
}

static inline void YuNetScoreDiagnostics_Observe(
    YuNetScoreDiagnostics *diagnostics, float score, float threshold)
{
  if (score > diagnostics->max_score)
  {
    diagnostics->max_score = score;
  }
  if (score > threshold)
  {
    diagnostics->candidates_above_threshold++;
  }
}

#endif /* YUNET_DIAGNOSTICS_H */
