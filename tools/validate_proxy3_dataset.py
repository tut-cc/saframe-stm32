#!/usr/bin/env python3
"""Validate a SAFRAME proxy-3 TFS directory and emit a JSON summary."""

import argparse
import json
from collections import Counter
from pathlib import Path


CLASS_NAMES = ("person", "book", "stop sign")
IMAGE_SUFFIXES = {".jpg", ".jpeg", ".png"}


def validate_dataset(root: Path) -> dict:
    if not root.is_dir():
        raise ValueError(f"dataset directory not found: {root}")

    image_count = 0
    label_count = 0
    class_counts = Counter()
    for image in sorted(root.iterdir()):
        if image.suffix.lower() not in IMAGE_SUFFIXES:
            continue
        image_count += 1
        if not image.exists():
            raise ValueError(f"broken image link: {image}")
        label = image.with_suffix(".txt")
        if not label.is_file():
            raise ValueError(f"missing label file: {label}")
        rows = [row for row in label.read_text(encoding="ascii").splitlines() if row]
        if not rows:
            raise ValueError(f"empty label file: {label}")
        for line_number, row in enumerate(rows, start=1):
            fields = row.split()
            if len(fields) != 5:
                raise ValueError(f"{label}:{line_number}: expected 5 fields")
            class_index = int(fields[0])
            coordinates = [float(value) for value in fields[1:]]
            if class_index not in range(len(CLASS_NAMES)):
                raise ValueError(f"{label}:{line_number}: invalid class {class_index}")
            if not all(0.0 <= value <= 1.0 for value in coordinates):
                raise ValueError(f"{label}:{line_number}: coordinate outside [0,1]")
            if (coordinates[2] <= 0.0) or (coordinates[3] <= 0.0):
                raise ValueError(f"{label}:{line_number}: non-positive box size")
            class_counts[class_index] += 1
            label_count += 1

    if image_count == 0:
        raise ValueError(f"no images found: {root}")
    missing_classes = [CLASS_NAMES[index] for index in range(3) if not class_counts[index]]
    if missing_classes:
        raise ValueError(f"classes without annotations: {', '.join(missing_classes)}")
    return {
        "root": str(root.resolve()),
        "class_names": list(CLASS_NAMES),
        "images": image_count,
        "annotations": label_count,
        "annotations_by_class": {
            CLASS_NAMES[index]: class_counts[index] for index in range(3)
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("dataset", type=Path)
    parser.add_argument("--summary", type=Path)
    args = parser.parse_args()
    summary = validate_dataset(args.dataset)
    rendered = json.dumps(summary, ensure_ascii=False, indent=2) + "\n"
    if args.summary:
        args.summary.parent.mkdir(parents=True, exist_ok=True)
        args.summary.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        raise SystemExit(f"proxy dataset: FAIL: {error}")
