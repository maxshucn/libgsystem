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

#ifndef __GD_LOCAL_ALLOC_H__
#define __GD_LOCAL_ALLOC_H__

#include <gio/gio.h>

G_BEGIN_DECLS

void gd_local_free (void *loc);
void gd_local_obj_unref (void *loc);
void gd_local_variant_unref (void *loc);
void gd_local_ptrarray_unref (void *loc);
void gd_local_hashtable_unref (void *loc);

#define gd_lfree __attribute__ ((cleanup(gd_local_free)))
#define gd_lobj __attribute__ ((cleanup(gd_local_obj_unref)))
#define gd_lvariant __attribute__ ((cleanup(gd_local_variant_unref)))
#define gd_lptrarray __attribute__ ((cleanup(gd_local_ptrarray_unref)))
#define gd_lhash __attribute__ ((cleanup(gd_local_hashtable_unref)))

G_END_DECLS

#endif
