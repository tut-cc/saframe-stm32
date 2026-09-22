#ifndef PRIVACY_FILTER_H
#define PRIVACY_FILTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRIVACY_MAX_FACES  (10U)

typedef enum
{
  PRIVACY_MODE_MASK = 0,
  PRIVACY_MODE_MOSAIC = 1,
} PrivacyMode;

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
  uint32_t inference_ms;
  uint32_t vision_ms;
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
const char *PrivacyFilter_ModeName(PrivacyMode mode);

#ifdef __cplusplus
}
#endif

#endif /* PRIVACY_FILTER_H */
