import unittest
from pathlib import Path


SCRIPT = Path(__file__).parents[1] / "tools" / "runpod_proxy3.sh"


class RunPodProxy3ScriptTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SCRIPT.read_text(encoding="utf-8")

    def test_pins_environment_and_persistent_root(self):
        self.assertIn("0f6210ed5156126b782e1c43249063a477484b20", self.source)
        self.assertIn("/workspace/saframe-proxy3", self.source)
        self.assertIn("uv_bin\" python install 3.11", self.source)
        self.assertIn("MPLBACKEND=Agg", self.source)
        self.assertIn("gpu_memory_mib < 15000", self.source)

    def test_validates_download_before_promotion(self):
        validation = self.source.index('unzip -Z -t "$partial"')
        promotion = self.source.index('mv "$partial" "$archive"')
        self.assertLess(validation, promotion)

    def test_all_runs_smoke_before_full_training(self):
        all_case = self.source.index("all)")
        smoke = self.source.index("run_smoke", all_case)
        full = self.source.index("run_full", all_case)
        self.assertLess(smoke, full)

    def test_resume_uses_last_model_and_remaining_epochs(self):
        self.assertIn("last_model.keras", self.source)
        self.assertIn("TARGET_EPOCHS - completed", self.source)
        self.assertIn("resume_epoch_offset", self.source)
        self.assertIn("training.resume_training=true", self.source)


if __name__ == "__main__":
    unittest.main()
