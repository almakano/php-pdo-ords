#ifndef PHP_ORDS_H
#define PHP_ORDS_H
#include <php.h>
#include <ext/pdo/php_pdo_driver.h>
#include <ext/json/php_json.h>
#include <Zend/zend_smart_str.h>
#define PHP_ORDS_VERSION "0.1.0"
typedef struct {
    char *endpoint, *username, *password, *token;
    long timeout_ms, offset, limit;
    zend_string *last_id, *last_id_name;
    char error[1024];
} ords_dbh_data;
void ords_string_free(char *);
char *ords_http_request(const char *, const char *, const char *, const char *, const char *, long, char *, size_t);
char *ords_http_execute(const char *, const char *, const char *, const char *, const char *, long, char *, size_t);
extern const pdo_driver_t pdo_ords_driver;
extern const struct pdo_stmt_methods pdo_ords_stmt_methods;
extern const struct pdo_dbh_methods pdo_ords_dbh_methods;
zend_string *pdo_ords_quote(pdo_dbh_t *, const zend_string *, enum pdo_param_type);
zend_long pdo_ords_do(pdo_dbh_t *, const zend_string *);
bool pdo_ords_txn_unsupported(pdo_dbh_t *);
bool pdo_ords_in_transaction(pdo_dbh_t *);
bool pdo_ords_set_attr(pdo_dbh_t *, zend_long, zval *);
int pdo_ords_get_attr(pdo_dbh_t *, zend_long, zval *);
void pdo_ords_fetch_error(pdo_dbh_t *, pdo_stmt_t *, zval *);
bool pdo_ords_prepare(pdo_dbh_t *, zend_string *, pdo_stmt_t *, zval *);
zend_string *pdo_ords_last_id(pdo_dbh_t *, const zend_string *);
#endif
