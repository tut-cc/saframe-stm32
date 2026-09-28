import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from verify_model_artifacts import verify, verify_runtime  # noqa: E402


NETWORK_HEADER = """
#define STAI_NETWORK_IN_1_WIDTH (320)
#define STAI_NETWORK_IN_1_HEIGHT (320)
#define STAI_NETWORK_IN_1_CHANNEL (3)
#define STAI_NETWORK_IN_1_FORMAT (STAI_FORMAT_U8)
#define STAI_NETWORK_IN_1_FLAGS (STAI_FLAG_PREALLOCATED|STAI_FLAG_CHANNEL_LAST)
#define STAI_NETWORK_OUT_NUM (3)
#define STAI_NETWORK_OUT_1_FORMAT (STAI_FORMAT_S8)
#define STAI_NETWORK_OUT_1_WIDTH (20)
#define STAI_NETWORK_OUT_1_HEIGHT (20)
#define STAI_NETWORK_OUT_1_CHANNEL (6)
#define STAI_NETWORK_OUT_2_FORMAT (STAI_FORMAT_S8)
#define STAI_NETWORK_OUT_2_WIDTH (40)
#define STAI_NETWORK_OUT_2_HEIGHT (40)
#define STAI_NETWORK_OUT_2_CHANNEL (6)
#define STAI_NETWORK_OUT_3_FORMAT (STAI_FORMAT_S8)
#define STAI_NETWORK_OUT_3_WIDTH (10)
#define STAI_NETWORK_OUT_3_HEIGHT (10)
#define STAI_NETWORK_OUT_3_CHANNEL (6)
"""

APP_CONFIG = """
#define POSTPROCESS_TYPE POSTPROCESS_OD_ST_YOLOX_UI
#define NB_CLASSES (1)
#define CLASSES_TABLE const char* classes_table[NB_CLASSES] = {\\
  "person"}\\

#define AI_OD_ST_YOLOX_PP_NB_CLASSES (1)
#define AI_OD_ST_YOLOX_PP_L_GRID_WIDTH (40)
#define AI_OD_ST_YOLOX_PP_L_GRID_HEIGHT (40)
#define AI_OD_ST_YOLOX_PP_M_GRID_WIDTH (20)
#define AI_OD_ST_YOLOX_PP_M_GRID_HEIGHT (20)
#define AI_OD_ST_YOLOX_PP_S_GRID_WIDTH (10)
#define AI_OD_ST_YOLOX_PP_S_GRID_HEIGHT (10)
#define AI_OD_ST_YOLOX_PP_NB_ANCHORS (1)
#define AI_OD_ST_YOLOX_PP_IOU_THRESHOLD (0.5)
#define AI_OD_ST_YOLOX_PP_CONF_THRESHOLD (0.6)
#define AI_OD_ST_YOLOX_PP_MAX_BOXES_LIMIT (10)
"""


class VerifyModelArtifactsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.model_dir = self.root / "Model"
        self.model_dir.mkdir()
        self.network = self.model_dir / "stai_network.h"
        self.config = self.root / "app_config.h"
        self.contract = ROOT / "modelzoo" / "st_yoloxn_person_320.json"
        self.network.write_text(NETWORK_HEADER, encoding="utf-8")
        self.config.write_text(APP_CONFIG, encoding="utf-8")

    def tearDown(self):
        self.temp.cleanup()

    def test_accepts_person_320_contract(self):
        self.assertEqual(
            verify(self.model_dir, self.config, self.contract),
            "st_yoloxn_d033_w025_320_int8_coco_person",
        )

    def test_rejects_wrong_input_size(self):
        self.network.write_text(
            NETWORK_HEADER.replace("IN_1_WIDTH (320)", "IN_1_WIDTH (480)"),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(ValueError, "STAI_NETWORK_IN_1_WIDTH"):
            verify(self.model_dir, self.config, self.contract)

    def test_rejects_wrong_class(self):
        self.config.write_text(APP_CONFIG.replace('"person"', '"book"'),
                               encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "CLASSES_TABLE"):
            verify(self.model_dir, self.config, self.contract)

    def test_rejects_float_output(self):
        self.network.write_text(
            NETWORK_HEADER.replace("STAI_FORMAT_S8", "STAI_FORMAT_F32", 1),
            encoding="utf-8",
        )
        with self.assertRaisesRegex(ValueError, "STAI_NETWORK_OUT_1_FORMAT"):
            verify(self.model_dir, self.config, self.contract)

    def test_runtime_versions_must_match_generated_network(self):
        (self.model_dir / "network.c").write_text(
            "#if LL_ATON_VERSION_MAJOR != 1 || LL_ATON_VERSION_MINOR != 1 || "
            "LL_ATON_VERSION_MICRO != 3 || LL_ATON_VERSION_DEV != 275\n#endif\n",
            encoding="utf-8",
        )
        edgeai = self.root / "EdgeAI"
        (edgeai / "Inc").mkdir(parents=True)
        (edgeai / "ll_aton").mkdir()
        (edgeai / "ll_aton" / "ll_aton_version.h").write_text(
            "#define LL_ATON_VERSION_MAJOR (1)\n"
            "#define LL_ATON_VERSION_MINOR (1)\n"
            "#define LL_ATON_VERSION_MICRO (3)\n"
            "#define LL_ATON_VERSION_DEV (275)\n",
            encoding="utf-8",
        )
        (edgeai / "Inc" / "stai.h").write_text(
            "#define STAI_TOOLS_VERSION_MAJOR (4)\n"
            "#define STAI_TOOLS_VERSION_MINOR (0)\n"
            "#define STAI_TOOLS_VERSION_MICRO (1)\n",
            encoding="utf-8",
        )
        (edgeai / "Inc" / "core_datatypes.h").write_text(
            "#define AI_PLATFORM_RUNTIME_MAJOR (12)\n"
            "#define AI_PLATFORM_RUNTIME_MINOR (0)\n"
            "#define AI_PLATFORM_RUNTIME_MICRO (1)\n",
            encoding="utf-8",
        )
        libraries = self.root / "Lib"
        libraries.mkdir()
        (libraries / "NetworkRuntime1201_CM55_GCC.a").touch()
        verify_runtime(self.model_dir, edgeai, libraries)

        (edgeai / "ll_aton" / "ll_aton_version.h").write_text(
            "#define LL_ATON_VERSION_MAJOR (1)\n"
            "#define LL_ATON_VERSION_MINOR (1)\n"
            "#define LL_ATON_VERSION_MICRO (3)\n"
            "#define LL_ATON_VERSION_DEV (262)\n",
            encoding="utf-8",
        )
        with self.assertRaisesRegex(ValueError, "LL_ATON runtime version"):
            verify_runtime(self.model_dir, edgeai, libraries)


if __name__ == "__main__":
    unittest.main()
