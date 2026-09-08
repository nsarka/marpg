#!/usr/bin/env bash
set -euo pipefail

if (( $# != 0 )); then
    printf 'No command-line arguments are supported. Edit server.toml instead.\n' >&2
    exit 1
fi

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

if [[ ! -x "$project_dir/build/server" ]]; then
    printf 'server executable not found. Build the game first:\n  cmake -S "%s" -B "%s/build"\n  cmake --build "%s/build" --target server\n' "$project_dir" "$project_dir" "$project_dir" >&2
    exit 1
fi

# Asset paths are relative to the build directory.
cd -- "$project_dir/build"
exec ./server
