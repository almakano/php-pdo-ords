#!/usr/bin/env bash
set -euo pipefail
[[ "$(uname -m)" == aarch64 ]] || { echo 'Run on a native ARM64 Linux host with matching PHP headers' >&2; exit 2; }
exec "$(dirname "$0")/build.sh"
