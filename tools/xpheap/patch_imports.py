"""Copy a PE executable and prepend an import of DLL!FUNCTION (default xpheap.dll!xpheap_marker).

The original import descriptors (and therefore every original IAT slot) are kept; only the
import directory is moved into a new section so the new descriptor can come first, which makes
the loader initialise the added DLL before msvcrt.dll. The original code and data are unchanged.
"""
import struct
import sys
from pathlib import Path


def align(x, a):
    return (x + a - 1) // a * a


def pe_checksum(data, checksum_offset):
    buf = bytearray(data)
    struct.pack_into("<I", buf, checksum_offset, 0)
    if len(buf) & 1:
        buf.append(0)
    total = sum(struct.unpack("<{}H".format(len(buf) // 2), buf))
    while total > 0xffff:
        total = (total & 0xffff) + (total >> 16)
    return (total + len(data)) & 0xffffffff


def patch(src, dst, dll=b"xpheap.dll", func=b"xpheap_marker", section=b".xpheap"):
    d = bytearray(Path(src).read_bytes())
    if len(d) < 0x40 or d[:2] != b"MZ":
        raise ValueError("not a DOS/PE executable")
    pe = struct.unpack_from("<I", d, 0x3c)[0]
    if pe + 24 > len(d) or d[pe:pe + 4] != b"PE\0\0":
        raise ValueError("missing PE signature")
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    opt = pe + 24
    osize = struct.unpack_from("<H", d, pe + 20)[0]
    if nsec < 1 or osize < 96 or opt + osize > len(d):
        raise ValueError("invalid PE header sizes")
    if struct.unpack_from("<H", d, opt)[0] != 0x10b:
        raise ValueError("expected 32-bit PE32 optional header")
    sect_align, file_align = struct.unpack_from("<II", d, opt + 32)
    if not sect_align or not file_align:
        raise ValueError("invalid PE alignment")
    size_of_headers, orig_checksum = struct.unpack_from("<II", d, opt + 60)
    num_dd = min(struct.unpack_from("<I", d, opt + 92)[0], (osize - 96) // 8)
    if num_dd < 2:
        raise ValueError("PE optional header lacks an import directory entry")
    dd = opt + 96
    imp_rva, imp_size = struct.unpack_from("<II", d, dd + 8)
    if not imp_rva or imp_size < 20:
        raise ValueError("PE executable has no import table")
    sh = opt + osize
    if size_of_headers < sh + 40 * nsec or size_of_headers > len(d) or sh + 40 * nsec > len(d):
        raise ValueError("invalid PE header layout")
    secs = [struct.unpack_from("<8sIIIIIIHHI", d, sh + 40 * i) for i in range(nsec)]
    raw_offsets = [s[4] for s in secs if s[4] and s[3]]
    first_raw = min(raw_offsets) if raw_offsets else len(d)
    new_headers_end = sh + 40 * (nsec + 1)
    if new_headers_end > first_raw:
        raise ValueError("no room for another section header")
    if new_headers_end > size_of_headers:
        grown_headers = align(new_headers_end, file_align)
        if grown_headers > first_raw:
            raise ValueError("no room for another section header")
        size_of_headers = grown_headers

    def rva2off(rva, size=1):
        for s in secs:
            span = max(s[1], s[3])
            if s[2] <= rva and rva + size <= s[2] + span:
                offset = rva - s[2]
                if not s[4] or offset + size > s[3] or s[4] + offset + size > len(d):
                    raise ValueError("RVA outside raw section data: " + hex(rva))
                return s[4] + offset
        raise ValueError(hex(rva))

    old = []
    cur_rva = imp_rva
    while True:
        o = rva2off(cur_rva, 20)
        desc = bytes(d[o:o + 20])
        if desc == b"\0" * 20:
            break
        old.append(desc)
        cur_rva += 20
    new_rva = align(max(s[2] + max(s[1], s[3]) for s in secs), sect_align)
    new_off = align(len(d), file_align)
    # layout: descriptors | INT | IAT | hint/name | dll name
    ndesc = len(old) + 2
    desc_size = 20 * ndesc
    int_rva = new_rva + desc_size
    iat_rva = int_rva + 8
    hn_rva = iat_rva + 8
    name_rva = hn_rva + 2 + len(func) + 1
    name_rva += name_rva & 1
    body = bytearray()
    body += struct.pack("<IIIII", int_rva, 0, 0, name_rva, iat_rva)
    for desc in old:
        body += desc
    body += b"\0" * 20
    body += struct.pack("<II", hn_rva, 0)
    body += struct.pack("<II", hn_rva, 0)
    body += struct.pack("<H", 0) + func + b"\0"
    if len(body) + new_rva != name_rva:
        body += b"\0" * (name_rva - new_rva - len(body))
    body += dll + b"\0"
    raw_size = align(len(body), file_align)
    d += b"\0" * (new_off - len(d))
    d += body + b"\0" * (raw_size - len(body))
    # section header (initialised data, read/write)
    struct.pack_into("<8sIIIIIIHHI", d, sh + 40 * nsec, section.ljust(8, b"\0")[:8], len(body), new_rva, raw_size, new_off,
                     0, 0, 0, 0, 0xC0000040)
    struct.pack_into("<H", d, pe + 6, nsec + 1)
    struct.pack_into("<II", d, opt + 56,
                     align(new_rva + len(body), sect_align), size_of_headers)       # SizeOfImage, SizeOfHeaders
    struct.pack_into("<II", d, dd + 8, new_rva, desc_size)                           # import directory
    if num_dd > 11:
        struct.pack_into("<II", d, dd + 8 * 11, 0, 0)                                # bound imports
    if orig_checksum:
        struct.pack_into("<I", d, opt + 64, pe_checksum(d, opt + 64))
    Path(dst).write_bytes(bytes(d))
    return new_rva


if __name__ == "__main__":
    print(hex(patch(sys.argv[1], sys.argv[2])))
