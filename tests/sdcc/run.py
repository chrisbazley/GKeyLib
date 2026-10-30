"""Check GKeyLib's larger static-data region, then use the shared test runner."""
import re
import runpy
import sys
from pathlib import Path

runner, simulator, image_name = sys.argv[1:]

for line in Path(image_name).with_suffix(".map").read_text().splitlines():
    match = re.match(
        r"^(_CODE|_HOME|_GSINIT|_GSFINAL|_INITIALIZER|_DATA|_INITIALIZED)"
        r"\s+([0-9A-F]+)\s+([0-9A-F]+)", line)
    if not match:
        continue

    area, address, size = match.groups()
    start, length = int(address, 16), int(size, 16)
    if not length:
        continue

    if area in ("_DATA", "_INITIALIZED"):
        if start < 0xc000 or start + length > 0xdf00:
            raise SystemExit(f"{area} exceeds GKeyLib's static-data region")
    elif start + length > 0xc000:
        raise SystemExit(f"{area} overlaps GKeyLib's static-data region")

sys.argv = [runner, simulator, image_name]
runpy.run_path(runner, run_name="__main__")
