#include "../include/php_ords.h"
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <Zend/zend_exceptions.h>

static void ords_dbh_close(pdo_dbh_t *dbh) {
    ords_dbh_data *data = dbh->driver_data;
    if (!data) return;
    if (data->password) { memset(data->password, 0, strlen(data->password)); efree(data->password); }
    if (data->token) { memset(data->token, 0, strlen(data->token)); efree(data->token); }
    if (data->username) efree(data->username);
    if (data->endpoint) efree(data->endpoint);
    if (data->last_id) zend_string_release(data->last_id);
    if (data->last_id_name) zend_string_release(data->last_id_name);
    efree(data);
    dbh->driver_data = NULL;
}
static int ords_dbh_factory(pdo_dbh_t *dbh, zval *options) {
    ords_dbh_data *data;
    zval *value;
    if (dbh->is_persistent) { zend_throw_exception(php_pdo_get_exception(), "PDO persistent connections are not supported by pdo_ords", 0); return 0; }
    data = ecalloc(1, sizeof(*data));
    data->endpoint = estrndup(dbh->data_source, dbh->data_source_len);
    data->username = dbh->username ? estrdup(dbh->username) : estrdup("");
    data->password = dbh->password ? estrdup(dbh->password) : estrdup("");
    data->timeout_ms = 30000;
    data->limit = 500;
    if (options && (value = zend_hash_index_find(Z_ARRVAL_P(options), PDO_ATTR_DRIVER_SPECIFIC + 1)) && Z_TYPE_P(value) == IS_STRING) data->token = estrdup(Z_STRVAL_P(value));
    else data->token = estrdup("");
    if (options && (value = zend_hash_index_find(Z_ARRVAL_P(options), PDO_ATTR_DRIVER_SPECIFIC + 2))) data->timeout_ms = zval_get_long(value);
    if (strncmp(data->endpoint, "https://", 8) && strncmp(data->endpoint, "http://", 7)) {
        snprintf(data->error, sizeof(data->error), "DSN must contain an HTTP(S) ORDS endpoint");
        ords_dbh_close(&(pdo_dbh_t){.driver_data = data});
        zend_throw_exception(php_pdo_get_exception(), "Invalid ORDS DSN", 0);
        return 0;
    }
    char error[1024] = {0};
    char *probe = ords_http_execute(data->endpoint, data->username, data->password, data->token, "SELECT 1 FROM dual", data->timeout_ms, error, sizeof(error));
    if (!probe) {
        strncpy(dbh->error_code, "08001", sizeof(pdo_error_type));
        pdo_raise_impl_error(dbh, NULL, dbh->error_code, error[0] ? error : "ORDS connection check failed");
        ords_dbh_close(&(pdo_dbh_t){.driver_data = data});
        return 0;
    }
    ords_string_free(probe);
    dbh->driver_data = data;
    dbh->methods = &pdo_ords_dbh_methods;
    dbh->alloc_own_columns = 1;
    dbh->max_escaped_char_length = 2;
    dbh->auto_commit = 1;
    return 1;
}
const pdo_driver_t pdo_ords_driver = { PDO_DRIVER_HEADER(ords), ords_dbh_factory };
const struct pdo_dbh_methods pdo_ords_dbh_methods = {
    .closer = ords_dbh_close,
    .preparer = pdo_ords_prepare,
    .doer = pdo_ords_do,
    .quoter = pdo_ords_quote,
    .begin = pdo_ords_txn_unsupported,
    .commit = pdo_ords_txn_unsupported,
    .rollback = pdo_ords_txn_unsupported,
    .set_attribute = pdo_ords_set_attr,
    .last_id = pdo_ords_last_id,
    .fetch_err = pdo_ords_fetch_error,
    .get_attribute = pdo_ords_get_attr,
    .in_transaction = pdo_ords_in_transaction
};

bool pdo_ords_txn_unsupported(pdo_dbh_t *dbh) {
    strncpy(dbh->error_code, "0A000", sizeof(pdo_error_type));
    snprintf(((ords_dbh_data *) dbh->driver_data)->error, sizeof(((ords_dbh_data *) dbh->driver_data)->error), "%s", "PDO transactions are unavailable: ORDS REST-Enabled SQL auto-commits each request");
    pdo_raise_impl_error(dbh, NULL, dbh->error_code, "PDO transactions are unavailable: ORDS REST-Enabled SQL auto-commits each request");
    return false;
}
bool pdo_ords_in_transaction(pdo_dbh_t *dbh) { return false; }
bool pdo_ords_set_attr(pdo_dbh_t *dbh, zend_long attr, zval *value) {
    if (attr == PDO_ATTR_TIMEOUT) { ((ords_dbh_data *) dbh->driver_data)->timeout_ms = zval_get_long(value) * 1000; return true; }
    if (attr == PDO_ATTR_DRIVER_SPECIFIC + 3 || attr == PDO_ATTR_DRIVER_SPECIFIC + 4) {
        zend_long number = zval_get_long(value);
        if (number < 0) return false;
        if (attr == PDO_ATTR_DRIVER_SPECIFIC + 3) ((ords_dbh_data *) dbh->driver_data)->offset = number;
        else ((ords_dbh_data *) dbh->driver_data)->limit = number;
        return true;
    }
    if (attr == PDO_ATTR_AUTOCOMMIT) return zval_is_true(value);
    if (attr == PDO_ATTR_EMULATE_PREPARES) return !zval_is_true(value);
    if (attr == PDO_ATTR_ERRMODE || attr == PDO_ATTR_DEFAULT_FETCH_MODE) return true;
    return false;
}
int pdo_ords_get_attr(pdo_dbh_t *dbh, zend_long attr, zval *value) {
    if (attr == PDO_ATTR_TIMEOUT) { ZVAL_LONG(value, ((ords_dbh_data *) dbh->driver_data)->timeout_ms / 1000); return 1; }
    if (attr == PDO_ATTR_DRIVER_SPECIFIC + 3) { ZVAL_LONG(value, ((ords_dbh_data *) dbh->driver_data)->offset); return 1; }
    if (attr == PDO_ATTR_DRIVER_SPECIFIC + 4) { ZVAL_LONG(value, ((ords_dbh_data *) dbh->driver_data)->limit); return 1; }
    if (attr == PDO_ATTR_DRIVER_SPECIFIC + 4) { ZVAL_LONG(value, ((ords_dbh_data *) dbh->driver_data)->limit); return 1; }
    return 0;
}
zend_string *pdo_ords_quote(pdo_dbh_t *dbh, const zend_string *value, enum pdo_param_type type) {
    smart_str quoted = {0};
    if (type == PDO_PARAM_NULL) return zend_string_init("NULL", 4, 0);
    if (type == PDO_PARAM_INT) return zend_long_to_str((zend_long) strtoll(ZSTR_VAL(value), NULL, 10));
    if (type == PDO_PARAM_BOOL) return zend_string_init((ZSTR_LEN(value) && ZSTR_VAL(value)[0] != '0') ? "1" : "0", 1, 0);
    smart_str_appendc(&quoted, '\'');
    for (size_t i = 0; i < ZSTR_LEN(value); i++) {
        if (ZSTR_VAL(value)[i] == '\'') smart_str_appendc(&quoted, '\'');
        smart_str_appendc(&quoted, ZSTR_VAL(value)[i]);
    }
    smart_str_appendc(&quoted, '\'');
    smart_str_0(&quoted);
    return quoted.s;
}
zend_long pdo_ords_do(pdo_dbh_t *dbh, const zend_string *sql) {
    ords_dbh_data *data = dbh->driver_data;
    char error[1024] = {0};
    data->error[0] = 0;
    char *response = ords_http_execute(data->endpoint, data->username, data->password, data->token, ZSTR_VAL(sql), data->timeout_ms, error, sizeof(error));
    zval decoded, *items, *item, *result;
    if (!response) { strncpy(dbh->error_code, "HY000", sizeof(pdo_error_type)); snprintf(data->error, sizeof(data->error), "%s", error); pdo_raise_impl_error(dbh, NULL, dbh->error_code, error); return -1; }
    php_json_decode_ex(&decoded, response, strlen(response), PHP_JSON_OBJECT_AS_ARRAY, 512);
    ords_string_free(response);
    items = Z_TYPE(decoded) == IS_ARRAY ? zend_hash_str_find(Z_ARRVAL(decoded), "items", sizeof("items") - 1) : NULL;
    item = items && Z_TYPE_P(items) == IS_ARRAY ? zend_hash_index_find(Z_ARRVAL_P(items), 0) : NULL;
    result = item && Z_TYPE_P(item) == IS_ARRAY ? zend_hash_str_find(Z_ARRVAL_P(item), "result", sizeof("result") - 1) : NULL;
    if (!result) { zval_ptr_dtor(&decoded); strncpy(dbh->error_code, "HY000", sizeof(pdo_error_type)); pdo_raise_impl_error(dbh, NULL, dbh->error_code, "Invalid ORDS response"); return -1; }
    zend_long affected = zval_get_long(result);
    zval_ptr_dtor(&decoded);
    return affected;
}

zend_string *pdo_ords_last_id(pdo_dbh_t *dbh, const zend_string *name) {
    ords_dbh_data *data = dbh->driver_data;
    if (!data || !data->last_id) return NULL;
    if (name && data->last_id_name && zend_binary_strcasecmp(ZSTR_VAL(name), ZSTR_LEN(name), ZSTR_VAL(data->last_id_name), ZSTR_LEN(data->last_id_name)) != 0) return NULL;
    if (name && !data->last_id_name) return NULL;
    return zend_string_copy(data->last_id);
}
