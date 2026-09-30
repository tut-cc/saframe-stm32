#ifndef PRIVACY_PIPELINE_QUEUE_H
#define PRIVACY_PIPELINE_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include "privacy_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRIVACY_RESULT_QUEUE_CAPACITY (4U)
#define PRIVACY_CAPTURE_QUEUE_CAPACITY (3U)
#define PRIVACY_BACKGROUND_BUFFER_COUNT (5U)
#define PRIVACY_NN_BUFFER_COUNT (3U)

/* Continuous capture does not wait for downstream stages, so each queue must
 * hold every buffer that can reach it: a captured job keeps its NN buffer until
 * the inference stage pops it, and a processed result keeps its background
 * buffer, of which one is always displayed. */
_Static_assert(PRIVACY_CAPTURE_QUEUE_CAPACITY >= PRIVACY_NN_BUFFER_COUNT,
               "capture queue must hold every NN buffer");
_Static_assert(PRIVACY_RESULT_QUEUE_CAPACITY >= PRIVACY_BACKGROUND_BUFFER_COUNT - 1U,
               "result queue must hold every non-displayed background buffer");

typedef struct
{
  uint32_t frame_number;
  uint32_t rgb_buffer_index;
  uint32_t nn_buffer_index;
  /* Frame-end interrupt of the previous captured frame. */
  uint32_t capture_started_cycles;
  /* Frame-end interrupt of the second pipe; the ADR-0006 deadline origin. */
  uint32_t capture_completed_cycles;
  uint32_t capture_us;
  /* From capture_completed_cycles until the capture task took the frame. */
  uint32_t capture_lag_us;
} PrivacyCaptureJob;

typedef struct
{
  PrivacyCaptureJob entries[PRIVACY_CAPTURE_QUEUE_CAPACITY];
  uint32_t read_index;
  uint32_t write_index;
  uint32_t count;
  uint32_t maximum_depth;
} PrivacyCaptureQueue;

typedef struct
{
  PrivacyFrameResult entries[PRIVACY_RESULT_QUEUE_CAPACITY];
  uint32_t read_index;
  uint32_t write_index;
  uint32_t count;
  uint32_t maximum_depth;
} PrivacyResultQueue;

typedef struct
{
  PrivacyFrameState states[PRIVACY_BACKGROUND_BUFFER_COUNT];
  uint32_t displayed_index;
} PrivacyBufferPool;

typedef enum
{
  PRIVACY_NN_BUFFER_FREE = 0,
  PRIVACY_NN_BUFFER_CAPTURING,
  PRIVACY_NN_BUFFER_COPYING,
} PrivacyNnBufferState;

typedef struct
{
  PrivacyNnBufferState states[PRIVACY_NN_BUFFER_COUNT];
} PrivacyNnBufferPool;

void PrivacyCaptureQueue_Init(PrivacyCaptureQueue *queue);
bool PrivacyCaptureQueue_Push(PrivacyCaptureQueue *queue,
                              const PrivacyCaptureJob *job);
bool PrivacyCaptureQueue_Pop(PrivacyCaptureQueue *queue,
                             PrivacyCaptureJob *job);
uint32_t PrivacyCaptureQueue_Depth(const PrivacyCaptureQueue *queue);
uint32_t PrivacyCaptureQueue_MaximumDepth(const PrivacyCaptureQueue *queue);

void PrivacyResultQueue_Init(PrivacyResultQueue *queue);
bool PrivacyResultQueue_Push(PrivacyResultQueue *queue,
                             const PrivacyFrameResult *result);
bool PrivacyResultQueue_Pop(PrivacyResultQueue *queue,
                            PrivacyFrameResult *result);
uint32_t PrivacyResultQueue_Depth(const PrivacyResultQueue *queue);
uint32_t PrivacyResultQueue_MaximumDepth(const PrivacyResultQueue *queue);
void PrivacyBufferPool_Init(PrivacyBufferPool *pool);
bool PrivacyBufferPool_Acquire(PrivacyBufferPool *pool, uint32_t *buffer_index);
bool PrivacyBufferPool_MarkCaptured(PrivacyBufferPool *pool, uint32_t buffer_index);
bool PrivacyBufferPool_MarkInference(PrivacyBufferPool *pool, uint32_t buffer_index);
bool PrivacyBufferPool_MarkProcessed(PrivacyBufferPool *pool, uint32_t buffer_index);
bool PrivacyBufferPool_Publish(PrivacyBufferPool *pool, uint32_t buffer_index,
                               uint32_t *released_index);
bool PrivacyBufferPool_Drop(PrivacyBufferPool *pool, uint32_t buffer_index);
/* Return a CAPTURING buffer whose camera frame was discarded. */
bool PrivacyBufferPool_Cancel(PrivacyBufferPool *pool, uint32_t buffer_index);
PrivacyFrameState PrivacyBufferPool_State(const PrivacyBufferPool *pool,
                                          uint32_t buffer_index);
void PrivacyNnBufferPool_Init(PrivacyNnBufferPool *pool);
bool PrivacyNnBufferPool_Acquire(PrivacyNnBufferPool *pool, uint32_t *buffer_index);
bool PrivacyNnBufferPool_MarkCopying(PrivacyNnBufferPool *pool, uint32_t buffer_index);
bool PrivacyNnBufferPool_Release(PrivacyNnBufferPool *pool, uint32_t buffer_index);
bool PrivacyNnBufferPool_Cancel(PrivacyNnBufferPool *pool, uint32_t buffer_index);

#ifdef __cplusplus
}
#endif

#endif /* PRIVACY_PIPELINE_QUEUE_H */
