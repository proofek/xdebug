/* $Id: srm_llist.c,v 1.1.1.1 2002-04-24 14:26:19 derick Exp $ */

/* The contents of this file are subject to the Vulcan Logic Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.vl-srm.net/vlpl/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is vl-srm.net code.
 *
 * The Initial Developer of the Original Code is the Vulcan Logic 
 * Group.  Portions created by Vulcan Logic Group are Copyright (C) 
 * 2000, 2001, 2002 Vulcan Logic Group. All Rights Reserved.
 *
 * Author(s): Sterling Hughes <sterling@php.net> 
 */

#include <stdlib.h>
#include <string.h>

#include "srm_llist.h"

srm_llist *srm_llist_alloc(srm_llist_dtor dtor)
{
	srm_llist *l;

	l = malloc(sizeof(srm_llist));
	l->size = 0;
	l->dtor = dtor;
	l->head = NULL;
	l->tail = NULL;

	return l;
}

int srm_llist_insert_next(srm_llist *l, srm_llist_element *e, const void *p)
{
	srm_llist_element  *ne;

	if (!e) {
		e = SRM_LLIST_TAIL(l);
	}

	ne = (srm_llist_element *) malloc(sizeof(srm_llist_element));
	ne->ptr = (void *) p;
	if (l->size == 0) {
		l->head = ne;
		l->head->prev = NULL;
		l->head->next = NULL;
		l->tail = ne;
	} else {
		ne->next = e->next;
		ne->prev = e;
		if (e->next) {
			e->next->prev = ne;
		} else {
			l->tail = ne;
		}
		e->next = ne;
	}

	++l->size;

	return 1;
}

int srm_llist_insert_prev(srm_llist *l, srm_llist_element *e, const void *p)
{
	srm_llist_element *ne;

	if (!e) {
		e = SRM_LLIST_HEAD(l);
	}

	ne = (srm_llist_element *) malloc(sizeof(srm_llist_element));
	ne->ptr = (void *) p;
	if (l->size == 0) {
		l->head = ne;
		l->head->prev = NULL;
		l->head->next = NULL;
		l->tail = ne;
	} else {
		ne->next = e;
		ne->prev = e->prev;
		if (e->prev)
			e->prev->next = ne;
		else
			l->head = ne;
		e->prev = ne;
	}

	++l->size;

	return 0;
}

int srm_llist_remove(srm_llist *l, srm_llist_element *e, void *user)
{
	if (e == NULL || l->size == 0)
		return 0;

	if (e == l->head) {
		l->head = e->next;

		if (l->head == NULL)
			l->tail = NULL;
		else
			e->next->prev = NULL;
	} else {
		e->prev->next = e->next;
		if (!e->next)
			l->tail = e->prev;
		else
			e->next->prev = e->prev;
	}

	l->dtor(user, e->ptr);
	free(e);
	--l->size;

	return 0;
}

int srm_llist_remove_next(srm_llist *l, srm_llist_element *e, void *user)
{
	return srm_llist_remove(l, e->next, user);
}

int srm_llist_remove_prev(srm_llist *l, srm_llist_element *e, void *user)
{
	return srm_llist_remove(l, e->prev, user);
}

srm_llist_element *srm_llist_jump(srm_llist *l, int where, int pos)
{
    srm_llist_element *e=NULL;
    int i;

    if (where == LIST_HEAD) {
        e = SRM_LLIST_HEAD(l);
        for (i = 0; i < pos; ++i) {
            e = SRM_LLIST_NEXT(e);
        }
    }
    else if (where == LIST_TAIL) {
        e = SRM_LLIST_TAIL(l);
        for (i = 0; i < pos; ++i) {
            e = SRM_LLIST_PREV(e);
        }
    }

    return e;
}

size_t srm_llist_count(srm_llist *l)
{
	return l->size;
}

void srm_llist_destroy(srm_llist *l, void *user)
{
	while (srm_llist_count(l) > 0) {
		srm_llist_remove(l, SRM_LLIST_TAIL(l), user);
	}

	free (l);
	l = NULL;
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * tab-width: 4
 * End:
 * vim600: fdm=marker
 * vim: noet sw=4 ts=4
 */
