"""Regression checks for build-generated historical declaration contexts."""

import hashlib
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import build as build_rom
import build_wine
import standin
from context_compile import prepare_context, wrap_function
from rom import ROOT, load_config


class GeneratedDeclarationsTest(unittest.TestCase):
    def test_plain_unit_gets_declarations_only_in_compiler_input(self):
        source = "typedef unsigned char u8;\n\nu8 example(void)\n{\n    return 0;\n}\n"
        region = {
            "source": "example.c",
            "name": "example",
            "area": "EXAMPLE",
            "symbol_phase_standins": 63,
            "symbol_phase_top_standins": 10,
        }
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            path = root / region["source"]
            path.write_text(source)
            with patch.object(build_wine, "ROOT", root):
                files, unit, _ = build_wine.region_files(region, root / "example.asm")
            expected = standin.block(10, top=True) + source.replace(
                "u8 example", standin.block(63) + "u8 example")
            self.assertEqual(files[unit], expected)
            self.assertEqual(path.read_text(), source)

    def test_existing_source_declarations_are_rejected(self):
        source = standin.block(10) + "u8 example(void)\n{\n    return 0;\n}\n"
        region = {
            "source": "example.c",
            "name": "example",
            "area": "EXAMPLE",
            "symbol_phase_standins": 10,
        }
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / region["source"]).write_text(source)
            with patch.object(build_wine, "ROOT", root):
                with self.assertRaisesRegex(ValueError, "only allowed right before"):
                    build_wine.region_files(region, root / "example.asm")

    def test_pinned_contexts_keep_exact_composed_inputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            for region in load_config()["regions"]:
                if "context" not in region:
                    continue
                with self.subTest(function=region["name"]):
                    _, unit = prepare_context(
                        region, Path(temporary) / (region["name"] + ".asm"))
                    self.assertEqual(hashlib.sha256(unit.read_bytes()).hexdigest(),
                                     region["context"]["unit_sha256"])

    def test_context_region_files_uses_patched_root_and_rejects_standins(self):
        region = next(item for item in load_config()["regions"]
                      if item["name"] == "sd_command")
        original = (ROOT / region["source"]).read_text()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / region["source"]
            source.parent.mkdir(parents=True)
            source.write_text(original.replace("return response;", "return 0;", 1))
            with patch.object(build_wine, "ROOT", root):
                with self.assertRaisesRegex(ValueError, "Composed context checksum mismatch"):
                    build_wine.region_files(region, root / "sd_command.asm")

        non_wine_context = next(item for item in load_config()["regions"]
                                if item["name"] == "fat_find_path")
        plain = (ROOT / non_wine_context["source"]).read_text()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / non_wine_context["source"]
            source.parent.mkdir(parents=True)
            source.write_text(
                plain.replace("u8 fat_find_path", standin.block(5) + "u8 fat_find_path", 1))
            with self.assertRaisesRegex(ValueError, "has 5 symbol-phase stand-ins"):
                prepare_context(non_wine_context, root / "fat_find_path.asm", source_root=root)

    def test_non_wine_profiles_reject_standins_including_gbdk_and_macros(self):
        for compiler_cfg in ({}, {"compiler": "r8604"}, {"compiler": "3.3.0-linux-x86"}):
            for snippet in ("extern int symbolPhase000;\n", "#define SYMBOL_PHASE_10(p) p\n"):
                region = {"kind": "c", "source": "unit.c", **compiler_cfg}
                with tempfile.TemporaryDirectory() as temporary:
                    root = Path(temporary)
                    (root / "unit.c").write_text(snippet + "void unit(void) {}\n")
                    with patch.object(build_rom, "ROOT", root):
                        with self.assertRaisesRegex(ValueError, "require profile"):
                            build_rom.reject_non_wine_standins(region)

    def test_wrap_function_emits_abi_metadata_unique_module_and_referenced_strings(self):
        asm_text = (
            "\t.module disk\n"
            "\t.optsdcc -mgbz80\n"
            "\t.area _CODE\n"
            "_diskCmd::\n"
            "\tld\tbc,#___str_1\n"
            "\tcall\t_helper\n"
            "\tret\n"
            "___str_0:\n"
            "\t.ascii \"UNREFERENCED_TRAILING\"\n"
            "\t.db 0x00\n"
            "\t.area _CONST\n"
            "___str_1:\n"
            "\t.ascii \"OK\"\n"
            "\t.db 0x00\n"
            "___str_2:\n"
            "\t.ascii \"OTHER\"\n"
            "\t.db 0x00\n"
        )
        region = {"name": "sd_command", "area": "C_0975"}
        with tempfile.TemporaryDirectory() as temporary:
            asm_path = Path(temporary) / "sd_command.asm"
            asm_path.write_text(asm_text)
            wrapper = wrap_function(region, asm_path, "diskCmd", {"_helper": "0x091d"})
            wrapped = wrapper.read_text()

        self.assertTrue(wrapped.startswith(
            ".module context_sd_command\n\t.optsdcc -msm83 sdcccall(0)\n_helper = 0x091d\n.area _C_0975\n"
        ))
        self.assertIn("___str_1:\n\t.ascii \"OK\"\n\t.db 0x00\n", wrapped)
        self.assertNotIn("UNREFERENCED_TRAILING", wrapped)
        self.assertNotIn("OTHER", wrapped)

    def test_wrap_function_rejects_missing_optsdcc_or_unbound_local_static(self):
        region = {"name": "sd_command", "area": "C_0975"}
        with tempfile.TemporaryDirectory() as temporary:
            asm_path = Path(temporary) / "bad_abi.asm"
            asm_path.write_text("_diskCmd::\n\tret\n")
            with self.assertRaisesRegex(ValueError, "ABI metadata"):
                wrap_function(region, asm_path, "diskCmd", {})

            static_asm = (
                "\t.optsdcc -mgbz80\n"
                "\t.area _DATA\n"
                "_diskCmd_state_1_2:\n"
                "\t.ds 1\n"
                "\t.area _CODE\n"
                "_diskCmd::\n"
                "\tld\thl,#_diskCmd_state_1_2\n"
                "\tret\n"
            )
            asm_path.write_text(static_asm)
            with self.assertRaisesRegex(ValueError, "Unbound function-local static symbol"):
                wrap_function(region, asm_path, "diskCmd", {})

            wrapper = wrap_function(region, asm_path, "diskCmd", {"_diskCmd_state_1_2": "0xc100"})
            self.assertIn("_diskCmd_state_1_2 = 0xc100\n", wrapper.read_text())


if __name__ == "__main__":
    unittest.main()
