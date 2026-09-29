#include "usb_webcam.h"

#include <assert.h>
#include <stddef.h>

static uint8_t clamp_u8(int32_t value)
{
  if (value < 0)
  {
    return 0U;
  }
  if (value > 255)
  {
    return 255U;
  }
  return (uint8_t)value;
}

static void rgb565_to_yuv(uint16_t pixel, uint8_t *y, int32_t *u, int32_t *v)
{
  const int32_t r = (int32_t)((pixel >> 11) & 0x1fU) * 255 / 31;
  const int32_t g = (int32_t)((pixel >> 5) & 0x3fU) * 255 / 63;
  const int32_t b = (int32_t)(pixel & 0x1fU) * 255 / 31;

  *y = clamp_u8((77 * r + 150 * g + 29 * b + 128) >> 8);
  *u = ((-43 * r - 85 * g + 128 * b + 128) >> 8) + 128;
  *v = ((128 * r - 107 * g - 21 * b + 128) >> 8) + 128;
}

#define MCU_WIDTH  (16U)
#define MCU_HEIGHT (8U)
#define MCU_BLOCK_SIZE (64U)
#define MCU_CB_OFFSET  (2U * MCU_BLOCK_SIZE)
#define MCU_CR_OFFSET  (3U * MCU_BLOCK_SIZE)
#define MCU_SIZE       (4U * MCU_BLOCK_SIZE)

/* Writes YCbCr 4:2:2 MCUs in raster order. Each 16 x 8 MCU holds the left
 * and right 8 x 8 luma blocks followed by the 8 x 8 Cb and Cr blocks. */
void UsbWebcam_ConvertRgb565ToYcbcr422Mcu(const uint16_t *source,
                                          uint32_t source_width,
                                          uint32_t source_height,
                                          uint8_t *destination,
                                          uint32_t destination_width,
                                          uint32_t destination_height)
{
  assert(source != NULL);
  assert(destination != NULL);
  assert((destination_width % MCU_WIDTH) == 0U);
  assert((destination_height % MCU_HEIGHT) == 0U);
  assert(source_width > 0U);
  assert(source_height > 0U);
  assert(destination_width > 0U);
  assert(destination_height > 0U);

  uint32_t crop_width = source_width;
  uint32_t crop_height = source_height;
  if ((uint64_t)source_width * destination_height >
      (uint64_t)source_height * destination_width)
  {
    crop_width = (uint32_t)((uint64_t)source_height * destination_width /
                            destination_height);
  }
  else
  {
    crop_height = (uint32_t)((uint64_t)source_width * destination_height /
                             destination_width);
  }
  const uint32_t crop_x = (source_width - crop_width) / 2U;
  const uint32_t crop_y = (source_height - crop_height) / 2U;

  const uint32_t mcu_columns = destination_width / MCU_WIDTH;
  for (uint32_t y_pos = 0U; y_pos < destination_height; y_pos++)
  {
    const uint32_t source_y = crop_y +
      (uint32_t)((uint64_t)y_pos * crop_height / destination_height);
    const uint16_t *line = source + source_y * source_width;
    const uint32_t mcu_row = y_pos % MCU_HEIGHT;
    uint8_t *mcu = destination +
      (y_pos / MCU_HEIGHT) * mcu_columns * MCU_SIZE;
    for (uint32_t x = 0U; x < destination_width; x += 2U)
    {
      uint8_t y0;
      uint8_t y1;
      int32_t u0;
      int32_t v0;
      int32_t u1;
      int32_t v1;
      const uint32_t source_x0 = crop_x +
        (uint32_t)((uint64_t)x * crop_width / destination_width);
      const uint32_t source_x1 = crop_x +
        (uint32_t)((uint64_t)(x + 1U) * crop_width / destination_width);
      rgb565_to_yuv(line[source_x0], &y0, &u0, &v0);
      rgb565_to_yuv(line[source_x1], &y1, &u1, &v1);

      const uint32_t mcu_x = x % MCU_WIDTH;
      uint8_t *luma = mcu + (mcu_x / 8U) * MCU_BLOCK_SIZE +
        mcu_row * 8U + (mcu_x % 8U);
      const uint32_t chroma = mcu_row * 8U + mcu_x / 2U;
      luma[0] = y0;
      luma[1] = y1;
      mcu[MCU_CB_OFFSET + chroma] = clamp_u8((u0 + u1) / 2);
      mcu[MCU_CR_OFFSET + chroma] = clamp_u8((v0 + v1) / 2);
      if (mcu_x == MCU_WIDTH - 2U)
      {
        mcu += MCU_SIZE;
      }
    }
  }
}
