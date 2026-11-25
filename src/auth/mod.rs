use reqwest::blocking::RequestBuilder;
pub fn apply(
    builder: RequestBuilder,
    username: &str,
    password: &str,
    token: &str,
) -> RequestBuilder {
    if !token.is_empty() {
        builder.bearer_auth(token)
    } else if !username.is_empty() {
        builder.basic_auth(username, Some(password))
    } else {
        builder
    }
}
