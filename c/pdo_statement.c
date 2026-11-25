#include "../include/php_ords.h"
#include <ext/json/php_json.h>
#include <string.h>

typedef struct { zval rows, metadata; zend_long cursor, affected; char error[1024]; } ords_stmt_data;
static void ords_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt, const char *message) {
    strncpy(dbh->error_code, "HY000", sizeof(pdo_error_type));
    if (stmt && stmt->driver_data) snprintf(((ords_stmt_data *) stmt->driver_data)->error, sizeof(((ords_stmt_data *) stmt->driver_data)->error), "%s", message);
    if (dbh->driver_data) snprintf(((ords_dbh_data *) dbh->driver_data)->error, sizeof(((ords_dbh_data *) dbh->driver_data)->error), "%s", message);
    if (stmt) strncpy(stmt->error_code, "HY000", sizeof(pdo_error_type));
    pdo_raise_impl_error(dbh, stmt, stmt ? stmt->error_code : dbh->error_code, message);
}
static int ords_request(pdo_dbh_t *dbh, pdo_stmt_t *stmt, zval *request, zval *decoded) {
    ords_dbh_data *data = dbh->driver_data;
    smart_str payload = {0};
    char error[1024] = {0};
    php_json_encode(&payload, request, PHP_JSON_UNESCAPED_UNICODE | PHP_JSON_UNESCAPED_SLASHES);
    smart_str_0(&payload);
    if (!payload.s) { ords_error(dbh, stmt, "Could not encode ORDS request"); return FAILURE; }
    char *response = ords_http_request(data->endpoint, data->username, data->password, data->token, ZSTR_VAL(payload.s), data->timeout_ms, error, sizeof(error));
    smart_str_free(&payload);
    if (!response) { ords_error(dbh, stmt, error[0] ? error : "ORDS request failed"); return FAILURE; }
    php_json_decode_ex(decoded, response, strlen(response), PHP_JSON_OBJECT_AS_ARRAY, 512);
    ords_string_free(response);
    if (Z_TYPE_P(decoded) != IS_ARRAY) { ords_error(dbh, stmt, "ORDS returned invalid JSON"); zval_ptr_dtor(decoded); return FAILURE; }
    return SUCCESS;
}
static const char *ords_bind_type(enum pdo_param_type type) {
    switch (PDO_PARAM_TYPE(type)) {
        case PDO_PARAM_INT: return "NUMBER";
        case PDO_PARAM_BOOL: return "BOOLEAN";
        case PDO_PARAM_LOB: return "BLOB";
        case PDO_PARAM_NULL: case PDO_PARAM_STR: default: return "VARCHAR2";
    }
}
static void ords_add_bind(zval *binds, struct pdo_bound_param_data *param) {
    zval bind, value;
    array_init(&bind);
    if (param->name && ZSTR_LEN(param->name)) {
        const char *name = ZSTR_VAL(param->name) + (ZSTR_VAL(param->name)[0] == ':' || ZSTR_VAL(param->name)[0] == '@' ? 1 : 0);
        add_assoc_string(&bind, "name", name);
    } else add_assoc_long(&bind, "index", param->paramno + 1);
    add_assoc_string(&bind, "data_type", (char *) ords_bind_type(param->param_type));
    if (param->param_type & PDO_PARAM_INPUT_OUTPUT) add_assoc_string(&bind, "mode", "inout");
    if (!(param->param_type & PDO_PARAM_INPUT_OUTPUT) || Z_ISREF(param->parameter)) {
        ZVAL_COPY_DEREF(&value, &param->parameter);
        switch (PDO_PARAM_TYPE(param->param_type)) {
            case PDO_PARAM_INT: convert_to_long(&value); break;
            case PDO_PARAM_BOOL: convert_to_boolean(&value); break;
            case PDO_PARAM_STR: case PDO_PARAM_LOB: if (Z_TYPE(value) != IS_NULL) convert_to_string(&value); break;
            default: break;
        }
        add_assoc_zval(&bind, "value", &value);
    } else add_assoc_string(&bind, "mode", "out");
    add_next_index_zval(binds, &bind);
}
static int ords_apply_out_binds(pdo_stmt_t *stmt, zval *item) {
    zval *out = zend_hash_str_find(Z_ARRVAL_P(item), "binds", sizeof("binds") - 1);
    if (!out || Z_TYPE_P(out) != IS_ARRAY || !stmt->bound_params) return SUCCESS;
    struct pdo_bound_param_data *param;
    ZEND_HASH_FOREACH_PTR(stmt->bound_params, param) {
        if (!(param->param_type & PDO_PARAM_INPUT_OUTPUT)) continue;
        if (!Z_ISREF(param->parameter)) { ords_error(stmt->dbh, stmt, "PDO OUT/INOUT binds require bindParam() with a variable reference"); return FAILURE; }
        zval *entry, *name, *result;
        ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(out), entry) {
            name = Z_TYPE_P(entry) == IS_ARRAY ? zend_hash_str_find(Z_ARRVAL_P(entry), "name", sizeof("name") - 1) : NULL;
            result = Z_TYPE_P(entry) == IS_ARRAY ? zend_hash_str_find(Z_ARRVAL_P(entry), "result", sizeof("result") - 1) : NULL;
            if (result && ((param->name && name && Z_TYPE_P(name) == IS_STRING && (zend_binary_strcasecmp(Z_STRVAL_P(name), Z_STRLEN_P(name), ZSTR_VAL(param->name) + (ZSTR_VAL(param->name)[0] == ':' || ZSTR_VAL(param->name)[0] == '@' ? 1 : 0), ZSTR_LEN(param->name) - (ZSTR_VAL(param->name)[0] == ':' || ZSTR_VAL(param->name)[0] == '@' ? 1 : 0)) == 0)) || (!param->name && name == NULL))) {
                zval_ptr_dtor(Z_REFVAL(param->parameter));
                ZVAL_COPY(Z_REFVAL(param->parameter), result);
                ords_dbh_data *dbh_data = stmt->dbh->driver_data;
                if (!dbh_data->last_id) {
                    dbh_data->last_id = zval_get_string(result);
                    if (param->name) dbh_data->last_id_name = zend_string_init(ZSTR_VAL(param->name) + (ZSTR_VAL(param->name)[0] == ':' || ZSTR_VAL(param->name)[0] == '@' ? 1 : 0), ZSTR_LEN(param->name) - (ZSTR_VAL(param->name)[0] == ':' || ZSTR_VAL(param->name)[0] == '@' ? 1 : 0), 0);
                }
                break;
            }
        } ZEND_HASH_FOREACH_END();
    } ZEND_HASH_FOREACH_END();
    return SUCCESS;
}
static int ords_stmt_dtor(pdo_stmt_t *stmt) {
    ords_stmt_data *data = stmt->driver_data;
    if (data) { zval_ptr_dtor(&data->rows); zval_ptr_dtor(&data->metadata); efree(data); }
    stmt->driver_data = NULL;
    return 1;
}
static int ords_stmt_execute(pdo_stmt_t *stmt) {
    zval decoded, *items, *item, *set, *rows, *metadata, *result;
    ords_stmt_data *data = stmt->driver_data;
    data->error[0] = 0;
    zend_string *query = stmt->active_query_string ? stmt->active_query_string : stmt->query_string;
    if (!query) return FAILURE;
    zval request, binds;
    array_init(&request);
    add_assoc_str(&request, "statementText", zend_string_copy(query));
    add_assoc_long(&request, "offset", ((ords_dbh_data *) stmt->dbh->driver_data)->offset);
    add_assoc_long(&request, "limit", ((ords_dbh_data *) stmt->dbh->driver_data)->limit);
    array_init(&binds);
    if (stmt->bound_params) {
        struct pdo_bound_param_data *param;
        bool has_output = false;
        ZEND_HASH_FOREACH_PTR(stmt->bound_params, param) { if (param->param_type & PDO_PARAM_INPUT_OUTPUT) has_output = true; } ZEND_HASH_FOREACH_END();
        if (has_output) {
            ords_dbh_data *dbh_data = stmt->dbh->driver_data;
            if (dbh_data->last_id) zend_string_release(dbh_data->last_id);
            if (dbh_data->last_id_name) zend_string_release(dbh_data->last_id_name);
            dbh_data->last_id = dbh_data->last_id_name = NULL;
        }
        ZEND_HASH_FOREACH_PTR(stmt->bound_params, param) { ords_add_bind(&binds, param); } ZEND_HASH_FOREACH_END();
    }
    if (zend_hash_num_elements(Z_ARRVAL(binds))) add_assoc_zval(&request, "binds", &binds); else zval_ptr_dtor(&binds);
    if (ords_request(stmt->dbh, stmt, &request, &decoded) == FAILURE) { zval_ptr_dtor(&request); return FAILURE; }
    zval_ptr_dtor(&request);
    items = zend_hash_str_find(Z_ARRVAL(decoded), "items", sizeof("items") - 1);
    item = items && Z_TYPE_P(items) == IS_ARRAY ? zend_hash_index_find(Z_ARRVAL_P(items), 0) : NULL;
    if (item && Z_TYPE_P(item) == IS_ARRAY && ords_apply_out_binds(stmt, item) == FAILURE) { zval_ptr_dtor(&decoded); return FAILURE; }
    set = item && Z_TYPE_P(item) == IS_ARRAY ? zend_hash_str_find(Z_ARRVAL_P(item), "resultSet", sizeof("resultSet") - 1) : NULL;
    rows = set && Z_TYPE_P(set) == IS_ARRAY ? zend_hash_str_find(Z_ARRVAL_P(set), "items", sizeof("items") - 1) : NULL;
    metadata = set && Z_TYPE_P(set) == IS_ARRAY ? zend_hash_str_find(Z_ARRVAL_P(set), "metadata", sizeof("metadata") - 1) : NULL;
    result = item && Z_TYPE_P(item) == IS_ARRAY ? zend_hash_str_find(Z_ARRVAL_P(item), "result", sizeof("result") - 1) : NULL;
    zval_ptr_dtor(&data->rows); zval_ptr_dtor(&data->metadata);
    if (rows && Z_TYPE_P(rows) == IS_ARRAY) ZVAL_COPY(&data->rows, rows); else array_init(&data->rows);
    if (metadata && Z_TYPE_P(metadata) == IS_ARRAY) ZVAL_COPY(&data->metadata, metadata); else array_init(&data->metadata);
    data->cursor = 0;
    data->affected = result ? zval_get_long(result) : 0;
    if (set && Z_TYPE_P(set) == IS_ARRAY) {
        zval *count = zend_hash_str_find(Z_ARRVAL_P(set), "count", sizeof("count") - 1);
        if (count) data->affected = zval_get_long(count);
    }
    stmt->row_count = data->affected;
    stmt->executed = 1;
    php_pdo_stmt_set_column_count(stmt, zend_hash_num_elements(Z_ARRVAL(data->metadata)));
    zval_ptr_dtor(&decoded);
    return 1;
}
static int ords_stmt_fetch(pdo_stmt_t *stmt, enum pdo_fetch_orientation orientation, zend_long offset) {
    ords_stmt_data *data = stmt->driver_data;
    if (orientation != PDO_FETCH_ORI_NEXT || data->cursor >= zend_hash_num_elements(Z_ARRVAL(data->rows))) return 0;
    data->cursor++;
    return 1;
}
static int ords_stmt_describe(pdo_stmt_t *stmt, int column) {
    ords_stmt_data *data = stmt->driver_data;
    zval *meta = zend_hash_index_find(Z_ARRVAL(data->metadata), column), *name, *precision;
    if (!meta || Z_TYPE_P(meta) != IS_ARRAY) return FAILURE;
    name = zend_hash_str_find(Z_ARRVAL_P(meta), "columnName", sizeof("columnName") - 1);
    precision = zend_hash_str_find(Z_ARRVAL_P(meta), "precision", sizeof("precision") - 1);
    if (name && Z_TYPE_P(name) == IS_STRING) stmt->columns[column].name = zend_string_copy(Z_STR_P(name));
    stmt->columns[column].precision = precision ? zval_get_long(precision) : 0;
    stmt->columns[column].maxlen = SIZE_MAX;
    return 1;
}
static int ords_stmt_get_col(pdo_stmt_t *stmt, int column, zval *value, enum pdo_param_type *type) {
    ords_stmt_data *data = stmt->driver_data;
    zval *meta = zend_hash_index_find(Z_ARRVAL(data->metadata), column), *key, *row, *cell;
    zend_string *name;
    if (!meta || !stmt->columns[column].name) return FAILURE;
    key = zend_hash_str_find(Z_ARRVAL_P(meta), "jsonColumnName", sizeof("jsonColumnName") - 1);
    name = key && Z_TYPE_P(key) == IS_STRING ? Z_STR_P(key) : stmt->columns[column].name;
    row = zend_hash_index_find(Z_ARRVAL(data->rows), data->cursor - 1);
    cell = row && Z_TYPE_P(row) == IS_ARRAY ? zend_hash_find(Z_ARRVAL_P(row), name) : NULL;
    if (!cell) ZVAL_NULL(value); else ZVAL_COPY(value, cell);
    return 1;
}

static int ords_stmt_column_meta(pdo_stmt_t *stmt, zend_long column, zval *value) {
    ords_stmt_data *data = stmt->driver_data;
    zval *meta = zend_hash_index_find(Z_ARRVAL(data->metadata), column), *field;
    const char *type_name;
    if (!meta || Z_TYPE_P(meta) != IS_ARRAY) return FAILURE;
    array_init(value);
    field = zend_hash_str_find(Z_ARRVAL_P(meta), "columnName", sizeof("columnName") - 1);
    if (field && Z_TYPE_P(field) == IS_STRING) add_assoc_str(value, "name", zend_string_copy(Z_STR_P(field)));
    field = zend_hash_str_find(Z_ARRVAL_P(meta), "columnTypeName", sizeof("columnTypeName") - 1);
    type_name = field && Z_TYPE_P(field) == IS_STRING ? Z_STRVAL_P(field) : "VARCHAR2";
    add_assoc_string(value, "native_type", type_name);
    add_assoc_long(value, "pdo_type", PDO_PARAM_STR);
    field = zend_hash_str_find(Z_ARRVAL_P(meta), "precision", sizeof("precision") - 1);
    if (field) add_assoc_long(value, "precision", zval_get_long(field));
    field = zend_hash_str_find(Z_ARRVAL_P(meta), "scale", sizeof("scale") - 1);
    if (field) add_assoc_long(value, "scale", zval_get_long(field));
    return SUCCESS;
}

const struct pdo_stmt_methods pdo_ords_stmt_methods = {
    .dtor = ords_stmt_dtor, .executer = ords_stmt_execute, .fetcher = ords_stmt_fetch,
    .describer = ords_stmt_describe, .get_col = ords_stmt_get_col,
    .get_column_meta = ords_stmt_column_meta
};
bool pdo_ords_prepare(pdo_dbh_t *dbh, zend_string *sql, pdo_stmt_t *stmt, zval *options) {
    ords_stmt_data *data = ecalloc(1, sizeof(*data));
    array_init(&data->rows); array_init(&data->metadata);
    stmt->driver_data = data; stmt->methods = &pdo_ords_stmt_methods;
    stmt->supports_placeholders = memchr(ZSTR_VAL(sql), '?', ZSTR_LEN(sql)) ? PDO_PLACEHOLDER_POSITIONAL : PDO_PLACEHOLDER_NAMED;
    return 1;
}

void pdo_ords_fetch_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt, zval *info) {
    ords_dbh_data *dbh_data = dbh->driver_data;
    ords_stmt_data *stmt_data = stmt ? stmt->driver_data : NULL;
    const char *message = stmt_data ? stmt_data->error : (dbh_data ? dbh_data->error : NULL);
    array_init(info);
    add_next_index_string(info, stmt ? stmt->error_code : dbh->error_code);
    add_next_index_null(info);
    if (message && message[0]) add_next_index_string(info, message);
    else add_next_index_null(info);
}
