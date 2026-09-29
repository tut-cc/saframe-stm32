#include <assert.h>
#include <stdint.h>

#include "usb_webcam.h"

static void test_primary_colors(void)
{
  const uint16_t input[] = {0xf800U, 0xf800U, 0x07e0U, 0x07e0U};
  uint8_t output[8] = {0};
  UsbWebcam_ConvertRgb565ToYuy2(input, 4U, 1U, output, 4U, 1U);
  assert(output[0] == 77U);
  assert(output[1] == 85U);
  assert(output[2] == 77U);
  assert(output[3] == 255U);
  assert(output[4] == 149U);
  assert(output[5] == 43U);
  assert(output[6] == 149U);
  assert(output[7] == 21U);
}

static void test_center_crop(void)
{
  const uint16_t input[] = {
    0xffffU, 0xf800U, 0xf800U, 0xffffU,
  };
  uint8_t output[4] = {0};
  UsbWebcam_ConvertRgb565ToYuy2(input, 4U, 1U, output, 2U, 1U);
  assert(output[0] == 77U);
  assert(output[1] == 85U);
  assert(output[2] == 77U);
  assert(output[3] == 255U);
}

static void test_center_crop_and_downscale(void)
{
  const uint16_t input[] = {
    0xffffU, 0xf800U, 0x07e0U, 0x001fU, 0xffffU, 0xffffU,
    0xffffU, 0xf800U, 0x07e0U, 0x001fU, 0xffffU, 0xffffU,
  };
  uint8_t output[4] = {0};
  UsbWebcam_ConvertRgb565ToYuy2(input, 6U, 2U, output, 2U, 1U);
  assert(output[0] == 77U);
  assert(output[2] == 29U);
}

int main(void)
{
  test_primary_colors();
  test_center_crop();
  test_center_crop_and_downscale();
  return 0;
}
