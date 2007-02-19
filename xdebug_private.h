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

#ifndef XDEBUG_PRIVATE_H
#define XDEBUG_PRIVATE_H

#include "zend_hash.h"
#include "xdebug_hash.h"
#include "xdebug_llist.h"

#define MICRO_IN_SEC 1000000.00

#ifdef ZTS
#include "TSRM.h"
#endif

char* xdebug_start_trace(char* fname, long options TSRMLS_DC);
void xdebug_stop_trace(TSRMLS_D);

typedef struct xdebug_var {
	char *name;
	void *addr;
} xdebug_var;

#define XFUNC_UNKNOWN        0x00
#define XFUNC_NORMAL         0x01
#define XFUNC_STATIC_MEMBER  0x02
#define XFUNC_MEMBER         0x03
#define XFUNC_NEW            0x04

#define XFUNC_INCLUDES       0x10
#define XFUNC_EVAL           0x10
#define XFUNC_INCLUDE        0x11
#define XFUNC_INCLUDE_ONCE   0x12
#define XFUNC_REQUIRE        0x13
#define XFUNC_REQUIRE_ONCE   0x14

#define XDEBUG_IS_FUNCTION(f) (f == XFUNC_NORMAL || f == XFUNC_STATIC_MEMBER || f == XFUNC_MEMBER)

#define XDEBUG_REGISTER_LONG_CONSTANT(__c) REGISTER_LONG_CONSTANT(#__c, __c, CONST_CS|CONST_PERSISTENT)

#define XDEBUG_NONE          0
#define XDEBUG_JIT           1
#define XDEBUG_REQ           2

#define XDEBUG_BREAK         1
#define XDEBUG_STEP          2

#define XDEBUG_INTERNAL      1
#define XDEBUG_EXTERNAL      2

#define XDEBUG_MAX_FUNCTION_LEN 1024

#define XDEBUG_TRACE_OPTION_APPEND       1
#define XDEBUG_TRACE_OPTION_COMPUTERIZED 2
#define XDEBUG_TRACE_OPTION_HTML         4

#define XDEBUG_CC_OPTION_UNUSED          1
#define XDEBUG_CC_OPTION_DEAD_CODE       2

#define STATUS_STARTING   0
#define STATUS_STOPPING   1
#define STATUS_STOPPED    2
#define STATUS_RUNNING    3
#define STATUS_BREAK      4

#define REASON_OK         0
#define REASON_ERROR      1
#define REASON_ABORTED    2
#define REASON_EXCEPTION  3

#define XDEBUG_ERROR_OK                              0
#define XDEBUG_ERROR_PARSE                           1
#define XDEBUG_ERROR_DUP_ARG                         2
#define XDEBUG_ERROR_INVALID_ARGS                    3
#define XDEBUG_ERROR_UNIMPLEMENTED                   4
#define XDEBUG_ERROR_COMMAND_UNAVAILABLE             5

#define XDEBUG_ERROR_CANT_OPEN_FILE                100
#define XDEBUG_ERROR_STREAM_REDIRECT_FAILED        101 /* unused */

#define XDEBUG_ERROR_BREAKPOINT_NOT_SET            200
#define XDEBUG_ERROR_BREAKPOINT_TYPE_NOT_SUPPORTED 201
#define XDEBUG_ERROR_BREAKPOINT_INVALID            202
#define XDEBUG_ERROR_BREAKPOINT_NO_CODE            203
#define XDEBUG_ERROR_BREAKPOINT_INVALID_STATE      204
#define XDEBUG_ERROR_NO_SUCH_BREAKPOINT            205
#define XDEBUG_ERROR_EVALUATING_CODE               206
#define XDEBUG_ERROR_INVALID_EXPRESSION            207 /* unused */

#define XDEBUG_ERROR_PROPERTY_NON_EXISTANT         300
#define XDEBUG_ERROR_STACK_DEPTH_INVALID           301
#define XDEBUG_ERROR_CONTEXT_INVALID               302 /* unused */

#define XDEBUG_ERROR_PROFILING_NOT_STARTED         400

#define XDEBUG_ERROR_ENCODING_NOT_SUPPORTED        900

typedef struct _xdebug_func {
	char *class;
	char *function;
	int   type;
	int   internal;
} xdebug_func;

typedef struct _xdebug_call_entry {
	int         type; /* 0 = function call, 1 = line */
	int         user_defined;
	char       *filename;
	char       *function;
	int         lineno;
	double      time_taken;
} xdebug_call_entry;

typedef struct xdebug_aggregate_entry {
	int         user_defined;
	char       *filename;
	char       *function;
	int         lineno;
	int         call_count;
	double      time_own;
	double      time_inclusive;
	HashTable  *call_list;
} xdebug_aggregate_entry;

typedef struct xdebug_profile {
	double        time;
	double        mark;
	long          memory;
	xdebug_llist *call_list;
} xdebug_profile;

typedef struct _function_stack_entry {
	/* function properties */
	xdebug_func  function;
	int          user_defined;

	/* location properties */
	int          level;
	char        *filename;
	int          lineno;
	char        *include_filename;

	/* argument properties */
	int          arg_done;
	int          varc;
	xdebug_var   *var;
	zval        *return_value;
	xdebug_llist *used_vars;
	HashTable   *symbol_table;

	/* tracing properties */
	signed long  memory;
	signed long  prev_memory;
	double       time;

	/* profiling properties */
	xdebug_profile profile;
#if 0
	double       time_taken;	
	unsigned int f_calls;
#endif

	/* misc properties */
	int          refcount;
	struct _function_stack_entry *prev;
	zend_op_array *op_array;
	xdebug_aggregate_entry *aggr_entry;
} function_stack_entry;

function_stack_entry *xdebug_get_stack_head(TSRMLS_D);
function_stack_entry *xdebug_get_stack_frame(int nr TSRMLS_DC);
function_stack_entry *xdebug_get_stack_tail(TSRMLS_D);

xdebug_hash* xdebug_used_var_hash_from_llist(xdebug_llist *list);

#endif


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
