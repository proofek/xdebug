/*
   +----------------------------------------------------------------------+
   | PHP Version 4                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997, 1998, 1999, 2000, 2001 The PHP Group             |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors:  Derick Rethans <d.rethans@jdimedia.nl>                     |
   +----------------------------------------------------------------------+
 */

#ifndef __HAVE_XDEBUG_HANDLER_GDB_H__
#define __HAVE_XDEBUG_HANDLER_GDB_H__

#include "xdebug_handlers.h"

int xdebug_gdb_init(xdebug_con context);
int xdebug_gdb_deinit(xdebug_con context);
int xdebug_gdb_error(xdebug_con context, int type, char *message, const char *location, const uint line, xdebug_llist *stack);

#define xdebug_handler_gdb { \
	xdebug_gdb_init,         \
	xdebug_gdb_deinit,       \
	xdebug_gdb_error         \
}

#endif

