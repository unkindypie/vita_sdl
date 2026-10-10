#!/bin/sh
# Official Linux x86_64 Vita3K 0.2.1 / 4154-b75d5e62 (2026-10-09).
# Asset IDs and SHA-256 must be updated together after emulator validation.
set -eu
binary=$1
managed_binary=$2
checksum=ec24d71a6c75db3eb1a03ceef20ca0f241cac59f28669dd5315172713c054d35
asset=https://api.github.com/repos/Vita3K/Vita3K/releases/assets/625596737

fail() { printf '%s\n' "$*" >&2; exit 1; }

installed_checksum=$(cat "$(dirname "$managed_binary")/.archive-sha256" 2>/dev/null || true)
if [ "$binary" = "$managed_binary" ] && { [ ! -x "$binary" ] || [ "$installed_checksum" != "$checksum" ]; }; then
    [ "$(uname -s)/$(uname -m)" = Linux/x86_64 ] ||
        fail 'Automatic Vita3K setup supports Linux x86_64 only. Set VITA3K=/path/to/your/Vita3K.'
    for tool in curl unzip sha256sum mktemp; do
        command -v "$tool" >/dev/null || fail "Vita3K setup requires $tool; see docs/VITA3K.md."
    done
    parent=$(dirname "$(dirname "$managed_binary")")
    mkdir -p "$parent"
    staging=$(mktemp -d "$parent/vita3k-download.XXXXXX")
    trap 'rm -f "$staging/emulator.zip"; rmdir "$staging" 2>/dev/null || true' EXIT
    printf '%s\n' 'Downloading pinned official Vita3K build 4154-b75d5e62...'
    curl --fail --location --retry 3 --connect-timeout 30 --max-time 600 \
        -H 'Accept: application/octet-stream' "$asset" -o "$staging/emulator.zip" ||
        fail 'Pinned Vita3K download failed. Retry or use VITA3K=/path/to/Vita3K; do not substitute an unchecked latest build.'
    printf '%s  %s\n' "$checksum" "$staging/emulator.zip" | sha256sum -c - ||
        fail 'Vita3K checksum mismatch; refusing to install.'
    # Extract only after authentication; keep archive resources next to executable.
    install_dir=$(dirname "$managed_binary")
    mkdir -p "$install_dir"
    unzip -q -o "$staging/emulator.zip" -d "$install_dir"
    chmod +x "$managed_binary"
    printf '%s\n' "$checksum" > "$install_dir/.archive-sha256"
fi

resolved=$(command -v "$binary") || fail "Vita3K executable not found: $binary"
[ -x "$resolved" ] || fail "Vita3K is not executable: $resolved"
command -v ldd >/dev/null || fail 'Vita3K host dependency check requires ldd (Linux).'
dependencies=$(ldd "$resolved" 2>&1) || fail "Cannot check Vita3K libraries: $dependencies"
case "$dependencies" in
    *'not found'*) printf '%s\n' "$dependencies" >&2
        fail 'Missing Vita3K host libraries. Install the packages listed in docs/VITA3K.md.' ;;
esac
printf 'Vita3K ready: %s\n' "$resolved"
