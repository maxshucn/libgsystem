/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2014 Colin Walters <walters@verbum.org>.
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

#ifndef __GSYSTEM_ERRORS_H__
#define __GSYSTEM_ERRORS_H__

#include <gio/gio.h>
#include <sys/stat.h>

G_BEGIN_DECLS

void gs_set_error_from_errno (GError **error, gint saved_errno);
void gs_set_prefix_error_from_errno (GError     **error,
                                     gint         errsv,
                                     const char  *format,
                                     ...) G_GNUC_PRINTF (3,4);

G_END_DECLS

#endif
