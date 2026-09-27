import ast
import json
import unittest
from pathlib import Path

import yaml


NOTEBOOK = (
    Path(__file__).parents[1] / "notebooks" / "SAFRAME_COCO_Proxy3_Colab.ipynb"
)


class ColabProxy3NotebookTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.notebook = json.loads(NOTEBOOK.read_text(encoding="utf-8"))
        cls.code = [
            "".join(cell["source"])
            for cell in cls.notebook["cells"]
            if cell["cell_type"] == "code"
        ]
        cls.source = "\n".join(cls.code)

    def test_all_code_cells_parse(self):
        for index, source in enumerate(self.code):
            with self.subTest(cell=index):
                ast.parse(source)

    def test_embedded_training_config_parses(self):
        config = None
        for source in self.code:
            tree = ast.parse(source)
            for node in tree.body:
                if (isinstance(node, ast.Assign)
                        and any(isinstance(target, ast.Name) and target.id == "config"
                                for target in node.targets)):
                    config = ast.literal_eval(node.value)
        self.assertIsNotNone(config)
        parsed = yaml.safe_load(config)
        self.assertEqual(parsed["dataset"]["format"], "darknet_yolo")
        self.assertEqual(parsed["dataset"]["class_names"],
                         ["person", "book", "stop sign"])
        self.assertEqual(parsed["model"]["input_shape"], "(320,320,3)")

    def test_contract_and_persistence_are_pinned(self):
        self.assertIn("0f6210ed5156126b782e1c43249063a477484b20", self.source)
        self.assertIn("CLASS_NAMES = ('person', 'book', 'stop sign')", self.source)
        self.assertIn("if free_gib < 45", self.source)
        self.assertIn("saframe-modelzoo-py311", self.source)
        self.assertIn("uv, 'python', 'install', '3.11'", self.source)
        self.assertIn("MZ_PYTHON, '-c'", self.source)
        self.assertIn("MPLBACKEND='Agg'", self.source)
        self.assertIn("env=headless_env", self.source)
        self.assertIn("drive.mount('/content/drive')", self.source)
        self.assertIn("operation_mode: chain_tqe", self.source)
        self.assertIn("dataset_name: darknet_yolo", self.source)
        self.assertIn("format: darknet_yolo", self.source)
        self.assertIn("quantization_output_type: int8", self.source)

    def test_setup_cell_imports_os_before_reading_environment(self):
        setup = next(source for source in self.code if "headless_env" in source)
        self.assertLess(setup.index("import os"), setup.index("os.environ"))

    def test_smoke_full_and_resume_cells_exist(self):
        self.assertIn("run_training('smoke', epochs=1", self.source)
        self.assertIn("run_training('full', epochs=500)\n", self.source)
        self.assertIn("run_training('full', epochs=500, resume=True)", self.source)
        self.assertIn("training.resume_training=true", self.source)
        self.assertIn("last_model.keras", self.source)
        self.assertIn("command = [MZ_PYTHON", self.source)


if __name__ == "__main__":
    unittest.main()
