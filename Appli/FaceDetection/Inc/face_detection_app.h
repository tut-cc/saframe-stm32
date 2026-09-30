#ifndef FACE_DETECTION_APP_H
#define FACE_DETECTION_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void FaceDetection_PreHALInit(void);
void FaceDetection_HardwareInit(void);
void FaceDetection_OSStarted(void);
void FaceDetection_Run(void);
#define DISPLAY_FRAME_READY  (1U << 0)
#define NN_FRAME_READY       (1U << 1)
#define CAPTURE_FRAME_ENDED  (1U << 2)

void FaceDetection_CameraFrameCallback(uint32_t pipe);
void FaceDetection_CameraVsyncCallback(uint32_t pipe);

#ifdef __cplusplus
}
#endif

#endif /* FACE_DETECTION_APP_H */
