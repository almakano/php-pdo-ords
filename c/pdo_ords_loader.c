#include "php.h"

extern zend_module_entry php_pdo_ords_module_entry;

static zend_module_entry *php_pdo_ords_force_link __attribute__((used)) = &php_pdo_ords_module_entry;
