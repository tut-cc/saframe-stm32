#ifndef CAMERA_CAPTURE_STREAM_H
#define CAMERA_CAPTURE_STREAM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Buffer rotation for the DCMIPP display pipe (Pipe 1) and NN pipe (Pipe 2)
 * running in continuous mode. Free of HAL and kernel dependencies so that it
 * can be tested on the host.
 *
 * The destination address is a shadow register latched at frame start, so the
 * VSYNC of frame K programs the buffer for frame K+1. When no free buffer is
 * armed, the pipe writes into a scratch buffer and that frame is discarded;
 * a buffer already handed downstream is never overwritten.
 *
 * OnVsync and OnFrameEnd run in the DCMIPP interrupt. Start, Arm and PopEvent
 * run in a task and must be called with the DCMIPP interrupt masked.
 */

#define CAPTURE_STREAM_PIPE_COUNT     (2U)
#define CAPTURE_STREAM_ARM_CAPACITY   (4U)
#define CAPTURE_STREAM_EVENT_CAPACITY (8U)
/* The pipe writes into the scratch buffer. */
#define CAPTURE_STREAM_SCRATCH        (0xFFFFFFFFUL)
/* No buffer is assigned (pipe not started, or nothing being written). */
#define CAPTURE_STREAM_NONE           (0xFFFFFFFEUL)

typedef enum
{
  CAPTURE_STREAM_PIPE_DISPLAY = 0,
  CAPTURE_STREAM_PIPE_NN = 1,
} CaptureStreamPipe;

typedef struct
{
  /* true when both halves are real buffers written from the same sensor frame. */
  bool valid;
  /* Pool index, or CAPTURE_STREAM_SCRATCH / CAPTURE_STREAM_NONE. When valid is
   * false, every real index here must be returned to its pool. */
  uint32_t rgb_index;
  uint32_t nn_index;
  /* CYCCNT read in the frame-end interrupt of the second pipe of the pair. */
  uint32_t completed_cycles;
} CaptureStreamEvent;

typedef struct
{
  uint32_t armed[CAPTURE_STREAM_ARM_CAPACITY];
  uint32_t armed_read_index;
  uint32_t armed_count;
  uint32_t queued;
  uint32_t writing;
} CaptureStreamPipeState;

typedef struct
{
  CaptureStreamPipeState pipes[CAPTURE_STREAM_PIPE_COUNT];
  bool pending_valid;
  CaptureStreamPipe pending_pipe;
  uint32_t pending_index;
  uint32_t pending_cycles;
  uint32_t pair_window_cycles;
  CaptureStreamEvent events[CAPTURE_STREAM_EVENT_CAPACITY];
  uint32_t event_read_index;
  uint32_t event_count;
  /* Pipe frames written into the scratch buffer because nothing was armed. */
  uint32_t scratch_frames;
  /* Completed pipe frames without a partner from the same sensor frame. */
  uint32_t unpaired_halves;
  /* VSYNC arrived while the previous frame end had not been seen. */
  uint32_t sync_errors;
  /* Events lost because the task did not drain them; their buffers leak. */
  uint32_t event_overflows;
} CaptureStream;

void CaptureStream_Init(CaptureStream *stream, uint32_t pair_window_cycles);
/* Record the buffer passed to the continuous-mode start of the pipe. */
bool CaptureStream_Start(CaptureStream *stream, CaptureStreamPipe pipe,
                         uint32_t buffer_index);
/* Hand a free (CAPTURING) buffer to the pipe for a later frame. */
bool CaptureStream_Arm(CaptureStream *stream, CaptureStreamPipe pipe,
                       uint32_t buffer_index);
uint32_t CaptureStream_ArmedCount(const CaptureStream *stream,
                                  CaptureStreamPipe pipe);
/* Returns the buffer to program for the next frame, or CAPTURE_STREAM_NONE
 * when the pipe has not been started. */
uint32_t CaptureStream_OnVsync(CaptureStream *stream, CaptureStreamPipe pipe);
void CaptureStream_OnFrameEnd(CaptureStream *stream, CaptureStreamPipe pipe,
                              uint32_t cycles);
bool CaptureStream_PopEvent(CaptureStream *stream, CaptureStreamEvent *event);

#ifdef __cplusplus
}
#endif

#endif /* CAMERA_CAPTURE_STREAM_H */
