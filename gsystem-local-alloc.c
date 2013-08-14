/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Colin Walters <walters@verbum.org>
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

#include "config.h"

#include "gsystem-local-alloc.h"

/**
 * SECTION:gslocalalloc
 * @title: GSystem local allocation
 * @short_description: Release local variables automatically when they go out of scope
 *
 * These macros leverage the GCC extension __attribute__ ((cleanup))
 * to allow calling a cleanup function such as g_free() when a
 * variable goes out of scope.  See <ulink
 * url="http://gcc.gnu.org/onlinedocs/gcc/Variable-Attributes.html">
 * for more information on the attribute.
 *
 * The provided macros make it easy to use the cleanup attribute for
 * types that come with GLib.  The primary two are #gs_free and
 * #gs_unref_object, which correspond to g_free() and
 * g_object_unref(), respectively.
 *
 * The rationale behind this is that particularly when handling error
 * paths, it can be very tricky to ensure the right variables are
 * freed.  With this, one simply applies gs_lobj to a
 * locally-allocated #GFile for example, and it will be automatically
 * unreferenced when it goes out of scope.
 *
 * Note - you should only use these macros for <emphasis>stack
 * allocated</emphasis> variables.  They don't provide garbage
 * collection or let you avoid freeing things.  They're simply a
 * compiler assisted deterministic mechanism for calling a cleanup
 * function when a stack frame ends.
 *
 * <example id="gs-lfree"><title>Calling g_free automatically</title>
 * <programlisting>
 *
 * GFile *
 * create_file (GError **error)
 * {
 *   gs_free char *random_id = NULL;
 *
 *   if (!prepare_file (error))
 *     return NULL;
 *
 *   random_id = alloc_random_id ();
 *
 *   return create_file_real (error);
 *   // Note that random_id is freed here automatically
 * }
 * </programlisting>
 * </example>
 *
 */

void
gs_local_free (void *loc)
{
  void **location = loc;
  if (location)
    g_free (*location);
}

#define _gs_local_free(type, function) do {           \
    void **location = loc;                            \
    if (location)                                     \
      {                                               \
        type *value = *location;                      \
        if (value)                                    \
          function (value);                           \
      }                                               \
  } while (0)

void
gs_local_obj_unref (void *loc)
{
  _gs_local_free(GObject, g_object_unref);
}

void
gs_local_variant_unref (void *loc)
{
  _gs_local_free(GVariant, g_variant_unref);
}

void
gs_local_variant_iter_free (void *loc)
{
  _gs_local_free(GVariantIter, g_variant_iter_free);
}

void
gs_local_variant_builder_unref (void *loc)
{
  _gs_local_free(GVariantBuilder, g_variant_builder_unref);
}

void
gs_local_ptrarray_unref (void *loc)
{
  _gs_local_free(GPtrArray, g_ptr_array_unref);
}

void
gs_local_array_unref (void *loc)
{
  _gs_local_free(GArray, g_array_unref);
}

void
gs_local_hashtable_unref (void *loc)
{
  _gs_local_free(GHashTable, g_hash_table_unref);
}

void
gs_local_checksum_free (void *loc)
{
  _gs_local_free(GChecksum, g_checksum_free);
}

void
gs_local_bytes_unref (void *loc)
{
  _gs_local_free(GBytes, g_bytes_unref);
}
