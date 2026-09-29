#ifndef USB_WEBCAM_H
#define USB_WEBCAM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USB_WEBCAM_WIDTH  (320U)
#define USB_WEBCAM_HEIGHT (240U)
#define USB_WEBCAM_FPS    (10U)
#define USB_WEBCAM_JPEG_QUALITY (75U)
/* YCbCr 4:2:2 MCUs (16 x 8 pixels, 256 bytes each) fed to the JPEG encoder. */
#define USB_WEBCAM_MCU_SIZE (USB_WEBCAM_WIDTH * USB_WEBCAM_HEIGHT * 2U)
/* Upper bound of one encoded frame, also advertised as dwMaxVideoFrameSize. */
#define USB_WEBCAM_JPEG_MAX_SIZE (USB_WEBCAM_WIDTH * USB_WEBCAM_HEIGHT)

void UsbWebcam_Init(void);
void UsbWebcam_IRQHandler(void);
bool UsbWebcam_SubmitRgb565(const uint16_t *source,
                            uint32_t source_width,
                            uint32_t source_height);
void UsbWebcam_ConvertRgb565ToYcbcr422Mcu(const uint16_t *source,
                                          uint32_t source_width,
                                          uint32_t source_height,
                                          uint8_t *destination,
                                          uint32_t destination_width,
                                          uint32_t destination_height);
uint32_t UsbWebcam_SubmittedFrames(void);
uint32_t UsbWebcam_DroppedFrames(void);
uint32_t UsbWebcam_LastJpegBytes(void);
uint32_t UsbWebcam_MaximumConvertUs(void);
uint32_t UsbWebcam_MaximumEncodeUs(void);
bool UsbWebcam_IsStreaming(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_WEBCAM_H */
