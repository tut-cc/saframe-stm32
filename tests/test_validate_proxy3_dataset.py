import importlib.util
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).parents[1] / "tools" / "validate_proxy3_dataset.py"
SPEC = importlib.util.spec_from_file_location("validate_proxy3_dataset", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class ValidateProxy3DatasetTest(unittest.TestCase):
    def make_dataset(self, root: Path) -> None:
        for index, class_index in enumerate((0, 1, 2)):
            (root / f"sample{index}.jpg").write_bytes(b"image")
            (root / f"sample{index}.txt").write_text(
                f"{class_index} 0.5 0.5 0.25 0.25\n", encoding="ascii"
            )

    def test_accepts_complete_contract(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.make_dataset(root)
            summary = MODULE.validate_dataset(root)
            self.assertEqual(summary["class_names"], ["person", "book", "stop sign"])
            self.assertEqual(summary["images"], 3)
            self.assertEqual(summary["annotations"], 3)

    def test_rejects_coordinate_outside_unit_interval(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self.make_dataset(root)
            (root / "sample0.txt").write_text("0 1.1 0.5 0.2 0.2\n", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "outside"):
                MODULE.validate_dataset(root)


if __name__ == "__main__":
    unittest.main()
