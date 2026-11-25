# Prebuilt extension binaries

Install the artifact matching both the machine architecture and PHP module API:

- `linux-aarch64/` — Linux ARM64
- `linux-x86_64/` — Linux x86_64
- `php-<API>/` — exact PHP module API; check with `php-config --phpapi`

Each directory contains `php_pdo_ords.so`. Verify downloads with `sha256sum -c SHA256SUMS` from this directory. The x86_64 artifacts are cross-compiled and require runtime validation on an x86_64 host.
