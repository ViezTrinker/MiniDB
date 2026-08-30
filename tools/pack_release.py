#!/usr/bin/env python3
"""Pack a Windows Release zip from out/build/x64-Release/bin (excluding logs)."""

from __future__ import annotations

import argparse
import os
import re
import zipfile


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DEFAULT_BIN_DIR = os.path.join(ROOT, "out", "build", "x64-Release", "bin")
CONSTANTS_PATH = os.path.join(ROOT, "src", "core", "constants.h")
SKIP_DIRECTORY_NAMES = {"logs"}


def read_app_version(constants_path: str) -> str:
   if not os.path.isfile(constants_path):
      return "unknown"

   with open(constants_path, "r", encoding="utf-8") as constants_file:
      contents = constants_file.read()

   match = re.search(
      r'inline\s+constexpr\s+std::string_view\s+AppVersion\s*=\s*"([^"]+)"',
      contents,
   )
   if match is None:
      return "unknown"

   return match.group(1)


def should_skip(relative_path: str) -> bool:
   parts = relative_path.replace("\\", "/").split("/")
   if not parts:
      return True

   return parts[0] in SKIP_DIRECTORY_NAMES


def pack_release(bin_dir: str, output_zip: str) -> int:
   if not os.path.isdir(bin_dir):
      print(f"Bin directory not found: {bin_dir}")
      return 1

   output_directory = os.path.dirname(output_zip)
   if output_directory:
      os.makedirs(output_directory, exist_ok=True)

   file_count = 0
   with zipfile.ZipFile(output_zip, "w", compression=zipfile.ZIP_DEFLATED) as archive:
      for root_directory, directory_names, file_names in os.walk(bin_dir):
         relative_root = os.path.relpath(root_directory, bin_dir)
         if relative_root == ".":
            relative_root = ""

         directory_names[:] = [
            name
            for name in directory_names
            if name not in SKIP_DIRECTORY_NAMES
            and not should_skip(os.path.join(relative_root, name) if relative_root else name)
         ]

         for file_name in file_names:
            absolute_path = os.path.join(root_directory, file_name)
            if relative_root:
               archive_name = os.path.join(relative_root, file_name)
            else:
               archive_name = file_name

            archive_name = archive_name.replace("\\", "/")
            if should_skip(archive_name):
               continue

            archive.write(absolute_path, arcname=archive_name)
            file_count += 1

   print(f"Wrote {output_zip} ({file_count} files)")
   return 0


def main() -> int:
   parser = argparse.ArgumentParser(
      description="Zip the Release bin folder (without logs) into the repo root."
   )
   parser.add_argument(
      "--bin-dir",
      default=DEFAULT_BIN_DIR,
      help=f"Source directory (default: {DEFAULT_BIN_DIR})",
   )
   parser.add_argument(
      "--output",
      default="",
      help="Output zip path (default: MiniDB-<version>-windows-x64.zip in repo root)",
   )
   arguments = parser.parse_args()

   version = read_app_version(CONSTANTS_PATH)
   output_zip = arguments.output
   if not output_zip:
      output_zip = os.path.join(ROOT, f"MiniDB-{version}-windows-x64.zip")
   elif not os.path.isabs(output_zip):
      output_zip = os.path.join(ROOT, output_zip)

   return pack_release(arguments.bin_dir, output_zip)


if __name__ == "__main__":
   raise SystemExit(main())
