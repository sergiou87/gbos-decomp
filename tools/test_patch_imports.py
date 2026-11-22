"""Unit tests for the PE32 import-prepending patcher."""

import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/xpheap"))
import patch_imports  # noqa: E402


def make_minimal_pe32(*, magic=0x10b, num_dd=16, size_of_headers=0x200,
                      virt_size=0x100, raw_size=0x200, imp_rva=0x1000, checksum=0):
    pe_off = 0x40
    opt_off = pe_off + 24
    opt_size = 96 + 8 * num_dd
    sh_off = opt_off + opt_size
    buf = bytearray(0x400)
    buf[0:2] = b"MZ"
    struct.pack_into("<I", buf, 0x3c, pe_off)
    buf[pe_off:pe_off + 4] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", buf, pe_off + 4, 0x14c, 1, 0, 0, 0, opt_size, 0x102)
    struct.pack_into("<H", buf, opt_off, magic)
    struct.pack_into("<II", buf, opt_off + 32, 0x1000, 0x200)
    struct.pack_into("<III", buf, opt_off + 56, 0x2000, size_of_headers, 0)
    struct.pack_into("<I", buf, opt_off + 92, num_dd)
    if num_dd >= 2:
        struct.pack_into("<II", buf, opt_off + 96 + 8, imp_rva, 40)
    struct.pack_into("<8sIIIIIIHHI", buf, sh_off,
                     b".idata\0\0", virt_size, 0x1000, raw_size, 0x200, 0, 0, 0, 0, 0xC0000040)
    # One fake original import descriptor at file offset 0x200 followed by 20-byte null terminator.
    struct.pack_into("<IIIII", buf, 0x200, 0x1040, 0, 0, 0x1060, 0x1050)
    if checksum:
        struct.pack_into("<I", buf, opt_off + 64, patch_imports.pe_checksum(buf, opt_off + 64))
    return bytes(buf), sh_off, opt_off


class PatchImportsTest(unittest.TestCase):
    def test_short_data_directory_does_not_clobber_section_header(self):
        image, sh_off, opt_off = make_minimal_pe32(num_dd=2, checksum=1)
        with tempfile.TemporaryDirectory() as temporary:
            src = Path(temporary) / "in.exe"
            dst = Path(temporary) / "out.exe"
            src.write_bytes(image)
            new_rva = patch_imports.patch(src, dst)
            self.assertEqual(new_rva, 0x2000)
            patched = dst.read_bytes()
            # First section header name must remain intact rather than zeroed by DataDirectory[11].
            self.assertEqual(patched[sh_off:sh_off + 8], b".idata\0\0")
            self.assertEqual(patched[sh_off + 40:sh_off + 48], b".xpheap\0")
            stored_sum = struct.unpack_from("<I", patched, opt_off + 64)[0]
            self.assertNotEqual(stored_sum, 0)
            self.assertEqual(stored_sum, patch_imports.pe_checksum(patched, opt_off + 64))

    def test_rejects_rva_in_uninitialized_virtual_tail(self):
        image, _, _ = make_minimal_pe32(virt_size=0x800, raw_size=0x200, imp_rva=0x1200)
        with tempfile.TemporaryDirectory() as temporary:
            src = Path(temporary) / "in.exe"
            dst = Path(temporary) / "out.exe"
            src.write_bytes(image)
            with self.assertRaisesRegex(ValueError, "outside raw section data"):
                patch_imports.patch(src, dst)

    def test_rejects_non_pe32_and_insufficient_size_of_headers(self):
        pe64, _, _ = make_minimal_pe32(magic=0x20b)
        bad_hdr, _, _ = make_minimal_pe32(size_of_headers=0x100)
        with tempfile.TemporaryDirectory() as temporary:
            src = Path(temporary) / "in.exe"
            dst = Path(temporary) / "out.exe"
            src.write_bytes(pe64)
            with self.assertRaisesRegex(ValueError, "PE32"):
                patch_imports.patch(src, dst)
            src.write_bytes(bad_hdr)
            with self.assertRaisesRegex(ValueError, "invalid PE header layout"):
                patch_imports.patch(src, dst)

    @unittest.skipUnless(shutil.which("i686-w64-mingw32-gcc"), "MinGW-w64 compiler unavailable")
    def test_patches_real_mingw_pe32_binary(self):
        with tempfile.TemporaryDirectory() as temporary:
            tmp = Path(temporary)
            c_file = tmp / "mini.c"
            exe = tmp / "mini.exe"
            out = tmp / "patched.exe"
            c_file.write_text("int main(void) { return 0; }\n")
            subprocess.run(["i686-w64-mingw32-gcc", "-O2", "-s", str(c_file), "-o", str(exe)],
                           check=True)
            patch_imports.patch(exe, out)
            patched = out.read_bytes()
            pe_off = struct.unpack_from("<I", patched, 0x3c)[0]
            opt_off = pe_off + 24
            stored_sum = struct.unpack_from("<I", patched, opt_off + 64)[0]
            self.assertNotEqual(stored_sum, 0)
            self.assertEqual(stored_sum, patch_imports.pe_checksum(patched, opt_off + 64))
            objdump = shutil.which("i686-w64-mingw32-objdump")
            if objdump:
                dump = subprocess.run([objdump, "-p", str(out)],
                                      capture_output=True, text=True, check=True).stdout
                self.assertIn("dll name: xpheap.dll", dump.lower())
                self.assertIn("xpheap_marker", dump)


if __name__ == "__main__":
    unittest.main()
