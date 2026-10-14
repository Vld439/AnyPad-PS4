#!/bin/sh
# Builds AnyPad PS5 without installing anything: the official PS5 payload SDK
# (the release zip, unpacked) mounted read-only into a container with clang 18
# and lld 18 for linux/amd64. The ELF is dist/AnyPad-PS5-<version>.elf, with the
# version from src/version.h, and a .sha256 beside it.
#
#   tools/build-elf-docker.sh /path/to/ps5-payload-sdk [image]
#
# The default image is any Ubuntu 24.04 one with clang-18 and lld-18, e.g. one
# made from:  FROM ubuntu:24.04 / RUN apt-get update && apt-get install -y clang-18 lld-18
set -e
SDK="${1:?usage: $0 /path/to/ps5-payload-sdk [image]}"
IMAGE="${2:-anypad-build}"
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -n 's/^#define ANYPAD_VERSION "\(.*\)"/\1/p' "$PROJ/src/version.h")"
[ -n "$VERSION" ] || { echo "no version in src/version.h" >&2; exit 1; }
OUT="dist/AnyPad-PS5-$VERSION.elf"
mkdir -p "$PROJ/dist" "$PROJ/build"
# The commit it is built from, for the log's first line ("-dirty": with
# changes not committed yet): two builds of one version stay told apart.
BUILD_ID="$(git -C "$PROJ" describe --always --dirty --abbrev=7 2>/dev/null || echo unknown)"
printf '#define ANYPAD_BUILD_ID "%s"\n' "$BUILD_ID" > "$PROJ/build/build_id.h"

docker run --rm --platform linux/amd64 -e OUT="$OUT" \
  -v "$SDK":/opt/ps5-payload-sdk-ro:ro -v "$PROJ":/work "$IMAGE" bash -c '
set -e
cp -r /opt/ps5-payload-sdk-ro /opt/ps5-payload-sdk
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
# the SDK wrappers ask llvm-config where LLVM lives
printf "#!/bin/sh\ncase \"\$1\" in --bindir) echo /usr/lib/llvm-18/bin;; --version) echo 18.1.3;; *) exit 1;; esac\n" \
  > /usr/local/bin/llvm-config
chmod +x /usr/local/bin/llvm-config
CC=$PS5_PAYLOAD_SDK/bin/prospero-clang
cd /work
rm -rf build/elf && mkdir -p build/elf
OBJS=""
for f in util log crc32 smp_crypto profiles generic host le web web_page config evstream hotkey lock netinfo usb_desc hci_usb ps5_power ps5_ui ps5_apps ps5_sysinfo launcher icon_data ps5_vpad ps5_main; do
  $CC -std=c11 -Wall -Wextra -Werror -O2 -Isrc -Ibuild -c src/$f.c -o build/elf/$f.o
  OBJS="$OBJS build/elf/$f.o"
done
$CC -o "$OUT" $OBJS -lScePad -lSceUserService -lSceSystemService -lSceAppInstUtil -ldl
'
cd "$PROJ"
{ shasum -a 256 "$OUT" 2>/dev/null || sha256sum "$OUT"; } | tee "$OUT.sha256"
ls -la "$OUT"
