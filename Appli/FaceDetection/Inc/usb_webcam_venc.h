#ifndef USB_WEBCAM_VENC_H
#define USB_WEBCAM_VENC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hardware JPEG encoding of RGB565 frames with the VENC peripheral. */
void UsbWebcamVenc_Init(uint32_t width, uint32_t height, uint32_t quality_level);
/* Encodes one frame whose data cache has been cleaned. Blocks the caller until
 * the VENC completes and returns the JPEG length, or 0 on failure. */
uint32_t UsbWebcamVenc_Encode(const uint16_t *frame, uint8_t *destination,
                              uint32_t destination_size);
void UsbWebcamVenc_IRQHandler(void);
uint32_t UsbWebcamVenc_PoolUsedBytes(void);
int32_t UsbWebcamVenc_LastError(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_WEBCAM_VENC_H */
