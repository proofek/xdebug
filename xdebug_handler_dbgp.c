/*
   +----------------------------------------------------------------------+
   | Xdebug                                                               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2002, 2003 Derick Rethans                              |
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

#include <sys/types.h>
#include <unistd.h>

#include "php.h"
#include "ext/standard/base64.h"
#include "ext/standard/php_string.h"
#include "ext/standard/url.h"
#include "main/php_version.h"
#include "TSRM.h"
#include "php_globals.h"
#include "php_xdebug.h"
#include "xdebug_com.h"
#include "xdebug_handler_dbgp.h"
#include "xdebug_hash.h"
#include "xdebug_llist.h"
#include "xdebug_mm.h"
#include "xdebug_var.h"
#include "xdebug_xml.h"

#ifdef PHP_WIN32
#include "win32/time.h"
#include <process.h>
#endif
#include <fcntl.h>

ZEND_EXTERN_MODULE_GLOBALS(xdebug)

/*****************************************************************************
** Constants and strings for statii and reasons
*/

/* Status structure */
#define DBGP_STATUS_STARTING  1
#define DBGP_STATUS_STOPPING  2
#define DBGP_STATUS_STOPPED   3
#define DBGP_STATUS_RUNNING   4
#define DBGP_STATUS_BREAK     5

char *xdebug_dbgp_status_strings[6] =
	{"", "starting", "stopping", "stopped", "running", "break"};

#define DBGP_REASON_OK        0
#define DBGP_REASON_ERROR     1
#define DBGP_REASON_ABORTED   2
#define DBGP_REASON_EXCEPTION 3

char *xdebug_dbgp_reason_strings[4] =
	{"ok", "error", "aborted", "exception"};


/*****************************************************************************
** Prototypes for debug command handlers
*/

/* DBGP_FUNC(break); */
DBGP_FUNC(breakpoint_disable);
DBGP_FUNC(breakpoint_enable);
DBGP_FUNC(breakpoint_get);
DBGP_FUNC(breakpoint_list);
DBGP_FUNC(breakpoint_remove);
DBGP_FUNC(breakpoint_set);

DBGP_FUNC(context_get);
DBGP_FUNC(context_names);

DBGP_FUNC(eval);
DBGP_FUNC(feature_get);
DBGP_FUNC(feature_set);

DBGP_FUNC(property_get);
DBGP_FUNC(property_set);
DBGP_FUNC(property_value);

DBGP_FUNC(source);
DBGP_FUNC(stack_depth);
DBGP_FUNC(stack_get);
DBGP_FUNC(status);

DBGP_FUNC(stderr);
DBGP_FUNC(stdout);

DBGP_FUNC(kill);
DBGP_FUNC(run);
DBGP_FUNC(step_into);
DBGP_FUNC(step_out);
DBGP_FUNC(step_over);
DBGP_FUNC(stop);

/*****************************************************************************
** Dispatcher tables for supported debug commands
*/

static xdebug_dbgp_cmd dbgp_commands[] = {
	/* DBGP_FUNC_ENTRY(break) */
	DBGP_FUNC_ENTRY(breakpoint_disable)
	DBGP_FUNC_ENTRY(breakpoint_enable)
	DBGP_FUNC_ENTRY(breakpoint_get)
	DBGP_FUNC_ENTRY(breakpoint_list)
	DBGP_FUNC_ENTRY(breakpoint_remove)
	DBGP_FUNC_ENTRY(breakpoint_set)

	DBGP_FUNC_ENTRY(context_get)
	DBGP_FUNC_ENTRY(context_names)

	DBGP_FUNC_ENTRY(eval)
	DBGP_FUNC_ENTRY(feature_get)
	DBGP_FUNC_ENTRY(feature_set)

	DBGP_FUNC_ENTRY(property_get)
	DBGP_FUNC_ENTRY(property_set)
	DBGP_FUNC_ENTRY(property_value)

	DBGP_FUNC_ENTRY(source)
	DBGP_FUNC_ENTRY(stack_depth)
	DBGP_FUNC_ENTRY(stack_get)
	DBGP_FUNC_ENTRY(status)

	DBGP_FUNC_ENTRY(stderr)
	DBGP_FUNC_ENTRY(stdout)

	DBGP_CONT_FUNC_ENTRY(kill)
	DBGP_CONT_FUNC_ENTRY(run)
	DBGP_CONT_FUNC_ENTRY(step_into)
	DBGP_CONT_FUNC_ENTRY(step_out)
	DBGP_CONT_FUNC_ENTRY(step_over)
	DBGP_CONT_FUNC_ENTRY(stop)
	{ NULL, NULL }
};

/*****************************************************************************
** Utility functions
*/

static xdebug_dbgp_cmd* lookup_cmd(char *cmd)
{
	xdebug_dbgp_cmd *ptr = dbgp_commands;

	while (ptr->name) {
		if (strcmp(ptr->name, cmd) == 0) {
			return ptr;
		}
		ptr++;
	}
	return NULL;
}

static char *make_message(xdebug_con *context, xdebug_xml_node *message, int *len)
{
	xdebug_str  xml_message = {0, 0, NULL};
	xdebug_str *ret;

	xdebug_str_ptr_init(ret);

	xdebug_xml_return_node(message, &xml_message);

	xdebug_str_add(ret, xdebug_sprintf("%d", xml_message.l), 1);
	xdebug_str_addl(ret, "\0", 1, 0);
	xdebug_str_add(ret, xml_message.d, 0);
	xdebug_str_addl(ret, "\0", 1, 0);
	xdebug_str_dtor(xml_message);

	*len = ret->l;
	return ret->d;
}

static void send_message(xdebug_con *context, xdebug_xml_node *message)
{
	char *tmp;
	int   len;

	tmp = make_message(context, message, &len);
	SSENDL(context->socket, tmp, len);
}


/*****************************************************************************
** Data returning functions
*/
static xdebug_xml_node* get_symbol_contents(char* name, int name_length)
{
	HashTable           *st = NULL;
	zval               **retval;
	TSRMLS_FETCH();

	st = XG(active_symbol_table);
	if (st && zend_hash_find(st, name, name_length, (void **) &retval) == SUCCESS) {
		return get_zval_value_xml_node(name, *retval);
	}

	st = EG(active_op_array)->static_variables;
	if (st) {
		if (zend_hash_find(st, name, name_length, (void **) &retval) == SUCCESS) {
			return get_zval_value_xml_node(name, *retval);
		}
	}
	
	st = &EG(symbol_table);
	if (zend_hash_find(st, name, name_length, (void **) &retval) == SUCCESS) {
		return get_zval_value_xml_node(name, *retval);
	}

	return NULL;
}

static char* return_source(char *file, int begin, int end TSRMLS_DC)
{
	int    fd;
	fd_buf fd_buffer = { NULL, 0 };
	int    i = begin;
	char  *line = NULL;
	xdebug_str source = { 0, 0, NULL };

	if (i < 0) {
		begin = 0;
		i = 0;
	}

	/* Read until the "begin" line has been read */
	if ((fd = open(file, 0)) == -1) {
		return NULL;
	}
	
	while (i > 0) {
		if (line) {
			free(line);
			line = NULL;
		}
		line = fd_read_line(fd, &fd_buffer, FD_RL_FILE);
		i--;
	}
	/* Read until the "end" line has been read */
	do {
		if (line) {
			xdebug_str_add(&source, line, 0);
			xdebug_str_addl(&source, "\n", 1, 0);
			free(line);
			line = NULL;
		}
		line = fd_read_line(fd, &fd_buffer, FD_RL_FILE);
		i++;
	} while (i < end + 1 - begin);

	/* Print last line */
	if (line) {
		free(line);
		line = NULL;
	}
	return source.d;
}

static xdebug_xml_node* return_breakpoint(function_stack_entry *i)
{
	char            *tmp_fname;
	xdebug_xml_node *tmp;
	TSRMLS_FETCH();

	tmp_fname = show_fname(i, 0 TSRMLS_CC);

	tmp = xdebug_xml_node_init("breakpoint");
	xdebug_xml_add_attribute_ex(tmp, "function", xdstrdup(tmp_fname), 0, 1);
	xdebug_xml_add_attribute_ex(tmp, "filename", xdstrdup(i->filename), 0, 1);
	xdebug_xml_add_attribute_ex(tmp, "lineno",   xdebug_sprintf("%ld", i->lineno), 0, 1);

	xdfree(tmp_fname);
	return tmp;
}

static xdebug_xml_node* return_stackframe(int nr)
{
	xdebug_llist_element *le = XDEBUG_LLIST_TAIL(XG(stack));
	int                   count_down = nr;
	function_stack_entry *i; 
	char                 *tmp_fname;
	xdebug_xml_node      *tmp;
	TSRMLS_FETCH();
	
	while (count_down) {
		count_down--;
		le = XDEBUG_LLIST_PREV(le);
	}
	i = XDEBUG_LLIST_VALP(le);

	tmp_fname = show_fname(i, 0 TSRMLS_CC);

	tmp = xdebug_xml_node_init("stack");
	xdebug_xml_add_attribute_ex(tmp, "function", xdstrdup(tmp_fname), 0, 1);
	xdebug_xml_add_attribute_ex(tmp, "filename", xdstrdup(i->filename), 0, 1);
	xdebug_xml_add_attribute_ex(tmp, "level",    xdebug_sprintf("%ld", nr), 0, 1);
	xdebug_xml_add_attribute_ex(tmp, "lineno",   xdebug_sprintf("%ld", i->lineno), 0, 1);

	xdfree(tmp_fname);
	return tmp;
}

/*****************************************************************************
** Client command handlers - Breakpoints
*/

/* Helper functions */
void xdebug_hash_admin_dtor(xdebug_brk_admin *admin)
{
	xdfree(admin->key);
	xdfree(admin);
}

static int breakpoint_admin_add(xdebug_con *context, int type, char *key)
{
	xdebug_brk_admin *admin = xdmalloc(sizeof(xdebug_brk_admin));
	char             *hkey;

	XG(breakpoint_count)++;
	admin->id   = getpid() * 10000 + XG(breakpoint_count);
	admin->type = type;
	admin->key  = xdstrdup(key);

	hkey = xdebug_sprintf("%lu", admin->id);
	xdebug_hash_add(context->breakpoint_list, hkey, strlen(hkey), (void*) admin);
	xdfree(hkey);

	return admin->id;
}

static int breakpoint_admin_fetch(xdebug_con *context, char *hkey, int *type, char **key)
{
	xdebug_brk_admin *admin;

	if (xdebug_hash_find(context->breakpoint_list, hkey, strlen(hkey), (void *) &admin)) {
		*type = admin->type;
		*key  = admin->key;
		return SUCCESS;
	} else {
		return FAILURE;
	}

}

static int breakpoint_admin_remove(xdebug_con *context, char *hkey)
{
	if (xdebug_hash_delete(context->breakpoint_list, hkey, strlen(hkey))) {
		return SUCCESS;
	} else {
		return FAILURE;
	}
}

DBGP_FUNC(breakpoint_disable)
{
	RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_UNIMPLEMENTED);
}

DBGP_FUNC(breakpoint_enable)
{
	RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_UNIMPLEMENTED);
}

static void breakpoint_brk_info_add(xdebug_xml_node *xml, xdebug_brk_info *brk)
{
	if (brk->file) {
		xdebug_xml_add_attribute_ex(xml, "filename", xdstrdup(brk->file), 0, 1);
	}
	if (brk->lineno) {
		xdebug_xml_add_attribute_ex(xml, "lineno", xdebug_sprintf("%lu", brk->lineno), 0, 1);
	}
	if (brk->functionname) {
		xdebug_xml_add_attribute_ex(xml, "function", xdstrdup(brk->functionname), 0, 1);
	}
	if (brk->classname) {
		xdebug_xml_add_attribute_ex(xml, "class", xdstrdup(brk->classname), 0, 1);
	}
}

static xdebug_brk_info* breakpoint_brk_info_fetch(int type, char *hkey)
{
	xdebug_llist_element *le;
	xdebug_brk_info      *brk = NULL;
	xdebug_arg           *parts = (xdebug_arg*) xdmalloc(sizeof(xdebug_arg));

	switch (type) {
		case BREAKPOINT_TYPE_LINE:
			/* First we split the key into filename and linenumber */
			xdebug_arg_init(parts);
			xdebug_explode("$", hkey, parts, -1);

			/* Second we loop through the list of file/line breakpoints to
			 * look for our thingy */
			for (le = XDEBUG_LLIST_HEAD(XG(context).line_breakpoints); le != NULL; le = XDEBUG_LLIST_NEXT(le)) {
				brk = XDEBUG_LLIST_VALP(le);

				if (atoi(parts->args[1]) == brk->lineno && memcmp(brk->file, parts->args[0], brk->file_len) == 0) {
					xdebug_arg_dtor(parts);
					return brk;
				}
			}

			/* Cleaning up */
			xdebug_arg_dtor(parts);
			break;

		case BREAKPOINT_TYPE_FUNCTION:
			if (xdebug_hash_find(XG(context).function_breakpoints, hkey, strlen(hkey), (void *) &brk)) {
				return brk;
			}
			break;

		case BREAKPOINT_TYPE_METHOD:
			if (xdebug_hash_find(XG(context).class_breakpoints, hkey, strlen(hkey), (void *) &brk)) {
				return brk;
			}
			break;
	}
	return brk;
}

static int breakpoint_remove(int type, char *hkey)
{
	xdebug_llist_element *le;
	xdebug_brk_info      *brk = NULL;
	xdebug_arg           *parts = (xdebug_arg*) xdmalloc(sizeof(xdebug_arg));

	switch (type) {
		case BREAKPOINT_TYPE_LINE:
			/* First we split the key into filename and linenumber */
			xdebug_arg_init(parts);
			xdebug_explode("$", hkey, parts, -1);

			/* Second we loop through the list of file/line breakpoints to
			 * look for our thingy */
			for (le = XDEBUG_LLIST_HEAD(XG(context).line_breakpoints); le != NULL; le = XDEBUG_LLIST_NEXT(le)) {
				brk = XDEBUG_LLIST_VALP(le);

				if (atoi(parts->args[1]) == brk->lineno && memcmp(brk->file, parts->args[2], brk->file_len) == 0) {
					xdebug_llist_remove(XG(context).line_breakpoints, le, NULL);
					return SUCCESS;
				}
			}

			/* Cleaning up */
			xdebug_arg_dtor(parts);
			break;

		case BREAKPOINT_TYPE_FUNCTION:
			if (xdebug_hash_delete(XG(context).function_breakpoints, hkey, strlen(hkey))) {
				return SUCCESS;
			}
			break;

		case BREAKPOINT_TYPE_METHOD:
			if (xdebug_hash_delete(XG(context).class_breakpoints, hkey, strlen(hkey))) {
				return SUCCESS;
			}
			break;
	}
	return FAILURE;
}

DBGP_FUNC(breakpoint_get)
{
	int                   type;
	char                 *hkey;
	xdebug_brk_info      *brk;

	if (!CMD_OPTION('d')) {
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_INVALID_ARGS);
	}
	/* Lets check if it exists */
	if (breakpoint_admin_fetch(context, CMD_OPTION('d'), &type, (char**) &hkey) == SUCCESS) {
		/* so it exists, now we're going to find it in the correct hash/list
		 * and return the info we have on it */
		brk = breakpoint_brk_info_fetch(type, hkey);
		breakpoint_brk_info_add(*retval, brk);
		/* Now we add some common attributes */
		xdebug_xml_add_attribute(*retval, "id", CMD_OPTION('d'));
	} else {
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_NO_SUCH_BREAKPOINT)
	}
}

static void breakpoint_list_helper(void *xml, xdebug_hash_element *he)
{
	xdebug_xml_node  *xml_node = (xdebug_xml_node*) xml;
	xdebug_xml_node  *child;
	xdebug_brk_admin *admin = (xdebug_brk_admin*) he->ptr;
	xdebug_brk_info  *brk;

	child = xdebug_xml_node_init("breakpoint");
	brk = breakpoint_brk_info_fetch(admin->type, admin->key);
	breakpoint_brk_info_add(child, brk);
	xdebug_xml_add_attribute_ex(child, "id", xdebug_sprintf("%lu", admin->id), 0, 1);
	xdebug_xml_add_child(xml_node, child);
}

DBGP_FUNC(breakpoint_list)
{
	xdebug_hash_apply(context->breakpoint_list, (void *) *retval, breakpoint_list_helper);
}

DBGP_FUNC(breakpoint_remove)
{
	int                   type;
	char                 *hkey;
	xdebug_brk_info      *brk;

	if (!CMD_OPTION('d')) {
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_INVALID_ARGS);
	}
	/* Lets check if it exists */
	if (breakpoint_admin_fetch(context, CMD_OPTION('d'), &type, (char**) &hkey)) {
		/* so it exists, now we're going to find it in the correct hash/list
		 * and return the info we have on it */
		brk = breakpoint_brk_info_fetch(type, hkey);
		breakpoint_brk_info_add(*retval, brk);
		/* Now we add some common attributes */
		xdebug_xml_add_attribute(*retval, "id", CMD_OPTION('d'));
		/* Now we remove the crap */
		breakpoint_remove(type, hkey);
		breakpoint_admin_remove(context, CMD_OPTION('d'));
	} else {
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_NO_SUCH_BREAKPOINT)
	}
}

DBGP_FUNC(breakpoint_set)
{
	xdebug_brk_info *extra_brk_info;
	char            *tmp_name;
	int              brk_id = 0;

	extra_brk_info = xdmalloc(sizeof(xdebug_brk_info));
	extra_brk_info->file = NULL;
	extra_brk_info->classname = NULL;
	extra_brk_info->functionname = NULL;
	extra_brk_info->condition = NULL;

	if (!CMD_OPTION('t')) {
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_INVALID_ARGS);
	}
	if (strcmp(CMD_OPTION('t'), "line") == 0) {
		if (!CMD_OPTION('f') || !CMD_OPTION('n')) {
			RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_INVALID_ARGS);
		}
		extra_brk_info->file = xdstrdup(CMD_OPTION('f'));
		extra_brk_info->file_len = strlen(extra_brk_info->file);
		extra_brk_info->lineno = strtol(CMD_OPTION('n'), NULL, 10);

		tmp_name = xdebug_sprintf("%s$%s", CMD_OPTION('f'), CMD_OPTION('n'));
		brk_id = breakpoint_admin_add(context, BREAKPOINT_TYPE_LINE, tmp_name);
		xdfree(tmp_name);
		xdebug_llist_insert_next(context->line_breakpoints, XDEBUG_LLIST_TAIL(context->line_breakpoints), (void*) extra_brk_info);
	} else 

	if (strcmp(CMD_OPTION('t'), "function") == 0) {
		if (!CMD_OPTION('m')) {
			RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_INVALID_ARGS);
		}
		extra_brk_info->functionname = xdstrdup(CMD_OPTION('m'));
		if (CMD_OPTION('a')) {
			int   res;

			extra_brk_info->classname = xdstrdup(CMD_OPTION('a'));
			tmp_name = xdebug_sprintf("%s::%s", CMD_OPTION('a'), CMD_OPTION('m'));
			res = xdebug_hash_add(context->class_breakpoints, tmp_name, strlen(tmp_name), (void*) extra_brk_info);
			brk_id = breakpoint_admin_add(context, BREAKPOINT_TYPE_METHOD, tmp_name);
			xdfree(tmp_name);

			if (!res) {
				RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_INVALID_ARGS);
			}
		} else {
			if (!xdebug_hash_add(context->function_breakpoints, CMD_OPTION('m'), strlen(CMD_OPTION('m')), (void*) extra_brk_info)) {
				RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_BREAKPOINT_NOT_SET);
			} else {
				brk_id = breakpoint_admin_add(context, BREAKPOINT_TYPE_FUNCTION, CMD_OPTION('m'));
			}
		}
	} else

	if (strcmp(CMD_OPTION('t'), "exception") == 0) {
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_BREAKPOINT_TYPE_NOT_SUPPORTED);
	} else

	if (strcmp(CMD_OPTION('t'), "watch") == 0) {
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_BREAKPOINT_TYPE_NOT_SUPPORTED);
	}

	xdebug_xml_add_attribute_ex(*retval, "id", xdebug_sprintf("%d", brk_id), 0, 1);
}

DBGP_FUNC(eval)
{
	int              old_error_reporting;
	char            *eval_string;
	xdebug_xml_node *ret_xml;
	zval             ret_zval;
	int              new_length;
	int              res;

	if (!CMD_OPTION('v')) {
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_INVALID_ARGS);
	}

	/* Remember error reporting level */
	old_error_reporting = EG(error_reporting);
	EG(error_reporting) = 0;

	/* base64 decode eval string */
	eval_string = php_base64_decode(CMD_OPTION('v'), strlen(CMD_OPTION('v')), &new_length);

	/* Do evaluation */
	XG(breakpoints_allowed) = 0;
	res = zend_eval_string(eval_string, &ret_zval, "xdebug eval" TSRMLS_CC);

	/* Clean up */
	EG(error_reporting) = old_error_reporting;
	XG(breakpoints_allowed) = 1;
	efree(eval_string);

	/* Handle result */
	if (res == FAILURE) {
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_EVALUATING_CODE);
	} else {
		ret_xml = get_zval_value_xml_node(NULL, &ret_zval);
		xdebug_xml_add_child(*retval, ret_xml);
		zval_dtor(&ret_zval);
	}
}


DBGP_FUNC(stderr)
{
	RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_UNIMPLEMENTED);
}

DBGP_FUNC(stdout)
{
	RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_UNIMPLEMENTED);
}


DBGP_FUNC(kill)
{
	zend_bailout();
}

DBGP_FUNC(run)
{
	xdebug_xml_add_attribute_ex(*retval, "filename", xdstrdup(context->program_name), 0, 1);
}

DBGP_FUNC(step_into)
{
	XG(context).do_next   = 0;
	XG(context).do_step   = 1;
	XG(context).do_finish = 0;
}

DBGP_FUNC(step_out)
{
	xdebug_llist_element *le;
	function_stack_entry *fse;

	XG(context).do_next   = 0;
	XG(context).do_step   = 0;
	XG(context).do_finish = 1;

	if (XG(stack)) {
		le = XDEBUG_LLIST_TAIL(XG(stack));
		fse = XDEBUG_LLIST_VALP(le);
		XG(context).next_level = fse->level - 1;
	} else {
		XG(context).next_level = -1;
	}
}

DBGP_FUNC(step_over)
{
	xdebug_llist_element *le;
	function_stack_entry *fse;

	XG(context).do_next   = 1;
	XG(context).do_step   = 0;
	XG(context).do_finish = 0;

	if (XG(stack)) {
		le = XDEBUG_LLIST_TAIL(XG(stack));
		fse = XDEBUG_LLIST_VALP(le);
		XG(context).next_level = fse->level;
	} else {
		XG(context).next_level = 0;
	}
}

DBGP_FUNC(stop)
{
	XG(remote_enabled) = 0;
}


DBGP_FUNC(source)
{
	char *source, *encoded_source;
	int   new_len;

	if (!CMD_OPTION('f')) {
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_INVALID_ARGS);
	}

	if (CMD_OPTION('b') && CMD_OPTION('e')) {
		source = return_source(CMD_OPTION('f'), strtol(CMD_OPTION('b'), NULL, 10), strtol(CMD_OPTION('e'), NULL, 10) TSRMLS_CC);
	} else {
		source = return_source(CMD_OPTION('f'), 0, 999999 TSRMLS_CC);
	}
	if (!source) {
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_CANT_OPEN_FILE);
	} else {
		xdebug_xml_add_attribute(*retval, "encoding", "base64");
		encoded_source = php_base64_encode(source, strlen(source), &new_len);
		xdebug_xml_add_text(*retval, xdstrdup(encoded_source));
		efree(encoded_source);
		xdfree(source);
	}
}

#define XDEBUG_STR_SWITCH_DECL       char *__switch_variable
#define XDEBUG_STR_SWITCH(s)         __switch_variable = (s);
#define XDEBUG_STR_CASE(s)           if (strcmp(__switch_variable, s) == 0) {
#define XDEBUG_STR_CASE_END          } else
#define XDEBUG_STR_CASE_DEFAULT      {
#define XDEBUG_STR_CASE_DEFAULT_END  }

DBGP_FUNC(feature_get)
{
	xdebug_dbgp_options *options;
	XDEBUG_STR_SWITCH_DECL;

	options = (xdebug_dbgp_options*) context->options;

	if (!CMD_OPTION('n')) {
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_INVALID_ARGS);
	}

	XDEBUG_STR_SWITCH(CMD_OPTION('n')) {
		XDEBUG_STR_CASE("encoding")
			xdebug_xml_add_text(*retval, xdstrdup("UTF-8"));
		XDEBUG_STR_CASE_END

		XDEBUG_STR_CASE("language_name")
			xdebug_xml_add_text(*retval, xdstrdup("PHP"));
		XDEBUG_STR_CASE_END

		XDEBUG_STR_CASE("language_supports_threads")
			xdebug_xml_add_attribute(*retval, "supported", "0");
		XDEBUG_STR_CASE_END
		
		XDEBUG_STR_CASE("language_version")
			xdebug_xml_add_text(*retval, xdstrdup(PHP_VERSION));
		XDEBUG_STR_CASE_END

		XDEBUG_STR_CASE("max_children")
			xdebug_xml_add_text(*retval, xdebug_sprintf("%l", options->max_children));
		XDEBUG_STR_CASE_END

		XDEBUG_STR_CASE("max_data")
			xdebug_xml_add_text(*retval, xdebug_sprintf("%l", options->max_data));
		XDEBUG_STR_CASE_END

		XDEBUG_STR_CASE("max_depth")
			xdebug_xml_add_text(*retval, xdebug_sprintf("%l", options->max_depth));
		XDEBUG_STR_CASE_END

		XDEBUG_STR_CASE("protocol_version")
			xdebug_xml_add_text(*retval, xdstrdup("1"));
		XDEBUG_STR_CASE_END

		XDEBUG_STR_CASE("supported_encodings")
			xdebug_xml_add_text(*retval, xdstrdup("UTF-8"));
		XDEBUG_STR_CASE_END

		XDEBUG_STR_CASE("supports_async")
			xdebug_xml_add_attribute(*retval, "supported", "0");
		XDEBUG_STR_CASE_END

		XDEBUG_STR_CASE_DEFAULT
			/* We need to check if the parameter exists as command */
		XDEBUG_STR_CASE_DEFAULT_END
	}
}

DBGP_FUNC(feature_set)
{
	xdebug_dbgp_options *options;
	XDEBUG_STR_SWITCH_DECL;

	options = (xdebug_dbgp_options*) context->options;

	if (!CMD_OPTION('n') || !CMD_OPTION('v')) {
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_INVALID_ARGS);
	}

	XDEBUG_STR_SWITCH(CMD_OPTION('n')) {

		XDEBUG_STR_CASE("encoding")
			if (strcmp(CMD_OPTION('v'), "UTF-8") != 0) {
				RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_ENCODING_NOT_SUPPORTED);
			}
		XDEBUG_STR_CASE_END

		XDEBUG_STR_CASE("max_children")
			options->max_children = strtol(CMD_OPTION('v'), NULL, 10);
		XDEBUG_STR_CASE_END

		XDEBUG_STR_CASE("max_data")
			options->max_data = strtol(CMD_OPTION('v'), NULL, 10);
		XDEBUG_STR_CASE_END

		XDEBUG_STR_CASE("max_depth")
			options->max_depth = strtol(CMD_OPTION('v'), NULL, 10);
		XDEBUG_STR_CASE_END

		XDEBUG_STR_CASE("multiple_sessions")
		XDEBUG_STR_CASE_END

		XDEBUG_STR_CASE_DEFAULT
			RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_INVALID_ARGS);
		XDEBUG_STR_CASE_DEFAULT_END
	}
}

DBGP_FUNC(property_get)
{
	xdebug_xml_node *var_data;

	if (!CMD_OPTION('n')) {
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_INVALID_ARGS);
	}

	XG(active_symbol_table) = EG(active_symbol_table);
	var_data = get_symbol_contents(CMD_OPTION('n'), strlen(CMD_OPTION('n')) + 1);
	XG(active_symbol_table) = NULL;

	if (var_data) {
		xdebug_xml_add_child(*retval, var_data);
	} else {
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_PROPERTY_NON_EXISTANT);
	}
}

DBGP_FUNC(property_set)
{
	RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_UNIMPLEMENTED);
}

DBGP_FUNC(property_value)
{
	RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_UNIMPLEMENTED);
}

static void attach_used_var_with_contents(void *xml, xdebug_hash_element* he)
{
	char               *name = (char*) he->ptr;
	xdebug_xml_node    *node = (xdebug_xml_node *) xml;
	xdebug_xml_node    *contents;

	contents = get_symbol_contents(name, strlen(name) + 1);
	if (contents) {
		xdebug_xml_add_child(node, contents);
	} else {
		contents = xdebug_xml_node_init("property");
		xdebug_xml_add_attribute_ex(contents, "name", xdstrdup(name), 0, 1);
		xdebug_xml_add_attribute(contents, "type", "uninitialized");
		xdebug_xml_add_child(node, contents);
	}
}

static int attach_local_vars(xdebug_xml_node *node, long depth, void (*func)(void *, xdebug_hash_element*))
{
	struct function_stack_entry *i;
	xdebug_hash                 *ht;
	
	if (XDEBUG_LLIST_TAIL(XG(stack))) {
		i = XDEBUG_LLIST_VALP(XDEBUG_LLIST_TAIL(XG(stack)));
		ht = i->used_vars;
		XG(active_symbol_table) = i->symbol_table;

		/* Only show vars when they are scanned */
		if (ht) {
			xdebug_hash_apply(ht, (void *) node, func);
		}
	}
	
	XG(active_symbol_table) = NULL;
	return 0;
}


DBGP_FUNC(stack_depth)
{
	xdebug_xml_add_attribute_ex(*retval, "depth", xdebug_sprintf("%lu", XG(level)), 0, 1);
}

DBGP_FUNC(stack_get)
{
	xdebug_xml_node      *stackframe;
	xdebug_llist_element *le;
	int                   counter = 0, depth;

	if (CMD_OPTION('d')) {
		depth = strtol(CMD_OPTION('d'), NULL, 10);
		if (depth >= 0 && depth < XG(level)) {
			stackframe = return_stackframe(depth);
			xdebug_xml_add_child(*retval, stackframe);
		} else {
			RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_UNIMPLEMENTED);
		}
	} else {
		counter = 0;
		for (le = XDEBUG_LLIST_TAIL(XG(stack)); le != NULL; le = XDEBUG_LLIST_PREV(le)) {
			stackframe = return_stackframe(counter);
			xdebug_xml_add_child(*retval, stackframe);
			counter++;
		}
	}
}

DBGP_FUNC(status)
{
	xdebug_xml_add_attribute(*retval, "status", xdebug_dbgp_status_strings[XG(status)]);
	xdebug_xml_add_attribute(*retval, "reason", xdebug_dbgp_reason_strings[XG(reason)]);
}


DBGP_FUNC(context_names)
{
	xdebug_xml_node *child;

	if (CMD_OPTION('d')) {
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_UNIMPLEMENTED);
	} else {
		child = xdebug_xml_node_init("context");
		xdebug_xml_add_attribute(child, "name", "Locals");
		xdebug_xml_add_attribute(child, "id", "0");
		xdebug_xml_add_child(*retval, child);
	}
}

DBGP_FUNC(context_get)
{
	int res;

	if (CMD_OPTION('d')) {
		/* res = attach_local_vars(*retval, atol(CMD_OPTION('d')), attach_used_var_with_contents); */
		RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_UNIMPLEMENTED);
	} else {
		res = attach_local_vars(*retval, 0, attach_used_var_with_contents);
	}
	switch (res) {
		case 1:
			RETURN_RESULT(XG(status), XG(reason), XDEBUG_ERROR_STACK_DEPTH_TOO_HIGH);
			break;
	}
}



/*****************************************************************************
** Parsing functions
*/

/* {{{ Constants for state machine */
#define STATE_NORMAL                   0
#define STATE_QUOTED                   1
#define STATE_OPT_FOLLOWS              2
#define STATE_SEP_FOLLOWS              3
#define STATE_VALUE_FOLLOWS_FIRST_CHAR 4
#define STATE_VALUE_FOLLOWS            5
#define STATE_SKIP_CHAR                6
/* }}} */

void xdebug_dbgp_arg_dtor(xdebug_dbgp_arg *arg)
{
	int i;

	for (i = 0; i < 26; i++) {
		if (arg->value[i]) {
			free(arg->value[i]);
		}
	}
	free(arg);
}

int xdebug_dbgp_parse_cmd(char *line, char **cmd, xdebug_dbgp_arg **ret_args)
{
	xdebug_dbgp_arg *args = NULL;
	char *ptr;
	int   state;
	char  opt = ' ', *value_begin = NULL;

	args = malloc(sizeof (xdebug_dbgp_arg));
	memset(args->value, 0, sizeof(args->value));
	*cmd = NULL;

	/* Find the end of the command, this is always on the first space */
	ptr = strchr(line, ' ');
	if (!ptr) {
		/* No space found. If the line is not empty, return the line
		 * and assume it only consists of the command name. If the line
		 * is 0 chars long, we return a failure. */
		if (strlen(line)) {
			*cmd = strdup(line);
			*ret_args = args;
			return XDEBUG_ERROR_OK;
		} else {
			goto parse_error;
		}
	} else {
		/* A space was found, so we copy everything before it
		 * into the cmd parameter. */
		*cmd = calloc(1, ptr - line + 1);
		memcpy(*cmd, line, ptr - line);
	}
	/* Now we loop until we find the end of the string, which is the \0
	 * character */
	state = STATE_NORMAL;
	do {
		ptr++;
		switch (state) {
			case STATE_NORMAL:
				if (*ptr != '-') {
					goto parse_error;
				} else {
					state = STATE_OPT_FOLLOWS;
				}
				break;
			case STATE_OPT_FOLLOWS:
				opt = *ptr;
				state = STATE_SEP_FOLLOWS;
				break;
			case STATE_SEP_FOLLOWS:
				if (*ptr != ' ') {
					goto parse_error;
				} else {
					state = STATE_VALUE_FOLLOWS_FIRST_CHAR;
					value_begin = ptr + 1;
				}
				break;
			case STATE_VALUE_FOLLOWS_FIRST_CHAR:
				if (*ptr == '"') {
					value_begin = ptr + 1;
					state = STATE_QUOTED;
				} else {
					state = STATE_VALUE_FOLLOWS;
				}
				break;
			case STATE_VALUE_FOLLOWS:
				if (*ptr == ' ' || *ptr == '\0') {

					if (!args->value[opt - 'a']) {
						args->value[opt - 'a'] = calloc(1, ptr - value_begin + 1);
						memcpy(args->value[opt - 'a'], value_begin, ptr - value_begin);
						state = STATE_NORMAL;
					} else {
						goto duplicate_opts;
					}
				}
				break;
			case STATE_QUOTED:
				if (*ptr == '"') {
					if (!args->value[opt - 'a']) {
						args->value[opt - 'a'] = calloc(1, ptr - value_begin + 1);
						memcpy(args->value[opt - 'a'], value_begin, ptr - value_begin);
						state = STATE_SKIP_CHAR;
					} else {
						goto duplicate_opts;
					}
				}
				break;
			case STATE_SKIP_CHAR:
				state = STATE_NORMAL;
				break;
				
		}
	} while (*ptr);
	*ret_args = args;
	return XDEBUG_ERROR_OK;

parse_error:
	*ret_args = NULL;
	xdebug_dbgp_arg_dtor(args);
	if (*cmd) {	
		free(*cmd);
	}
	return XDEBUG_ERROR_PARSE;

duplicate_opts:
	*ret_args = NULL;
	xdebug_dbgp_arg_dtor(args);
	if (*cmd) {	
		free(*cmd);
	}
	return XDEBUG_ERROR_DUP_ARG;
}

int xdebug_dbgp_parse_option(xdebug_con *context, char* line, int flags, xdebug_xml_node *retval)
{
	char *cmd;
	int res;
	xdebug_dbgp_arg *args;
	xdebug_dbgp_cmd *command;
	xdebug_xml_node *error;

	res = xdebug_dbgp_parse_cmd(line, (char**) &cmd, (xdebug_dbgp_arg**) &args);
	if (res != XDEBUG_ERROR_OK) {
		error = xdebug_xml_node_init("error");
		xdebug_xml_add_attribute_ex(error, "code", xdebug_sprintf("%lu", res), 0, 1);
		xdebug_xml_add_child(retval, error);
		return -1;
	}

	command = lookup_cmd(cmd);
	if (command) {
		command->handler((xdebug_xml_node**) &retval, context, args);
		return command->cont;
	} else {
		return -1;
	}
}

/*****************************************************************************
** Handlers for debug functions
*/

int xdebug_dbgp_init(xdebug_con *context, int mode)
{
	char *option;
	int   ret;
	xdebug_dbgp_options *options;
	xdebug_xml_node *response, *child;
	TSRMLS_FETCH();

	response = xdebug_xml_node_init("init");

/* {{{ XML Init Stuff*/
	child = xdebug_xml_node_init("engine");
	xdebug_xml_add_attribute(child, "version", XDEBUG_VERSION);
	xdebug_xml_add_text(child, xdstrdup(XDEBUG_NAME));
	xdebug_xml_add_child(response, child);

	child = xdebug_xml_node_init("author");
	xdebug_xml_add_text(child, xdstrdup(XDEBUG_AUTHOR));
	xdebug_xml_add_child(response, child);

	child = xdebug_xml_node_init("url");
	xdebug_xml_add_text(child, xdstrdup(XDEBUG_URL));
	xdebug_xml_add_child(response, child);

	child = xdebug_xml_node_init("copyright");
	xdebug_xml_add_text(child, xdstrdup(XDEBUG_COPYRIGHT));
	xdebug_xml_add_child(response, child);

	context->buffer = xdmalloc(sizeof(fd_buf));
	context->buffer->buffer = NULL;
	context->buffer->buffer_size = 0;

	send_message(context, response);
/* }}} */

	context->options = xdmalloc(sizeof(xdebug_dbgp_options));
	options = (xdebug_dbgp_options*) context->options;
	options->max_children = 2048;
	options->max_data     = 524288;
	options->max_depth    = 16;

/* {{{ Initialize auto globals in Zend Engine 2 */
#ifdef ZEND_ENGINE_2
	zend_is_auto_global("_ENV",     sizeof("_ENV")-1     TSRMLS_CC);
	zend_is_auto_global("_GET",     sizeof("_GET")-1     TSRMLS_CC);
	zend_is_auto_global("_POST",    sizeof("_POST")-1    TSRMLS_CC);
	zend_is_auto_global("_COOKIE",  sizeof("_COOKIE")-1  TSRMLS_CC);
	zend_is_auto_global("_REQUEST", sizeof("_REQUEST")-1 TSRMLS_CC);
	zend_is_auto_global("_FILES",   sizeof("_FILES")-1   TSRMLS_CC);
	zend_is_auto_global("_SERVER",  sizeof("_SERVER")-1  TSRMLS_CC);
#endif
/* }}} */

	context->breakpoint_list = xdebug_hash_alloc(64, (xdebug_hash_dtor) xdebug_hash_admin_dtor);
	context->function_breakpoints = xdebug_hash_alloc(64, (xdebug_hash_dtor) xdebug_hash_brk_dtor);
	context->class_breakpoints = xdebug_hash_alloc(64, (xdebug_hash_dtor) xdebug_hash_brk_dtor);
	context->line_breakpoints = xdebug_llist_alloc((xdebug_llist_dtor) xdebug_llist_brk_dtor);
	do {
		option = fd_read_line(context->socket, context->buffer, FD_RL_SOCKET);
		if (!option) {
			return 0;
		}

		response = xdebug_xml_node_init("response");
		ret = xdebug_dbgp_parse_option(context, option, 0, response);
		if (ret != 1) {
			send_message(context, response);
		}
		xdebug_xml_node_dtor(response);

		free(option);
	} while (1 != ret);

	return 1;
}

int xdebug_dbgp_deinit(xdebug_con *context)
{
	xdfree(context->options);
	xdebug_hash_destroy(context->function_breakpoints);
	xdebug_hash_destroy(context->class_breakpoints);
	xdebug_llist_destroy(context->line_breakpoints, NULL);
	xdfree(context->buffer);

	return 1;
}

int xdebug_dbgp_error(xdebug_con *context, int type, char *message, const char *location, const uint line, xdebug_llist *stack)
{
	char               *errortype;
	int                 ret;
	char               *option;
	xdebug_xml_node     *response;

	errortype = error_type(type);
/*
	runtime_allowed = (
		(type != E_ERROR) && 
		(type != E_CORE_ERROR) &&
		(type != E_COMPILE_ERROR) &&
		(type != E_USER_ERROR)
	) ? XDEBUG_BREAKPOINT | XDEBUG_RUNTIME : 0;
*/
	response = xdebug_xml_node_init("response");
	xdebug_xml_add_attribute_ex(response, "exception", xdstrdup(errortype), 0, 1);
	xdebug_xml_add_text(response, xdstrdup(message));
	send_message(context, response);
	xdebug_xml_node_dtor(response);
	xdfree(errortype);

	do {
		option = fd_read_line(context->socket, context->buffer, FD_RL_SOCKET);
		if (!option) {
			return 0;
		}

		response = xdebug_xml_node_init("response");
		ret = xdebug_dbgp_parse_option(context, option, 0, response);
		if (ret != 1) {
			send_message(context, response);
		}
		xdebug_xml_node_dtor(response);

		free(option);
	} while (1 != ret);

	return 1;
}

int xdebug_dbgp_breakpoint(xdebug_con *context, xdebug_llist *stack, char *file, int lineno, int type)
{
	struct function_stack_entry *i;
	int    ret;
	char  *option;
	xdebug_xml_node *response;

	i = XDEBUG_LLIST_VALP(XDEBUG_LLIST_TAIL(stack));

	if (type == XDEBUG_BREAK) {
		response = return_breakpoint(i);
		send_message(context, response);
		xdebug_xml_node_dtor(response);
	}

	do {
		option = fd_read_line(context->socket, context->buffer, FD_RL_SOCKET);
		if (!option) {
			return 0;
		}

		response = xdebug_xml_node_init("response");
		ret = xdebug_dbgp_parse_option(context, option, 0, response);
		if (ret != 1) {
			send_message(context, response);
		}
		xdebug_xml_node_dtor(response);

		free(option);
	} while (1 != ret);

	return 1;
}
