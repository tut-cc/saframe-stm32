#include <tk/tkernel.h>
#include <tm/tmonitor.h>

#include "face_detection_app.h"

LOCAL void face_detection_task(INT stacd, void *exinf);

LOCAL T_CTSK face_detection_task_config = {
  .itskpri = 5,
  .stksz = 32 * 1024,
  .task = face_detection_task,
  .tskatr = TA_HLNG | TA_RNG3,
};

LOCAL void face_detection_task(INT stacd, void *exinf)
{
  (void)stacd;
  (void)exinf;

  FaceDetection_OSStarted();
  tm_putstring((UB *)"Starting vision task.\n");
  FaceDetection_Run();

  tk_ext_tsk();
}

EXPORT INT usermain(void)
{
  ID task_id;

  tm_putstring((UB *)"Start User-main program.\n");
  task_id = tk_cre_tsk(&face_detection_task_config);
  if (task_id <= 0)
  {
    tm_putstring((UB *)"Failed to create face detection task.\n");
    return task_id;
  }

  ER ercd = tk_sta_tsk(task_id, 0);
  if (ercd != E_OK)
  {
    tm_putstring((UB *)"Failed to start face detection task.\n");
    return ercd;
  }

  tk_slp_tsk(TMO_FEVR);
  return 0;
}
