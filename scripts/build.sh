#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source "$HOME/.cargo/env"
arch="$(uname -m)"
case "$arch" in aarch64|x86_64) ;; *) echo "Unsupported architecture: $arch" >&2; exit 2 ;; esac
php_config="${PHP_CONFIG:-php-config}"
php_api="$("$php_config" --phpapi)"
cargo build --release
mkdir -p "dist/linux-$arch/php-$php_api"
install -m 755 target/release/libphp_pdo_ords.so "dist/linux-$arch/php-$php_api/php_pdo_ords.so"
file "dist/linux-$arch/php-$php_api/php_pdo_ords.so"
