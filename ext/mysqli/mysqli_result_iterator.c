/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2014 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Georg Richter <georg@php.net>                               |
  |          Andrey Hristov <andrey@php.net>                             |
  |          Ulf Wendel <uw@php.net>                                     |
  +----------------------------------------------------------------------+

  $Id: mysqli.c 299335 2010-05-13 11:05:09Z andrey $
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <signal.h>

#include "php.h"
#include "php_ini.h"
#include "php_mysqli_structs.h"
#include "mysqli_priv.h"
#include "zend_interfaces.h"


extern zend_object_iterator_funcs php_mysqli_result_iterator_funcs;

typedef struct {
	zend_object_iterator  intern;
	mysqli_object *result;
	zval current_row;
	my_longlong row_num;
} php_mysqli_result_iterator;


/* {{{ */
zend_object_iterator *php_mysqli_result_get_iterator(zend_class_entry *ce, zval *object, int by_ref TSRMLS_DC)
{
	php_mysqli_result_iterator *iterator;

	if (by_ref) {
		zend_error(E_ERROR, "An iterator cannot be used with foreach by reference");
	}
	iterator = ecalloc(1, sizeof(php_mysqli_result_iterator));
	zend_iterator_init(&iterator->intern TSRMLS_CC);

	ZVAL_COPY(&iterator->intern.data, object);
	iterator->intern.funcs = &php_mysqli_result_iterator_funcs;
	iterator->result = Z_MYSQLI_P(object);
	iterator->row_num = -1;

	return &iterator->intern;
}
/* }}} */

/* {{{ */
static void php_mysqli_result_iterator_dtor(zend_object_iterator *iter TSRMLS_DC)
{
	php_mysqli_result_iterator *iterator = (php_mysqli_result_iterator*)iter;

	/* cleanup handled in sxe_object_dtor as we dont always have an iterator wrapper */
	zval_ptr_dtor(&iterator->intern.data);
	zval_ptr_dtor(&iterator->current_row);
}
/* }}} */

/* {{{ */
static int php_mysqli_result_iterator_valid(zend_object_iterator *iter TSRMLS_DC)
{
	php_mysqli_result_iterator *iterator = (php_mysqli_result_iterator*) iter;

	return Z_TYPE(iterator->current_row) == IS_ARRAY ? SUCCESS : FAILURE;
}
/* }}} */

/* {{{ */
static zval *php_mysqli_result_iterator_current_data(zend_object_iterator *iter TSRMLS_DC)
{
	php_mysqli_result_iterator *iterator = (php_mysqli_result_iterator*) iter;

	return &iterator->current_row;
}
/* }}} */

/* {{{ */
static void php_mysqli_result_iterator_move_forward(zend_object_iterator *iter TSRMLS_DC)
{

	php_mysqli_result_iterator *iterator = (php_mysqli_result_iterator*) iter;
	mysqli_object *intern = iterator->result;
	MYSQL_RES	*result;

	MYSQLI_FETCH_RESOURCE_BY_OBJ(result, MYSQL_RES *, intern, "mysqli_result", MYSQLI_STATUS_VALID);

	zval_ptr_dtor(&iterator->current_row);
	php_mysqli_fetch_into_hash_aux(&iterator->current_row, result, MYSQLI_ASSOC TSRMLS_CC);
	if (Z_TYPE(iterator->current_row) == IS_ARRAY) {
		iterator->row_num++;
	}
}
/* }}} */

/* {{{ */
static void php_mysqli_result_iterator_rewind(zend_object_iterator *iter TSRMLS_DC)
{
	php_mysqli_result_iterator *iterator = (php_mysqli_result_iterator*) iter;
	mysqli_object *intern = iterator->result;
	MYSQL_RES	*result;

	MYSQLI_FETCH_RESOURCE_BY_OBJ(result, MYSQL_RES *, intern, "mysqli_result", MYSQLI_STATUS_VALID);

	if (mysqli_result_is_unbuffered(result)) {
#if MYSQLI_USE_MYSQLND
		if (result->unbuf->eof_reached) {
#else
		if (result->eof) {
#endif
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Data fetched with MYSQLI_USE_RESULT can be iterated only once");
			return;
		}
	} else {
		mysql_data_seek(result, 0);
	}
	iterator->row_num = -1;
	php_mysqli_result_iterator_move_forward(iter TSRMLS_CC);
}
/* }}} */

/* {{{ php_mysqli_result_iterator_current_key */
static void php_mysqli_result_iterator_current_key(zend_object_iterator *iter, zval *key TSRMLS_DC)
{
	php_mysqli_result_iterator *iterator = (php_mysqli_result_iterator*) iter;

	ZVAL_INT(key, iterator->row_num);
}
/* }}} */

/* {{{ php_mysqli_result_iterator_funcs */
zend_object_iterator_funcs php_mysqli_result_iterator_funcs = {
	php_mysqli_result_iterator_dtor,
	php_mysqli_result_iterator_valid,
	php_mysqli_result_iterator_current_data,
	php_mysqli_result_iterator_current_key,
	php_mysqli_result_iterator_move_forward,
	php_mysqli_result_iterator_rewind,
};
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
