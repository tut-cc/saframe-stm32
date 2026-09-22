#include "privacy_filter.h"

#include <stddef.h>
#include <string.h>

#define ARGB4444_OPAQUE_BLACK  (0xF000U)
#define MOSAIC_BLOCK_SIZE      (16U)

static uint16_t rgb565_to_argb4444(uint16_t color)
{
  const uint16_t red = (color >> 12) & 0x0FU;
  const uint16_t green = (color >> 7) & 0x0FU;
  const uint16_t blue = (color >> 1) & 0x0FU;

  return (uint16_t)(0xF000U | (red << 8) | (green << 4) | blue);
}

static void fill_overlay_rect(const PrivacyRenderTarget *target,
                              uint32_t x,
                              uint32_t y,
                              uint32_t width,
                              uint32_t height,
                              uint16_t color)
{
  if ((target == NULL) || (target->overlay == NULL) ||
      (x >= target->overlay_width) || (y >= target->overlay_height))
  {
    return;
  }

  if (width > (target->overlay_width - x))
  {
    width = target->overlay_width - x;
  }
  if (height > (target->overlay_height - y))
  {
    height = target->overlay_height - y;
  }

  for (uint32_t row = 0; row < height; row++)
  {
    uint16_t *line = &target->overlay[((y + row) * target->overlay_width) + x];
    for (uint32_t column = 0; column < width; column++)
    {
      line[column] = color;
    }
  }
}

static void render_mask(const PrivacyRoi *roi, const PrivacyRenderTarget *target)
{
  fill_overlay_rect(target, (uint32_t)roi->x, (uint32_t)roi->y,
                    roi->width, roi->height, ARGB4444_OPAQUE_BLACK);
}

static void render_mosaic(const PrivacyRoi *roi, const PrivacyRenderTarget *target)
{
  const uint32_t x_start = (uint32_t)roi->x;
  const uint32_t y_start = (uint32_t)roi->y;
  uint32_t x_end = x_start + roi->width;
  uint32_t y_end = y_start + roi->height;

  if (x_end > target->overlay_width)
  {
    x_end = target->overlay_width;
  }
  if (y_end > target->overlay_height)
  {
    y_end = target->overlay_height;
  }

  for (uint32_t y = y_start; y < y_end; y += MOSAIC_BLOCK_SIZE)
  {
    const uint32_t block_height =
        ((y_end - y) < MOSAIC_BLOCK_SIZE) ? (y_end - y) : MOSAIC_BLOCK_SIZE;

    for (uint32_t x = x_start; x < x_end; x += MOSAIC_BLOCK_SIZE)
    {
      const uint32_t block_width =
          ((x_end - x) < MOSAIC_BLOCK_SIZE) ? (x_end - x) : MOSAIC_BLOCK_SIZE;
      uint16_t color = ARGB4444_OPAQUE_BLACK;

      const uint32_t sample_x = x + (block_width / 2U);
      const uint32_t sample_y = y + (block_height / 2U);
      if ((sample_x >= target->background_x) &&
          (sample_y >= target->background_y))
      {
        const uint32_t background_x = sample_x - target->background_x;
        const uint32_t background_y = sample_y - target->background_y;
        if ((background_x < target->background_width) &&
            (background_y < target->background_height) &&
            (target->background != NULL))
        {
          color = rgb565_to_argb4444(
              target->background[(background_y * target->background_width) + background_x]);
        }
      }

      fill_overlay_rect(target, x, y, block_width, block_height, color);
    }
  }
}

void PrivacyFilter_Clear(const PrivacyRenderTarget *target)
{
  if ((target == NULL) || (target->overlay == NULL))
  {
    return;
  }

  memset(target->overlay, 0,
         target->overlay_width * target->overlay_height * sizeof(target->overlay[0]));
}

void PrivacyFilter_Render(const PrivacyFrameResult *result,
                          PrivacyMode mode,
                          const PrivacyRenderTarget *target)
{
  if ((result == NULL) || (target == NULL))
  {
    return;
  }

  PrivacyFilter_Clear(target);

  uint32_t count = result->face_count;
  if (count > PRIVACY_MAX_FACES)
  {
    count = PRIVACY_MAX_FACES;
  }

  for (uint32_t i = 0; i < count; i++)
  {
    const PrivacyRoi *roi = &result->faces[i];
    if ((roi->width == 0U) || (roi->height == 0U) ||
        (roi->x < 0) || (roi->y < 0))
    {
      continue;
    }

    if (mode == PRIVACY_MODE_MOSAIC)
    {
      render_mosaic(roi, target);
    }
    else
    {
      render_mask(roi, target);
    }
  }
}

const char *PrivacyFilter_ModeName(PrivacyMode mode)
{
  return (mode == PRIVACY_MODE_MOSAIC) ? "MOSAIC" : "MASK";
}
