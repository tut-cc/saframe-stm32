import importlib.util
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).parents[1] / "tools" / "extract_coco_proxy_classes.py"
SPEC = importlib.util.spec_from_file_location("extract_coco_proxy_classes", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class ExtractCocoProxyClassesTest(unittest.TestCase):
    def test_filters_images_and_remaps_categories_in_contract_order(self):
        source = {
            "images": [
                {"id": 10, "width": 10, "height": 10},
                {"id": 11, "width": 10, "height": 10},
                {"id": 12, "width": 10, "height": 10},
            ],
            "annotations": [
                {"id": 1, "image_id": 10, "category_id": 84, "bbox": [1, 1, 2, 2]},
                {"id": 2, "image_id": 10, "category_id": 1, "bbox": [1, 1, 2, 2]},
                {"id": 3, "image_id": 11, "category_id": 13, "bbox": [-2, 8, 5, 5]},
                {"id": 4, "image_id": 12, "category_id": 3, "bbox": [1, 1, 2, 2]},
                {"id": 5, "image_id": 11, "category_id": 13, "bbox": [20, 20, 1, 1]},
            ],
            "categories": [],
        }
        result, counts = MODULE.filter_coco(source)
        self.assertEqual([image["id"] for image in result["images"]], [10, 11])
        self.assertEqual(
            [annotation["category_id"] for annotation in result["annotations"]],
            [2, 1, 3],
        )
        self.assertEqual(
            [(category["id"], category["name"]) for category in result["categories"]],
            [(1, "person"), (2, "book"), (3, "stop sign")],
        )
        self.assertEqual([counts[index] for index in (1, 2, 3)], [1, 1, 1])
        clipped = next(item for item in result["annotations"] if item["id"] == 3)
        self.assertEqual(clipped["bbox"], [0.0, 8.0, 3.0, 2.0])
        self.assertEqual(clipped["area"], 6.0)

    def test_writes_zero_based_normalized_darknet_labels(self):
        filtered = {
            "images": [{"id": 5, "file_name": "sample.jpg", "width": 100, "height": 50}],
            "annotations": [
                {"image_id": 5, "category_id": 2, "bbox": [10, 5, 20, 10]},
            ],
        }
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            images = root / "images"
            output = root / "darknet"
            images.mkdir()
            (images / "sample.jpg").write_bytes(b"image")
            MODULE.write_darknet_dataset(filtered, images, output)
            self.assertTrue((output / "sample.jpg").is_symlink())
            self.assertEqual(
                (output / "sample.txt").read_text(encoding="ascii"),
                "1 0.20000000 0.20000000 0.20000000 0.20000000\n",
            )


if __name__ == "__main__":
    unittest.main()
