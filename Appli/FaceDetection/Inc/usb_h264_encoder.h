#ifndef USB_H264_ENCODER_H
#define USB_H264_ENCODER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void UsbH264Encoder_Init(uint32_t width, uint32_t height, uint32_t fps,
                         uint32_t bitrate);
int32_t UsbH264Encoder_Encode(const uint16_t *input, uint8_t *output,
                              size_t output_capacity, bool force_idr,
                              bool prepend_headers);
bool UsbH264Encoder_LastWasIdr(void);
void UsbH264Encoder_IRQHandler(void);

#endif /* USB_H264_ENCODER_H */
