"""Package the existing Windows executable; no recompilation or benchmarks here."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
from pathlib import Path
import re
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--build-id", default="local")
    parser.add_argument("--run-url", default="Local build; no GitHub Actions run")
    parser.add_argument("--out", type=Path, default=ROOT / "dist")
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9a-f]{40}", args.commit):
        parser.error("--commit must be the full 40-character source commit SHA")
    exe = ROOT / "source/build/AM5MemoryLab.exe"
    binary = exe.read_bytes()
    if len(binary) < 64 or binary[:2] != b"MZ":
        raise ValueError("Missing or invalid Windows executable")
    offset = struct.unpack_from("<I", binary, 0x3C)[0]
    if (offset + 26 > len(binary) or binary[offset:offset + 4] != b"PE\0\0" or
            struct.unpack_from("<H", binary, offset + 4)[0] != 0x8664 or
            struct.unpack_from("<H", binary, offset + 24)[0] != 0x20B):
        raise ValueError("Executable is not Windows x64 PE32+")
    files = {"AM5MemoryLab.exe": exe}
    for name in ("START.cmd", "QUICK.cmd", "SELFTEST.cmd", "profile.ini", "README_zh-CN.md"):
        path = ROOT / name
        if not path.is_file() or path.is_symlink():
            raise ValueError(f"Required runtime file missing or a symlink: {name}")
        files[name] = path
    compiler = subprocess.check_output(["clang", "--version"], text=True).splitlines()[0]
    linker = subprocess.check_output(["lld-link", "--version"], text=True).strip()
    info = (
        "AM5 Native Memory Lab - Windows x64 build\n"
        f"Source commit: {args.commit}\nBuild ID: {args.build_id}\n"
        f"UTC: {datetime.now(timezone.utc).isoformat()}\nWorkflow run: {args.run_url}\n"
        f"Compiler: {compiler}\nLinker: {linker}\n"
        f"EXE SHA256: {sha256(exe)}\n\n"
        "Requires Windows x64 and AVX2. This executable is unsigned.\n"
        "The release workflow runs Linux and Windows functional tests before publication.\n"
        "Consult the linked workflow run for actual results; CI scores are not your PC's scores.\n"
        "Neither CI nor kernel self-tests prove memory overclock stability.\n"
        "profile.ini contains annotations only; no BIOS or voltage changes are made.\n"
        "Source: https://github.com/aksjfds/AM5-Native-Memory-Lab\n"
    )
    args.out.mkdir(parents=True, exist_ok=True)
    archive = args.out / "AM5MemoryLab-Windows-x64.zip"
    metadata = args.out / "BUILD_INFO.txt"
    sums = args.out / "SHA256SUMS.txt"
    if any(path.exists() for path in (archive, metadata, sums)):
        raise FileExistsError("Release outputs already exist; use an empty output directory")
    metadata.write_text(info, encoding="utf-8", newline="\n")
    prefix = "AM5-Native-Memory-Lab/"
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as handle:
        for name, path in sorted(files.items()):
            handle.write(path, prefix + name)
        handle.writestr(prefix + "BUILD_INFO.txt", info)
    with zipfile.ZipFile(archive) as handle:
        if handle.testzip() is not None:
            raise ValueError("ZIP integrity check failed")
    sums.write_text("".join(f"{sha256(path)}  {path.name}\n" for path in (archive, metadata)),
                    encoding="utf-8", newline="\n")
    print(f"Created {archive} ({archive.stat().st_size} bytes), BUILD_INFO.txt and SHA256SUMS.txt")


if __name__ == "__main__":
    main()
