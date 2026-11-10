from argparse import ArgumentParser
from pathlib import Path
from dataclasses import dataclass
import struct

BOOT_MAGIC = b"ANDROID!"

@dataclass
class BootHeader:
  kernel_size: int
  ramdisk_size: int
  header_size: int
  page_size: int

def align(value, alignment):
  return (value + alignment - 1) // alignment * alignment

def get_boot_image_size(boot: Path) -> int:
  data = boot.read_bytes()
  if data[:8] != BOOT_MAGIC:
    raise ValueError("Invalid boot image magic")

  version = struct.unpack_from("<I", data, 40)[0]
  if version > 4:
    raise ValueError(f"Invalid header version: {version}")

  if version <= 2:
    header_size = 1632
    kernel_size = struct.unpack_from("<I", data, 8)[0]
    ramdisk_size = struct.unpack_from("<I", data, 16)[0]
    page_size = struct.unpack_from("<I", data, 36)[0]
  else:
    header_size = struct.unpack_from("<I", data, 20)[0]
    kernel_size = struct.unpack_from("<I", data, 8)[0]
    ramdisk_size = struct.unpack_from("<I", data, 12)[0]
    page_size = 4096

  offset = align(header_size, page_size)
  offset += align(kernel_size, page_size)
  offset += align(ramdisk_size, page_size)

  return offset

def main(silicium: Path, boot: Path, fdt: Path, output: Path):
  data = silicium.read_bytes()

  data += boot.read_bytes()[0:get_boot_image_size(boot)]
  data += fdt.read_bytes()

  output.write_bytes(data)

if __name__ == "__main__":
  parser = ArgumentParser()

  parser.add_argument("-s", "--silicium", type=Path, required=True)
  parser.add_argument("-b", "--boot", type=Path, required=True)
  parser.add_argument("-f", "--fdt", type=Path, required=True)
  parser.add_argument("-o", "--output", type=Path, required=True)

  args = parser.parse_args()
  main(args.silicium, args.boot, args.fdt, args.output)