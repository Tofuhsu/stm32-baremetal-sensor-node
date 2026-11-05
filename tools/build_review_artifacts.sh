#!/usr/bin/env bash
# Build on the user's Mac/Linux ARM GCC host; collect exact binary + disassembly.
set -euo pipefail
firmware_repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
for tool in arm-none-eabi-gcc arm-none-eabi-objcopy arm-none-eabi-objdump; do
    command -v "$tool" >/dev/null || { echo "missing tool: $tool" >&2; exit 2; }
done
make -C "$firmware_repo" clean
make -C "$firmware_repo"
arm-none-eabi-objdump -d "$firmware_repo/build/firmware.elf" > "$firmware_repo/build/firmware.disasm"
python3 - "$firmware_repo" <<'PY'
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import zipfile
root=Path(sys.argv[1])
files=['build/firmware.elf','build/firmware.bin','build/firmware.map','build/firmware.disasm']
metadata={'compiler':subprocess.check_output(['arm-none-eabi-gcc','--version'],text=True).splitlines()[0],
          'files':{f:hashlib.sha256((root/f).read_bytes()).hexdigest() for f in files},
          'source':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((root/'src').glob('*')) if p.is_file()}}
(root/'build/review_metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
with zipfile.ZipFile(root/'build/firmware-review-artifacts.zip','w',zipfile.ZIP_DEFLATED) as archive:
    for name in files+['build/review_metadata.json']:
        archive.write(root/name,Path(name).name)
print(root/'build/firmware-review-artifacts.zip')
PY
