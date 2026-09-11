#ifndef FACE_DETECTION_APP_H
#define FACE_DETECTION_APP_H

#ifdef __cplusplus
extern "C" {
#endif

void FaceDetection_PreHALInit(void);
void FaceDetection_HardwareInit(void);
void FaceDetection_OSStarted(void);
void FaceDetection_Run(void);
void FaceDetection_CameraFrameCallback(void);

#ifdef __cplusplus
}
#endif

#endif /* FACE_DETECTION_APP_H */
