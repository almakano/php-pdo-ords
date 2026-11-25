#include "../include/php_ords.h"
#include <ext/standard/info.h>

PHP_MINIT_FUNCTION(php_ords) {
    if (php_pdo_register_driver(&pdo_ords_driver) == FAILURE) return FAILURE;
    REGISTER_LONG_CONSTANT("PDO_ORDS_ATTR_ACCESS_TOKEN", PDO_ATTR_DRIVER_SPECIFIC + 1, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PDO_ORDS_ATTR_TIMEOUT_MS", PDO_ATTR_DRIVER_SPECIFIC + 2, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PDO_ORDS_ATTR_OFFSET", PDO_ATTR_DRIVER_SPECIFIC + 3, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("PDO_ORDS_ATTR_LIMIT", PDO_ATTR_DRIVER_SPECIFIC + 4, CONST_CS | CONST_PERSISTENT);
    return SUCCESS;
}
PHP_MSHUTDOWN_FUNCTION(php_ords) { php_pdo_unregister_driver(&pdo_ords_driver); return SUCCESS; }
PHP_MINFO_FUNCTION(php_ords) {
    php_info_print_table_start();
    php_info_print_table_row(2, "PDO ORDS support", "enabled");
    php_info_print_table_row(2, "Version", PHP_ORDS_VERSION);
    php_info_print_table_row(2, "HTTP backend", "Rust + reqwest/rustls");
    php_info_print_table_end();
}
static const zend_module_dep php_ords_deps[] = {
    ZEND_MOD_REQUIRED("pdo")
    ZEND_MOD_REQUIRED("json")
    ZEND_MOD_END
};
zend_module_entry php_pdo_ords_module_entry = {
    STANDARD_MODULE_HEADER_EX, NULL, php_ords_deps,
    "pdo_ords", NULL, PHP_MINIT(php_ords), PHP_MSHUTDOWN(php_ords), NULL,
    NULL, PHP_MINFO(php_ords), PHP_ORDS_VERSION, STANDARD_MODULE_PROPERTIES
};
void *ords_php_module_entry(void) { return &php_pdo_ords_module_entry; }
