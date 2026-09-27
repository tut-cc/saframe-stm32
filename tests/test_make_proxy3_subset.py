import importlib.util
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).parents[1] / "tools" / "make_proxy3_subset.py"
SPEC = importlib.util.spec_from_file_location("make_proxy3_subset", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class MakeProxy3SubsetTest(unittest.TestCase):
    def test_subset_is_deterministic_and_contains_all_classes(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "source"
            output = root / "output"
            source.mkdir()
            for index, class_index in enumerate((0, 0, 1, 2, 0)):
                (source / f"{index}.jpg").write_bytes(b"image")
                (source / f"{index}.txt").write_text(
                    f"{class_index} 0.5 0.5 0.2 0.2\n", encoding="ascii"
                )
            count = MODULE.make_subset(source, output, 3)
            self.assertEqual(count, 4)
            self.assertEqual(sorted(path.name for path in output.glob("*.txt")),
                             ["0.txt", "1.txt", "2.txt", "3.txt"])
            self.assertTrue(all(path.is_symlink() for path in output.iterdir()))


if __name__ == "__main__":
    unittest.main()
