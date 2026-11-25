#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ -f .env ]]; then set -a; source .env; set +a; fi
php_bin="${PHP_BIN:-php}"
if "$php_bin" -r 'exit(in_array("ords", PDO::getAvailableDrivers(), true) ? 0 : 1);'; then
  "$php_bin" "${LIVE_ORDS_TEST:-tests/live-ords.php}"
else
  "$php_bin" -d extension="${PDO_ORDS_EXTENSION:-$(php-config --extension-dir)/php_pdo_ords.so}" "${LIVE_ORDS_TEST:-tests/live-ords.php}"
fi
