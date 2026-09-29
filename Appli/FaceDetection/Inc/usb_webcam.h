#ifndef USB_WEBCAM_H
#define USB_WEBCAM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USB_WEBCAM_WIDTH  (1280U)
#define USB_WEBCAM_HEIGHT (720U)
#define USB_WEBCAM_FPS    (15U)
/* VENC JPEG quantization level, 0 (smallest) to 9 (best quality). */
#define USB_WEBCAM_JPEG_QUALITY (6U)
/* Upper bound of one encoded frame, also advertised as dwMaxVideoFrameSize.
 * One isochronous transaction per microframe carries about 272 KB per frame
 * at 30 fps, so frames above this size could not keep up anyway. */
#define USB_WEBCAM_JPEG_MAX_SIZE (256U * 1024U)

/* Why frames offered while streaming were not sent. */
typedef struct
{
  uint32_t stream_starts; /* times the host started streaming */
  uint32_t busy;    /* both USB buffers still owned by UVC */
  uint32_t encode;  /* VENC failed or the JPEG did not fit */
  uint32_t show;    /* UVC still holds a frame that has not started */
  int32_t last_encode_error;
} UsbWebcam_DropCounts;

void UsbWebcam_Init(void);
void UsbWebcam_IRQHandler(void);
void UsbWebcam_VencIRQHandler(void);
/* The frame must be USB_WEBCAM_WIDTH x USB_WEBCAM_HEIGHT RGB565 with its data
 * cache already cleaned. */
bool UsbWebcam_SubmitRgb565(const uint16_t *frame);
uint32_t UsbWebcam_SubmittedFrames(void);
uint32_t UsbWebcam_DroppedFrames(void);
uint32_t UsbWebcam_LastJpegBytes(void);
uint32_t UsbWebcam_MaximumEncodeUs(void);
UsbWebcam_DropCounts UsbWebcam_GetDropCounts(void);
/* TEMPORARY: VENC bus error diagnostic, see usb_webcam.c. */
void UsbWebcam_SelfTest(const uint16_t *frame, const char *label);
/* Returns 0 on success or the VENC error code. */
int32_t UsbWebcam_SelfTestEncodeOnce(const uint16_t *frame, bool to_axisram2);
bool UsbWebcam_IsStreaming(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_WEBCAM_H */
