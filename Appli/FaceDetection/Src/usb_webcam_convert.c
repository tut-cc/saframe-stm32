#include "usb_webcam.h"
#include <assert.h>
#include <stddef.h>

void UsbWebcam_CopyRgb565CenterDownsample(const uint16_t *source,
    uint32_t source_width, uint32_t source_height, uint16_t *destination,
    uint32_t destination_width, uint32_t destination_height)
{
  assert(source != NULL && destination != NULL);
  assert(source_width >= destination_width && source_height >= destination_height);
  uint32_t crop_width = source_width;
  uint32_t crop_height = source_height;
  if ((uint64_t)source_width * destination_height >
      (uint64_t)source_height * destination_width)
    crop_width = (uint32_t)((uint64_t)source_height * destination_width / destination_height);
  else
    crop_height = (uint32_t)((uint64_t)source_width * destination_height / destination_width);
  const uint32_t crop_x = (source_width - crop_width) / 2U;
  const uint32_t crop_y = (source_height - crop_height) / 2U;
  /* Step floor(i * crop / destination) by accumulation: a 64-bit division per
   * pixel is a library call on Cortex-M55 and took about 10 ms per frame. */
  uint32_t y_offset = 0U;
  uint32_t y_remainder = 0U;
  for (uint32_t y = 0U; y < destination_height; y++) {
    const uint16_t *source_row = &source[(crop_y + y_offset) * source_width + crop_x];
    uint16_t *destination_row = &destination[y * destination_width];
    uint32_t x_offset = 0U;
    uint32_t x_remainder = 0U;
    for (uint32_t x = 0U; x < destination_width; x++) {
      destination_row[x] = source_row[x_offset];
      x_remainder += crop_width;
      while (x_remainder >= destination_width) {
        x_remainder -= destination_width;
        x_offset++;
      }
    }
    y_remainder += crop_height;
    while (y_remainder >= destination_height) {
      y_remainder -= destination_height;
      y_offset++;
    }
  }
}
