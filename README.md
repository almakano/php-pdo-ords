# php_pdo_ords

Native PHP PDO driver for Oracle REST Data Services. The extension registers the `ords` PDO driver and uses a Rust HTTP/JSON backend. Oracle Instant Client, OCI8, `pdo_oci`, JDBC and SQL*Net are not used.

> Development status: native PDO driver with live ORDS verification. Server-side JSON binds, ORDS pagination, PL/SQL OUT/INOUT and OUT-bind `lastInsertId()` are implemented and live-tested against Autonomous Database 26ai on ARM64 PHP 8.4 and 8.5. Prebuilt binaries are included for ARM64 and x86_64 on PHP 8.1–8.5; all ARM64 versions pass the mock integration matrix, while x86_64 binaries are cross-compiled and ELF-verified (runtime testing requires an x86_64 host).

## Architecture

- `c/php_ords.c` — PHP module lifecycle and driver registration.
- `c/pdo_driver.c` — PDO database-handle callbacks, DSN, quoting, attributes.
- `c/pdo_statement.c` — PDO statement callbacks, result metadata and fetch operations.
- `src/http` — HTTP(S), TLS, timeouts and response-size enforcement.
- `src/auth` — Basic Authentication and OAuth2 Bearer tokens.
- `src/connection` — endpoint configuration and SQL URL.
- `src/result`, `src/types`, `src/errors` — ORDS response model, type mapping and errors.

The PDO ABI is implemented by the C shim against the installed PHP headers (`src/php_ffi` exposes the Rust C ABI). Rust owns the HTTP/JSON backend. This is a native PDO driver, not a PHP-level wrapper.

## Requirements

- Linux ARM64 (`aarch64`) or x86_64, glibc, Ubuntu 22.04+.
- PHP development headers matching the PHP runtime, PDO and JSON.
- Rust stable, Cargo, C compiler, `php-config`, `make`.
- ORDS REST-Enabled SQL service enabled for the target schema, and an account with the SQL Developer role.

PHP module ABI is version-specific. Build separately for each PHP API version (8.1, 8.2, 8.3, 8.4, 8.5); do not copy one `.so` across PHP minor versions.

| PHP | Zend module API | PDO driver API | Build artifacts |
|---|---:|---:|---|
| 8.1 | `20210902` | `20170320` | ARM64 mock-tested, x86_64 cross-compiled |
| 8.2 | `20220829` | `20170320` | ARM64 mock-tested, x86_64 cross-compiled |
| 8.3 | `20230831` | `20170320` | ARM64 mock-tested, x86_64 cross-compiled |
| 8.4 | `20240924` | `20240423` | ARM64 mock-tested, x86_64 cross-compiled |
| 8.5 | `20250925` | `20240423` | ARM64 mock-tested, x86_64 cross-compiled |

All ten architecture/API combinations are included under `dist/`. The five ARM64 binaries load and pass mock integration tests on their matching PHP runtimes. x86_64 binaries were cross-compiled and ELF-verified on ARM64; runtime validation still requires an x86_64 PHP host. SHA-256 checksums are stored in `dist/SHA256SUMS`.

## Install with PIE (Packagist)

The extension is packaged as `almakano/php-pdo-ords` (`php-ext`, extension name `pdo_ords`). After publishing the repository to GitHub and submitting it to Packagist, install it for the active PHP runtime with:

```sh
pie install almakano/php-pdo-ords
php --ri pdo_ords
```

PIE builds the Rust backend and PDO module from source. The target system needs PHP development headers matching the active PHP runtime, PDO/JSON, Rust stable + Cargo, a C toolchain, autoconf, and make. PIE selects the PHP API via its active `php-config`; each PHP API/architecture is built separately. The project-specific `make install` scripts remain available for direct/manual installation.

## Build and install

```sh
./scripts/build-arm64.sh     # native ARM64 host
./scripts/build-x86_64.sh    # native x86_64, or ARM64 cross-build with PHP_HEADERS_DIR
sudo ./scripts/install.sh
php -m | grep pdo_ords
php --ri pdo_ords
```

Artifacts are placed under `dist/linux-<arch>/php-<PHP_API>/php_pdo_ords.so`; prebuilt ARM64 and x86_64 binaries for PHP 8.1–8.5 are committed in `dist/`. To build/install a specific PHP minor version, select its matching `php-config`/CLI binary, e.g. `PHP_CONFIG=php-config8.4 ./scripts/build-arm64.sh` and `sudo env PHP_CONFIG=php-config8.4 PHP_BIN=php8.4 PHP_MM=8.4 make install`. For ARM64-to-x86_64 cross-builds, install `gcc-x86-64-linux-gnu`, add the Rust target `x86_64-unknown-linux-gnu`, unpack matching x86_64 PHP development headers and set `PHP_HEADERS_DIR` (and `PHP_API` if needed) before running `./scripts/build-x86_64.sh`.

```sh
sudo ./scripts/uninstall.sh
```

## Connect

```php
$pdo = new PDO(
    'ords:https://example.com/ords/hr/',
    'HR',
    $password,
    [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]
);

$stmt = $pdo->prepare('SELECT employee_id, first_name FROM employees WHERE employee_id = :id');
$stmt->bindValue(':id', 100, PDO::PARAM_INT);
$stmt->execute();
$row = $stmt->fetch(PDO::FETCH_ASSOC);
```

The DSN is the REST-enabled schema endpoint; the driver appends `/_/sql`. PDO construction verifies endpoint and credentials with a read-only `SELECT 1 FROM dual`; the driver does not retain a database session between calls. Use `PDO_ORDS_ATTR_ACCESS_TOKEN` as a driver option for an OAuth2 access token and `PDO_ORDS_ATTR_TIMEOUT_MS` for a request timeout in milliseconds:

```php
$pdo = new PDO($dsn, null, null, [
    PDO_ORDS_ATTR_ACCESS_TOKEN => $accessToken,
    PDO_ORDS_ATTR_TIMEOUT_MS => 15000,
]);
```

Username/password uses Basic Authentication. HTTPS is mandatory for non-loopback endpoints; rustls certificate validation is enabled. Plain HTTP is accepted only for local mock tests.

```php
$pdo->setAttribute(PDO_ORDS_ATTR_OFFSET, 0);
$pdo->setAttribute(PDO_ORDS_ATTR_LIMIT, 100);

$id = null;
$stmt = $pdo->prepare('BEGIN :id := 123; END;');
$stmt->bindParam(':id', $id, PDO::PARAM_INT | PDO::PARAM_INPUT_OUTPUT, 40);
$stmt->execute();
$generatedId = $pdo->lastInsertId('id');
```

## Current PDO support

| Feature | Status |
|---|---|
| Driver registration, PDO construction | Implemented |
| `prepare`, `query`, `exec`, `execute` | Implemented; mock-tested |
| Named/positional placeholders | ORDS JSON binds are sent server-side by the current source builds; live feature validation is recorded for PHP 8.4/8.5 ARM64 |
| `fetch`, `fetchAll`, `fetchColumn`, `fetchObject` | Implemented through PDO core; mock-tested |
| `FETCH_ASSOC`, `FETCH_NUM`, `FETCH_BOTH`, `FETCH_OBJ`, `FETCH_COLUMN` | Mock-tested across PHP 8.1–8.5 |
| `rowCount`, `columnCount`, `getColumnMeta` | Implemented; SELECT `rowCount` is the returned page count and is not portable PDO behavior |
| NULL, boolean, integer, float, string | Implemented and covered by mock integration tests |
| `bindParam`, `bindValue` | Server-side typed JSON binds (`NUMBER`, `BOOLEAN`, `VARCHAR2`); live feature validation is recorded for PHP 8.4/8.5 ARM64 |
| Basic Auth, OAuth2 bearer token, TLS | Implemented; Basic Auth mock-tested |
| PDO errorCode/errorInfo and PDOException | SQLSTATE mapping and driver error text implemented; mock-tested |
| Request/connect timeout, response size cap | Implemented (16 MiB response cap, 1 MiB SQL cap) |
| Pagination | `PDO_ORDS_ATTR_OFFSET` and `PDO_ORDS_ATTR_LIMIT` set ORDS request pagination; default offset `0`, limit `500` |
| PL/SQL | Named `PDO::PARAM_INPUT_OUTPUT` binds support ORDS `INOUT` and response `result` values; live feature validation is recorded for PHP 8.4/8.5 ARM64 |
| `beginTransaction`, `commit`, `rollBack` | Rejected with SQLSTATE `0A000`; no fake transaction state |
| `lastInsertId` | Returns the first named OUT/INOUT bind result from the latest statement with output binds; supports `lastInsertId('bind_name')`. Does not query sequence `CURRVAL` across stateless ORDS sessions |
| Persistent connections | Explicitly rejected |
| Full metadata, LOB streaming, multiple rowsets | Not implemented |

## ORDS limitations and transaction semantics

REST-Enabled SQL uses HTTPS POST to `/ords/<schema_alias>/_/sql`; each request maps to a new database session and auto-commits successful statements. See the [REST-Enabled SQL documentation](https://docs.oracle.com/en/database/oracle/oracle-rest-data-services/26.3/orddg/rest-enabled-sql-service.html). ORDS accepts JSON `statementText`, `binds`, `limit` and `offset`. Current builds send named and positional binds as typed JSON values and apply pagination on each prepared statement request; live feature validation is recorded for PHP 8.4/8.5 ARM64. These are per-request binds, not a persistent server-side prepared handle: ORDS still creates a new database session per HTTP request. Each ordinary request auto-commits successful statements. PDO transaction calls are therefore rejected with SQLSTATE `0A000`.

ORDS Sessionless Transactions are supported only from Oracle Database 23ai Release 23.6. They require explicit start/resume/commit/rollback calls and the returned GTRID in `x-ords-sessionless-transaction-id` on each SQL request. See the [ORDS transaction documentation](https://docs.oracle.com/en/database/oracle/oracle-rest-data-services/26.3/orddg/developing-REST-applications.html). A future backend can enable PDO transactions only after detecting and validating this capability on the server.

For `lastInsertId()`, use an OUT bind such as `INSERT ... RETURNING id INTO :id` and bind `:id` with `PDO::PARAM_INPUT_OUTPUT`; the first returned output value is cached until another output-bind execution. A sequence `CURRVAL` lookup is not possible across independent stateless ORDS requests. The adapter intentionally sends one statement per PDO call even though ORDS can process batches.

## Tests

```sh
make test  # Rust unit tests + PDO integration matrix PHP 8.1–8.5
```

The local mock validates the PDO ABI path across PHP 8.1–8.5. Advanced binds and pagination are covered by mock tests on PHP 8.4/8.5. For a real Oracle check, copy `.env.example` to `.env`, set the REST-enabled schema alias and credentials, then run `tests/run-live-ords.sh`; run `LIVE_ORDS_TEST=tests/live-ords-features.php tests/run-live-ords.sh` to verify server-side IN binds, pagination, OUT/INOUT and `lastInsertId()`. The live feature suite passed against `Almakano-AI-01` (Oracle Database 26ai) on ARM64 PHP 8.4 and 8.5. All five ARM64 binaries pass the mock integration matrix; x86_64 binaries are cross-compiled and ELF-verified but still need runtime testing on an x86_64 host.

## Diagnostics

- `php --ri pdo_ords` — module version and backend.
- `PDO::getAvailableDrivers()` — must contain `ords`.
- Ensure `pdo` and `json` are loaded before `pdo_ords`.
- Use `PDO::ERRMODE_EXCEPTION` to receive HTTP/ORDS errors as `PDOException`.
- Never place credentials or access tokens in the DSN URL or application logs.
