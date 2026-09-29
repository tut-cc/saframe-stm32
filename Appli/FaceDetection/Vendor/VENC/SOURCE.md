# VENC source provenance

- Repository: https://github.com/STMicroelectronics/x-cube-n6-ai-h264-usb-uvc
- Release: `v2.2.1`
- Commit: `530dcbb7b8778617cbd0c2080f6a320d2568d432`
- Imported: 2026-09-29
- Upstream license manifest: `LICENSE.md` at the repository root

## Code incorporated into this repository

The following upstream code was copied into the repository and is compiled into
the firmware:

- `STM32Cube_FW_N6/Middlewares/Third_Party/VideoEncoder/{inc,source/common,source/h264}`
  -> `VideoEncoder/` (only the files required by the H.264 build)
- `STM32Cube_FW_N6/Middlewares/ST/VideoEncoder_EWL/{ewl_impl.c,ewl_impl.h}`
  -> `VideoEncoder_EWL/`
- `STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Inc/stm32n6xx_ll_venc.h`
  -> `../HAL/Inc/stm32n6xx_ll_venc.h`
- `STM32Cube_FW_N6/Drivers/STM32N6xx_HAL_Driver/Src/stm32n6xx_ll_venc.c`
  -> `../HAL/Src/stm32n6xx_ll_venc.c`

JPEG encoder sources, FreeRTOS, USBX, camera code, generated AI code, and the
reference application's source files were not copied from this repository.

## Local modifications to incorporated code

Copyright and license headers from upstream are retained. The following copied
files contain local compatibility changes:

- `VideoEncoder/inc/basetype.h`: coexist with C `stdbool.h` definitions.
- `VideoEncoder/source/common/encasiccontroller_v2.c` and
  `VideoEncoder/source/common/encpreprocess.c`: avoid boolean-context product
  warnings without changing the condition.
- `VideoEncoder/source/h264/H264Cabac.c`: make the upstream table-pointer cast
  explicit for GCC 14.
- `VideoEncoder/source/h264/H264Init.c` and
  `VideoEncoder/source/h264/H264RateControl.c`: GCC 14 warning fixes.
- `VideoEncoder_EWL/ewl_impl.c`: add the user synchronization callback used by
  μT-Kernel and warning-safe user-allocation branches.

The application-side integration in `../../Src/usb_h264_encoder.c` and
`../../Src/usb_webcam.c` is project code. It calls the incorporated APIs but is
not a copy of the upstream application files.

## Reference-only material

The following upstream files and documents were inspected to determine the
configuration and integration sequence. They were not copied into this
repository:

- `Src/app_enc.c`: H.264 initialization, VBR controls, GOP/IDR handling, and
  stream-start SPS/PPS behavior.
- `Src/app.c`: VENC input/output buffer placement and UVC frame submission.
- `Src/main.c`: MPU, RIF, clock, and VENC initialization order.
- `Lib/uvcl/README.md`: H.264 payload selection and `UVCL_ShowFrame()` buffer
  ownership contract.
- ST wiki, "Introduction to Hardware Video Encoding with STM32": frame mode,
  supported input formats, rate control, and memory placement guidance.
- STM32CubeN6 `VENC_SDCard_ThreadX` README: independent confirmation of the
  STM32N6 VENC initialization and 30 fps frame-mode arrangement.

The upstream root `LICENSE.md` identifies its `Src` and `Lib/uvcl` material as
SLA0044. Those items were used only as references and are not redistributed
from this upstream repository here.

## Copyright and licenses

- VideoEncoder: Copyright (c) 2015-2022 Verisilicon Inc. and Copyright (c)
  2011-2014 Google Inc.; BSD-3-Clause.
- VideoEncoder_EWL: Copyright (c) 2023 STMicroelectronics; BSD-3-Clause.
- STM32N6 LL VENC: Copyright (c) 2023 STMicroelectronics; BSD-3-Clause.

The retained component license files are `VideoEncoder/LICENSE.txt` and
`VideoEncoder_EWL/LICENSE.txt`. The LL driver uses the repository's
`Drivers/STM32N6xx_HAL_Driver/LICENSE.txt`. The required copyright notices,
conditions, and disclaimer for source and binary redistribution are reproduced
in the repository-root `THIRD_PARTY_NOTICES.md`.

The release and commit were checked against the upstream releases page on
2026-09-29. The integration was also compared with ST's `app_enc.c`, `app.c`,
UVCL documentation, and STM32N6 hardware video encoding documentation after
the port was completed.
