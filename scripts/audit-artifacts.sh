#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

NRO_PATH="dist/sd/switch/KaiLocale/KaiLocale.nro"
ELF_PATH="build/KaiLocale.elf"
MAP_PATH="build/KaiLocale.map"

echo "[audit] Checking canonical SD layout..."
test -f "$NRO_PATH" || { echo "FAIL: missing $NRO_PATH" >&2; exit 1; }
NRO_COUNT="$(find dist/sd -type f -name '*.nro' | wc -l | tr -d ' ')"
if [ "$NRO_COUNT" != "1" ]; then
    echo "FAIL: expected exactly 1 .nro under dist/sd, found $NRO_COUNT" >&2
    exit 1
fi

echo "[audit] Checking unstripped ELF symbols & safety..."
test -f "$ELF_PATH" || { echo "FAIL: missing $ELF_PATH" >&2; exit 1; }
test -f "$MAP_PATH" || { echo "FAIL: missing $MAP_PATH" >&2; exit 1; }

echo "[audit] Checking linker map for Switch 2 libounce and zero Switch 1 / OunceVK libs..."
if ! grep -Eq '/libounce/lib/libnx\.a' "$MAP_PATH"; then
    echo "FAIL: linker map does not link /libounce/lib/libnx.a" >&2
    exit 1
fi
FORBIDDEN_MAP_RE='(/opt/devkitpro/libnx/|libdeko3d|libnvtegra|libnvidia|libnvdrv|portlibs/switch(2)?/lib/lib(EGL|GLESv2|glapi|drm_nouveau|vulkan|ouncevk)|libouncevk)'
if grep -Eq "$FORBIDDEN_MAP_RE" "$MAP_PATH"; then
    echo "FAIL: forbidden Switch 1 or OunceVK library detected in $MAP_PATH" >&2
    grep -En "$FORBIDDEN_MAP_RE" "$MAP_PATH" >&2 || true
    exit 1
fi

echo "[audit] Checking single build configuration & provenance manifest..."
test ! -f "CMakeLists.txt" || { echo "FAIL: competing CMakeLists.txt present" >&2; exit 1; }
test -s "README.md" || { echo "FAIL: missing or empty README.md" >&2; exit 1; }
python3 - << 'PY'
import json

with open("UPSTREAM.json", "r", encoding="utf-8") as f:
    raw = f.read()
assert "Ticket #" not in raw, "Dev-phase 'Ticket #' text left in UPSTREAM.json"
d = json.loads(raw)
assert d["sdk_packages"]["libounce"]["version"] == "20261123-1"
assert d["sdk_packages"]["libkaihen"]["version"] == "20261123-1"
assert d["container"]["digest"].startswith("sha256:")
PY

echo "[audit] Checking LICENSE, LICENSES/, and THIRD_PARTY_NOTICES.md references..."
python3 - << 'PY'
import os
import re

assert os.path.isfile("LICENSE") and os.path.getsize("LICENSE") > 0, "Missing or empty LICENSE"
assert os.path.isfile("THIRD_PARTY_NOTICES.md") and os.path.getsize("THIRD_PARTY_NOTICES.md") > 0, \
    "Missing or empty THIRD_PARTY_NOTICES.md"

with open("THIRD_PARTY_NOTICES.md", "r", encoding="utf-8") as f:
    notices = f.read()

refs = sorted(set(re.findall(r"(?:LICENSES|assets)/[A-Za-z0-9_.\-/]+", notices)))
license_refs = [r for r in refs if r.startswith("LICENSES/")]
assert license_refs, "THIRD_PARTY_NOTICES.md contains no LICENSES/ references"

for path in refs:
    assert os.path.isfile(path) and os.path.getsize(path) > 0, \
        f"Referenced notice file missing or empty: {path}"

bundled_licenses = sorted(
    f"LICENSES/{name}" for name in os.listdir("LICENSES")
    if os.path.isfile(os.path.join("LICENSES", name))
)
assert bundled_licenses == sorted(license_refs), (
    f"Mismatch between LICENSES/ files {bundled_licenses} and referenced {sorted(license_refs)}"
)
PY

echo "[audit] Checking repo/dist for proprietary Nintendo files, secrets, or personal identifiers..."
PROPRIETARY_FILES="$(find . -maxdepth 5 -not -path './.git/*' \( -iname '*.bfttf' -o -iname 'switch_font*.ttf' -o -iname 'keymap_*.ttf' -o -iname '*.xci' -o -iname '*.nsp' -o -iname '*.nca' -o -iname 'prod.keys' -o -iname 'title.keys' -o -iname 'hbmenu.nro' \))"
if [ -n "$PROPRIETARY_FILES" ]; then
    echo "FAIL: proprietary or out-of-scope file detected:" >&2
    echo "$PROPRIETARY_FILES" >&2
    exit 1
fi
python3 - << 'PY'
import os
import re
import subprocess

SECRET_RE = re.compile(
    r"(/Users/[A-Za-z0-9._-]+|/home/[A-Za-z0-9._-]+|ghp_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|AKIA[0-9A-Z]{16}|-----BEGIN [A-Z ]*PRIVATE KEY-----)"
)

if os.path.isdir(".git"):
    files = subprocess.check_output(["git", "ls-files", "-z"], text=False).split(b"\x00")
    paths = [p.decode("utf-8", "replace") for p in files if p]
else:
    paths = []
    for root, dirs, filenames in os.walk("."):
        dirs[:] = [d for d in dirs if d not in (".git", "build", "build-host", "dist")]
        for fn in filenames:
            paths.append(os.path.join(root, fn))

for path in paths:
    if not os.path.isfile(path):
        continue
    with open(path, "rb") as f:
        data = f.read()
    if b"\x00" in data:
        continue
    text = data.decode("utf-8", "replace")
    # Allow documenting this very regex in scripts/audit-artifacts.sh and scripts/package-release.sh
    if path in ("scripts/audit-artifacts.sh", "scripts/package-release.sh"):
        continue
    m = SECRET_RE.search(text)
    assert not m, f"Prohibited secret or personal path pattern {m.group(0)!r} found in {path}"
PY

echo "[audit] Auditing ELF sections, NRO0/OUN0/ASET/NACP, and embedded 256x256 baseline JPEG icon..."
python3 - "$NRO_PATH" "$ELF_PATH" "$MAP_PATH" << 'PY'
import struct
import sys

nro_path, elf_path, map_path = sys.argv[1:4]

# 1. Verify ELF64 retains .debug_info and .symtab and has no network/write symbols
with open(elf_path, "rb") as f:
    elf = f.read()
assert elf[:4] == b"\x7fELF" and elf[4] == 2 and elf[5] == 1, "Invalid 64-bit little-endian ELF"
e_shoff = struct.unpack_from("<Q", elf, 40)[0]
e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", elf, 58)
sh_hdrs = [struct.unpack_from("<IIQQQQIIQQ", elf, e_shoff + i * e_shentsize) for i in range(e_shnum)]
shstr_off, shstr_size = sh_hdrs[e_shstrndx][4], sh_hdrs[e_shstrndx][5]
shstrtab = elf[shstr_off:shstr_off + shstr_size]

def read_cstr(blob, off):
    end = blob.find(b"\x00", off)
    return blob[off:end if end != -1 else len(blob)].decode("utf-8", "replace")

section_names = {read_cstr(shstrtab, h[0]) for h in sh_hdrs}
for required_sec in (".debug_info", ".symtab"):
    assert required_sec in section_names, f"ELF missing {required_sec}"

# Verify no forbidden telemetry/network, Switch 1 conversion, or unrelated settings-write symbols in ELF .strtab
strtab_data = b""
for h in sh_hdrs:
    if read_cstr(shstrtab, h[0]) == ".strtab":
        strtab_data = elf[h[4]:h[4] + h[5]]
        break
for forbidden_sym in (
    b"socketInitialize",
    b"curl_easy_",
    b"setMakeLanguage",
    b"setMakeLanguageCode",
    b"setsysSetRegionCode",
    b"setsysSetSerialNumber",
    b"setsysSetDeviceNickname",
):
    assert forbidden_sym not in strtab_data, f"Forbidden symbol present in ELF: {forbidden_sym!r}"

# 2. Parse NRO0 header
with open(nro_path, "rb") as f:
    nro = f.read()

assert len(nro) >= 0x80, "NRO too small"
unused, mod_offset = struct.unpack_from("<II", nro, 0)
magic, unk1, nro_size, unk2 = struct.unpack_from("<IIII", nro, 0x10)
assert magic == 0x304F524E, f"Bad NRO magic: {hex(magic)}"
assert len(nro) > nro_size, f"Missing ASET section (len={len(nro)}, nro_size={nro_size})"

# 3. Verify MOD0 and OUN0 (NroOunceInfo) in first 0x1000 bytes
assert mod_offset + 0x20 <= min(len(nro), 0x1000), f"Unexpected mod_offset: {hex(mod_offset)}"
assert nro[mod_offset:mod_offset + 4] == b"MOD0", "Missing MOD0 magic at mod_offset"
oun_idx = nro[:0x1000].find(b"OUN0")
assert oun_idx != -1, "Missing OUN0 (NroOunceInfo) in first 0x1000 bytes"
oun_magic, oun_platform, oun_version = struct.unpack_from("<III", nro, oun_idx)
assert oun_magic == 0x304E554F
assert oun_platform == 2, f"Expected NroPlatform_Switch2 (2), got {oun_platform}"
assert oun_version == 1, f"Expected NroOunceInfo version 1, got {oun_version}"

# 4. Verify ASET header
aset = nro[nro_size:]
aset_magic, aset_ver, icon_off, icon_size, nacp_off, nacp_size, romfs_off, romfs_size = struct.unpack_from("<IIQQQQQQ", aset, 0)
assert aset_magic == 0x54455341, f"Bad ASET magic: {hex(aset_magic)}"
assert aset_ver == 0, f"Unexpected ASET version: {aset_ver}"
assert icon_size > 0 and icon_off + icon_size <= len(aset), "Invalid ASET icon section"
assert nacp_size >= 0x4000 and nacp_off + nacp_size <= len(aset), "Invalid ASET NACP section"

# 5. Verify NACP metadata (entry 0 title & author + display_version at 0x3060)
nacp = aset[nacp_off:nacp_off + nacp_size]
title = nacp[0x000:0x200].split(b"\x00", 1)[0].decode("utf-8")
author = nacp[0x200:0x300].split(b"\x00", 1)[0].decode("utf-8")
version = nacp[0x3060:0x3070].split(b"\x00", 1)[0].decode("utf-8")
assert title == "KaiLocale", f"Unexpected NACP title: {title!r}"
assert author == "thedavidweng", f"Unexpected NACP author: {author!r}"
assert version == "0.1.0", f"Unexpected NACP version: {version!r}"

# 6. Verify embedded icon is 256x256 baseline (SOF0) 3-component JPEG
icon = aset[icon_off:icon_off + icon_size]
assert icon[:2] == b"\xff\xd8" and icon[-2:] == b"\xff\xd9", "Embedded icon is not valid JPEG"
i = 2
sof = None
while i < len(icon) - 1:
    if icon[i] != 0xFF:
        break
    while i < len(icon) and icon[i] == 0xFF:
        i += 1
    marker = icon[i]
    i += 1
    if marker in (0xD8, 0xD9) or (0xD0 <= marker <= 0xD7):
        continue
    seg_len = struct.unpack_from(">H", icon, i)[0]
    if marker in (0xC0, 0xC1, 0xC2):
        h, w, comps = struct.unpack_from(">HHB", icon, i + 3)
        sof = (marker, w, h, comps)
        break
    i += seg_len
assert sof == (0xC0, 256, 256, 3), f"Expected 256x256 baseline RGB JPEG (SOF0), got {sof}"

print(f"[audit] PASS: NRO={len(nro)}B, ELF={len(elf)}B, OUN0(platform={oun_platform},ver={oun_version}), "
      f"NACP(title={title},author={author},version={version}), icon={sof[1]}x{sof[2]} baseline JPEG ({icon_size}B)")
PY
