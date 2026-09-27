#!/usr/bin/env python3
"""Reject generated artifacts that do not implement the proxy 3-class contract."""

import argparse
import re
import sys
from pathlib import Path


def macro(text: str, name: str) -> int:
    match = re.search(rf"#define\s+{re.escape(name)}\s+\(?([0-9]+)\)?", text)
    if not match:
        raise ValueError(f"missing numeric macro {name}")
    return int(match.group(1))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("model_dir", type=Path)
    parser.add_argument("app_config", type=Path)
    args = parser.parse_args()

    network = (args.model_dir / "stai_network.h").read_text(encoding="utf-8")
    config = args.app_config.read_text(encoding="utf-8")
    expected_shape = {
        "STAI_NETWORK_IN_1_WIDTH": 320,
        "STAI_NETWORK_IN_1_HEIGHT": 320,
        "STAI_NETWORK_IN_1_CHANNEL": 3,
    }
    for name, expected in expected_shape.items():
        actual = macro(network, name)
        if actual != expected:
            raise ValueError(f"{name}={actual}, expected {expected}")

    if macro(config, "NB_CLASSES") != 3:
        raise ValueError("NB_CLASSES must be 3")
    if macro(config, "AI_OD_ST_YOLOX_PP_NB_CLASSES") != 3:
        raise ValueError("AI_OD_ST_YOLOX_PP_NB_CLASSES must be 3")
    names = [config.find(f'\"{name}\"') for name in ("person", "book", "stop sign")]
    if any(position < 0 for position in names) or names != sorted(names):
        raise ValueError("class table must contain person, book, stop sign in that order")

    print("proxy model artifacts: PASS (320x320 RGB, 3 classes)")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(f"proxy model artifacts: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
