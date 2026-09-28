#ifndef PRIVACY_PIPELINE_QUEUE_H
#define PRIVACY_PIPELINE_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include "privacy_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRIVACY_RESULT_QUEUE_CAPACITY (3U)
#define PRIVACY_BACKGROUND_BUFFER_COUNT (3U)

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

void PrivacyResultQueue_Init(PrivacyResultQueue *queue);
bool PrivacyResultQueue_Push(PrivacyResultQueue *queue,
                             const PrivacyFrameResult *result);
bool PrivacyResultQueue_Pop(PrivacyResultQueue *queue,
                            PrivacyFrameResult *result);
uint32_t PrivacyResultQueue_Depth(const PrivacyResultQueue *queue);
uint32_t PrivacyResultQueue_MaximumDepth(const PrivacyResultQueue *queue);
void PrivacyBufferPool_Init(PrivacyBufferPool *pool);
bool PrivacyBufferPool_Acquire(PrivacyBufferPool *pool, uint32_t *buffer_index);
bool PrivacyBufferPool_MarkProcessed(PrivacyBufferPool *pool, uint32_t buffer_index);
bool PrivacyBufferPool_Publish(PrivacyBufferPool *pool, uint32_t buffer_index,
                               uint32_t *released_index);
bool PrivacyBufferPool_Drop(PrivacyBufferPool *pool, uint32_t buffer_index);
PrivacyFrameState PrivacyBufferPool_State(const PrivacyBufferPool *pool,
                                          uint32_t buffer_index);

#ifdef __cplusplus
}
#endif

#endif /* PRIVACY_PIPELINE_QUEUE_H */
