use zeroize::Zeroizing;
pub struct ConnectionConfig {
    pub endpoint: String,
    pub username: Zeroizing<String>,
    pub password: Zeroizing<String>,
    pub token: Zeroizing<String>,
    pub timeout_ms: u64,
}
impl ConnectionConfig {
    pub fn sql_url(&self) -> String {
        format!("{}/_/sql", self.endpoint.trim_end_matches('/'))
    }
}
