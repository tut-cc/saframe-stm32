#include "camera_capture_stream.h"

#include <stddef.h>
#include <string.h>

static bool IsValidPipe(CaptureStreamPipe pipe)
{
  return (uint32_t)pipe < CAPTURE_STREAM_PIPE_COUNT;
}

static bool IsRealBuffer(uint32_t buffer_index)
{
  return (buffer_index != CAPTURE_STREAM_SCRATCH) &&
         (buffer_index != CAPTURE_STREAM_NONE);
}

static void PushEvent(CaptureStream *stream, const CaptureStreamEvent *event)
{
  if (!event->valid && !IsRealBuffer(event->rgb_index) &&
      !IsRealBuffer(event->nn_index))
  {
    /* Nothing to publish and nothing to return to a pool. */
    return;
  }
  if (stream->event_count >= CAPTURE_STREAM_EVENT_CAPACITY)
  {
    stream->event_overflows++;
    return;
  }
  const uint32_t write_index = (stream->event_read_index + stream->event_count) %
                               CAPTURE_STREAM_EVENT_CAPACITY;
  stream->events[write_index] = *event;
  stream->event_count++;
}

static void ReleaseHalf(CaptureStream *stream, CaptureStreamPipe pipe,
                        uint32_t buffer_index, uint32_t cycles)
{
  CaptureStreamEvent event = {
    .valid = false,
    .rgb_index = CAPTURE_STREAM_NONE,
    .nn_index = CAPTURE_STREAM_NONE,
    .completed_cycles = cycles,
  };
  if (pipe == CAPTURE_STREAM_PIPE_DISPLAY)
  {
    event.rgb_index = buffer_index;
  }
  else
  {
    event.nn_index = buffer_index;
  }
  PushEvent(stream, &event);
}

void CaptureStream_Init(CaptureStream *stream, uint32_t pair_window_cycles)
{
  if (stream == NULL)
  {
    return;
  }
  memset(stream, 0, sizeof(*stream));
  for (uint32_t i = 0U; i < CAPTURE_STREAM_PIPE_COUNT; i++)
  {
    stream->pipes[i].queued = CAPTURE_STREAM_NONE;
    stream->pipes[i].writing = CAPTURE_STREAM_NONE;
  }
  stream->pair_window_cycles = pair_window_cycles;
}

bool CaptureStream_Start(CaptureStream *stream, CaptureStreamPipe pipe,
                         uint32_t buffer_index)
{
  if ((stream == NULL) || !IsValidPipe(pipe) ||
      (stream->pipes[pipe].queued != CAPTURE_STREAM_NONE) ||
      (buffer_index == CAPTURE_STREAM_NONE))
  {
    return false;
  }
  stream->pipes[pipe].queued = buffer_index;
  return true;
}

bool CaptureStream_Arm(CaptureStream *stream, CaptureStreamPipe pipe,
                       uint32_t buffer_index)
{
  if ((stream == NULL) || !IsValidPipe(pipe) || !IsRealBuffer(buffer_index))
  {
    return false;
  }
  CaptureStreamPipeState *state = &stream->pipes[pipe];
  if (state->armed_count >= CAPTURE_STREAM_ARM_CAPACITY)
  {
    return false;
  }
  const uint32_t write_index = (state->armed_read_index + state->armed_count) %
                               CAPTURE_STREAM_ARM_CAPACITY;
  state->armed[write_index] = buffer_index;
  state->armed_count++;
  return true;
}

uint32_t CaptureStream_ArmedCount(const CaptureStream *stream,
                                  CaptureStreamPipe pipe)
{
  if ((stream == NULL) || !IsValidPipe(pipe))
  {
    return 0U;
  }
  return stream->pipes[pipe].armed_count;
}

uint32_t CaptureStream_OnVsync(CaptureStream *stream, CaptureStreamPipe pipe)
{
  if ((stream == NULL) || !IsValidPipe(pipe))
  {
    return CAPTURE_STREAM_NONE;
  }
  CaptureStreamPipeState *state = &stream->pipes[pipe];
  if (state->queued == CAPTURE_STREAM_NONE)
  {
    return CAPTURE_STREAM_NONE;
  }
  if (state->writing != CAPTURE_STREAM_NONE)
  {
    /* The frame end of the previous frame was missed; its content is not
     * trusted as a complete frame. */
    stream->sync_errors++;
    if (IsRealBuffer(state->writing))
    {
      ReleaseHalf(stream, pipe, state->writing, 0U);
    }
  }

  /* The address programmed at the previous VSYNC has just been latched. */
  state->writing = state->queued;
  if (state->armed_count > 0U)
  {
    state->queued = state->armed[state->armed_read_index];
    state->armed_read_index = (state->armed_read_index + 1U) %
                              CAPTURE_STREAM_ARM_CAPACITY;
    state->armed_count--;
  }
  else
  {
    state->queued = CAPTURE_STREAM_SCRATCH;
    stream->scratch_frames++;
  }
  return state->queued;
}

bool CaptureStream_OnFrameEnd(CaptureStream *stream, CaptureStreamPipe pipe,
                              uint32_t cycles)
{
  if ((stream == NULL) || !IsValidPipe(pipe))
  {
    return false;
  }
  CaptureStreamPipeState *state = &stream->pipes[pipe];
  const uint32_t done = state->writing;
  state->writing = CAPTURE_STREAM_NONE;
  if (done == CAPTURE_STREAM_NONE)
  {
    return false;
  }

  if (stream->pending_valid)
  {
    if ((stream->pending_pipe != pipe) &&
        ((cycles - stream->pending_cycles) <= stream->pair_window_cycles))
    {
      const uint32_t rgb_index = (pipe == CAPTURE_STREAM_PIPE_DISPLAY) ?
                                 done : stream->pending_index;
      const uint32_t nn_index = (pipe == CAPTURE_STREAM_PIPE_NN) ?
                                done : stream->pending_index;
      const CaptureStreamEvent event = {
        .valid = IsRealBuffer(rgb_index) && IsRealBuffer(nn_index),
        .rgb_index = rgb_index,
        .nn_index = nn_index,
        .completed_cycles = cycles,
      };
      stream->pending_valid = false;
      PushEvent(stream, &event);
      return true;
    }

    /* The pending half belongs to a sensor frame the other pipe did not
     * deliver (e.g. the pipes started on different frames). */
    stream->unpaired_halves++;
    ReleaseHalf(stream, stream->pending_pipe, stream->pending_index,
                stream->pending_cycles);
    stream->pending_valid = true;
    stream->pending_pipe = pipe;
    stream->pending_index = done;
    stream->pending_cycles = cycles;
    return true;
  }

  stream->pending_valid = true;
  stream->pending_pipe = pipe;
  stream->pending_index = done;
  stream->pending_cycles = cycles;
  return false;
}

bool CaptureStream_PopEvent(CaptureStream *stream, CaptureStreamEvent *event)
{
  if ((stream == NULL) || (event == NULL) || (stream->event_count == 0U))
  {
    return false;
  }
  *event = stream->events[stream->event_read_index];
  stream->event_read_index = (stream->event_read_index + 1U) %
                             CAPTURE_STREAM_EVENT_CAPACITY;
  stream->event_count--;
  return true;
}
