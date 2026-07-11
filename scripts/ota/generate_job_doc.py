"""
generate_job_doc.py

Generates the AWS IoT Job document for an OTA firmware update.
Output is written to stdout — redirect to a file (e.g. > job_document.json).

The field names and types must match exactly what custom_job_parser.c on the
device expects:

  custom_job_parser.c parses:
    "streamID"              → string  — AWS IoT Stream name passed to mqttDownloader
    "files[0].fileSize"     → uint32  — parsed with atoi()
    "files[0].fileChecksum" → uint32  — parsed with strtoul(..., 0), accepts decimal or 0x hex

Usage:
  python generate_job_doc.py \\
      --stream-id     <stream-name>   \\
      --file-size     <bytes>         \\
      --file-checksum <decimal-crc>
"""

import argparse
import json
import sys


def main():
    parser = argparse.ArgumentParser(
        description="Generate AWS IoT OTA job document for this device"
    )
    parser.add_argument(
        "--stream-id",
        required=True,
        help="AWS IoT Stream name (value of 'streamID' in the job document)",
    )
    parser.add_argument(
        "--file-size",
        required=True,
        type=int,
        help="Firmware binary size in bytes",
    )
    parser.add_argument(
        "--file-checksum",
        required=True,
        type=int,
        help="STM32 CRC-32/MPEG-2 checksum as a decimal integer "
             "(output of: python compute_crc32.py --decimal <firmware.bin>)",
    )
    args = parser.parse_args()

    # fileId 0 matches the file_id field in custom_job_doc_fields_t and the
    # mqttDownloader file ID used in ota_application.c.
    doc = {
        "streamID": args.stream_id,
        "files": [
            {
                "fileId":       0,
                "fileSize":     args.file_size,
                "fileChecksum": args.file_checksum,
            }
        ],
    }

    json.dump(doc, sys.stdout, indent=2)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
