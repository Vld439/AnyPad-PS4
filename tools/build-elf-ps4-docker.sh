#!/bin/sh
# Builds AnyPad PS4: the official PS4 payload SDK
# mounted into a container with clang and lld.
# The ELF is dist/AnyPad-PS4-<version>.elf.
#
#   tools/build-elf-ps4-docker.sh /path/to/ps4-payload-sdk [image]
#
set -e
SDK="${1:?usage: $0 /path/to/ps4-payload-sdk [image]}"
IMAGE="${2:-anypad-build-ps4}"
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -n 's/^#define ANYPAD_VERSION "\(.*\)"/\1/p' "$PROJ/src/version.h")"
[ -n "$VERSION" ] || { echo "no version in src/version.h" >&2; exit 1; }
OUT="dist/AnyPad-PS4-$VERSION.elf"
mkdir -p "$PROJ/dist" "$PROJ/build"

BUILD_ID="$(git -C "$PROJ" describe --always --dirty --abbrev=7 2>/dev/null || echo unknown)"
printf '#define ANYPAD_BUILD_ID "%s"\n' "$BUILD_ID" > "$PROJ/build/build_id.h"

DOCKER_BIN="$(which podman 2>/dev/null || which docker 2>/dev/null)"
[ -n "$DOCKER_BIN" ] || { echo "Neither docker nor podman found in PATH" >&2; exit 1; }

"$DOCKER_BIN" run --rm --platform linux/amd64 -e OUT="$OUT" \
  -v "$SDK":/opt/ps4-payload-sdk-ro:ro -v "$PROJ":/work "$IMAGE" bash -c '
set -e
cp -r /opt/ps4-payload-sdk-ro /opt/ps4-payload-sdk
export PS4_PAYLOAD_SDK=/opt/ps4-payload-sdk
printf "#!/bin/sh\ncase \"\$1\" in --bindir) echo /usr/lib/llvm-18/bin;; --version) echo 18.1.3;; *) exit 1;; esac\n" \
  > /usr/local/bin/llvm-config
chmod +x /usr/local/bin/llvm-config
command -v make >/dev/null || { echo "the image needs make" >&2; exit 1; }
cd /work
rm -rf build/ps4
make -f ps4.mk
'
cd "$PROJ"
{ shasum -a 256 "$OUT" 2>/dev/null || sha256sum "$OUT"; } | tee "$OUT.sha256"
ls -la "$OUT"
