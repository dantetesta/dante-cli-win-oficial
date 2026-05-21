#!/usr/bin/env bash
# Dante CLI — version bumper.
#
# Usage:
#   ./scripts/bump-version.sh patch   # default — 1.0.0 → 1.0.1
#   ./scripts/bump-version.sh minor   # 1.0.x → 1.1.0
#   ./scripts/bump-version.sh major   # 1.x.y → 2.0.0
#   ./scripts/bump-version.sh set 1.2.3
#
# Atualiza atomicamente TODOS os arquivos que carregam versão.
# A fonte da verdade são as 3 linhas DANTE_VERSION_{MAJOR,MINOR,PATCH}
# de cmake/DanteVersion.cmake.

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

read_components() {
    python3 - <<'PY'
import re, pathlib, sys
src = pathlib.Path("cmake/DanteVersion.cmake").read_text()
def grab(key):
    m = re.search(rf"^set\({key}\s+(\d+)\)", src, re.M)
    if not m:
        sys.exit(f"cannot parse {key} from DanteVersion.cmake")
    return m.group(1)
print(grab("DANTE_VERSION_MAJOR"))
print(grab("DANTE_VERSION_MINOR"))
print(grab("DANTE_VERSION_PATCH"))
PY
}

comps_str="$(read_components)"
MAJ="$(printf '%s\n' "$comps_str" | sed -n '1p')"
MIN="$(printf '%s\n' "$comps_str" | sed -n '2p')"
PAT="$(printf '%s\n' "$comps_str" | sed -n '3p')"
current="${MAJ}.${MIN}.${PAT}"

mode="${1:-patch}"
case "$mode" in
    patch) PAT=$((PAT + 1)) ;;
    minor) MIN=$((MIN + 1)); PAT=0 ;;
    major) MAJ=$((MAJ + 1)); MIN=0; PAT=0 ;;
    set)
        [[ $# -ge 2 ]] || { echo "usage: bump-version.sh set X.Y.Z" >&2; exit 2; }
        IFS='.' read -r MAJ MIN PAT <<<"$2"
        ;;
    *)
        echo "usage: bump-version.sh [patch|minor|major|set X.Y.Z]" >&2
        exit 2 ;;
esac

new="${MAJ}.${MIN}.${PAT}"
new_quad_dot="${MAJ}.${MIN}.${PAT}.0"
new_quad_comma="${MAJ},${MIN},${PAT},0"

echo "Bumping ${current} → ${new}"

MAJ="$MAJ" MIN="$MIN" PAT="$PAT" \
NEW="$new" QD="$new_quad_dot" QC="$new_quad_comma" \
OLD="$current" \
python3 <<'PY'
import os, re, pathlib

MAJ = os.environ["MAJ"]
MIN = os.environ["MIN"]
PAT = os.environ["PAT"]
NEW = os.environ["NEW"]
QD  = os.environ["QD"]
QC  = os.environ["QC"]
OLD = os.environ["OLD"]

def edit(path, transforms):
    p = pathlib.Path(path)
    if not p.exists():
        print(f"  skip {path} (missing)")
        return
    s = p.read_text(encoding="utf-8")
    original = s
    for pattern, replacement, flags in transforms:
        s = re.sub(pattern, replacement, s, flags=flags)
    if s != original:
        p.write_text(s, encoding="utf-8")
        print(f"  edited {path}")

edit("cmake/DanteVersion.cmake", [
    (r"^set\(DANTE_VERSION_MAJOR\s+\d+\)", f"set(DANTE_VERSION_MAJOR {MAJ})", re.M),
    (r"^set\(DANTE_VERSION_MINOR\s+\d+\)", f"set(DANTE_VERSION_MINOR {MIN})", re.M),
    (r"^set\(DANTE_VERSION_PATCH\s+\d+\)", f"set(DANTE_VERSION_PATCH {PAT})", re.M),
])

edit("installer/inno/dante-cli.iss", [
    (r'#define MyAppVersion\s+"[^"]+"', f'#define MyAppVersion     "{NEW}"', 0),
])

edit("installer/nsis/dante-cli.nsi", [
    (r'!define APPVERSION\s+"[^"]+"',                f'!define APPVERSION         "{NEW}"', 0),
    (r'VIProductVersion "[^"]+"',                    f'VIProductVersion "{QD}"', 0),
    (r'VIAddVersionKey "FileVersion"\s+"[^"]+"',     f'VIAddVersionKey "FileVersion" "{QD}"', 0),
    (r'VIAddVersionKey "ProductVersion"\s+"[^"]+"',  f'VIAddVersionKey "ProductVersion" "{NEW}"', 0),
])

# dante_app.c uses APP_VERSION_W (the active source file)
edit("installer/bootstrap/dante_app.c", [
    (r'#define APP_VERSION_W\s+L"[^"]+"', f'#define APP_VERSION_W   L"{NEW}"', 0),
])

edit("installer/bootstrap/dante_cli_bootstrap.rc", [
    (r' FILEVERSION \d+,\d+,\d+,\d+',                f' FILEVERSION {QC}', 0),
    (r' PRODUCTVERSION \d+,\d+,\d+,\d+',             f' PRODUCTVERSION {QC}', 0),
    (r'VALUE "FileVersion",\s+"[^"]+"',              f'VALUE "FileVersion",      "{QD}"', 0),
    (r'VALUE "ProductVersion",\s+"[^"]+"',           f'VALUE "ProductVersion",   "{QD}"', 0),
])

edit("src/app/app.rc", [
    (r' FILEVERSION \d+,\d+,\d+,\d+',                f' FILEVERSION {QC}', 0),
    (r' PRODUCTVERSION \d+,\d+,\d+,\d+',             f' PRODUCTVERSION {QC}', 0),
    (r'VALUE "FileVersion",\s+"[^"]+"',              f'VALUE "FileVersion",      "{QD}"', 0),
    (r'VALUE "ProductVersion",\s+"[^"]+"',           f'VALUE "ProductVersion",   "{QD}"', 0),
])

edit("installer/bootstrap/dante_cli_bootstrap.manifest", [
    (r'<assemblyIdentity version="\d+\.\d+\.\d+\.\d+"', f'<assemblyIdentity version="{QD}"', 0),
])

old_escaped = re.escape(OLD)
for f in ["README.md", "INSTALL.md", "ENTREGAVEIS.md"]:
    edit(f, [
        (rf"DanteCLI-Setup-{old_escaped}-x64\.exe",  f"DanteCLI-Setup-{NEW}-x64.exe", 0),
        (rf"\b{old_escaped}-alpha\b",                 f"{NEW}-alpha", 0),
        (rf"version\s+{old_escaped}\b",               f"version {NEW}", re.I),
    ])
PY

echo
echo "OK — bumped to ${new}"
echo "Next: ./scripts/rebuild-installer.sh"
