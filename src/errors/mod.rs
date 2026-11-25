use serde_json::Value;
pub fn oracle_message(body: &Value) -> String {
    body.pointer("/error/message")
        .and_then(Value::as_str)
        .or_else(|| body.pointer("/error/stackTrace").and_then(Value::as_str))
        .unwrap_or("ORDS returned an unsuccessful SQL response")
        .to_owned()
}
