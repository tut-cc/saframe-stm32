#ifndef USB_WEBCAM_H
#define USB_WEBCAM_H
#include <stdbool.h>
#include <stdint.h>
#define USB_WEBCAM_WIDTH (320U)
#define USB_WEBCAM_HEIGHT (240U)
#define USB_WEBCAM_FPS (30U)
#define USB_WEBCAM_BITRATE (1000000U)
#define USB_WEBCAM_H264_MAX_SIZE (128U * 1024U)
void UsbWebcam_Init(void);
void UsbWebcam_IRQHandler(void);
void UsbWebcam_Venc_IRQHandler(void);
bool UsbWebcam_SubmitRgb565(const uint16_t *source, uint32_t source_width,
                            uint32_t source_height);
void UsbWebcam_CopyRgb565CenterDownsample(const uint16_t *source,
    uint32_t source_width, uint32_t source_height, uint16_t *destination,
    uint32_t destination_width, uint32_t destination_height);
uint32_t UsbWebcam_EncodedFrames(void);
uint32_t UsbWebcam_RepeatedFrames(void);
uint32_t UsbWebcam_EncodeDroppedFrames(void);
uint32_t UsbWebcam_LastBytes(void);
uint32_t UsbWebcam_MaximumEncodeUs(void);
uint32_t UsbWebcam_IdrCount(void);
bool UsbWebcam_IsStreaming(void);
#endif
