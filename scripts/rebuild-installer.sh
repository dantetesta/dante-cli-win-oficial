#!/usr/bin/env bash
# Recompila o bootstrap .exe (mingw-w64 cross) e o instalador NSIS.
# Usado após bump-version.sh.

set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

VERSION="$(awk -F'"' '/^set\(DANTE_VERSION /{print $2; exit}' cmake/DanteVersion.cmake)"
echo "Building installer for version ${VERSION}"

# 1. Cross-compile bootstrap .exe
(
    cd installer/bootstrap
    x86_64-w64-mingw32-windres dante_cli_bootstrap.rc -O coff -o bootstrap.res.o
    x86_64-w64-mingw32-gcc \
        -O2 -s \
        -municode \
        -mwindows \
        -DUNICODE -D_UNICODE \
        -Wall -Wextra \
        -o "Dante CLI.exe" \
        dante_app.c \
        bootstrap.res.o \
        -static -static-libgcc \
        -lcomctl32 -lshell32 -luser32 -lgdi32 -lkernel32 -lpsapi -lmsimg32 -lole32 -lwinhttp -lwinmm
)
echo "  bootstrap built: $(/bin/ls -lh "installer/bootstrap/Dante CLI.exe" | awk '{print $5}')"

# 2. Stage dist/Release
rm -rf dist/Release
mkdir -p dist/Release dist/installer
cp "installer/bootstrap/Dante CLI.exe" dist/Release/
cp README.md INSTALL.md LICENSE.txt dist/Release/

# 3. Regenerate wizard images (if mascot changed)
python3 scripts/make-wizard-images.py >/dev/null

# 4. Compile NSIS installer (clean old ones first)
rm -f dist/installer/*.exe
(
    cd installer/nsis
    makensis -V2 -DDIST_DIR=../../dist/Release -DOUT_DIR=../../dist/installer dante-cli.nsi >/dev/null
)

echo
echo "DONE:"
/bin/ls -lh dist/installer/
file dist/installer/*.exe
