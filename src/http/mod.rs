use crate::{auth, connection::ConnectionConfig};
use reqwest::blocking::Client;
use serde_json::json;
use std::{io::Read, time::Duration};
use zeroize::Zeroizing;
const MAX_RESPONSE_BYTES: u64 = 16 * 1024 * 1024;
pub fn execute(
    endpoint: &str,
    username: Zeroizing<String>,
    password: Zeroizing<String>,
    token: Zeroizing<String>,
    sql: &str,
    timeout_ms: i64,
) -> Result<String, String> {
    let parsed = reqwest::Url::parse(endpoint).map_err(|_| "Invalid ORDS endpoint URL")?;
    if parsed.query().is_some() || parsed.fragment().is_some() {
        return Err("ORDS endpoint must not contain a query string or fragment".into());
    }
    if !parsed.username().is_empty() || parsed.password().is_some() {
        return Err(
            "Credentials must be passed through PDO constructor options, not embedded in the URL"
                .into(),
        );
    }
    if parsed.scheme() != "https"
        && !(parsed.scheme() == "http"
            && matches!(parsed.host_str(), Some("localhost" | "127.0.0.1" | "::1")))
    {
        return Err(
            "HTTPS is required; plain HTTP is permitted only for loopback mock tests".into(),
        );
    }
    if sql.len() > 1024 * 1024 {
        return Err("SQL statement exceeds the 1 MiB safety limit".into());
    }
    let config = ConnectionConfig {
        endpoint: endpoint.into(),
        username,
        password,
        token,
        timeout_ms: timeout_ms.clamp(100, 300_000) as u64,
    };
    let client = Client::builder()
        .timeout(Duration::from_millis(config.timeout_ms))
        .connect_timeout(Duration::from_millis(config.timeout_ms.min(30_000)))
        .https_only(config.endpoint.starts_with("https://"))
        .redirect(reqwest::redirect::Policy::none())
        .build()
        .map_err(|e| format!("HTTP client: {e}"))?;
    execute_payload_with_client(&client, &config, &json!({"statementText": sql}).to_string())
}
pub fn execute_payload(
    endpoint: &str,
    username: Zeroizing<String>,
    password: Zeroizing<String>,
    token: Zeroizing<String>,
    payload: &str,
    timeout_ms: i64,
) -> Result<String, String> {
    let parsed = reqwest::Url::parse(endpoint).map_err(|_| "Invalid ORDS endpoint URL")?;
    if parsed.query().is_some()
        || parsed.fragment().is_some()
        || !parsed.username().is_empty()
        || parsed.password().is_some()
    {
        return Err("Invalid ORDS endpoint URL".into());
    }
    if parsed.scheme() != "https"
        && !(parsed.scheme() == "http"
            && matches!(parsed.host_str(), Some("localhost" | "127.0.0.1" | "::1")))
    {
        return Err(
            "HTTPS is required; plain HTTP is permitted only for loopback mock tests".into(),
        );
    }
    if payload.len() > 1024 * 1024 {
        return Err("ORDS request exceeds the 1 MiB safety limit".into());
    }
    let config = ConnectionConfig {
        endpoint: endpoint.into(),
        username,
        password,
        token,
        timeout_ms: timeout_ms.clamp(100, 300_000) as u64,
    };
    let client = Client::builder()
        .timeout(Duration::from_millis(config.timeout_ms))
        .connect_timeout(Duration::from_millis(config.timeout_ms.min(30_000)))
        .https_only(config.endpoint.starts_with("https://"))
        .redirect(reqwest::redirect::Policy::none())
        .build()
        .map_err(|e| format!("HTTP client: {e}"))?;
    execute_payload_with_client(&client, &config, payload)
}
fn execute_payload_with_client(
    client: &Client,
    config: &ConnectionConfig,
    payload: &str,
) -> Result<String, String> {
    let response = auth::apply(
        client
            .post(config.sql_url())
            .header(reqwest::header::CONTENT_TYPE, "application/json")
            .body(payload.to_owned()),
        &config.username,
        &config.password,
        &config.token,
    )
    .send()
    .map_err(|e| format!("ORDS transport: {e}"))?;
    let status = response.status();
    if response
        .content_length()
        .is_some_and(|length| length > MAX_RESPONSE_BYTES)
    {
        return Err("ORDS response exceeds the 16 MiB safety limit".into());
    }
    let mut bytes = Vec::new();
    response
        .take(MAX_RESPONSE_BYTES + 1)
        .read_to_end(&mut bytes)
        .map_err(|e| format!("ORDS response read: {e}"))?;
    if bytes.len() as u64 > MAX_RESPONSE_BYTES {
        return Err("ORDS response exceeds the 16 MiB safety limit".into());
    }
    let text = String::from_utf8(bytes).map_err(|e| format!("ORDS response is not UTF-8: {e}"))?;
    if !status.is_success() {
        return Err(format!(
            "ORDS HTTP {}: {}",
            status.as_u16(),
            text.chars().take(2048).collect::<String>()
        ));
    }
    let decoded = crate::result::decode(&text)?;
    let _ = (
        decoded.columns.len(),
        decoded.rows.len(),
        decoded.column_types.len(),
        decoded.affected,
    );
    Ok(text)
}
