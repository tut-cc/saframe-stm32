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
  assert(!PrivacyBufferPool_Acquire(&pool, &released));
  assert(PrivacyBufferPool_MarkProcessed(&pool, first));
  assert(PrivacyBufferPool_Publish(&pool, first, &released));
  assert(released == 0U);
  assert(PrivacyBufferPool_State(&pool, first) == PRIVACY_FRAME_DISPLAYED);
  assert(PrivacyBufferPool_State(&pool, released) == PRIVACY_FRAME_FREE);
  assert(PrivacyBufferPool_MarkProcessed(&pool, second));
  assert(PrivacyBufferPool_Drop(&pool, second));
  assert(PrivacyBufferPool_State(&pool, second) == PRIVACY_FRAME_FREE);
  assert(!PrivacyBufferPool_Drop(&pool, first));
}

int main(void)
{
  test_empty_full_and_fifo_order();
  test_wraparound();
  test_buffer_pool_state_transitions();
  puts("privacy_pipeline_queue_test: PASS");
  return 0;
}
