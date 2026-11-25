pub fn oracle_type(name: &str) -> &'static str {
    match name.to_ascii_uppercase().as_str() {
        "NUMBER" | "FLOAT" | "BINARY_FLOAT" | "BINARY_DOUBLE" => "numeric",
        "DATE" | "TIMESTAMP" => "datetime",
        "BLOB" | "RAW" | "LONG RAW" => "binary",
        "CLOB" | "NCLOB" | "LONG" => "text",
        _ => "string",
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn maps_oracle_native_types() {
        assert_eq!(oracle_type("NUMBER"), "numeric");
        assert_eq!(oracle_type("CLOB"), "text");
        assert_eq!(oracle_type("VARCHAR2"), "string");
    }
}
