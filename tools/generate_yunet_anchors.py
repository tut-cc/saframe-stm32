#!/usr/bin/env python3
"""Generate the YuNet grid-anchor C headers without the modelzoo Python pipeline.

fd_yunet_anchors_{32,16,8}.h are normally emitted by stm32ai-modelzoo-services'
face_detection deployment pipeline (gen_h_user_file_n6_yunet, which calls
generate_yunet_anchor in face_detection/tf/src/postprocessing/postprocess.py).
That function is a pure, deterministic grid computation with no dependency on
the model weights or training data:

    def generate_yunet_anchor(input_size, strides):
        centers = []
        for stride in strides:
            anchor_centers = np.stack(
                np.mgrid[:(input_size[1] // stride), :(input_size[0] // stride)][::-1],
                axis=-1)
            anchor_centers = (anchor_centers * stride).astype(np.float32).reshape(-1, 2)
            centers.append(anchor_centers)
        return centers

For a square image this reduces to: for each stride, a (size/stride)^2 grid of
(x, y) = (col * stride, row * stride) points in row-major order. This script
reimplements exactly that (standard library only, no numpy/onnxruntime/hydra),
using the image size and strides already confirmed against this repo's actual
generated Model/stai_network.h (see app_config.h).
"""
import pathlib

IMG_SIZE = 320
STRIDES = (32, 16, 8)  # matches AI_FD_YUNET_PP_OUT_{32,16,8}_NB_BOXES ordering
NB_BOXES = {32: 100, 16: 400, 8: 1600}  # confirmed via Model/stai_network.h

OUT_DIR = pathlib.Path(__file__).resolve().parent.parent / \
    "Appli/FaceDetection/Vendor/Postprocess/Inc"

HEADER_TEMPLATE = """#ifndef __ANCHORS_{stride}_H__
#define __ANCHORS_{stride}_H__

const int16_t g_Anchors_{stride}[{length}] = {{ {values} }};
#endif /* __ANCHORS_{stride}_H__ */
"""


def generate_anchor(size: int, stride: int) -> list[tuple[int, int]]:
    n = size // stride
    return [(col * stride, row * stride) for row in range(n) for col in range(n)]


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for stride in STRIDES:
        anchors = generate_anchor(IMG_SIZE, stride)
        assert len(anchors) == NB_BOXES[stride], \
            f"stride {stride}: expected {NB_BOXES[stride]} anchors, got {len(anchors)}"
        flat = [v for xy in anchors for v in xy]
        values = ", ".join(str(v) for v in flat)
        out_path = OUT_DIR / f"fd_yunet_anchors_{stride}.h"
        out_path.write_text(
            HEADER_TEMPLATE.format(stride=stride, length=len(flat), values=values),
            encoding="utf-8", newline="\n")
        print(f"wrote {out_path} ({len(flat)} int16_t values)")


if __name__ == "__main__":
    main()
