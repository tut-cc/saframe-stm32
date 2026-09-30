#include <assert.h>
#include <stdio.h>

#include "camera_capture_stream.h"
#include "privacy_filter.h"

/* 1 cycle = 1 us keeps the arithmetic readable. */
#define FRAME_CYCLES  (33333U)
#define PAIR_WINDOW   (FRAME_CYCLES / 2U)
#define DEADLINE      (33000U)

#define DISPLAY CAPTURE_STREAM_PIPE_DISPLAY
#define NN      CAPTURE_STREAM_PIPE_NN

static void start_stream(CaptureStream *stream, uint32_t rgb, uint32_t nn)
{
  CaptureStream_Init(stream, PAIR_WINDOW);
  assert(CaptureStream_Start(stream, DISPLAY, rgb));
  assert(CaptureStream_Start(stream, NN, nn));
}

static void vsync_both(CaptureStream *stream, uint32_t *next_rgb, uint32_t *next_nn)
{
  *next_rgb = CaptureStream_OnVsync(stream, DISPLAY);
  *next_nn = CaptureStream_OnVsync(stream, NN);
}

static void test_steady_state_rotates_armed_buffers(void)
{
  CaptureStream stream;
  CaptureStreamEvent event;
  uint32_t next_rgb;
  uint32_t next_nn;
  start_stream(&stream, 1U, 0U);
  assert(CaptureStream_Arm(&stream, DISPLAY, 2U));
  assert(CaptureStream_Arm(&stream, NN, 1U));

  /* Frame 0 is written into the start buffers; frame 1 is queued at its VSYNC. */
  vsync_both(&stream, &next_rgb, &next_nn);
  assert(next_rgb == 2U);
  assert(next_nn == 1U);
  CaptureStream_OnFrameEnd(&stream, DISPLAY, 1000U);
  assert(!CaptureStream_PopEvent(&stream, &event));
  CaptureStream_OnFrameEnd(&stream, NN, 1200U);
  assert(CaptureStream_PopEvent(&stream, &event));
  assert(event.valid);
  assert(event.rgb_index == 1U);
  assert(event.nn_index == 0U);
  assert(event.completed_cycles == 1200U);
  assert(!CaptureStream_PopEvent(&stream, &event));

  assert(CaptureStream_Arm(&stream, DISPLAY, 3U));
  assert(CaptureStream_Arm(&stream, NN, 2U));
  vsync_both(&stream, &next_rgb, &next_nn);
  assert(next_rgb == 3U);
  assert(next_nn == 2U);
  CaptureStream_OnFrameEnd(&stream, DISPLAY, 1000U + FRAME_CYCLES);
  CaptureStream_OnFrameEnd(&stream, NN, 1200U + FRAME_CYCLES);
  assert(CaptureStream_PopEvent(&stream, &event));
  assert(event.valid);
  assert(event.rgb_index == 2U);
  assert(event.nn_index == 1U);
  assert(stream.scratch_frames == 0U);
  assert(stream.unpaired_halves == 0U);
  assert(stream.sync_errors == 0U);
}

static void test_no_armed_buffer_writes_scratch(void)
{
  CaptureStream stream;
  CaptureStreamEvent event;
  uint32_t next_rgb;
  uint32_t next_nn;
  start_stream(&stream, 1U, 0U);

  vsync_both(&stream, &next_rgb, &next_nn);
  /* Buffers 1 and 0 now belong to frame 0 and must never be reprogrammed. */
  assert(next_rgb == CAPTURE_STREAM_SCRATCH);
  assert(next_nn == CAPTURE_STREAM_SCRATCH);
  assert(stream.scratch_frames == 2U);
  CaptureStream_OnFrameEnd(&stream, DISPLAY, 1000U);
  CaptureStream_OnFrameEnd(&stream, NN, 1100U);
  assert(CaptureStream_PopEvent(&stream, &event));
  assert(event.valid);

  /* Frame 1 goes to scratch and yields no event at all. */
  vsync_both(&stream, &next_rgb, &next_nn);
  assert(next_rgb == CAPTURE_STREAM_SCRATCH);
  assert(next_nn == CAPTURE_STREAM_SCRATCH);
  CaptureStream_OnFrameEnd(&stream, DISPLAY, 1000U + FRAME_CYCLES);
  CaptureStream_OnFrameEnd(&stream, NN, 1100U + FRAME_CYCLES);
  assert(!CaptureStream_PopEvent(&stream, &event));
}

static void test_display_pipe_starts_one_frame_early(void)
{
  CaptureStream stream;
  CaptureStreamEvent event;
  CaptureStream_Init(&stream, PAIR_WINDOW);
  assert(CaptureStream_Start(&stream, DISPLAY, 1U));
  assert(CaptureStream_Arm(&stream, DISPLAY, 2U));

  /* Pipe 1 captures sensor frame K alone; Pipe 2 starts at K+1. */
  assert(CaptureStream_OnVsync(&stream, DISPLAY) == 2U);
  assert(CaptureStream_OnVsync(&stream, NN) == CAPTURE_STREAM_NONE);
  assert(CaptureStream_Start(&stream, NN, 0U));
  assert(CaptureStream_Arm(&stream, NN, 1U));
  CaptureStream_OnFrameEnd(&stream, DISPLAY, 1000U);
  assert(!CaptureStream_PopEvent(&stream, &event));

  /* Frame K+1: Pipe 2 completes first. It must not pair with Pipe 1's K. */
  assert(CaptureStream_OnVsync(&stream, DISPLAY) == CAPTURE_STREAM_SCRATCH);
  assert(CaptureStream_OnVsync(&stream, NN) == 1U);
  CaptureStream_OnFrameEnd(&stream, NN, 1000U + FRAME_CYCLES);
  assert(CaptureStream_PopEvent(&stream, &event));
  assert(!event.valid);
  assert(event.rgb_index == 1U);
  assert(event.nn_index == CAPTURE_STREAM_NONE);
  assert(stream.unpaired_halves == 1U);
  CaptureStream_OnFrameEnd(&stream, DISPLAY, 1100U + FRAME_CYCLES);
  assert(CaptureStream_PopEvent(&stream, &event));
  assert(event.valid);
  assert(event.rgb_index == 2U);
  assert(event.nn_index == 0U);
  assert(event.completed_cycles == 1100U + FRAME_CYCLES);
}

static void test_same_pipe_twice_releases_first_half(void)
{
  CaptureStream stream;
  CaptureStreamEvent event;
  start_stream(&stream, 1U, 0U);
  assert(CaptureStream_Arm(&stream, DISPLAY, 2U));

  assert(CaptureStream_OnVsync(&stream, DISPLAY) == 2U);
  CaptureStream_OnFrameEnd(&stream, DISPLAY, 1000U);
  assert(CaptureStream_OnVsync(&stream, DISPLAY) == CAPTURE_STREAM_SCRATCH);
  /* Even within the window, two halves of the same pipe are never a pair. */
  CaptureStream_OnFrameEnd(&stream, DISPLAY, 1001U);
  assert(CaptureStream_PopEvent(&stream, &event));
  assert(!event.valid);
  assert(event.rgb_index == 1U);
  assert(stream.pending_valid);
  assert(stream.pending_index == 2U);
}

static void test_scratch_half_returns_real_half(void)
{
  CaptureStream stream;
  CaptureStreamEvent event;
  uint32_t next_rgb;
  uint32_t next_nn;
  start_stream(&stream, 1U, 0U);
  assert(CaptureStream_Arm(&stream, DISPLAY, 2U));

  vsync_both(&stream, &next_rgb, &next_nn);
  assert(next_rgb == 2U);
  assert(next_nn == CAPTURE_STREAM_SCRATCH);
  CaptureStream_OnFrameEnd(&stream, NN, 1000U);
  CaptureStream_OnFrameEnd(&stream, DISPLAY, 1100U);
  assert(CaptureStream_PopEvent(&stream, &event));
  assert(event.valid);

  /* Frame 1: display buffer 2 is real, NN half is scratch. */
  vsync_both(&stream, &next_rgb, &next_nn);
  CaptureStream_OnFrameEnd(&stream, NN, 1000U + FRAME_CYCLES);
  CaptureStream_OnFrameEnd(&stream, DISPLAY, 1100U + FRAME_CYCLES);
  assert(CaptureStream_PopEvent(&stream, &event));
  assert(!event.valid);
  assert(event.rgb_index == 2U);
  assert(event.nn_index == CAPTURE_STREAM_SCRATCH);
}

static void test_missed_frame_end_is_sync_error(void)
{
  CaptureStream stream;
  CaptureStreamEvent event;
  start_stream(&stream, 1U, 0U);
  assert(CaptureStream_Arm(&stream, DISPLAY, 2U));
  assert(CaptureStream_Arm(&stream, DISPLAY, 3U));

  assert(CaptureStream_OnVsync(&stream, DISPLAY) == 2U);
  /* The frame end of buffer 1 never arrives. */
  assert(CaptureStream_OnVsync(&stream, DISPLAY) == 3U);
  assert(stream.sync_errors == 1U);
  assert(CaptureStream_PopEvent(&stream, &event));
  assert(!event.valid);
  assert(event.rgb_index == 1U);
  assert(stream.pipes[DISPLAY].writing == 2U);
}

static void test_arm_capacity_and_event_overflow(void)
{
  CaptureStream stream;
  start_stream(&stream, 0U, 0U);
  for (uint32_t i = 0U; i < CAPTURE_STREAM_ARM_CAPACITY; i++)
  {
    assert(CaptureStream_Arm(&stream, DISPLAY, i + 1U));
  }
  assert(!CaptureStream_Arm(&stream, DISPLAY, 9U));
  assert(!CaptureStream_Arm(&stream, NN, CAPTURE_STREAM_SCRATCH));
  assert(CaptureStream_ArmedCount(&stream, DISPLAY) == CAPTURE_STREAM_ARM_CAPACITY);

  CaptureStream_Init(&stream, PAIR_WINDOW);
  assert(CaptureStream_Start(&stream, DISPLAY, 0U));
  for (uint32_t i = 0U; i <= CAPTURE_STREAM_EVENT_CAPACITY + 1U; i++)
  {
    assert(CaptureStream_Arm(&stream, DISPLAY, i + 1U));
    (void)CaptureStream_OnVsync(&stream, DISPLAY);
    CaptureStream_OnFrameEnd(&stream, DISPLAY, i * FRAME_CYCLES);
  }
  assert(stream.event_count == CAPTURE_STREAM_EVENT_CAPACITY);
  assert(stream.event_overflows == 1U);
}

/* #4: the deadline starts at the frame-end interrupt, not when the capture
 * task resumes. A frame that completes safety processing 38 ms after the
 * interrupt must be dropped even if the task picked it up 6 ms late. */
static void test_deadline_starts_at_frame_end_interrupt(void)
{
  CaptureStream stream;
  CaptureStreamEvent event;
  uint32_t next_rgb;
  uint32_t next_nn;
  const uint32_t interrupt_at = 500000U;
  const uint32_t task_resumed_at = interrupt_at + 6000U;
  const uint32_t safety_completed_at = interrupt_at + 38000U;
  start_stream(&stream, 1U, 0U);

  vsync_both(&stream, &next_rgb, &next_nn);
  CaptureStream_OnFrameEnd(&stream, DISPLAY, interrupt_at - 200U);
  CaptureStream_OnFrameEnd(&stream, NN, interrupt_at);
  assert(CaptureStream_PopEvent(&stream, &event));
  assert(event.valid);
  assert(event.completed_cycles == interrupt_at);

  assert(PrivacyFrame_IsWithinDeadline(task_resumed_at, safety_completed_at,
                                       DEADLINE));
  assert(!PrivacyFrame_IsWithinDeadline(event.completed_cycles,
                                        safety_completed_at, DEADLINE));
  assert(PrivacyFrame_IsWithinDeadline(event.completed_cycles,
                                       interrupt_at + DEADLINE, DEADLINE));
}

static void test_pair_window_handles_counter_wrap(void)
{
  CaptureStream stream;
  CaptureStreamEvent event;
  uint32_t next_rgb;
  uint32_t next_nn;
  start_stream(&stream, 1U, 0U);

  vsync_both(&stream, &next_rgb, &next_nn);
  CaptureStream_OnFrameEnd(&stream, DISPLAY, UINT32_MAX - 100U);
  CaptureStream_OnFrameEnd(&stream, NN, 100U);
  assert(CaptureStream_PopEvent(&stream, &event));
  assert(event.valid);
  assert(event.completed_cycles == 100U);
}

static void test_frame_end_reports_settled_frames_once(void)
{
  CaptureStream stream;
  uint32_t next_rgb;
  uint32_t next_nn;
  start_stream(&stream, 1U, 0U);

  /* Real pair: only the second pipe settles the frame. */
  vsync_both(&stream, &next_rgb, &next_nn);
  assert(!CaptureStream_OnFrameEnd(&stream, DISPLAY, 1000U));
  assert(CaptureStream_OnFrameEnd(&stream, NN, 1100U));

  /* Scratch pair: no event, but still reported so the task re-arms. */
  vsync_both(&stream, &next_rgb, &next_nn);
  assert(!CaptureStream_OnFrameEnd(&stream, NN, 1000U + FRAME_CYCLES));
  assert(CaptureStream_OnFrameEnd(&stream, DISPLAY, 1100U + FRAME_CYCLES));
  assert(stream.event_count == 1U);

  /* A lone half given up when the next frame arrives. */
  vsync_both(&stream, &next_rgb, &next_nn);
  assert(!CaptureStream_OnFrameEnd(&stream, DISPLAY, 1000U + 2U * FRAME_CYCLES));
  (void)CaptureStream_OnVsync(&stream, DISPLAY);
  assert(CaptureStream_OnFrameEnd(&stream, DISPLAY, 1000U + 3U * FRAME_CYCLES));
}

int main(void)
{
  test_steady_state_rotates_armed_buffers();
  test_no_armed_buffer_writes_scratch();
  test_display_pipe_starts_one_frame_early();
  test_same_pipe_twice_releases_first_half();
  test_scratch_half_returns_real_half();
  test_missed_frame_end_is_sync_error();
  test_arm_capacity_and_event_overflow();
  test_deadline_starts_at_frame_end_interrupt();
  test_pair_window_handles_counter_wrap();
  test_frame_end_reports_settled_frames_once();
  puts("camera_capture_stream_test: PASS");
  return 0;
}
