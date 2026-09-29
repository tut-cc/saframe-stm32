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

int main(void)
{
  test_exact_two_to_one_center_crop();
  test_800_by_480_uses_center_640();
  return 0;
}
