#include "privacy_filter.h"
#include "privacy_pipeline_queue.h"

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

static void fill_rgb565_rect(uint16_t *frame,
                             uint32_t frame_width,
                             uint32_t x,
                             uint32_t y,
                             uint32_t width,
                             uint32_t height,
                             uint16_t color)
{
  for (uint32_t row = 0; row < height; row++)
  {
    uint16_t *line = &frame[((y + row) * frame_width) + x];
    for (uint32_t column = 0; column < width; column++)
    {
      line[column] = color;
    }
  }
}

static uint16_t average_rgb565_block(const uint16_t *frame,
                                     uint32_t frame_width,
                                     uint32_t x,
                                     uint32_t y,
                                     uint32_t width,
                                     uint32_t height)
{
  uint32_t red = 0U;
  uint32_t green = 0U;
  uint32_t blue = 0U;
  const uint32_t pixel_count = width * height;

  for (uint32_t row = 0; row < height; row++)
  {
    const uint16_t *line = &frame[((y + row) * frame_width) + x];
    for (uint32_t column = 0; column < width; column++)
    {
      const uint16_t pixel = line[column];
      red += (pixel >> 11) & 0x1FU;
      green += (pixel >> 5) & 0x3FU;
      blue += pixel & 0x1FU;
    }
  }

  red /= pixel_count;
  green /= pixel_count;
  blue /= pixel_count;
  return (uint16_t)((red << 11) | (green << 5) | blue);
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

  uint32_t count = result->detection_count;
  if (count > PRIVACY_MAX_DETECTIONS)
  {
    count = PRIVACY_MAX_DETECTIONS;
  }

  for (uint32_t i = 0; i < count; i++)
  {
    const PrivacyRoi *roi = &result->detections[i];
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

void PrivacyFilter_ApplyRgb565(const PrivacyFrameResult *result,
                               PrivacyMode mode,
                               uint16_t *frame,
                               uint32_t width,
                               uint32_t height)
{
  if ((result == NULL) || (frame == NULL) || (width == 0U) || (height == 0U))
  {
    return;
  }

  uint32_t count = result->detection_count;
  if (count > PRIVACY_MAX_DETECTIONS)
  {
    count = PRIVACY_MAX_DETECTIONS;
  }

  for (uint32_t i = 0; i < count; i++)
  {
    const PrivacyRoi *roi = &result->detections[i];
    if ((roi->x < 0) || (roi->y < 0) || (roi->width == 0U) || (roi->height == 0U))
    {
      continue;
    }

    const uint32_t x_start = (uint32_t)roi->x;
    const uint32_t y_start = (uint32_t)roi->y;
    if ((x_start >= width) || (y_start >= height))
    {
      continue;
    }
    const uint32_t x_end = (roi->width > (width - x_start)) ? width : x_start + roi->width;
    const uint32_t y_end = (roi->height > (height - y_start)) ? height : y_start + roi->height;

    if (mode == PRIVACY_MODE_MASK)
    {
      fill_rgb565_rect(frame, width, x_start, y_start,
                       x_end - x_start, y_end - y_start, 0U);
      continue;
    }

    for (uint32_t y = y_start; y < y_end; y += MOSAIC_BLOCK_SIZE)
    {
      const uint32_t block_height =
          ((y_end - y) < MOSAIC_BLOCK_SIZE) ? (y_end - y) : MOSAIC_BLOCK_SIZE;
      for (uint32_t x = x_start; x < x_end; x += MOSAIC_BLOCK_SIZE)
      {
        const uint32_t block_width =
            ((x_end - x) < MOSAIC_BLOCK_SIZE) ? (x_end - x) : MOSAIC_BLOCK_SIZE;
        const uint16_t color = average_rgb565_block(frame, width, x, y,
                                                    block_width, block_height);
        fill_rgb565_rect(frame, width, x, y, block_width, block_height, color);
      }
    }
  }
}

bool PrivacyFrame_IsWithinDeadline(uint32_t started_at,
                                   uint32_t completed_at,
                                   uint32_t deadline_cycles)
{
  return (completed_at - started_at) <= deadline_cycles;
}

const char *PrivacyFilter_ModeName(PrivacyMode mode)
{
  return (mode == PRIVACY_MODE_MOSAIC) ? "MOSAIC" : "MASK";
}

bool PrivacyProxyClass_IsValid(int32_t class_index)
{
  return (class_index >= 0) &&
         ((uint32_t)class_index < PRIVACY_PROXY_CLASS_COUNT);
}

const char *PrivacyProxyClass_InternalName(uint32_t class_index)
{
  static const char *const names[PRIVACY_PROXY_CLASS_COUNT] = {
    "person", "book", "stop sign"
  };
  return (class_index < PRIVACY_PROXY_CLASS_COUNT) ? names[class_index] : "invalid";
}

const char *PrivacyProxyClass_DisplayName(uint32_t class_index)
{
  static const char *const names[PRIVACY_PROXY_CLASS_COUNT] = {
    "FACE", "DOCUMENT", "LOGO"
  };
  return (class_index < PRIVACY_PROXY_CLASS_COUNT) ? names[class_index] : "INVALID";
}

void PrivacyCaptureQueue_Init(PrivacyCaptureQueue *queue)
{
  if (queue != NULL)
  {
    memset(queue, 0, sizeof(*queue));
  }
}

bool PrivacyCaptureQueue_Push(PrivacyCaptureQueue *queue,
                              const PrivacyCaptureJob *job)
{
  if ((queue == NULL) || (job == NULL) ||
      (queue->count >= PRIVACY_CAPTURE_QUEUE_CAPACITY))
  {
    return false;
  }
  queue->entries[queue->write_index] = *job;
  queue->write_index = (queue->write_index + 1U) % PRIVACY_CAPTURE_QUEUE_CAPACITY;
  queue->count++;
  if (queue->count > queue->maximum_depth)
  {
    queue->maximum_depth = queue->count;
  }
  return true;
}

bool PrivacyCaptureQueue_Pop(PrivacyCaptureQueue *queue,
                             PrivacyCaptureJob *job)
{
  if ((queue == NULL) || (job == NULL) || (queue->count == 0U))
  {
    return false;
  }
  *job = queue->entries[queue->read_index];
  queue->read_index = (queue->read_index + 1U) % PRIVACY_CAPTURE_QUEUE_CAPACITY;
  queue->count--;
  return true;
}

uint32_t PrivacyCaptureQueue_Depth(const PrivacyCaptureQueue *queue)
{
  return (queue != NULL) ? queue->count : 0U;
}

uint32_t PrivacyCaptureQueue_MaximumDepth(const PrivacyCaptureQueue *queue)
{
  return (queue != NULL) ? queue->maximum_depth : 0U;
}

void PrivacyResultQueue_Init(PrivacyResultQueue *queue)
{
  if (queue != NULL)
  {
    memset(queue, 0, sizeof(*queue));
  }
}

bool PrivacyResultQueue_Push(PrivacyResultQueue *queue,
                             const PrivacyFrameResult *result)
{
  if ((queue == NULL) || (result == NULL) ||
      (queue->count >= PRIVACY_RESULT_QUEUE_CAPACITY))
  {
    return false;
  }
  queue->entries[queue->write_index] = *result;
  queue->write_index = (queue->write_index + 1U) % PRIVACY_RESULT_QUEUE_CAPACITY;
  queue->count++;
  if (queue->count > queue->maximum_depth)
  {
    queue->maximum_depth = queue->count;
  }
  return true;
}

bool PrivacyResultQueue_Pop(PrivacyResultQueue *queue,
                            PrivacyFrameResult *result)
{
  if ((queue == NULL) || (result == NULL) || (queue->count == 0U))
  {
    return false;
  }
  *result = queue->entries[queue->read_index];
  queue->read_index = (queue->read_index + 1U) % PRIVACY_RESULT_QUEUE_CAPACITY;
  queue->count--;
  return true;
}

uint32_t PrivacyResultQueue_Depth(const PrivacyResultQueue *queue)
{
  return (queue != NULL) ? queue->count : 0U;
}

uint32_t PrivacyResultQueue_MaximumDepth(const PrivacyResultQueue *queue)
{
  return (queue != NULL) ? queue->maximum_depth : 0U;
}

void PrivacyBufferPool_Init(PrivacyBufferPool *pool)
{
  if (pool == NULL)
  {
    return;
  }
  for (uint32_t i = 0U; i < PRIVACY_BACKGROUND_BUFFER_COUNT; i++)
  {
    pool->states[i] = PRIVACY_FRAME_FREE;
  }
  pool->displayed_index = 0U;
  pool->states[pool->displayed_index] = PRIVACY_FRAME_DISPLAYED;
}

bool PrivacyBufferPool_Acquire(PrivacyBufferPool *pool, uint32_t *buffer_index)
{
  if ((pool == NULL) || (buffer_index == NULL))
  {
    return false;
  }
  for (uint32_t i = 0U; i < PRIVACY_BACKGROUND_BUFFER_COUNT; i++)
  {
    if (pool->states[i] == PRIVACY_FRAME_FREE)
    {
      pool->states[i] = PRIVACY_FRAME_CAPTURING;
      *buffer_index = i;
      return true;
    }
  }
  return false;
}

bool PrivacyBufferPool_MarkCaptured(PrivacyBufferPool *pool, uint32_t buffer_index)
{
  if ((pool == NULL) || (buffer_index >= PRIVACY_BACKGROUND_BUFFER_COUNT) ||
      (pool->states[buffer_index] != PRIVACY_FRAME_CAPTURING))
  {
    return false;
  }
  pool->states[buffer_index] = PRIVACY_FRAME_CAPTURED;
  return true;
}

bool PrivacyBufferPool_MarkInference(PrivacyBufferPool *pool, uint32_t buffer_index)
{
  if ((pool == NULL) || (buffer_index >= PRIVACY_BACKGROUND_BUFFER_COUNT) ||
      (pool->states[buffer_index] != PRIVACY_FRAME_CAPTURED))
  {
    return false;
  }
  pool->states[buffer_index] = PRIVACY_FRAME_INFERENCE;
  return true;
}

bool PrivacyBufferPool_MarkProcessed(PrivacyBufferPool *pool, uint32_t buffer_index)
{
  if ((pool == NULL) || (buffer_index >= PRIVACY_BACKGROUND_BUFFER_COUNT) ||
      (pool->states[buffer_index] != PRIVACY_FRAME_INFERENCE))
  {
    return false;
  }
  pool->states[buffer_index] = PRIVACY_FRAME_PROCESSED;
  return true;
}

bool PrivacyBufferPool_Publish(PrivacyBufferPool *pool, uint32_t buffer_index,
                               uint32_t *released_index)
{
  if ((pool == NULL) || (released_index == NULL) ||
      (buffer_index >= PRIVACY_BACKGROUND_BUFFER_COUNT) ||
      (pool->states[buffer_index] != PRIVACY_FRAME_PROCESSED) ||
      (pool->states[pool->displayed_index] != PRIVACY_FRAME_DISPLAYED))
  {
    return false;
  }
  *released_index = pool->displayed_index;
  pool->states[*released_index] = PRIVACY_FRAME_FREE;
  pool->states[buffer_index] = PRIVACY_FRAME_DISPLAYED;
  pool->displayed_index = buffer_index;
  return true;
}

bool PrivacyBufferPool_Drop(PrivacyBufferPool *pool, uint32_t buffer_index)
{
  if ((pool == NULL) || (buffer_index >= PRIVACY_BACKGROUND_BUFFER_COUNT) ||
      (pool->states[buffer_index] != PRIVACY_FRAME_PROCESSED))
  {
    return false;
  }
  pool->states[buffer_index] = PRIVACY_FRAME_DROPPED;
  pool->states[buffer_index] = PRIVACY_FRAME_FREE;
  return true;
}

bool PrivacyBufferPool_Cancel(PrivacyBufferPool *pool, uint32_t buffer_index)
{
  if ((pool == NULL) || (buffer_index >= PRIVACY_BACKGROUND_BUFFER_COUNT) ||
      (pool->states[buffer_index] != PRIVACY_FRAME_CAPTURING))
  {
    return false;
  }
  pool->states[buffer_index] = PRIVACY_FRAME_FREE;
  return true;
}

PrivacyFrameState PrivacyBufferPool_State(const PrivacyBufferPool *pool,
                                          uint32_t buffer_index)
{
  if ((pool == NULL) || (buffer_index >= PRIVACY_BACKGROUND_BUFFER_COUNT))
  {
    return PRIVACY_FRAME_DROPPED;
  }
  return pool->states[buffer_index];
}

void PrivacyNnBufferPool_Init(PrivacyNnBufferPool *pool)
{
  if (pool != NULL)
  {
    for (uint32_t i = 0U; i < PRIVACY_NN_BUFFER_COUNT; i++)
    {
      pool->states[i] = PRIVACY_NN_BUFFER_FREE;
    }
  }
}

bool PrivacyNnBufferPool_Acquire(PrivacyNnBufferPool *pool, uint32_t *buffer_index)
{
  if ((pool == NULL) || (buffer_index == NULL))
  {
    return false;
  }
  for (uint32_t i = 0U; i < PRIVACY_NN_BUFFER_COUNT; i++)
  {
    if (pool->states[i] == PRIVACY_NN_BUFFER_FREE)
    {
      pool->states[i] = PRIVACY_NN_BUFFER_CAPTURING;
      *buffer_index = i;
      return true;
    }
  }
  return false;
}

bool PrivacyNnBufferPool_MarkCopying(PrivacyNnBufferPool *pool, uint32_t buffer_index)
{
  if ((pool == NULL) || (buffer_index >= PRIVACY_NN_BUFFER_COUNT) ||
      (pool->states[buffer_index] != PRIVACY_NN_BUFFER_CAPTURING))
  {
    return false;
  }
  pool->states[buffer_index] = PRIVACY_NN_BUFFER_COPYING;
  return true;
}

bool PrivacyNnBufferPool_Release(PrivacyNnBufferPool *pool, uint32_t buffer_index)
{
  if ((pool == NULL) || (buffer_index >= PRIVACY_NN_BUFFER_COUNT) ||
      (pool->states[buffer_index] != PRIVACY_NN_BUFFER_COPYING))
  {
    return false;
  }
  pool->states[buffer_index] = PRIVACY_NN_BUFFER_FREE;
  return true;
}

bool PrivacyNnBufferPool_Cancel(PrivacyNnBufferPool *pool, uint32_t buffer_index)
{
  if ((pool == NULL) || (buffer_index >= PRIVACY_NN_BUFFER_COUNT) ||
      (pool->states[buffer_index] != PRIVACY_NN_BUFFER_CAPTURING))
  {
    return false;
  }
  pool->states[buffer_index] = PRIVACY_NN_BUFFER_FREE;
  return true;
}
