#!/usr/bin/env python3
"""Create a deterministic symlink subset containing every proxy class."""

import argparse
from pathlib import Path


IMAGE_SUFFIXES = (".jpg", ".jpeg", ".png")


def make_subset(source: Path, output: Path, limit: int) -> int:
    if limit < 3:
        raise ValueError("limit must be at least 3")
    output.mkdir(parents=True, exist_ok=True)
    selected: list[tuple[Path, Path]] = []
    seen_classes = set()
    for label in sorted(source.glob("*.txt")):
        rows = [row for row in label.read_text(encoding="ascii").splitlines() if row]
        classes = {int(row.split()[0]) for row in rows}
        images = [label.with_suffix(suffix) for suffix in IMAGE_SUFFIXES]
        image = next((candidate for candidate in images if candidate.exists()), None)
        if image is None:
            raise ValueError(f"image not found for {label}")
        if len(selected) < limit or seen_classes != {0, 1, 2}:
            selected.append((image, label))
            seen_classes.update(classes)
        if len(selected) >= limit and seen_classes == {0, 1, 2}:
            break

    if seen_classes != {0, 1, 2}:
        raise ValueError(f"source does not contain all classes: {sorted(seen_classes)}")
    for image, label in selected:
        for source_path in (image, label):
            destination = output / source_path.name
            if destination.exists():
                if destination.resolve() != source_path.resolve():
                    raise ValueError(f"conflicting subset entry: {destination}")
                continue
            destination.symlink_to(source_path.resolve())
    return len(selected)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--limit", type=int, required=True)
    args = parser.parse_args()
    count = make_subset(args.source, args.output, args.limit)
    print(f"subset={args.output} images={count}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        raise SystemExit(f"proxy subset: FAIL: {error}")
