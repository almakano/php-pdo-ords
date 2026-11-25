#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/mock_ords.py >/tmp/php_ords_mock.log 2>&1 &
mock_pid=$!
trap 'kill "$mock_pid" 2>/dev/null || true' EXIT
sleep 1
for spec in '8.1 20210902' '8.2 20220829' '8.3 20230831' '8.4 20240924' '8.5 20250925'; do
  read -r version api <<< "$spec"
  php_bin="php$version"
  extension="$PWD/dist/linux-aarch64/php-$api/php_pdo_ords.so"
  pdo_extension="/usr/lib/php/$api/pdo.so"
  [[ -x "$(command -v "$php_bin")" && -f "$extension" && -f "$pdo_extension" ]] || { echo "Missing PHP $version runtime or artifact" >&2; exit 2; }
  echo "PHP $version / API $api"
  "$php_bin" -n -d "extension=$pdo_extension" -d "extension=$extension" tests/driver.php
  "$php_bin" -n -d "extension=$pdo_extension" -d "extension=$extension" tests/integration.php
done
