#include <assert.h>
#include <stdio.h>

#include "privacy_pipeline_queue.h"

static PrivacyFrameResult make_result(uint32_t frame_number)
{
  PrivacyFrameResult result = {0};
  result.frame_number = frame_number;
  return result;
}

static void test_empty_full_and_fifo_order(void)
{
  PrivacyResultQueue queue;
  PrivacyFrameResult result;
  PrivacyResultQueue_Init(&queue);

  assert(PrivacyResultQueue_Depth(&queue) == 0U);
  assert(!PrivacyResultQueue_Pop(&queue, &result));

  for (uint32_t i = 1U; i <= PRIVACY_RESULT_QUEUE_CAPACITY; i++)
  {
    const PrivacyFrameResult input = make_result(i);
    assert(PrivacyResultQueue_Push(&queue, &input));
  }
  assert(PrivacyResultQueue_Depth(&queue) == PRIVACY_RESULT_QUEUE_CAPACITY);
  assert(PrivacyResultQueue_MaximumDepth(&queue) == PRIVACY_RESULT_QUEUE_CAPACITY);
  const PrivacyFrameResult overflow = make_result(99U);
  assert(!PrivacyResultQueue_Push(&queue, &overflow));

  for (uint32_t i = 1U; i <= PRIVACY_RESULT_QUEUE_CAPACITY; i++)
  {
    assert(PrivacyResultQueue_Pop(&queue, &result));
    assert(result.frame_number == i);
  }
  assert(!PrivacyResultQueue_Pop(&queue, &result));
}

static void test_wraparound(void)
{
  PrivacyResultQueue queue;
  PrivacyFrameResult result;
  PrivacyResultQueue_Init(&queue);

  for (uint32_t i = 1U; i <= 2U; i++)
  {
    const PrivacyFrameResult input = make_result(i);
    assert(PrivacyResultQueue_Push(&queue, &input));
  }
  assert(PrivacyResultQueue_Pop(&queue, &result));
  assert(result.frame_number == 1U);

  for (uint32_t i = 3U; i <= 4U; i++)
  {
    const PrivacyFrameResult input = make_result(i);
    assert(PrivacyResultQueue_Push(&queue, &input));
  }
  for (uint32_t expected = 2U; expected <= 4U; expected++)
  {
    assert(PrivacyResultQueue_Pop(&queue, &result));
    assert(result.frame_number == expected);
  }
}

static void test_buffer_pool_state_transitions(void)
{
  PrivacyBufferPool pool;
  uint32_t first;
  uint32_t second;
  uint32_t released;
  PrivacyBufferPool_Init(&pool);
  assert(PrivacyBufferPool_State(&pool, 0U) == PRIVACY_FRAME_DISPLAYED);
  assert(PrivacyBufferPool_Acquire(&pool, &first));
  assert(PrivacyBufferPool_Acquire(&pool, &second));
  assert(first != second);
  assert(PrivacyBufferPool_Acquire(&pool, &released));
  assert(!PrivacyBufferPool_Acquire(&pool, &released));
  assert(PrivacyBufferPool_MarkCaptured(&pool, first));
  assert(PrivacyBufferPool_MarkInference(&pool, first));
  assert(PrivacyBufferPool_MarkProcessed(&pool, first));
  assert(PrivacyBufferPool_Publish(&pool, first, &released));
  assert(released == 0U);
  assert(PrivacyBufferPool_State(&pool, first) == PRIVACY_FRAME_DISPLAYED);
  assert(PrivacyBufferPool_State(&pool, released) == PRIVACY_FRAME_FREE);
  assert(PrivacyBufferPool_MarkCaptured(&pool, second));
  assert(PrivacyBufferPool_MarkInference(&pool, second));
  assert(PrivacyBufferPool_MarkProcessed(&pool, second));
  assert(PrivacyBufferPool_Drop(&pool, second));
  assert(PrivacyBufferPool_State(&pool, second) == PRIVACY_FRAME_FREE);
  assert(!PrivacyBufferPool_Drop(&pool, first));
}

static void test_capture_queue_and_nn_pool(void)
{
  PrivacyCaptureQueue queue;
  PrivacyNnBufferPool nn_pool;
  PrivacyCaptureJob job;
  uint32_t nn0;
  uint32_t nn1;
  PrivacyCaptureQueue_Init(&queue);
  PrivacyNnBufferPool_Init(&nn_pool);

  assert(PrivacyNnBufferPool_Acquire(&nn_pool, &nn0));
  assert(PrivacyNnBufferPool_Acquire(&nn_pool, &nn1));
  assert(nn0 != nn1);
  assert(!PrivacyNnBufferPool_Acquire(&nn_pool, &job.nn_buffer_index));
  assert(PrivacyNnBufferPool_MarkCopying(&nn_pool, nn0));
  assert(PrivacyNnBufferPool_Release(&nn_pool, nn0));
  assert(PrivacyNnBufferPool_Acquire(&nn_pool, &nn0));

  for (uint32_t i = 0U; i < PRIVACY_CAPTURE_QUEUE_CAPACITY; i++)
  {
    PrivacyCaptureJob input = {
      .frame_number = i + 1U,
      .rgb_buffer_index = i,
      .nn_buffer_index = i % PRIVACY_NN_BUFFER_COUNT,
    };
    assert(PrivacyCaptureQueue_Push(&queue, &input));
  }
  assert(!PrivacyCaptureQueue_Push(&queue, &job));
  assert(PrivacyCaptureQueue_MaximumDepth(&queue) == PRIVACY_CAPTURE_QUEUE_CAPACITY);
  for (uint32_t i = 0U; i < PRIVACY_CAPTURE_QUEUE_CAPACITY; i++)
  {
    assert(PrivacyCaptureQueue_Pop(&queue, &job));
    assert(job.frame_number == i + 1U);
    assert(job.rgb_buffer_index == i);
  }
  assert(!PrivacyCaptureQueue_Pop(&queue, &job));
}

int main(void)
{
  test_empty_full_and_fifo_order();
  test_wraparound();
  test_buffer_pool_state_transitions();
  test_capture_queue_and_nn_pool();
  puts("privacy_pipeline_queue_test: PASS");
  return 0;
}
