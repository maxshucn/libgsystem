/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2014 Colin Walters <walters@verbum.org>
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

#include "gsystem-errors.h"
#include "gsystem-local-alloc.h"
#include <glib-unix.h>

/**
 * gs_set_error_from_errno:
 * @error: (allow-none): Error
 * @saved_errno: errno value
 * 
 * Set @error to an error with domain %G_IO_ERROR, and code based on
 * the value of @saved_errno.  The error message is set using a
 * literal return from g_strerror().
 */
void
gs_set_error_from_errno (GError **error, gint saved_errno)
{
  g_set_error_literal (error,
                       G_IO_ERROR,
                       g_io_error_from_errno (saved_errno),
                       g_strerror (saved_errno));
  errno = saved_errno;
}

/**
 * gs_set_prefix_error_from_errno:
 * @error: (allow-none): Error
 * @saved_errno: errno value
 * @format: Format string for printf
 * 
 * Set @error to an error with domain %G_IO_ERROR, and code based on
 * the value of @saved_errno.  The error message is prefixed with the
 * result of @format, a colon and space, then the result of
 * g_strerror().
 */
void
gs_set_prefix_error_from_errno (GError     **error,
                                gint         saved_errno,
                                const char  *format,
                                ...)
{
  gs_free char *formatted = NULL;
  va_list args;
  
  va_start (args, format);
  formatted = g_strdup_vprintf (format, args);
  va_end (args);
  
  g_set_error (error,
               G_IO_ERROR,
               g_io_error_from_errno (saved_errno),
               "%s: %s", formatted, g_strerror (saved_errno));
  errno = saved_errno;
}
