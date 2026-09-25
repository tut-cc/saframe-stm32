#!/usr/bin/env python3
"""Reject generated YuNet artifacts that cannot consume DCMIPP RGB888 data."""

from pathlib import Path
import re
import sys


MODEL_DIR = Path(__file__).resolve().parent
HEADER = MODEL_DIR / "stai_network.h"
NETWORK = MODEL_DIR / "network.c"


def require(text: str, pattern: str, description: str) -> None:
    if re.search(pattern, text, re.MULTILINE) is None:
        raise ValueError(f"missing or invalid {description}")


def main() -> int:
    header = HEADER.read_text(encoding="utf-8")
    network = NETWORK.read_text(encoding="utf-8")

    require(header, r"^#define STAI_NETWORK_IN_NUM \(1\)$", "input count")
    require(header, r"^#define STAI_NETWORK_IN_1_FORMAT \(STAI_FORMAT_U8\)$", "UINT8 input")
    require(header, r"^#define STAI_NETWORK_IN_1_SIZE \(307200\)$", "320x320x3 input size")
    require(header, r"^#define STAI_NETWORK_IN_1_CHANNEL \(3\)$", "input channels")
    require(header, r"^#define STAI_NETWORK_IN_1_HEIGHT \(320\)$", "input height")
    require(header, r"^#define STAI_NETWORK_IN_1_WIDTH \(320\)$", "input width")
    require(
        header,
        r"^#define STAI_NETWORK_IN_1_FLAGS .*STAI_FLAG_CHANNEL_LAST.*$",
        "channel-last input flag",
    )
    require(
        header,
        r"^#define STAI_NETWORK_IN_1_SHAPE\s+\\\n\s*\{\s+\\\n"
        r"\s*1, 320, 320, 3\s+\\\n\s*\}$",
        "NHWC input shape",
    )
    require(header, r"^#define STAI_NETWORK_OUT_NUM \(12\)$", "YuNet output count")

    output_formats = header.partition("#define STAI_NETWORK_OUT_FORMATS")[2].partition(
        "#define STAI_NETWORK_OUT_SIZES"
    )[0]
    if output_formats.count("STAI_FORMAT_S8") != 12:
        raise ValueError("the 12 YuNet outputs are not all INT8")

    input_buffer = re.search(
        r'\.name = "Input_0_out_0",(?P<body>.*?)\n\s*\},', network, re.DOTALL
    )
    if input_buffer is None:
        raise ValueError("network input buffer metadata is missing")
    # STEdgeAI 4.0.1 exposes the user-facing buffer as CHANNEL_LAST/NHWC in
    # stai_network.h. Its compiler-private logical descriptor remains
    # CHPos_First/{1,320,3,320}, while mem_shape records the physical NHWC
    # storage. Do not patch generated LL_ATON data based on the logical shape.
    require(
        network,
        r"buff_info__mem_shape_F_1_320_320_3\[\]\s*=\s*"
        r"\{\s*1,\s*320,\s*320,\s*3\s*\}",
        "physical NHWC input memory shape",
    )
    if ".mem_shape = buff_info__mem_shape_F_1_320_320_3" not in input_buffer.group("body"):
        raise ValueError("network input buffer does not use the physical NHWC memory shape")
    if ".scale = buff_info_Input_0_out_0_quant_scale" not in input_buffer.group("body"):
        raise ValueError("network input quantization metadata is missing")

    for output_index in range(1, 13):
        require(
            header,
            rf"^#define STAI_NETWORK_OUT_{output_index}_SCALE_OFFSET_NUM \(1\)$",
            f"output {output_index} quantization metadata",
        )

    print("YuNet model layout: PASS (320x320x3 UINT8 channel-last, 12 INT8 outputs)")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        print(f"YuNet model layout: FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
