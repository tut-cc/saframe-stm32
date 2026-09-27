#!/usr/bin/env python3
"""Create a deterministic three-class COCO annotation file for SAFRAME."""

import argparse
import json
import os
from collections import Counter
from pathlib import Path


TARGETS = (
    (1, 1, "person"),
    (84, 2, "book"),
    (13, 3, "stop sign"),
)


def filter_coco(source: dict) -> tuple[dict, Counter]:
    source_to_target = {source_id: target_id for source_id, target_id, _ in TARGETS}
    target_categories = [
        {"id": target_id, "name": name, "supercategory": "proxy_privacy"}
        for _, target_id, name in TARGETS
    ]

    annotations = []
    image_ids = set()
    counts = Counter()
    for annotation in source.get("annotations", []):
        source_category = annotation.get("category_id")
        if source_category not in source_to_target:
            continue
        converted = dict(annotation)
        converted["category_id"] = source_to_target[source_category]
        annotations.append(converted)
        image_ids.add(converted["image_id"])
        counts[converted["category_id"]] += 1

    images = [
        image for image in source.get("images", [])
        if image.get("id") in image_ids
    ]
    result = {
        "info": dict(source.get("info", {})),
        "licenses": list(source.get("licenses", [])),
        "images": images,
        "annotations": annotations,
        "categories": target_categories,
    }
    return result, counts


def write_tfs_dataset(filtered: dict, source_images: Path, output: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    images = {image["id"]: image for image in filtered["images"]}
    by_image: dict[int, list[dict]] = {image_id: [] for image_id in images}
    for annotation in filtered["annotations"]:
        by_image[annotation["image_id"]].append(annotation)

    for image_id, image in images.items():
        filename = image["file_name"]
        source = (source_images / filename).resolve()
        if not source.is_file():
            raise FileNotFoundError(source)
        destination = output / filename
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.exists():
            destination.symlink_to(os.path.relpath(source, destination.parent))

        width = float(image["width"])
        height = float(image["height"])
        labels = []
        for annotation in by_image[image_id]:
            x, y, box_width, box_height = (float(value) for value in annotation["bbox"])
            if (box_width <= 0.0) or (box_height <= 0.0):
                continue
            class_index = int(annotation["category_id"]) - 1
            center_x = (x + (box_width / 2.0)) / width
            center_y = (y + (box_height / 2.0)) / height
            labels.append(
                f"{class_index} {center_x:.8f} {center_y:.8f} "
                f"{box_width / width:.8f} {box_height / height:.8f}"
            )
        (output / Path(filename).with_suffix(".txt")).write_text(
            "\n".join(labels) + "\n", encoding="ascii"
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path, help="COCO instances_*.json")
    parser.add_argument("output", type=Path, help="filtered COCO JSON")
    parser.add_argument("--images", type=Path, help="source COCO image directory")
    parser.add_argument("--tfs-output", type=Path,
                        help="write Model Zoo TFS image/YOLO-label directory")
    args = parser.parse_args()

    with args.source.open("r", encoding="utf-8") as stream:
        source = json.load(stream)
    result, counts = filter_coco(source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as stream:
        json.dump(result, stream, ensure_ascii=False, separators=(",", ":"))
        stream.write("\n")
    if (args.tfs_output is not None):
        if args.images is None:
            parser.error("--images is required with --tfs-output")
        write_tfs_dataset(result, args.images, args.tfs_output)

    print(f"images={len(result['images'])} annotations={len(result['annotations'])}")
    for category in result["categories"]:
        print(f"class={category['id'] - 1} name={category['name']} annotations={counts[category['id']]}")
    if args.tfs_output is not None:
        print(f"tfs_output={args.tfs_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
