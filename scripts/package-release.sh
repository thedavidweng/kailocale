#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

NRO_PATH="dist/sd/switch/KaiLocale/KaiLocale.nro"
ELF_PATH="build/KaiLocale.elf"
MAP_PATH="build/KaiLocale.map"
RELEASE_DIR="dist/release"
DEBUG_DIR="dist/debug"

./scripts/audit-artifacts.sh

VERSION="$(python3 -c 'import json; print(json.load(open("UPSTREAM.json"))["version"])')"
SD_ZIP_NAME="KaiLocale-v${VERSION}-sd.zip"
DEBUG_ELF_NAME="KaiLocale-v${VERSION}.elf"

rm -rf "$RELEASE_DIR" "$DEBUG_DIR"
mkdir -p "$RELEASE_DIR" "$DEBUG_DIR"

cp -f "$NRO_PATH" "$RELEASE_DIR/KaiLocale.nro"
cp -f "$ELF_PATH" "$DEBUG_DIR/$DEBUG_ELF_NAME"

python3 - "$RELEASE_DIR/KaiLocale.nro" "$RELEASE_DIR/$SD_ZIP_NAME" "$DEBUG_DIR/$DEBUG_ELF_NAME" "$RELEASE_DIR/BUILDINFO.json" "$VERSION" << 'PY'
import datetime
import hashlib
import json
import os
import re
import subprocess
import sys
import zipfile

nro_path, zip_path, elf_path, buildinfo_path, version = sys.argv[1:6]

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()

source_commit = "unknown"
commit_ts = 1795651200  # fallback UTC epoch if .git absent
commit_iso = None
source_dirty = None
if os.path.isdir(".git"):
    source_commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    commit_ts = int(subprocess.check_output(["git", "show", "-s", "--format=%ct", "HEAD"], text=True).strip())
    commit_iso = subprocess.check_output(["git", "show", "-s", "--format=%cI", "HEAD"], text=True).strip()
    dirty_out = subprocess.check_output(["git", "status", "--porcelain"], text=True).strip()
    source_dirty = bool(dirty_out)

dt = datetime.datetime.fromtimestamp(commit_ts, tz=datetime.timezone.utc)
zip_dt = (max(1980, dt.year), dt.month, dt.day, dt.hour, dt.minute, dt.second)

# Package canonical SD archive containing strictly switch/KaiLocale/KaiLocale.nro
with open(nro_path, "rb") as f:
    nro_bytes = f.read()

with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
    zi = zipfile.ZipInfo("switch/KaiLocale/KaiLocale.nro", date_time=zip_dt)
    zi.create_system = 3  # Unix
    zi.external_attr = 0o644 << 16
    zi.compress_type = zipfile.ZIP_DEFLATED
    zf.writestr(zi, nro_bytes)

# Verify archive structure and byte equality
with zipfile.ZipFile(zip_path, "r") as zf:
    names = zf.namelist()
    assert names == ["switch/KaiLocale/KaiLocale.nro"], f"Unexpected SD zip layout: {names}"
    assert zf.read("switch/KaiLocale/KaiLocale.nro") == nro_bytes, "SD zip NRO payload mismatch"

with open("UPSTREAM.json", "r", encoding="utf-8") as f:
    upstream = json.load(f)

sdk_packages = {}
for name in ("libounce", "libkaihen"):
    pkg = upstream["sdk_packages"][name]
    sdk_packages[name] = {
        "repo": pkg["repo"],
        "version": pkg["version"],
        "sha256": pkg["sha256"],
        "installed": pkg["installed"],
        "linked": pkg["linked"],
    }

portlibs = {}
for name, pkg in upstream["portlib_packages"].items():
    portlibs[name] = {
        "repo": pkg["repo"],
        "version": pkg["version"],
        "sha256": pkg["sha256"],
    }

buildinfo = {
    "schema_version": 1,
    "product": "KaiLocale",
    "version": version,
    "source_commit": source_commit,
    "commit_timestamp": commit_iso,
    "source_dirty": source_dirty,
    "build_environment": {
        "container_image": upstream["container"]["image"],
        "container_digest": upstream["container"]["digest"],
        "platform_manifests": upstream["container"]["platform_manifests"],
        "target_arch": "aarch64-none-elf",
        "switch2_platform": "OUN0 (platform=2, version=1)",
    },
    "sdk_packages": sdk_packages,
    "portlib_packages": portlibs,
    "artifacts": {
        "KaiLocale.nro": {
            "size_bytes": os.path.getsize(nro_path),
            "sha256": sha256_file(nro_path),
        },
        os.path.basename(zip_path): {
            "size_bytes": os.path.getsize(zip_path),
            "sha256": sha256_file(zip_path),
            "entries": ["switch/KaiLocale/KaiLocale.nro"],
        },
    },
    "debug_symbols": {
        "filename": os.path.basename(elf_path),
        "retained_separately": True,
        "size_bytes": os.path.getsize(elf_path),
        "sha256": sha256_file(elf_path),
    },
}

serialized = json.dumps(buildinfo, indent=2) + "\n"
for pattern in (r"/Users/", r"/home/", r"ghp_", r"github_pat_", r"AKIA[0-9A-Z]{16}", r"BEGIN .* PRIVATE KEY"):
    assert not re.search(pattern, serialized), f"Sanitization check failed on BUILDINFO.json for pattern {pattern}"

with open(buildinfo_path, "w", encoding="utf-8") as f:
    f.write(serialized)
PY

(
    cd "$RELEASE_DIR"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum BUILDINFO.json "$SD_ZIP_NAME" KaiLocale.nro > SHA256SUMS
        sha256sum -c SHA256SUMS
    else
        shasum -a 256 BUILDINFO.json "$SD_ZIP_NAME" KaiLocale.nro > SHA256SUMS
        shasum -a 256 -c SHA256SUMS
    fi
)

echo "[package] Release artifacts staged in $RELEASE_DIR:"
ls -lh "$RELEASE_DIR"
echo "[package] Separate maintainer debug symbols staged in $DEBUG_DIR:"
ls -lh "$DEBUG_DIR"
