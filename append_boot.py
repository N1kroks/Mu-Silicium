#!/usr/bin/env python3

from argparse import ArgumentParser
from pathlib import Path

import struct

BOOT_MAGIC = b"ANDROID!"
PAGE_SIZE_V3 = 4096

def align (value: int, alignment: int) -> int:
  return (value + alignment - 1) // alignment * alignment

def get_boot_image_size (boot: Path) -> int:
  data = boot.read_bytes ()
  if len (data) < 44:
    raise ValueError ("Boot image is too small")

  if data[:8] != BOOT_MAGIC:
    raise ValueError ("Invalid boot image magic")

  version = struct.unpack_from ("<I", data, 40)[0]
  if version > 4:
    raise ValueError (f"Invalid header version: {version}")

  if version <= 2:
    header_size  = 1632
    kernel_size  = struct.unpack_from ("<I", data, 8)[0]
    ramdisk_size = struct.unpack_from ("<I", data, 16)[0]
    page_size    = struct.unpack_from ("<I", data, 36)[0]
  else:
    header_size  = struct.unpack_from ("<I", data, 20)[0]
    kernel_size  = struct.unpack_from ("<I", data, 8)[0]
    ramdisk_size = struct.unpack_from ("<I", data, 12)[0]
    page_size    = PAGE_SIZE_V3

  size  = align (header_size, page_size)
  size += align (kernel_size, page_size)
  size += align (ramdisk_size, page_size)

  return size

if __name__ == "__main__":
  parser = ArgumentParser ()

  parser.add_argument ("-s", "--silicium", type=Path, required=True)
  parser.add_argument ("-b", "--boot", type=Path, required=True)
  parser.add_argument ("-f", "--fdt", type=Path, required=True)
  parser.add_argument ("-o", "--output", type=Path, required=True)

  args = parser.parse_args ()

  boot_size = get_boot_image_size (args.boot)
  boot_data = args.boot.read_bytes ()

  data = bytearray ()
  data.extend (args.silicium.read_bytes ())
  data.extend (boot_data[:boot_size])
  data.extend (args.fdt.read_bytes ())

  args.output.parent.mkdir (parents=True, exist_ok=True)
  args.output.write_bytes (data)