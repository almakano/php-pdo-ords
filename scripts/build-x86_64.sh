#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -m)" == x86_64 ]]; then exec ./scripts/build.sh; fi
[[ "$(uname -m)" == aarch64 ]] || { echo 'Build x86_64 on x86_64 or cross-build from ARM64' >&2; exit 2; }
: "${PHP_HEADERS_DIR:?Set PHP_HEADERS_DIR to extracted x86_64 PHP development headers matching the target runtime}"
: "${PHP_API:=$(php-config --phpapi)}"
for header in main/php_config.h Zend/zend_config.h ext/pdo/php_pdo_driver.h; do
  [[ -f "$PHP_HEADERS_DIR/$header" ]] || { echo "Missing target header: $PHP_HEADERS_DIR/$header" >&2; exit 2; }
done
command -v x86_64-linux-gnu-gcc >/dev/null || { echo 'Install gcc-x86-64-linux-gnu first' >&2; exit 2; }
. "$HOME/.cargo/env"
php_config_wrapper="$(mktemp)"
trap 'rm -f "$php_config_wrapper"' EXIT
cat > "$php_config_wrapper" <<WRAPPER
#!/bin/sh
case "\$1" in
  --includes) printf '%s\\n' '-I$PHP_HEADERS_DIR -I$PHP_HEADERS_DIR/main -I$PHP_HEADERS_DIR/TSRM -I$PHP_HEADERS_DIR/Zend -I$PHP_HEADERS_DIR/ext -I$PHP_HEADERS_DIR/ext/date/lib' ;;
  --phpapi) printf '%s\\n' '$PHP_API' ;;
  --include-dir) printf '%s\\n' '$PHP_HEADERS_DIR' ;;
  *) exit 2 ;;
esac
WRAPPER
chmod +x "$php_config_wrapper"
export PHP_CONFIG="$php_config_wrapper"
export CC_x86_64_unknown_linux_gnu=x86_64-linux-gnu-gcc
export AR_x86_64_unknown_linux_gnu=x86_64-linux-gnu-ar
export CARGO_TARGET_X86_64_UNKNOWN_LINUX_GNU_LINKER=x86_64-linux-gnu-gcc
cargo build --release --target x86_64-unknown-linux-gnu
mkdir -p "dist/linux-x86_64/php-$PHP_API"
install -m 755 target/x86_64-unknown-linux-gnu/release/libphp_pdo_ords.so "dist/linux-x86_64/php-$PHP_API/php_pdo_ords.so"
file "dist/linux-x86_64/php-$PHP_API/php_pdo_ords.so"
