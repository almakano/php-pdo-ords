use serde_json::Value;
#[derive(Debug, Default)]
pub struct QueryResult {
    pub columns: Vec<Value>,
    pub rows: Vec<Value>,
    pub column_types: Vec<&'static str>,
    pub affected: i64,
}
pub fn decode(body: &str) -> Result<QueryResult, String> {
    let json: Value =
        serde_json::from_str(body).map_err(|error| format!("Invalid ORDS JSON: {error}"))?;
    let items = json
        .get("items")
        .and_then(Value::as_array)
        .ok_or_else(|| "ORDS response contains no statement result".to_owned())?;
    if items.len() != 1 {
        return Err(
            "ORDS returned multiple statement results; execute one SQL statement per PDO call"
                .into(),
        );
    }
    let item = &items[0];
    if item.get("error").is_some() || item.get("result").and_then(Value::as_i64).unwrap_or(0) < 0 {
        return Err(crate::errors::oracle_message(item));
    }
    let result = item.get("resultSet");
    let columns = result
        .and_then(|value| value.get("metadata"))
        .and_then(Value::as_array)
        .cloned()
        .unwrap_or_default();
    let column_types = columns
        .iter()
        .map(|column| {
            crate::types::oracle_type(
                column
                    .get("columnTypeName")
                    .and_then(Value::as_str)
                    .unwrap_or(""),
            )
        })
        .collect();
    let affected = result
        .and_then(|value| value.get("count"))
        .and_then(Value::as_i64)
        .or_else(|| item.get("result").and_then(Value::as_i64))
        .unwrap_or(0);
    Ok(QueryResult {
        columns,
        rows: result
            .and_then(|value| value.get("items"))
            .and_then(Value::as_array)
            .cloned()
            .unwrap_or_default(),
        column_types,
        affected,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn parses_query_rows_and_metadata() {
        let result = decode(r#"{"items":[{"result":0,"resultSet":{"metadata":[{"columnName":"ID","columnTypeName":"NUMBER"}],"items":[{"id":7}],"count":1}}]}"#).unwrap();
        assert_eq!(result.affected, 1);
        assert_eq!(result.columns.len(), 1);
        assert_eq!(result.rows[0]["id"], 7);
    }
    #[test]
    fn rejects_missing_statement_result() {
        assert!(decode(r#"{"items":[]}"#).is_err());
    }
}
