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
  for (uint32_t y = 0U; y < destination_height; y++) {
    const uint32_t sy = crop_y + (uint32_t)((uint64_t)y * crop_height / destination_height);
    for (uint32_t x = 0U; x < destination_width; x++) {
      const uint32_t sx = crop_x + (uint32_t)((uint64_t)x * crop_width / destination_width);
      destination[y * destination_width + x] = source[sy * source_width + sx];
    }
  }
}
