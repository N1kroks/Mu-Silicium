from argparse import ArgumentParser
from pathlib import Path
import tempfile
import subprocess
import re

CONFIG = {
    "Qualcomm/QcomPkg": {
        "ACTLR_EL1": "HAS_ACTLR_EL1_UNIMPLEMENTED_ERRATA",
        "AMCNTENSET0_EL0": "HAS_AMCNTENSET0_EL0_UNIMPLEMENTED_ERRATA",
        "IRM": "HAS_GIC_V3_WITHOUT_IRM_FLAG_SUPPORT_ERRATA",
        "IRM_X8": "HAS_GIC_V3_WITHOUT_IRM_FLAG_SUPPORT_ERRATA",
        "PSCI_MEMPROTECT": "HAS_PSCI_MEMPROTECT_FAILING_ERRATA"
    },
    "Samsung/SamsungPkg": {
        "CNTFRQ_EL0": "HAS_CNTFRQ_EL0_EMPTY_ERRATA"
    }
}

ENTRYPOINT_RE = re.compile (rf"^\s*OslArm64TransferToKernel:")
FRAME_RE = re.compile (r"^\s*mov\s*x29,\s*sp")
EXITPOINT_RE = re.compile (rf"\s*bl\s*DoSomething")

HEADER = """\
/**
  Copyright (c) 2022-2023 DuoWoA authors
  Copyright (c) 2026 Project Silicium

  SPDX-License-Identifier: MIT
**/

#ifndef _SHELL_CODE_H_
#define _SHELL_CODE_H_

STATIC
UINT8
TransferToKernelShellCode[] = {
"""
FOOTER = """\
};

#endif /* _SHELL_CODE_H_ */
"""

def clean_assembly (input_file: Path, output_file: Path):
    lines = input_file.read_text ().splitlines (keepends=True)

    for i, line in enumerate (lines):
        if ENTRYPOINT_RE.match (line):
            for j in range (i + 1, len (lines)):
                if FRAME_RE.match (lines[j]):
                    del lines[:j + 1]
                    break

    for i, line in enumerate (lines):
        if EXITPOINT_RE.match (line):
            del lines[i:]
            break

    output_file.write_text (".text\n" + ".align 4\n" + "Start:\n" + "".join (lines))

def compile_shellcode (source_file: Path, debug: bool) -> bytes:
    with tempfile.TemporaryDirectory (prefix="errata_patch_") as tmp:
        tmp = Path (tmp)

        if debug:
            assembly_file = source_file.with_name ("shellcode.S")
            cleaned_file = source_file.with_name ("cleaned_shellcode.S")
        else:
            assembly_file = tmp / "shellcode.S"
            cleaned_file = tmp / "cleaned_shellcode.S"
        elf_file = tmp / "shellcode.elf"
        binary_file = tmp / "shellcode.bin"

        subprocess.run ([f"aarch64-linux-gnu-gcc", "-S", "-O1", "-fno-unwind-tables", "-fno-asynchronous-unwind-tables", "-o", str (assembly_file), str (source_file)], check=True)
        clean_assembly (assembly_file, cleaned_file)

        subprocess.run ([f"aarch64-linux-gnu-gcc", "-c", str (cleaned_file), "-o", str (elf_file)], check=True)
        subprocess.run ([f"aarch64-linux-gnu-objcopy", "-O", "binary", str (elf_file), str (binary_file)], check=True)

        return binary_file.read_bytes ()

def main ():
    parser = ArgumentParser (description="Rebuild KEP Patches.")
    parser.add_argument ("-d", "--debug", action="store_true", help="Keep generated asm file in patch folder")
    args = parser.parse_args ()

    for pkg, patches in CONFIG.items ():
        pkg_path = Path (f"Silicon") / pkg / "Library" / "ErrataPatchesLib"
        shellcodes: dict[str, bytes] = {}

        for patch, define in patches.items ():
            source = pkg_path / "Reference" / patch / "ShellCode.c"
            data = compile_shellcode (source, args.debug)
            shellcodes[define] = shellcodes.get (define, b"") + data

        with open (pkg_path / "ShellCode.h", "w") as f:
            f.write (HEADER)

            for index, (define, data) in enumerate (shellcodes.items ()):
                f.write (f"#if {define} == 1\n")

                chunks = [
                    "  " + ", ".join (f"0x{byte:02X}" for byte in data[i:i + 4])
                    for i in range (0, len(data), 4)
                ]

                if chunks:
                    f.write (",\n".join(chunks))
                    f.write (",\n")

                f.write ("#endif\n")

                if index < len (shellcodes) - 1:
                    f.write ("\n")

            f.write (FOOTER)

if __name__ == "__main__":
    main ()