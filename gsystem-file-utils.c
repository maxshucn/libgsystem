/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 William Jon McCann <mccann@redhat.com>
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "libgsystem.h"
#include <glib/gstdio.h>
#include <gio/gunixinputstream.h>
#include <glib-unix.h>
 
static int
_open_fd_noatime (const char *path)
{
  int fd;

#ifdef O_NOATIME
  fd = g_open (path, O_RDONLY | O_NOATIME, 0);
  /* Only the owner or superuser may use O_NOATIME; so we may get
   * EPERM.  EINVAL may happen if the kernel is really old...
   */
  if (fd == -1 && (errno == EPERM || errno == EINVAL))
#endif
    fd = g_open (path, O_RDONLY, 0);
  
  return fd;
}

/**
 * gs_file_read_noatime:
 * @file: a #GFile
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Like g_file_read(), but try to avoid updating the file's
 * access time.  This should be used by background scanning
 * components such as search indexers, antivirus programs, etc.
 *
 * Returns: (transfer full): A new input stream, or %NULL on error
 */
GInputStream *
gs_file_read_noatime (GFile         *file,
                      GCancellable  *cancellable,
                      GError       **error)
{
  gs_lfree char *path = NULL;
  int fd;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  path = g_file_get_path (file);
  if (path == NULL)
    return NULL;

  fd = _open_fd_noatime (path);
  if (fd < 0)
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   "%s", g_strerror (errno));
      return NULL;
    }

  return g_unix_input_stream_new (fd, TRUE);
}

/**
 * gs_file_get_path_cached:
 *
 * Like g_file_get_path(), but returns a constant copy so callers
 * don't need to free the result.
 */
const char *
gs_file_get_path_cached (GFile *file)
{
  const char *path;

  path = g_object_get_data ((GObject*)file, "gs-file-path");
  if (!path)
    {
      path = g_file_get_path (file);
      g_assert (path != NULL);
      g_object_set_data_full ((GObject*)file, "gs-file-path", (char*)path, (GDestroyNotify)g_free);
    }
  return path;
}

/**
 * gs_file_get_basename_cached:
 *
 * Like g_file_get_basename(), but returns a constant copy so callers
 * don't need to free the result.
 */
const char *
gs_file_get_basename_cached (GFile *file)
{
  const char *name;

  name = g_object_get_data ((GObject*)file, "gs-file-name");
  if (!name)
    {
      name = g_file_get_basename (file);
      g_object_set_data_full ((GObject*)file, "gs-file-name", (char*)name, (GDestroyNotify)g_free);
    }
  return name;
}

/**
 * gs_file_rename:
 * @from: Current path
 * @to: New path
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * This function wraps the raw Unix function rename().
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
gs_file_rename (GFile          *from,
                GFile          *to,
                GCancellable   *cancellable,
                GError        **error)
{
  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  if (rename (gs_file_get_path_cached (from),
              gs_file_get_path_cached (to)) < 0)
    {
      int errsv = errno;
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                   "Failed to rename %s to %s: ", gs_file_get_path_cached (from),
                   gs_file_get_path_cached (to));
      return FALSE;
    }
  return TRUE;
}

/**
 * gs_file_unlink:
 * @path: Path to file
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Like g_file_delete(), except this function does not follow Unix
 * symbolic links, and will delete a symbolic link even if it's
 * pointing to a nonexistent file.  In other words, this function
 * merely wraps the raw Unix function unlink().
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
gs_file_unlink (GFile          *path,
                GCancellable   *cancellable,
                GError        **error)
{
  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  if (unlink (gs_file_get_path_cached (path)) < 0)
    {
      int errsv = errno;
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                   "Failed to unlink %s: ", gs_file_get_path_cached (path));
      return FALSE;
    }
  return TRUE;
}

/**
 * gs_file_ensure_directory:
 * @dir: Path to create as directory
 * @with_parents: Also create parent directories
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Like g_file_make_directory(), except does not throw an error if the
 * directory already exists.
 */
gboolean
gs_file_ensure_directory (GFile         *dir,
                          gboolean       with_parents, 
                          GCancellable  *cancellable,
                          GError       **error)
{
  gboolean ret = FALSE;
  GError *temp_error = NULL;

  if (!g_file_make_directory (dir, cancellable, &temp_error))
    {
      if (with_parents &&
          g_error_matches (temp_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
          gs_lobj GFile *parent = NULL;

          g_clear_error (&temp_error);

          parent = g_file_get_parent (dir);
          if (parent)
            {
              if (!gs_file_ensure_directory (parent, TRUE, cancellable, error))
                goto out;
            }
          if (!gs_file_ensure_directory (dir, FALSE, cancellable, error))
            goto out;
        }
      else if (!g_error_matches (temp_error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
          g_propagate_error (error, temp_error);
          goto out;
        }
      else
        g_clear_error (&temp_error);
    }

  ret = TRUE;
 out:
  return ret;
}

/**
 * gs_file_load_contents_utf8:
 * @file: Path to file whose contents must be UTF-8
 * @cancellable:
 * @error:
 *
 * Like g_file_load_contents(), except validates the contents are
 * UTF-8.
 */
gchar *
gs_file_load_contents_utf8 (GFile         *file,
                            GCancellable  *cancellable,
                            GError       **error)
{
  gboolean ret = FALSE;
  gsize len;
  char *ret_contents = NULL;

  if (!g_file_load_contents (file, cancellable, &ret_contents, &len,
                             NULL, error))
    goto out;
  if (!g_utf8_validate (ret_contents, len, NULL))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Invalid UTF-8");
      goto out;
    }

  ret = TRUE;
 out:
  if (!ret)
    {
      g_free (ret_contents);
      return NULL;
    }
  return ret_contents;
}

