"""
Computes CRC-32/MPEG-2 over a firmware .bin file, matching the STM32F4
hardware CRC peripheral output exactly.

Algorithm parameters:
  Polynomial : 0x04C11DB7
  Initial    : 0xFFFFFFFF
  Reflect in : No
  Reflect out: No
  XOR out    : 0x00000000

Usage:
  python compute_crc32.py <firmware.bin>           # human-readable output
  python compute_crc32.py --decimal <firmware.bin> # print only the decimal integer (for CI scripting)

The printed hex value is to be embeded in the AWS IoT Job document as:
  "files": [{ "fileChecksum": <value>, ... }]
"""

import sys
import struct
import argparse

POLY = 0x04C11DB7
INIT = 0xFFFFFFFF


def crc32_mpeg2(data: bytes) -> int:
    """
    Compute CRC-32/MPEG-2 over data.

    The STM32 hardware CRC unit operates on 32-bit words. If the input
    length is not a multiple of 4, the remaining bytes are zero-padded
    (matching CRC->DR word-write behaviour on STM32).

    How it works:
      1. The CRC register starts at 0xFFFFFFFF (INIT value).
      2. For each 32-bit word in the input:
           a. XOR the word into the top of the CRC register.
           b. Run 32 LFSR shift steps (shift left; if old MSB was 1, XOR
              with the polynomial). This is the same feedback loop the
              STM32 CRC peripheral runs in hardware.
      3. The final register value is the CRC — no output XOR is applied
         (XOR out = 0x00000000), so the raw register value is the result.

    Why big-endian word parsing:
      The STM32 CRC unit has no bit or byte reflection — it processes the
      MSB of each word first. struct.unpack '>I' gives us that same order.

    Why \x00 padding and not \xff:
      Writing a partial word to CRC->DR on STM32 zero-fills the unused
      bytes in hardware. We replicate that here so the Python output matches
      the hardware output for firmware binaries whose size is not a multiple
      of 4 bytes.
    """
    # Pad to a multiple of 4 bytes (STM32 feeds 32-bit words)
    remainder = len(data) % 4
    if remainder:
        data = data + b'\x00' * (4 - remainder)

    crc = INIT
    for i in range(0, len(data), 4):
        word = struct.unpack_from('>I', data, i)[0]  # big-endian: no reflection
        crc ^= word
        for _ in range(32):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ POLY) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF

    return crc


def main():
    parser = argparse.ArgumentParser(description="Compute STM32 CRC-32/MPEG-2 over a firmware binary")
    parser.add_argument("bin_path", help="Path to firmware .bin file")
    parser.add_argument(
        "--decimal",
        action="store_true",
        help="Print only the decimal CRC integer (for CI scripting — no other output)",
    )
    args = parser.parse_args()

    try:
        with open(args.bin_path, 'rb') as f:
            firmware = f.read()
    except FileNotFoundError:
        print(f"Error: file not found: {args.bin_path}", file=sys.stderr)
        sys.exit(1)

    if len(firmware) == 0:
        print("Error: file is empty", file=sys.stderr)
        sys.exit(1)

    checksum = crc32_mpeg2(firmware)

    if args.decimal:
        # Machine-readable output for CI: just the integer, nothing else.
        print(checksum)
        return

    print(f"File   : {args.bin_path}")
    print(f"Size   : {len(firmware)} bytes")
    print(f"CRC-32 : 0x{checksum:08X} ({checksum})")
    print()
    print("Job document field (decimal):")
    print(f'  "files": [{{"fileChecksum": {checksum}, ...}}]')
    print()
    print("Job document field (hex string — use strtoul-compatible format):")
    print(f'  "files": [{{"fileChecksum": "0x{checksum:08X}", ...}}]')


if __name__ == '__main__':
    main()
