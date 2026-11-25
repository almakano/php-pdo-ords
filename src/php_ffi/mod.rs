use crate::http;
use std::{
    ffi::{CStr, CString},
    os::raw::c_char,
};
use zeroize::Zeroizing;

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ords_http_execute(
    endpoint: *const c_char,
    username: *const c_char,
    password: *const c_char,
    token: *const c_char,
    sql: *const c_char,
    timeout_ms: i64,
    error: *mut c_char,
    error_len: usize,
) -> *mut c_char {
    let read = |ptr: *const c_char| {
        if ptr.is_null() {
            String::new()
        } else {
            unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() }
        }
    };
    let endpoint = read(endpoint);
    let username = Zeroizing::new(read(username));
    let password = Zeroizing::new(read(password));
    let token = Zeroizing::new(read(token));
    let sql = Zeroizing::new(read(sql));
    match http::execute(&endpoint, username, password, token, &sql, timeout_ms) {
        Ok(body) => CString::new(body)
            .map(CString::into_raw)
            .unwrap_or(std::ptr::null_mut()),
        Err(message) => {
            if !error.is_null() && error_len > 0 {
                let bytes = message.as_bytes();
                let count = bytes.len().min(error_len - 1);
                unsafe {
                    std::ptr::copy_nonoverlapping(bytes.as_ptr(), error.cast(), count);
                    *error.add(count) = 0;
                }
            }
            std::ptr::null_mut()
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ords_http_request(
    endpoint: *const c_char,
    username: *const c_char,
    password: *const c_char,
    token: *const c_char,
    payload: *const c_char,
    timeout_ms: i64,
    error: *mut c_char,
    error_len: usize,
) -> *mut c_char {
    let read = |ptr: *const c_char| {
        if ptr.is_null() {
            String::new()
        } else {
            unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() }
        }
    };
    let (endpoint, username, password, token, payload) = (
        read(endpoint),
        Zeroizing::new(read(username)),
        Zeroizing::new(read(password)),
        Zeroizing::new(read(token)),
        Zeroizing::new(read(payload)),
    );
    match http::execute_payload(&endpoint, username, password, token, &payload, timeout_ms) {
        Ok(body) => CString::new(body)
            .map(CString::into_raw)
            .unwrap_or(std::ptr::null_mut()),
        Err(message) => {
            if !error.is_null() && error_len > 0 {
                let bytes = message.as_bytes();
                let count = bytes.len().min(error_len - 1);
                unsafe {
                    std::ptr::copy_nonoverlapping(bytes.as_ptr(), error.cast(), count);
                    *error.add(count) = 0;
                }
            }
            std::ptr::null_mut()
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ords_string_free(value: *mut c_char) {
    if !value.is_null() {
        unsafe {
            drop(CString::from_raw(value));
        }
    }
}

#[cfg(not(test))]
#[used]
static KEEP_PHP_MODULE: unsafe extern "C" fn() -> *mut std::ffi::c_void = ords_php_module_entry;
#[cfg(not(test))]
unsafe extern "C" {
    fn ords_php_module_entry() -> *mut std::ffi::c_void;
}

#[cfg(not(test))]
#[unsafe(no_mangle)]
pub extern "C" fn get_module() -> *mut std::ffi::c_void {
    unsafe { ords_php_module_entry() }
}
