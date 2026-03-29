#!/usr/bin/env python3
"""
Decrypt JPEG files produced by UnitCam firmware (*.ucam).

Requires: pip install pycryptodome

Same passphrase as in menuconfig: Picsaver (SD JPEG) -> Passphrase.
"""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

MAGIC = b"UCJF0001"
SALT = b"UnitCamSdEncV1!x"
PBKDF2_ITERATIONS = 10000
KEY_LEN = 32


def decrypt_ucam(blob: bytes, password: str) -> bytes:
    if len(blob) < 8 + 12 + 4 + 16:
        raise ValueError("file too small to be a .ucam payload")
    if blob[:8] != MAGIC:
        raise ValueError(f"bad magic (expected {MAGIC!r})")
    iv = blob[8:20]
    plain_len = struct.unpack("<I", blob[20:24])[0]
    if plain_len > 30 * 1024 * 1024:
        raise ValueError("implausible plaintext length")
    end_ct = 24 + plain_len
    if len(blob) < end_ct + 16:
        raise ValueError("truncated file")
    ciphertext = blob[24:end_ct]
    tag = blob[end_ct : end_ct + 16]

    key = hashlib.pbkdf2_hmac(
        "sha256", password.encode("utf-8"), SALT, PBKDF2_ITERATIONS, dklen=KEY_LEN
    )

    try:
        from Crypto.Cipher import AES
    except ImportError as e:
        raise SystemExit(
            "Missing dependency: pip install pycryptodome"
        ) from e

    cipher = AES.new(key, AES.MODE_GCM, nonce=iv)
    return cipher.decrypt_and_verify(ciphertext, tag)


def ucam_to_jpg_filename(name: str) -> str:
    lower = name.lower()
    if lower.endswith(".ucam"):
        return name[: -len(".ucam")] + ".jpg"
    return name + ".jpg"


def decrypt_file(ucam_path: Path, password: str, out_path: Path) -> None:
    data = ucam_path.read_bytes()
    jpeg = decrypt_ucam(data, password)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(jpeg)
    print(f"Wrote {out_path} ({len(jpeg)} bytes)")


def main() -> None:
    p = argparse.ArgumentParser(
        description="Decrypt UnitCam .ucam payloads to .jpg (single file or whole folder)"
    )
    p.add_argument(
        "input",
        help="Path to one .ucam file, or a folder containing *.ucam",
    )
    p.add_argument(
        "-o",
        "--output",
        help="Output JPEG path (single file), or output folder (batch). Default: next to each .ucam",
    )
    p.add_argument(
        "-p",
        "--password",
        required=True,
        help="Same passphrase as CONFIG_PICSAVER_ENCRYPTION_PASSWORD in firmware",
    )
    args = p.parse_args()

    inp = Path(args.input)
    if not inp.exists():
        print(f"Error: not found: {inp}", file=sys.stderr)
        sys.exit(1)

    if inp.is_file():
        out: Path | None = Path(args.output) if args.output else None
        if out is None:
            out = inp.parent / ucam_to_jpg_filename(inp.name)
        elif out.is_dir():
            out = out / ucam_to_jpg_filename(inp.name)
        try:
            decrypt_file(inp, args.password, out)
        except Exception as ex:
            print(f"Error: {ex}", file=sys.stderr)
            sys.exit(1)
        return

    if inp.is_dir():
        files = sorted(inp.glob("*.ucam"))
        if not files:
            print(f"No *.ucam files in {inp}", file=sys.stderr)
            sys.exit(1)

        out_root: Path | None = None
        if args.output:
            out_root = Path(args.output)
            if out_root.exists() and not out_root.is_dir():
                print(
                    "Error: when input is a folder, -o must be a folder (or omit it).",
                    file=sys.stderr,
                )
                sys.exit(1)
            out_root.mkdir(parents=True, exist_ok=True)

        errors = 0
        for fp in files:
            if out_root is not None:
                out_path = out_root / ucam_to_jpg_filename(fp.name)
            else:
                out_path = fp.parent / ucam_to_jpg_filename(fp.name)
            try:
                decrypt_file(fp, args.password, out_path)
            except Exception as ex:
                print(f"{fp}: {ex}", file=sys.stderr)
                errors += 1
        if errors:
            sys.exit(1)
        return

    print(f"Error: not a file or directory: {inp}", file=sys.stderr)
    sys.exit(1)


if __name__ == "__main__":
    main()
