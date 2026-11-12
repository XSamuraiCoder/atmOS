#!/usr/bin/env python3
"""mkmod.py ELF IMAGE OUT [--max-mem N]

Turns a mod linked at address 0 into a relocatable atmOS mod file:
  OUT = IMAGE (header patched with the relocation count) + a table of 16-bit offsets.

Diagnostic build: instead of hardcoding one relocation type, this version
  - reports every relocation type the ELF actually contains (so the correct
    R_Z80_* numbering can be read straight off the output),
  - treats any non-NONE, non-absolute relocation against an allocated section
    as a candidate 24-bit pointer,
  - validates that the 3 bytes at each candidate offset look like a 0-based
    pointer (value < bss_end) before accepting it: pointers into the image AND
    into the mod's .bss are both real. A .bss global's address is >= image_end,
    and dropping its relocation is exactly the "NMI reset caused by writing to
    flash" crash - the mod then stores to the global's link-time address (a
    small number like 0x0C2A, which is flash on the CE),
  - merges overlapping candidates (a 24-bit pointer sometimes emits a reloc
    for each byte; patching the lowest offset of each cluster is correct for
    little-endian eZ80 addresses),
  - dies if the resulting table is empty, since a mod with no relocations
    cannot run at a dynamic address.

Relocations against absolute symbols (ROM routines) are skipped, and the
32-byte header holds plain offsets, so it is not relocated either.
"""
import struct, sys

HEADER = 32
OFF_ENTRY, OFF_IMAGE_END, OFF_BSS_END, OFF_COUNT = 21, 24, 27, 30
SIGNATURE = b"atmOS mod: "
SHT_RELA, SHT_REL = 4, 9
SHF_ALLOC = 2
SHN_ABS = 0xFFF1
R_Z80_NONE = 0


def die(msg):
    sys.exit("mkmod: error: " + msg)


def u24(b, o):
    return b[o] | (b[o + 1] << 8) | (b[o + 2] << 16)


def read_relocs(elf):
    if elf[:4] != b"\x7fELF" or elf[4] != 1 or elf[5] != 1:
        die("not a 32-bit little-endian ELF file")
    shoff, = struct.unpack_from("<I", elf, 0x20)
    shentsize, shnum = struct.unpack_from("<HH", elf, 0x2E)
    sects = [struct.unpack_from("<IIIIIIIIII", elf, shoff + i * shentsize) for i in range(shnum)]
    # (name, type, flags, addr, offset, size, link, info, addralign, entsize)
    found = []
    for name, typ, flags, addr, off, size, link, info, align, entsize in sects:
        if typ not in (SHT_RELA, SHT_REL):
            continue
        if typ == SHT_REL:
            die("REL relocation sections are not supported")
        target = sects[info]
        if not target[2] & SHF_ALLOC:
            continue
        sym = sects[link]
        for i in range(size // 12):
            r_off, r_info, _ = struct.unpack_from("<IIi", elf, off + i * 12)
            idx, rtype = r_info >> 8, r_info & 0xFF
            _, value, _, _, _, shndx = struct.unpack_from("<IIIBBH", elf, sym[4] + idx * 16)
            found.append((r_off, rtype, shndx, idx))
    return found


def main():
    args = sys.argv[1:]
    max_mem = 16384
    if "--max-mem" in args:
        i = args.index("--max-mem")
        max_mem = int(args[i + 1], 0)
        del args[i:i + 2]
    if len(args) != 3:
        die("usage: mkmod.py ELF IMAGE OUT [--max-mem N]")
    elf = open(args[0], "rb").read()
    img = bytearray(open(args[1], "rb").read())

    if len(img) < HEADER or bytes(img[:len(SIGNATURE)]) != SIGNATURE:
        die("image does not start with the 'atmOS mod: ' header (missing ATMOS_MOD_HEADER?)")
    entry, image_end, bss_end = u24(img, OFF_ENTRY), u24(img, OFF_IMAGE_END), u24(img, OFF_BSS_END)
    if image_end != len(img):
        die("header says image is %d bytes but the file is %d" % (image_end, len(img)))
    if not HEADER <= entry < image_end:
        die("entry point 0x%X is outside the image" % entry)
    if bss_end < image_end:
        die("bss end 0x%X is before the image end" % bss_end)
    if bss_end > max_mem:
        die("mod needs %d bytes of memory, the limit is %d" % (bss_end, max_mem))
    if len(img) > 0xFFFF:
        die("image is larger than 64 KB")

    # ---- pass 1: what relocation types does this toolchain actually emit? ----
    type_counts = {}
    for r_off, rtype, shndx, idx in read_relocs(elf):
        if rtype == R_Z80_NONE or shndx == SHN_ABS:
            continue
        type_counts[rtype] = type_counts.get(rtype, 0) + 1
    if type_counts:
        print("mkmod: relocation types present (type x count): " +
              ", ".join("%d x%d" % (t, n) for t, n in sorted(type_counts.items())))
        print("mkmod: (compare with the 'Type' column of: z80-none-elf-readelf -r <elf>)")
    else:
        print("mkmod: WARNING no relocations found in the ELF at all")

    # ---- pass 2: collect candidate pointer offsets ----
    # A candidate is any reloc (non-NONE, non-absolute) whose 3 bytes at r_off
    # look like a 0-based pointer into the mod's memory: image (value < image_end)
    # OR .bss (image_end <= value < bss_end). Wrong-type candidates whose bytes
    # don't parse as an offset are skipped and reported per type.
    candidates = []          # (offset, type)
    rejected = {}            # type -> count of value-validated failures
    for r_off, rtype, shndx, idx in read_relocs(elf):
        if rtype == R_Z80_NONE or shndx == SHN_ABS:
            continue
        if shndx == 0:
            die("relocation against an undefined symbol (index %d)" % idx)
        if r_off < HEADER:
            continue                          # the header holds offsets, not addresses
        if r_off + 3 > image_end:
            die("relocation at 0x%X is outside the image" % r_off)
        if u24(img, r_off) < bss_end:         # FIX: was < image_end, which dropped
            candidates.append((r_off, rtype)) # every pointer to a .bss global
        else:
            rejected[rtype] = rejected.get(rtype, 0) + 1
    for t, n in sorted(rejected.items()):
        print("mkmod: skipped %d reloc(s) of type %d (value outside the mod's memory: "
              "not a 0-based image/.bss offset)" % (n, t))

    # ---- pass 3: merge overlapping candidates into one patch per pointer ----
    # If the toolchain emits one reloc per byte of a 24-bit value, the lowest
    # offset of each cluster is the start of the (little-endian) pointer.
    candidates.sort()
    offsets = []
    for r_off, _ in candidates:
        if offsets and r_off - offsets[-1] < 3:
            continue                          # same 3-byte pointer as the previous one
        offsets.append(r_off)

    if not offsets:
        die("no valid relocations found: the mod would keep its link-time "
            "(address-0) pointers and crash when loaded elsewhere. Check the "
            "relocation types reported above against your toolchain's "
            "R_Z80_* numbering and adjust R_Z80 selection in this script.")

    img[OFF_COUNT:OFF_COUNT + 2] = struct.pack("<H", len(offsets))
    out = bytes(img) + b"".join(struct.pack("<H", o) for o in offsets)
    open(args[2], "wb").write(out)
    print("mkmod: %d bytes image, %d bytes memory, %d relocations -> %d bytes" %
          (image_end, bss_end, len(offsets), len(out)))
    print("mkmod: first few offsets: " +
          ", ".join("0x%X" % o for o in offsets[:10]))


if __name__ == "__main__":
    main()