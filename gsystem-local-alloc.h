/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Colin Walters <walters@verbum.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GSYSTEM_LOCAL_ALLOC_H__
#define __GSYSTEM_LOCAL_ALLOC_H__

#include <gio/gio.h>

G_BEGIN_DECLS

/* These functions shouldn't be invoked directly;
 * they are stubs that:
 * 1) Take a pointer to the location (typically itself a pointer).
 * 2) Provide %NULL-safety where it doesn't exist already (e.g. g_object_unref)
 */
void gs_local_free (void *loc);
void gs_local_obj_unref (void *loc);
void gs_local_variant_unref (void *loc);
void gs_local_ptrarray_unref (void *loc);
void gs_local_hashtable_unref (void *loc);

/**
 * gs_lfree:
 *
 * Call g_free() on a variable location when it goes out of scope.
 */
#define gs_lfree __attribute__ ((cleanup(gs_local_free)))

/**
 * gs_lobj:
 *
 * Call g_object_unref() on a variable location when it goes out of
 * scope.  Note that unlike g_object_unref(), the variable may be
 * %NULL.
 */
#define gs_lobj __attribute__ ((cleanup(gs_local_obj_unref)))

/**
 * gs_lvariant:
 *
 * Call g_variant_unref() on a variable location when it goes out of
 * scope.  Note that unlike g_variant_unref(), the variable may be
 * %NULL.
 */
#define gs_lvariant __attribute__ ((cleanup(gs_local_variant_unref)))

/**
 * gs_lptrarray:
 *
 * Call g_ptr_array_unref() on a variable location when it goes out of
 * scope.  Note that unlike g_ptr_array_unref(), the variable may be
 * %NULL.

 */
#define gs_lptrarray __attribute__ ((cleanup(gs_local_ptrarray_unref)))

/**
 * gs_lhash:
 *
 * Call g_hash_table_unref() on a variable location when it goes out
 * of scope.  Note that unlike g_hash_table_unref(), the variable may
 * be %NULL.
 */
#define gs_lhash __attribute__ ((cleanup(gs_local_hashtable_unref)))

G_END_DECLS

#endif
