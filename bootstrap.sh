#!/usr/bin/env bash
# Install the prerequisites for MARPG on a fresh Ubuntu 24.04 VPS.
set -euo pipefail

if (( $# != 0 )); then
    printf 'Usage: bash bootstrap.sh\n' >&2
    exit 1
fi
if [[ ! -r /etc/os-release ]]; then
    printf 'This script requires Ubuntu 24.04.\n' >&2
    exit 1
fi
# shellcheck source=/dev/null
source /etc/os-release
if [[ "${ID:-}" != ubuntu || "${VERSION_ID:-}" != 24.04 ]]; then
    printf 'This script requires Ubuntu 24.04 (found %s).\n' "${PRETTY_NAME:-unknown OS}" >&2
    exit 1
fi

privilege=()
if (( EUID != 0 )); then
    if ! command -v sudo >/dev/null 2>&1; then
        printf 'Run this script as root, or install sudo and use an administrator account.\n' >&2
        exit 1
    fi
    privilege=(sudo)
fi

# Universe contains some build tools/development packages on minimal VPS images.
"${privilege[@]}" apt-get update
"${privilege[@]}" env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates software-properties-common
"${privilege[@]}" add-apt-repository -y universe
"${privilege[@]}" apt-get update

# CMake downloads the pinned SFML 3, tmxlite and JSON sources. Do not install
# Ubuntu's libsfml-dev: the project builds its own matching SFML version.
# The common library links SFML Graphics, and configuration includes Audio,
# so even a server-only build needs their development dependencies.
"${privilege[@]}" env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkg-config git curl \
    libx11-dev libxrandr-dev libxcursor-dev libxi-dev libudev-dev \
    libgl1-mesa-dev libegl1-mesa-dev libfreetype-dev \
    libflac-dev libogg-dev libvorbis-dev

printf '\nPrerequisites installed. Run these commands from the MARPG checkout:\n'
printf '  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF\n'
printf '  cmake --build build --target server --parallel 1\n'
printf '  ./server.sh\n'
printf '\nOne build job is recommended on small VPS instances. Increase it if RAM allows.\n'
