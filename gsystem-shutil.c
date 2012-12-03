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
#include <glib-unix.h>
#include <sys/stat.h>

static gboolean
cp_internal (GFile         *src,
             GFile         *dest,
             gboolean       use_hardlinks,
             GCancellable  *cancellable,
             GError       **error)
{
  gboolean ret = FALSE;
  gs_unref_object GFileEnumerator *enumerator = NULL;
  gs_unref_object GFileInfo *file_info = NULL;
  GError *temp_error = NULL;

  enumerator = g_file_enumerate_children (src, "standard::type,standard::name,unix::mode",
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          cancellable, error);
  if (!enumerator)
    goto out;

  if (!gs_file_ensure_directory (dest, FALSE, cancellable, error))
    goto out;

  while ((file_info = g_file_enumerator_next_file (enumerator, cancellable, &temp_error)) != NULL)
    {
      const char *name = g_file_info_get_name (file_info);
      gs_unref_object GFile *src_child = g_file_get_child (src, name);
      gs_unref_object GFile *dest_child = g_file_get_child (dest, name);

      if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY)
        {
          if (!gs_file_ensure_directory (dest_child, FALSE, cancellable, error))
            goto out;

          /* Can't do this even though we'd like to; it fails with an error about
           * setting standard::type not being supported =/
           *
           if (!g_file_set_attributes_from_info (dest_child, file_info, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
           cancellable, error))
           goto out;
          */
          if (chmod (gs_file_get_path_cached (dest_child),
                     g_file_info_get_attribute_uint32 (file_info, "unix::mode")) == -1)
            {
              int errsv = errno;
              g_set_error_literal (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                                   g_strerror (errsv));
              goto out;
            }

          if (!cp_internal (src_child, dest_child, use_hardlinks, cancellable, error))
            goto out;
        }
      else
        {
          gboolean did_link = FALSE;
          (void) unlink (gs_file_get_path_cached (dest_child));
          if (use_hardlinks)
            {
              if (link (gs_file_get_path_cached (src_child), gs_file_get_path_cached (dest_child)) == -1)
                {
                  if (!(errno == EMLINK || errno == EXDEV))
                    {
                      int errsv = errno;
                      g_set_error_literal (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                                           g_strerror (errsv));
                      goto out;
                    }
                  use_hardlinks = FALSE;
                }
              else
                did_link = TRUE;
            }
          if (!did_link)
            {
              if (!g_file_copy (src_child, dest_child,
                                G_FILE_COPY_OVERWRITE | G_FILE_COPY_ALL_METADATA | G_FILE_COPY_NOFOLLOW_SYMLINKS,
                                cancellable, NULL, NULL, error))
                goto out;
            }
        }
      g_clear_object (&file_info);
    }
  if (temp_error)
    {
      g_propagate_error (error, temp_error);
      goto out;
    }

  ret = TRUE;
 out:
  return ret;
}

/**
 * gs_shutil_cp_al_or_fallback:
 * @src: Source path
 * @dest: Destination path
 * @cancellable:
 * @error:
 *
 * Recursively copy path @src (which must be a directory) to the
 * target @dest.  If possible, hardlinks are used; if a hardlink is
 * not possible, a regular copy is created.  Any existing files are
 * overwritten.
 *
 * Returns: %TRUE on success
 */
gboolean
gs_shutil_cp_al_or_fallback (GFile         *src,
                             GFile         *dest,
                             GCancellable  *cancellable,
                             GError       **error)
{
  return cp_internal (src, dest, TRUE, cancellable, error);
}

/**
 * gs_shutil_cp_a:
 * @src: Source path
 * @dest: Destination path
 * @cancellable:
 * @error:
 *
 * Recursively copy path @src (which must be a directory) to the
 * target @dest.  Any existing files are overwritten.
 *
 * Returns: %TRUE on success
 */
gboolean
gs_shutil_cp_a (GFile         *src,
                GFile         *dest,
                GCancellable  *cancellable,
                GError       **error)
{
  return cp_internal (src, dest, FALSE, cancellable, error);
}

/**
 * gs_shutil_rm_rf:
 * @path: A file or directory
 * @cancellable:
 * @error:
 *
 * Recursively delete the filename referenced by @path; it may be a
 * file or directory.  No error is thrown if @path does not exist.
 */
gboolean
gs_shutil_rm_rf (GFile        *path,
                 GCancellable *cancellable,
                 GError      **error)
{
  gboolean ret = FALSE;
  gs_unref_object GFileEnumerator *dir_enum = NULL;
  gs_unref_object GFileInfo *file_info = NULL;
  GError *temp_error = NULL;

  dir_enum = g_file_enumerate_children (path, "standard::type,standard::name", 
                                        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                        cancellable, &temp_error);
  if (!dir_enum)
    {
      if (g_error_matches (temp_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
          g_clear_error (&temp_error);
          ret = TRUE;
        }
      else
        g_propagate_error (error, temp_error);

      goto out;
    }

  while ((file_info = g_file_enumerator_next_file (dir_enum, cancellable, &temp_error)) != NULL)
    {
      gs_unref_object GFile *subpath = NULL;
      GFileType type;
      const char *name;

      type = g_file_info_get_attribute_uint32 (file_info, "standard::type");
      name = g_file_info_get_attribute_byte_string (file_info, "standard::name");
      
      subpath = g_file_get_child (path, name);

      if (type == G_FILE_TYPE_DIRECTORY)
        {
          if (!gs_shutil_rm_rf (subpath, cancellable, error))
            goto out;
        }
      else
        {
          if (!gs_file_unlink (subpath, cancellable, error))
            goto out;
        }
      g_clear_object (&file_info);
    }
  if (temp_error)
    {
      g_propagate_error (error, temp_error);
      goto out;
    }

  if (!g_file_delete (path, cancellable, error))
    goto out;

  ret = TRUE;
 out:
  return ret;
}

