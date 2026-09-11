#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "privacy_filter.h"

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

int main(void)
{
  test_mask_and_clear();
  test_mosaic_color_and_clipping();
  test_invalid_roi_is_ignored();
  puts("privacy_filter_test: PASS");
  return 0;
}
