#!/usr/bin/env python3
"""Compatibility wrapper for the proxy 3-class artifact contract."""

import argparse
import sys
from pathlib import Path

from verify_model_artifacts import verify


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("model_dir", type=Path)
    parser.add_argument("app_config", type=Path)
    args = parser.parse_args()

    project_root = Path(__file__).resolve().parent.parent
    contract = project_root / "modelzoo" / "st_yoloxn_proxy3_320.json"
    contract_id = verify(args.model_dir, args.app_config, contract)
    print(f"proxy model artifacts: PASS ({contract_id})")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, TypeError, ValueError) as error:
        print(f"proxy model artifacts: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
