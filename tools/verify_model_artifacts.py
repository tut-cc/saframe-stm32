#!/usr/bin/env python3
"""Validate generated Model Zoo artifacts against an explicit model contract."""

import argparse
import json
import re
import sys
from pathlib import Path


def macro_text(text: str, name: str) -> str:
    match = re.search(rf"#define\s+{re.escape(name)}\s+(.+)", text)
    if not match:
        raise ValueError(f"missing macro {name}")
    return match.group(1).strip().strip("()")


def macro_int(text: str, name: str) -> int:
    value = macro_text(text, name)
    match = re.fullmatch(r"([0-9]+)[UuLl]*", value)
    if not match:
        raise ValueError(f"{name} is not a numeric macro: {value}")
    return int(match.group(1))


def macro_float(text: str, name: str) -> float:
    value = macro_text(text, name)
    try:
        return float(value.rstrip("fF"))
    except ValueError as error:
        raise ValueError(f"{name} is not a floating-point macro: {value}") from error


def class_names(config: str, expected_count: int) -> list[str]:
    start = config.find("#define CLASSES_TABLE")
    if start < 0:
        raise ValueError("missing CLASSES_TABLE")
    names = re.findall(r'"([^"]+)"', config[start:start + 512])
    if len(names) < expected_count:
        raise ValueError("CLASSES_TABLE has fewer entries than NB_CLASSES")
    return names[:expected_count]


def require_equal(actual: object, expected: object, label: str) -> None:
    if actual != expected:
        raise ValueError(f"{label}={actual!r}, expected {expected!r}")


def verify_runtime(model_dir: Path, edgeai_dir: Path, library_dir: Path) -> None:
    """Reject a generated network combined with a partially updated runtime."""
    generated = (model_dir / "network.c").read_text(encoding="utf-8")
    aton = (edgeai_dir / "ll_aton" / "ll_aton_version.h").read_text(
        encoding="utf-8"
    )
    stai = (edgeai_dir / "Inc" / "stai.h").read_text(encoding="utf-8")
    core = (edgeai_dir / "Inc" / "core_datatypes.h").read_text(
        encoding="utf-8"
    )

    guard = re.search(
        r"LL_ATON_VERSION_MAJOR\s*!=\s*(\d+).*?"
        r"LL_ATON_VERSION_MINOR\s*!=\s*(\d+).*?"
        r"LL_ATON_VERSION_MICRO\s*!=\s*(\d+).*?"
        r"LL_ATON_VERSION_DEV\s*!=\s*(\d+)",
        generated,
        re.DOTALL,
    )
    if not guard:
        raise ValueError("generated network has no LL_ATON version guard")
    generated_aton = tuple(int(value) for value in guard.groups())
    runtime_aton = tuple(
        macro_int(aton, f"LL_ATON_VERSION_{name}")
        for name in ("MAJOR", "MINOR", "MICRO", "DEV")
    )
    require_equal(runtime_aton, generated_aton, "LL_ATON runtime version")

    tools_version = tuple(
        macro_int(stai, f"STAI_TOOLS_VERSION_{name}")
        for name in ("MAJOR", "MINOR", "MICRO")
    )
    if tools_version != (4, 0, 1):
        raise ValueError(f"STAI tools version={tools_version!r}, expected (4, 0, 1)")

    library_version = tuple(
        macro_int(core, f"AI_PLATFORM_RUNTIME_{name}")
        for name in ("MAJOR", "MINOR", "MICRO")
    )
    library_name = "NetworkRuntime{}{}{}_CM55_GCC.a".format(*library_version)
    if not (library_dir / library_name).is_file():
        raise ValueError(f"missing matching runtime library {library_name}")


def verify(model_dir: Path, app_config: Path, contract_path: Path) -> str:
    contract = json.loads(contract_path.read_text(encoding="utf-8"))
    if contract.get("contract_version") != 1:
        raise ValueError("unsupported or missing contract_version")

    network = (model_dir / "stai_network.h").read_text(encoding="utf-8")
    config = app_config.read_text(encoding="utf-8")
    model_input = contract["input"]
    outputs = contract["outputs"]
    postprocess = contract["postprocess"]
    expected_classes = postprocess["classes"]

    for macro_name, key in (
        ("STAI_NETWORK_IN_1_WIDTH", "width"),
        ("STAI_NETWORK_IN_1_HEIGHT", "height"),
        ("STAI_NETWORK_IN_1_CHANNEL", "channels"),
    ):
        require_equal(macro_int(network, macro_name), model_input[key], macro_name)
    require_equal(macro_text(network, "STAI_NETWORK_IN_1_FORMAT"),
                  model_input["format"], "STAI_NETWORK_IN_1_FORMAT")
    input_flags = macro_text(network, "STAI_NETWORK_IN_1_FLAGS")
    if "STAI_FLAG_PREALLOCATED" not in input_flags:
        raise ValueError("generated model input must be preallocated")

    require_equal(macro_int(config, "NB_CLASSES"), len(expected_classes),
                  "NB_CLASSES")
    require_equal(macro_int(config, "AI_OD_ST_YOLOX_PP_NB_CLASSES"),
                  len(expected_classes), "AI_OD_ST_YOLOX_PP_NB_CLASSES")
    require_equal(class_names(config, len(expected_classes)), expected_classes,
                  "CLASSES_TABLE")
    require_equal(macro_text(config, "POSTPROCESS_TYPE"), postprocess["type"],
                  "POSTPROCESS_TYPE")
    require_equal(macro_int(config, "AI_OD_ST_YOLOX_PP_MAX_BOXES_LIMIT"),
                  postprocess["max_boxes"], "AI_OD_ST_YOLOX_PP_MAX_BOXES_LIMIT")
    if abs(macro_float(config, "AI_OD_ST_YOLOX_PP_CONF_THRESHOLD") -
           postprocess["confidence_threshold"]) > 1e-9:
        raise ValueError("confidence threshold does not match contract")
    if abs(macro_float(config, "AI_OD_ST_YOLOX_PP_IOU_THRESHOLD") -
           postprocess["iou_threshold"]) > 1e-9:
        raise ValueError("IoU threshold does not match contract")

    generated_grids = sorted(
        macro_int(config, f"AI_OD_ST_YOLOX_PP_{size}_GRID_WIDTH")
        for size in ("S", "M", "L")
    )
    expected_grids = sorted(outputs["grid_sizes"])
    require_equal(generated_grids, expected_grids, "postprocess grid widths")
    for size in ("S", "M", "L"):
        require_equal(
            macro_int(config, f"AI_OD_ST_YOLOX_PP_{size}_GRID_HEIGHT"),
            macro_int(config, f"AI_OD_ST_YOLOX_PP_{size}_GRID_WIDTH"),
            f"AI_OD_ST_YOLOX_PP_{size}_GRID_HEIGHT",
        )

    output_count = macro_int(network, "STAI_NETWORK_OUT_NUM")
    require_equal(output_count, outputs["count"], "STAI_NETWORK_OUT_NUM")
    anchors = macro_int(config, "AI_OD_ST_YOLOX_PP_NB_ANCHORS")
    expected_channels = anchors * (len(expected_classes) + 5)
    output_grids = []
    for index in range(1, output_count + 1):
        require_equal(macro_text(network, f"STAI_NETWORK_OUT_{index}_FORMAT"),
                      outputs["format"], f"STAI_NETWORK_OUT_{index}_FORMAT")
        width = macro_int(network, f"STAI_NETWORK_OUT_{index}_WIDTH")
        height = macro_int(network, f"STAI_NETWORK_OUT_{index}_HEIGHT")
        require_equal(height, width, f"STAI_NETWORK_OUT_{index}_HEIGHT")
        output_grids.append(width)
        require_equal(macro_int(network, f"STAI_NETWORK_OUT_{index}_CHANNEL"),
                      expected_channels, f"STAI_NETWORK_OUT_{index}_CHANNEL")
    require_equal(sorted(output_grids), expected_grids, "output tensor grids")

    return contract["id"]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("model_dir", type=Path)
    parser.add_argument("app_config", type=Path)
    parser.add_argument("contract", type=Path)
    parser.add_argument("--edgeai-dir", type=Path)
    parser.add_argument("--library-dir", type=Path)
    args = parser.parse_args()
    contract_id = verify(args.model_dir, args.app_config, args.contract)
    if (args.edgeai_dir is None) != (args.library_dir is None):
        raise ValueError("--edgeai-dir and --library-dir must be used together")
    if args.edgeai_dir is not None:
        verify_runtime(args.model_dir, args.edgeai_dir, args.library_dir)
    print(f"model artifacts: PASS ({contract_id})")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, json.JSONDecodeError, OSError, TypeError, ValueError) as error:
        print(f"model artifacts: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
