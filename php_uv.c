/*
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2011 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Shuhei Tanuma <chobieee@gmail.com>                          |
   +----------------------------------------------------------------------+
 */


#include "php_uv.h"

extern void php_uv_init(TSRMLS_D);
extern zend_class_entry *uv_class_entry;

static int uv_resource_handle;
static int uv_connect_handle;

void php_uv_init(TSRMLS_D);

void static destruct_uv(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	php_uv_t *obj = (php_uv_t *)rsrc->ptr;
	
	// todo
	//if (Z_TYPE_P(cons->car) == IS_RESOURCE) {
	//	zend_list_delete(Z_RESVAL_P(cons->car));
//}

	efree(obj);
}


PHP_MINIT_FUNCTION(uv) {
	php_uv_init(TSRMLS_C);
	uv_resource_handle = zend_register_list_destructors_ex(destruct_uv, NULL, PHP_UV_RESOURCE_NAME, module_number);
	uv_connect_handle  = zend_register_list_destructors_ex(destruct_uv, NULL, PHP_UV_CONNECT_RESOURCE_NAME, module_number);

	return SUCCESS;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_uv_run, 0, 0, 1)
	ZEND_ARG_INFO(0, loop)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_uv_tcp_connect, 0, 0, 2)
	ZEND_ARG_INFO(0, resource)
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()


ZEND_BEGIN_ARG_INFO_EX(arginfo_uv_tcp_init, 0, 0, 0)
	ZEND_ARG_INFO(0, loop)
ZEND_END_ARG_INFO()


ZEND_BEGIN_ARG_INFO_EX(arginfo_uv_listen, 0, 0, 3)
	ZEND_ARG_INFO(0, resource)
	ZEND_ARG_INFO(0, backlog)
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_uv_accept, 0, 0, 2)
	ZEND_ARG_INFO(0, server)
	ZEND_ARG_INFO(0, client)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_uv_read_start, 0, 0, 2)
	ZEND_ARG_INFO(0, server)
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()


ZEND_BEGIN_ARG_INFO_EX(arginfo_uv_tcp_bind, 0, 0, 1)
	ZEND_ARG_INFO(0, resource)
	ZEND_ARG_INFO(0, address)
	ZEND_ARG_INFO(0, port)
ZEND_END_ARG_INFO()


PHP_FUNCTION(uv_run)
{
	uv_run(uv_default_loop());
}

static void php_uv_tcp_connect_cb(uv_connect_t *conn_req, int status)
{
	fprintf(stderr,"status: %d\n", status);
}


PHP_FUNCTION(uv_tcp_bind)
{
	zval *resource;
	char *address;
	int address_len;
	long port = 8080;
	php_uv_t *uv;
	int r;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"zsl",&resource, &address, &address_len, &port) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(uv, php_uv_t *, &resource, -1, PHP_UV_RESOURCE_NAME, uv_resource_handle);
	memset(&uv->addr,'\0',sizeof(struct sockaddr_in));
	uv->addr = uv_ip4_addr(address, port);
	
	r = uv_tcp_bind((uv_handle_t*)uv->socket, uv->addr);
	if (r) {
		fprintf(stderr,"bind error %d\n", r);
	}
}

PHP_FUNCTION(uv_accept)
{
	zval *z_svr,*z_cli;
	php_uv_t *client;
	uv_connect_t *stream;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"zz",&z_svr, &z_cli) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(stream, uv_connect_t *, &z_svr, -1, PHP_UV_CONNECT_RESOURCE_NAME, uv_connect_handle);
	ZEND_FETCH_RESOURCE(client, php_uv_t *, &z_cli, -1, PHP_UV_RESOURCE_NAME, uv_resource_handle);
	
	client->socket->data = stream;
	uv_accept(stream, (uv_stream_t *)client->socket);
}


static void php_uv_listen_cb(uv_stream_t* handle, int status)
{
	TSRMLS_FETCH();
	zval *retval_ptr,**params = NULL;
	zval *server;
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;
	char *is_callable_error = NULL;

	php_uv_t *uv = (php_uv_t*)handle->data;
	
	if(zend_fcall_info_init(uv->listen_cb, 0, &fci,&fcc,NULL,&is_callable_error TSRMLS_CC) == SUCCESS) {
		if (is_callable_error) {
			fprintf(stderr,"to be a valid callback\n");
		}
	}
	
	/* for now */
	fci.retval_ptr_ptr = &retval_ptr;
	params = emalloc(sizeof(zval**) * 1);

	MAKE_STD_ZVAL(server);
	ZEND_REGISTER_RESOURCE(server, handle, uv_connect_handle);
	params[0] = server;

	fci.params = &params;
	fci.param_count = 1;
	
	//zend_fcall_info_args(&fci, params TSRMLS_CC);
	zend_call_function(&fci, &fcc TSRMLS_CC);

	//zend_fcall_info_args_clear(&fcc, 1);
	zval_ptr_dtor(&retval_ptr);
}

static void after_read(uv_stream_t* stream, ssize_t nread, uv_buf_t buf)
{
	fprintf(stderr,"after read");
}

static uv_buf_t php_uv_read_alloc(uv_handle_t* handle, size_t suggested_size)
{
	fprintf(stderr,"uv_read_alloc");
	return uv_buf_init(malloc(suggested_size), suggested_size);
}

PHP_FUNCTION(uv_read_start)
{
	zval *client, *callback;
	php_uv_t *uv;
	uv_stream_t *stream;
	long backlog = SOMAXCONN;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"zz",&client, &callback) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(uv, php_uv_t *, &client, -1, PHP_UV_RESOURCE_NAME, uv_resource_handle);
	stream = (uv_stream_t *)uv->socket;
	
	uv_read_start(stream, php_uv_read_alloc, after_read);
}

PHP_FUNCTION(uv_listen)
{
	zval *resource, *callback;
	long backlog = SOMAXCONN;
	php_uv_t *uv;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"zlz",&resource, &backlog, &callback) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(uv, php_uv_t *, &resource, -1, PHP_UV_RESOURCE_NAME, uv_resource_handle);

	ZVAL_ZVAL(uv->listen_cb,callback,1,1);
	
	uv_listen((uv_stream_t*)uv->socket, backlog, php_uv_listen_cb);
}

PHP_FUNCTION(uv_tcp_connect)
{
	zval *resource;
	php_uv_t *uv;
	zend_fcall_info fci = {
		0,NULL,NULL,NULL,NULL,0,NULL,NULL
	};
	zend_fcall_info_cache fci_cache;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"zf",&resource,&fci,&fci_cache) == FAILURE) {
		return;
	}
	
	ZEND_FETCH_RESOURCE(uv, php_uv_t *, &resource, -1, PHP_UV_RESOURCE_NAME, uv_resource_handle);
	uv->fci_connect = fci;
	uv->fcc_connect = fci_cache;
	
	uv_tcp_connect(&uv->connect, &uv->socket, uv->addr, php_uv_tcp_connect_cb);
}

PHP_FUNCTION(uv_tcp_init)
{
	int r;
	/* TODO */
	zval *loop;
	php_uv_t *uv;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
		"|z",&loop) == FAILURE) {
		return;
	}

	uv = (php_uv_t *)emalloc(sizeof(php_uv_t));
	uv_tcp_t *tcp = emalloc(sizeof(uv_tcp_t));
	uv->socket = tcp;

	r = uv_tcp_init(uv_default_loop(), tcp);
	if (r) {
		fprintf(stderr, "Socket creation error\n");
		return;
	}
	uv->socket->data = uv;
	MAKE_STD_ZVAL(uv->listen_cb);
	
	ZEND_REGISTER_RESOURCE(return_value, uv, uv_resource_handle);
}

static zend_function_entry uv_functions[] = {
	PHP_FE(uv_run, arginfo_uv_run)
	PHP_FE(uv_tcp_init, arginfo_uv_tcp_init)
	PHP_FE(uv_tcp_bind, arginfo_uv_tcp_bind)
	PHP_FE(uv_listen, arginfo_uv_listen)
	PHP_FE(uv_accept, arginfo_uv_accept)
	PHP_FE(uv_read_start, arginfo_uv_read_start)
	PHP_FE(uv_tcp_connect, arginfo_uv_tcp_connect)
	{NULL, NULL, NULL}
};


PHP_MINFO_FUNCTION(uv)
{
	php_printf("PHP libuv Extension\n");
}

zend_module_entry uv_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"uv",
	uv_functions,					/* Functions */
	PHP_MINIT(uv),	/* MINIT */
	NULL,					/* MSHUTDOWN */
	NULL,					/* RINIT */
	NULL,					/* RSHUTDOWN */
	PHP_MINFO(uv),	/* MINFO */
#if ZEND_MODULE_API_NO >= 20010901
	PHP_UV_EXTVER,
#endif
	STANDARD_MODULE_PROPERTIES
};


#ifdef COMPILE_DL_UV
ZEND_GET_MODULE(uv)
#endif