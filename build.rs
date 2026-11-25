use std::{env, process::Command};

fn main() {
    let php_config = env::var("PHP_CONFIG").unwrap_or_else(|_| "php-config".into());
    let includes = Command::new(&php_config)
        .arg("--includes")
        .output()
        .expect("PHP_CONFIG must point to php-config or a compatible wrapper");
    assert!(includes.status.success(), "php-config --includes failed");
    let php_api = Command::new(&php_config)
        .arg("--phpapi")
        .output()
        .expect("php-config --phpapi failed");
    assert!(php_api.status.success(), "php-config --phpapi failed");
    let php_api = String::from_utf8(php_api.stdout)
        .expect("PHP API must be UTF-8")
        .trim()
        .to_owned();
    let include_dir = Command::new(&php_config)
        .arg("--include-dir")
        .output()
        .expect("php-config --include-dir failed");
    assert!(
        include_dir.status.success(),
        "php-config --include-dir failed"
    );
    let driver_header = std::path::PathBuf::from(
        String::from_utf8(include_dir.stdout)
            .expect("include dir must be UTF-8")
            .trim(),
    )
    .join("ext/pdo/php_pdo_driver.h");
    let header = std::fs::read_to_string(&driver_header).expect("PHP PDO driver header is missing");
    let driver_api = header
        .lines()
        .find(|line| line.trim_start().starts_with("#define PDO_DRIVER_API"))
        .expect("PDO_DRIVER_API is missing from PHP headers");
    let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default();
    assert!(
        matches!(target_arch.as_str(), "aarch64" | "x86_64"),
        "Unsupported target architecture: {target_arch}"
    );
    println!("cargo:warning=PHP API {php_api}; {driver_api}; target {target_arch}");
    println!("cargo:rerun-if-env-changed=PHP_CONFIG");
    let mut build = cc::Build::new();
    build
        .define("COMPILE_DL_PDO_ORDS", "1")
        .file("c/php_ords.c")
        .file("c/pdo_driver.c")
        .file("c/pdo_statement.c")
        .flag_if_supported("-Wno-unused-parameter");
    for include in String::from_utf8(includes.stdout)
        .expect("php-config output must be UTF-8")
        .split_whitespace()
    {
        build.flag(include);
    }
    build.compile("php_ords_shim");
    println!("cargo:rerun-if-changed=c/php_ords.c");
    println!("cargo:rerun-if-changed=c/pdo_driver.c");
    println!("cargo:rerun-if-changed=c/pdo_statement.c");
    println!("cargo:rerun-if-changed=include/php_ords.h");
    println!("cargo:rustc-link-lib=static=php_ords_shim");
    println!(
        "cargo:rustc-env=PHP_CONFIG={}",
        env::var("PHP_CONFIG").unwrap_or_else(|_| "php-config".into())
    );
}
