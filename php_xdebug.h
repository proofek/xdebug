/*
   +----------------------------------------------------------------------+
   | Xdebug                                                               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007 Derick Rethans      |
   +----------------------------------------------------------------------+
   | This source file is subject to version 1.0 of the Xdebug license,    |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://xdebug.derickrethans.nl/license.php                           |
   | If you did not receive a copy of the Xdebug license and are unable   |
   | to obtain it through the world-wide-web, please send a note to       |
   | xdebug@derickrethans.nl so we can mail you a copy immediately.       |
   +----------------------------------------------------------------------+
   | Authors:  Derick Rethans <derick@xdebug.org>                         |
   +----------------------------------------------------------------------+
 */

#ifndef PHP_XDEBUG_H
#define PHP_XDEBUG_H

#define XDEBUG_NAME       "Xdebug"
#define XDEBUG_VERSION    "2.0.2"
#define XDEBUG_AUTHOR     "Derick Rethans"
#define XDEBUG_COPYRIGHT  "Copyright (c) 2002-2007 by Derick Rethans"
#define XDEBUG_URL        "http://xdebug.org"

#include "php.h"

#include "xdebug_handlers.h"
#include "xdebug_hash.h"
#include "xdebug_llist.h"

extern zend_module_entry xdebug_module_entry;
#define phpext_xdebug_ptr &xdebug_module_entry

#define MICRO_IN_SEC 1000000.00

#ifdef PHP_WIN32
#define PHP_XDEBUG_API __declspec(dllexport)
#else
#define PHP_XDEBUG_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#if MEMORY_LIMIT
# define HAVE_PHP_MEMORY_USAGE 1
#elif PHP_VERSION_ID >= 50201
# define HAVE_PHP_MEMORY_USAGE 1
#else
# define HAVE_PHP_MEMORY_USAGE 0
#endif

#if PHP_VERSION_ID >= 50200
# define XG_MEMORY_USAGE()		zend_memory_usage(0 TSRMLS_CC) 
# define XG_MEMORY_PEAK_USAGE()	zend_memory_peak_usage(0 TSRMLS_CC) 
#else
# define XG_MEMORY_USAGE()		AG(allocated_memory)
# define XG_MEMORY_PEAK_USAGE()	AG(allocated_memory_peak)
#endif

PHP_MINIT_FUNCTION(xdebug);
PHP_MSHUTDOWN_FUNCTION(xdebug);
PHP_RINIT_FUNCTION(xdebug);
PHP_RSHUTDOWN_FUNCTION(xdebug);
PHP_MINFO_FUNCTION(xdebug);
#ifdef ZEND_ENGINE_2
ZEND_MODULE_POST_ZEND_DEACTIVATE_D(xdebug);
#endif

/* call stack functions */
PHP_FUNCTION(xdebug_get_stack_depth);
PHP_FUNCTION(xdebug_get_function_stack);
PHP_FUNCTION(xdebug_print_function_stack);
PHP_FUNCTION(xdebug_get_declared_vars);
PHP_FUNCTION(xdebug_call_class);
PHP_FUNCTION(xdebug_call_function);
PHP_FUNCTION(xdebug_call_file);
PHP_FUNCTION(xdebug_call_line);

PHP_FUNCTION(xdebug_set_time_limit);

PHP_FUNCTION(xdebug_var_dump);
PHP_FUNCTION(xdebug_debug_zval);
PHP_FUNCTION(xdebug_debug_zval_stdout);

/* activation functions */
PHP_FUNCTION(xdebug_enable);
PHP_FUNCTION(xdebug_disable);
PHP_FUNCTION(xdebug_is_enabled);

/* breaking functions */
PHP_FUNCTION(xdebug_break);

/* tracing functions */
PHP_FUNCTION(xdebug_start_trace);
PHP_FUNCTION(xdebug_stop_trace);
PHP_FUNCTION(xdebug_get_tracefile_name);

/* profiling functions */
PHP_FUNCTION(xdebug_get_profiler_filename);
PHP_FUNCTION(xdebug_dump_aggr_profiling_data);
PHP_FUNCTION(xdebug_clear_aggr_profiling_data);

/* misc functions */
PHP_FUNCTION(xdebug_dump_superglobals);
#if HAVE_PHP_MEMORY_USAGE
PHP_FUNCTION(xdebug_memory_usage);
PHP_FUNCTION(xdebug_peak_memory_usage);
#endif
PHP_FUNCTION(xdebug_time_index);

ZEND_BEGIN_MODULE_GLOBALS(xdebug)
	int           status;
	int           reason;

	long          level;
	xdebug_llist *stack;
	long          max_nesting_level;
	zend_bool     default_enable;
	zend_bool     collect_includes;
	long          collect_params;
	zend_bool     collect_return;
	zend_bool     collect_vars;
	zend_bool     extended_info;
	zend_bool     show_ex_trace;
	zend_bool     show_local_vars;
	zend_bool     show_mem_delta;
	char         *manual_url;
	double        start_time;
	HashTable    *active_symbol_table;
	zend_execute_data *active_execute_data;
	unsigned int  prev_memory;

	void        (*orig_var_dump_func)(INTERNAL_FUNCTION_PARAMETERS);
	void        (*orig_set_time_limit_func)(INTERNAL_FUNCTION_PARAMETERS);

	FILE         *trace_file;
	zend_bool     do_trace;
	zend_bool     auto_trace;
	char         *trace_output_dir;
	char         *trace_output_name;
	long          trace_options;
	long          trace_format;
	char         *tracefile_name;
	char         *last_exception_trace;

	/* variable dumping limitation settings */
	long          display_max_children;
	long          display_max_data;
	long          display_max_depth;

	/* used for code coverage */
	zend_bool     do_code_coverage;
	xdebug_hash  *code_coverage;
	zend_bool     code_coverage_unused;
	zend_bool     code_coverage_dead_code_analysis;
	unsigned int  function_count;
	int           reserved_offset;

	/* superglobals */
	zend_bool     dump_globals;
	zend_bool     dump_once;
	zend_bool     dump_undefined;
	zend_bool     dumped;
	xdebug_llist  server;
	xdebug_llist  get;
	xdebug_llist  post;
	xdebug_llist  cookie;
	xdebug_llist  files;
	xdebug_llist  env;
	xdebug_llist  request;
	xdebug_llist  session;

	/* remote settings */
	zend_bool     remote_enable;  /* 0 */
	long          remote_port;    /* 9000 */
	char         *remote_host;    /* localhost */
	long          remote_mode;    /* XDEBUG_NONE, XDEBUG_JIT, XDEBUG_REQ */
	char         *remote_handler; /* php3, gdb, dbgp */
	zend_bool     remote_autostart; /* Disables the requirement for XDEBUG_SESSION_START */
	char         *remote_log;       /* Filename to log protocol communication to */
	FILE         *remote_log_file;  /* File handler for protocol log */

	char         *ide_key;    /* from environment, USER, USERNAME or empty */

	/* remote debugging globals */
	zend_bool     remote_enabled;
	zend_bool     breakpoints_allowed;
	xdebug_con    context;
	unsigned int  breakpoint_count;
	unsigned int  no_exec;

	/* profiler settings */
	zend_bool     profiler_enable;
	char         *profiler_output_dir;
	char         *profiler_output_name; /* "pid" or "crc32" */
	zend_bool     profiler_enable_trigger;
	zend_bool     profiler_append;

	/* profiler globals */
	zend_bool     profiler_enabled;
	FILE         *profile_file;
	char         *profile_filename;

	/* DBGp globals */
	char         *lastcmd;
	char         *lasttransid;

	/* output redirection */
	php_output_globals stdio;
	int stdout_redirected;
	int stderr_redirected;
	int stdin_redirected;

	/* aggregate profiling */
	HashTable  aggr_calls;
	zend_bool  profiler_aggregate;

ZEND_END_MODULE_GLOBALS(xdebug)

#ifdef ZTS
#define XG(v) TSRMG(xdebug_globals_id, zend_xdebug_globals *, v)
#else
#define XG(v) (xdebug_globals.v)
#endif
	
#endif


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
