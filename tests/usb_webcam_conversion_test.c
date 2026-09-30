#include <assert.h>
#include <stdint.h>
#include "usb_webcam.h"

static void test_exact_two_to_one_center_crop(void)
{
  uint16_t input[8U * 4U];
  uint16_t output[2U * 2U] = {0};
  for (uint32_t y = 0; y < 4U; y++)
    for (uint32_t x = 0; x < 8U; x++)
      input[y * 8U + x] = (uint16_t)(y * 100U + x);
  UsbWebcam_CopyRgb565CenterDownsample(input, 8U, 4U, output, 2U, 2U);
  assert(output[0] == 2U && output[1] == 4U);
  assert(output[2] == 202U && output[3] == 204U);
}

static void test_800_by_480_uses_center_640(void)
{
  static uint16_t input[800U * 480U];
  static uint16_t output[320U * 240U];
  for (uint32_t y = 0; y < 480U; y++)
    for (uint32_t x = 0; x < 800U; x++)
      input[y * 800U + x] = (uint16_t)x;
  UsbWebcam_CopyRgb565CenterDownsample(input, 800U, 480U, output, 320U, 240U);
  assert(output[0] == 80U && output[319] == 718U);
  assert(output[239U * 320U] == 80U);
}

/* The original per-pixel 64-bit division, kept as the reference. */
static uint16_t reference_pixel(const uint16_t *source, uint32_t sw, uint32_t sh,
                                uint32_t dw, uint32_t dh, uint32_t x, uint32_t y)
{
  uint32_t cw = sw;
  uint32_t ch = sh;
  if ((uint64_t)sw * dh > (uint64_t)sh * dw)
    cw = (uint32_t)((uint64_t)sh * dw / dh);
  else
    ch = (uint32_t)((uint64_t)sw * dh / dw);
  const uint32_t sx = (sw - cw) / 2U + (uint32_t)((uint64_t)x * cw / dw);
  const uint32_t sy = (sh - ch) / 2U + (uint32_t)((uint64_t)y * ch / dh);
  return source[sy * sw + sx];
}

static void test_matches_division_reference(void)
{
  static const uint32_t sizes[][4] = {
    {480U, 480U, 320U, 240U}, {800U, 480U, 320U, 240U},
    {333U, 251U, 97U, 61U}, {320U, 240U, 320U, 240U},
  };
  static uint16_t input[800U * 480U];
  static uint16_t output[320U * 240U];
  for (uint32_t i = 0; i < 800U * 480U; i++)
    input[i] = (uint16_t)(i * 2654435761U >> 16);
  for (uint32_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
    const uint32_t sw = sizes[s][0], sh = sizes[s][1];
    const uint32_t dw = sizes[s][2], dh = sizes[s][3];
    UsbWebcam_CopyRgb565CenterDownsample(input, sw, sh, output, dw, dh);
    for (uint32_t y = 0; y < dh; y++)
      for (uint32_t x = 0; x < dw; x++)
        assert(output[y * dw + x] == reference_pixel(input, sw, sh, dw, dh, x, y));
  }
}

int main(void)
{
  test_exact_two_to_one_center_crop();
  test_800_by_480_uses_center_640();
  test_matches_division_reference();
  return 0;
}
