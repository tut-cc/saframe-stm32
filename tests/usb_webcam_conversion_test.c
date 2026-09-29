#include <assert.h>
#include <stdint.h>

#include "usb_webcam.h"

#define MCU_WIDTH  (16U)
#define MCU_HEIGHT (8U)
#define MCU_SIZE   (256U)
#define CB_OFFSET  (128U)
#define CR_OFFSET  (192U)

static void fill(uint16_t *pixels, uint32_t count, uint16_t value)
{
  for (uint32_t i = 0U; i < count; i++)
  {
    pixels[i] = value;
  }
}

static void test_primary_colors(void)
{
  /* Red in the left half of the MCU, green in the right half. */
  uint16_t input[MCU_WIDTH * MCU_HEIGHT];
  for (uint32_t y = 0U; y < MCU_HEIGHT; y++)
  {
    fill(&input[y * MCU_WIDTH], 8U, 0xf800U);
    fill(&input[y * MCU_WIDTH + 8U], 8U, 0x07e0U);
  }
  uint8_t output[MCU_SIZE] = {0};
  UsbWebcam_ConvertRgb565ToYcbcr422Mcu(input, MCU_WIDTH, MCU_HEIGHT, output,
                                       MCU_WIDTH, MCU_HEIGHT);
  for (uint32_t i = 0U; i < 64U; i++)
  {
    assert(output[i] == 77U);
    assert(output[64U + i] == 149U);
  }
  for (uint32_t row = 0U; row < MCU_HEIGHT; row++)
  {
    for (uint32_t column = 0U; column < 8U; column++)
    {
      const uint32_t index = row * 8U + column;
      /* Chroma column c covers source pixels 2c and 2c+1. */
      assert(output[CB_OFFSET + index] == (column < 4U ? 85U : 43U));
      assert(output[CR_OFFSET + index] == (column < 4U ? 255U : 21U));
    }
  }
}

static void test_luma_block_layout(void)
{
  /* A single white pixel on black locates where one position is written. */
  uint16_t input[MCU_WIDTH * MCU_HEIGHT];
  for (uint32_t y = 0U; y < MCU_HEIGHT; y++)
  {
    for (uint32_t x = 0U; x < MCU_WIDTH; x++)
    {
      input[y * MCU_WIDTH + x] = (x == 9U && y == 3U) ? 0xffffU : 0x0000U;
    }
  }
  uint8_t output[MCU_SIZE] = {0};
  UsbWebcam_ConvertRgb565ToYcbcr422Mcu(input, MCU_WIDTH, MCU_HEIGHT, output,
                                       MCU_WIDTH, MCU_HEIGHT);
  for (uint32_t i = 0U; i < 128U; i++)
  {
    /* (x=9, y=3) lands in the right luma block at row 3, column 1. */
    assert(output[i] == ((i == 64U + 3U * 8U + 1U) ? 255U : 0U));
  }
}

static void test_mcu_raster_order(void)
{
  /* 32 x 16 destination: four MCUs, each a different colour. */
  const uint16_t colors[4] = {0xf800U, 0x07e0U, 0x001fU, 0xffffU};
  const uint8_t lumas[4] = {77U, 149U, 29U, 255U};
  uint16_t input[32U * 16U];
  for (uint32_t y = 0U; y < 16U; y++)
  {
    for (uint32_t x = 0U; x < 32U; x++)
    {
      input[y * 32U + x] = colors[(y / MCU_HEIGHT) * 2U + x / MCU_WIDTH];
    }
  }
  uint8_t output[4U * MCU_SIZE] = {0};
  UsbWebcam_ConvertRgb565ToYcbcr422Mcu(input, 32U, 16U, output, 32U, 16U);
  for (uint32_t mcu = 0U; mcu < 4U; mcu++)
  {
    assert(output[mcu * MCU_SIZE] == lumas[mcu]);
    assert(output[mcu * MCU_SIZE + 127U] == lumas[mcu]);
  }
}

static void test_center_crop_and_downscale(void)
{
  /* 64 x 8 source, 16 x 8 destination: the centre 32 x 8 is kept and every
   * second column is sampled. */
  uint16_t input[64U * MCU_HEIGHT];
  for (uint32_t y = 0U; y < MCU_HEIGHT; y++)
  {
    fill(&input[y * 64U], 16U, 0xffffU);
    fill(&input[y * 64U + 16U], 16U, 0xf800U);
    fill(&input[y * 64U + 32U], 16U, 0x001fU);
    fill(&input[y * 64U + 48U], 16U, 0xffffU);
  }
  uint8_t output[MCU_SIZE] = {0};
  UsbWebcam_ConvertRgb565ToYcbcr422Mcu(input, 64U, MCU_HEIGHT, output,
                                       MCU_WIDTH, MCU_HEIGHT);
  for (uint32_t i = 0U; i < 64U; i++)
  {
    assert(output[i] == 77U);
    assert(output[64U + i] == 29U);
  }
}

int main(void)
{
  test_primary_colors();
  test_luma_block_layout();
  test_mcu_raster_order();
  test_center_crop_and_downscale();
  return 0;
}
