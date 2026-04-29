#!/usr/bin/env python3
"""Small Android boot image v0-v2 writer used when host mkbootimg is absent."""

import argparse
import hashlib
import struct
import sys


BOOT_MAGIC = b"ANDROID!"
BOOT_NAME_SIZE = 16
BOOT_ARGS_SIZE = 512
BOOT_EXTRA_ARGS_SIZE = 1024


def parse_int(value):
    return int(value, 0)


def parse_os_version(version, patch_level):
    major = minor = patch = 0
    if version:
        parts = version.split(".")
        if len(parts) > 3:
            raise ValueError("os_version must be MAJOR[.MINOR[.PATCH]]")
        parts += ["0"] * (3 - len(parts))
        major, minor, patch = (int(part) for part in parts)

    year = month = 0
    if patch_level:
        parts = patch_level.split("-")
        if len(parts) != 2:
            raise ValueError("os_patch_level must be YYYY-MM")
        year, month = (int(part) for part in parts)
        year -= 2000

    if not (0 <= major <= 127 and 0 <= minor <= 127 and 0 <= patch <= 127):
        raise ValueError("os_version components must be between 0 and 127")
    if not (0 <= year <= 127 and 0 <= month <= 12):
        raise ValueError("os_patch_level is out of range")

    return (major << 25) | (minor << 18) | (patch << 11) | (year << 4) | month


def read_optional(path):
    if not path:
        return b""
    with open(path, "rb") as file:
        return file.read()


def padded(data, page_size):
    padding = (-len(data)) % page_size
    if padding:
        return data + (b"\0" * padding)
    return data


def addr32(base, offset):
    return (base + offset) & 0xFFFFFFFF


def fixed_bytes(value, size, field_name):
    encoded = value.encode("ascii")
    if len(encoded) > size:
        raise ValueError(f"{field_name} is too long")
    return encoded + (b"\0" * (size - len(encoded)))


def split_cmdline(cmdline):
    encoded = cmdline.encode("ascii")
    limit = BOOT_ARGS_SIZE + BOOT_EXTRA_ARGS_SIZE
    if len(encoded) > limit:
        raise ValueError("cmdline is too long")
    first = encoded[:BOOT_ARGS_SIZE]
    extra = encoded[BOOT_ARGS_SIZE:]
    return (
        first + (b"\0" * (BOOT_ARGS_SIZE - len(first))),
        extra + (b"\0" * (BOOT_EXTRA_ARGS_SIZE - len(extra))),
    )


def image_id(kernel, ramdisk, second, dtb):
    digest = hashlib.sha1()
    for payload in (kernel, ramdisk, second, dtb):
        digest.update(payload)
        digest.update(struct.pack("<I", len(payload)))
    return struct.unpack("<8I", digest.digest() + (b"\0" * 12))


def build_header(args, kernel, ramdisk, second, recovery_dtbo, dtb):
    os_version = parse_os_version(args.os_version, args.os_patch_level)
    name = fixed_bytes(args.board, BOOT_NAME_SIZE, "board")
    cmdline, extra_cmdline = split_cmdline(args.cmdline)
    header_version = args.header_version
    if header_version < 0 or header_version > 2:
        raise ValueError("this fallback mkbootimg only supports header versions 0-2")

    kernel_addr = addr32(args.base, args.kernel_offset)
    ramdisk_addr = addr32(args.base, args.ramdisk_offset)
    second_addr = addr32(args.base, args.second_offset)
    tags_addr = addr32(args.base, args.tags_offset)
    dtb_addr = args.base + args.dtb_offset
    header_size = 1660 if header_version == 2 else 1648 if header_version == 1 else 1632
    recovery_dtbo_offset = 0

    header = struct.pack(
        "<8s10I16s512s8I1024s",
        BOOT_MAGIC,
        len(kernel),
        kernel_addr,
        len(ramdisk),
        ramdisk_addr,
        len(second),
        second_addr,
        tags_addr,
        args.pagesize,
        header_version,
        os_version,
        name,
        cmdline,
        *image_id(kernel, ramdisk, second, dtb),
        extra_cmdline,
    )

    if header_version >= 1:
        header += struct.pack("<IQI", len(recovery_dtbo), recovery_dtbo_offset, header_size)
    if header_version >= 2:
        header += struct.pack("<IQ", len(dtb), dtb_addr)

    return header


def write_image(args):
    kernel = read_optional(args.kernel)
    ramdisk = read_optional(args.ramdisk)
    second = read_optional(args.second)
    recovery_dtbo = read_optional(args.recovery_dtbo)
    dtb = read_optional(args.dtb)
    header = build_header(args, kernel, ramdisk, second, recovery_dtbo, dtb)

    with open(args.output, "wb") as output:
        output.write(padded(header, args.pagesize))
        output.write(padded(kernel, args.pagesize))
        output.write(padded(ramdisk, args.pagesize))
        if second:
            output.write(padded(second, args.pagesize))
        if recovery_dtbo:
            output.write(padded(recovery_dtbo, args.pagesize))
        if dtb:
            output.write(padded(dtb, args.pagesize))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel", required=True)
    parser.add_argument("--ramdisk", default="")
    parser.add_argument("--second", default="")
    parser.add_argument("--dtb", default="")
    parser.add_argument("--recovery_dtbo", default="")
    parser.add_argument("--pagesize", type=parse_int, default=2048)
    parser.add_argument("--base", type=parse_int, default=0x10000000)
    parser.add_argument("--kernel_offset", type=parse_int, default=0x00008000)
    parser.add_argument("--ramdisk_offset", type=parse_int, default=0x01000000)
    parser.add_argument("--second_offset", type=parse_int, default=0x00F00000)
    parser.add_argument("--tags_offset", type=parse_int, default=0x00000100)
    parser.add_argument("--dtb_offset", type=parse_int, default=0x01F00000)
    parser.add_argument("--header_version", type=parse_int, default=0)
    parser.add_argument("--os_version", default="")
    parser.add_argument("--os_patch_level", default="")
    parser.add_argument("--cmdline", default="")
    parser.add_argument("--board", default="")
    parser.add_argument("-o", "--output", required=True)
    args = parser.parse_args()

    if args.pagesize <= 0 or args.pagesize & (args.pagesize - 1):
        sys.exit("pagesize must be a positive power of two")

    try:
        write_image(args)
    except (OSError, ValueError) as error:
        sys.exit(f"mkbootimg.py: {error}")


if __name__ == "__main__":
    main()
