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

#include <string.h>

#include "libgsystem.h"
#include <glib/gstdio.h>
#include <gio/gunixinputstream.h>
#include <glib-unix.h>

static int
close_nointr (int fd)
{
  int res;
  do
    res = close (fd);
  while (G_UNLIKELY (res != 0 && errno == EINTR));
  return res;
}

static void
close_nointr_noerror (int fd)
{
  (void) close_nointr (fd);
}

static int
open_nointr (const char *path, int flags, mode_t mode)
{
  int res;
  do
    res = open (path, flags, mode);
  while (G_UNLIKELY (res != 0 && errno == EINTR));
  return res;
}

static int
_open_fd_noatime (const char *path)
{
  int fd;

#ifdef O_NOATIME
  fd = open_nointr (path, O_RDONLY | O_NOATIME, 0);
  /* Only the owner or superuser may use O_NOATIME; so we may get
   * EPERM.  EINVAL may happen if the kernel is really old...
   */
  if (fd == -1 && (errno == EPERM || errno == EINVAL))
#endif
    fd = open_nointr (path, O_RDONLY, 0);
  
  return fd;
}

static inline void
_set_error_from_errno (GError **error)
{
  int errsv = errno;
  g_set_error_literal (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                       g_strerror (errsv));
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
  const char *path = NULL;
  int fd;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  path = gs_file_get_path_cached (file);
  if (path == NULL)
    return NULL;

  fd = _open_fd_noatime (path);
  if (fd < 0)
    {
      _set_error_from_errno (error);
      return NULL;
    }

  return g_unix_input_stream_new (fd, TRUE);
}

/**
 * gs_file_map_noatime: (skip)
 * @file: a #GFile
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Like g_mapped_file_new(), but try to avoid updating the file's
 * access time.  This should be used by background scanning
 * components such as search indexers, antivirus programs, etc.
 *
 * Returns: (transfer full): A new mapped file, or %NULL on error
 */
GMappedFile *
gs_file_map_noatime (GFile         *file,
                     GCancellable  *cancellable,
                     GError       **error)
{
  const char *path;
  int fd;
  GMappedFile *ret;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  path = gs_file_get_path_cached (file);
  if (path == NULL)
    return NULL;

  fd = _open_fd_noatime (path);
  if (fd < 0)
    {
      _set_error_from_errno (error);
      return NULL;
    }
  
  ret = g_mapped_file_new_from_fd (fd, FALSE, error);
  close_nointr_noerror (fd); /* Ignore errors - we always want to close */

  return ret;
}

/**
 * gs_file_map_readonly:
 * @file: a #GFile
 * @cancellable:
 * @error:
 *
 * Return a #GBytes which references a readonly view of the contents of
 * @file.  This function uses #GMappedFile internally.
 *
 * Returns: (transfer full): a newly referenced #GBytes
 */
GBytes *
gs_file_map_readonly (GFile         *file,
                      GCancellable  *cancellable,
                      GError       **error)
{
  GMappedFile *mfile;
  GBytes *ret;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  mfile = g_mapped_file_new (gs_file_get_path_cached (file), FALSE, error);
  if (!mfile)
    return NULL;

  ret = g_mapped_file_get_bytes (mfile);
  g_mapped_file_unref (mfile);
  return ret;
}

/**
 * gs_file_sync_data:
 * @file: a #GFile
 * @cancellable:
 * @error:
 *
 * Wraps the UNIX fdatasync() function, which ensures that the data in
 * @file is on non-volatile storage.
 */
gboolean
gs_file_sync_data (GFile          *file,
                   GCancellable   *cancellable,
                   GError        **error)
{
  gboolean ret = FALSE;
  int res;
  int fd = -1; 

  fd = _open_fd_noatime (gs_file_get_path_cached (file));
  if (fd < 0)
    {
      _set_error_from_errno (error);
      goto out;
    }

  do
    res = fdatasync (fd);
  while (G_UNLIKELY (res != 0 && errno == EINTR));
  if (res != 0)
    {
      _set_error_from_errno (error);
      goto out;
    }

  res = close_nointr (fd);
  if (res != 0)
    {
      _set_error_from_errno (error);
      goto out;
    }
  fd = -1;
  
  ret = TRUE;
 out:
  if (fd != -1)
    close_nointr_noerror (fd);
  return ret;
}

static const char *
get_default_tmp_prefix (void)
{
  static char *tmpprefix = NULL;

  if (g_once_init_enter (&tmpprefix))
    {
      const char *prgname = g_get_prgname ();
      const char *p;
      char *prefix;

      p = strrchr (prgname, '/');
      if (p)
        prgname = p + 1;

      prefix = g_strdup_printf ("tmp-%s%u-", prgname, getuid ());
      
      g_once_init_leave (&tmpprefix, prefix);
    }

  return tmpprefix;
}

static char *
gen_tmp_name (const char *prefix,
              const char *suffix)
{
  static const char table[] = "ABCEDEFGHIJKLMNOPQRSTUVWXYZabcedefghijklmnopqrstuvwxyz0123456789";
  GString *str = g_string_new ("");
  guint i;

  if (!prefix)
    prefix = get_default_tmp_prefix ();
  if (!suffix)
    suffix = "tmp";

  g_string_append (str, prefix);
  for (i = 0; i < 8; i++)
    {
      int offset = g_random_int_range (0, sizeof (table) - 1);
      g_string_append_c (str, (guint8)table[offset]);
    }
  g_string_append_c (str, '.');
  g_string_append (str, suffix);

  return g_string_free (str, FALSE);
}

static gboolean
linkcopy_internal_attempt (GFile          *src,
                          GFile          *dest,
                          GFile          *dest_parent,
                          GFileCopyFlags  flags,
                          gboolean        sync_data,
                          gboolean        enable_guestfs_fuse_workaround,
                          gboolean       *out_try_again,
                          GCancellable   *cancellable,
                          GError        **error)
{
  gboolean ret = FALSE;
  gboolean ret_try_again = FALSE;
  int res;
  char *tmp_name = NULL;
  GFile *tmp_dest = NULL;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    goto out;

  tmp_name = gen_tmp_name (NULL, NULL);
  tmp_dest = g_file_get_child (dest_parent, tmp_name);

  res = link (gs_file_get_path_cached (src), gs_file_get_path_cached (tmp_dest));
  if (res == -1)
    {
      if (errno == EEXIST)
        {
          /* Nothing, fall through */
          ret_try_again = TRUE;
          ret = TRUE;
          goto out;
        }
      else if (errno == EXDEV || errno == EMLINK || errno == EPERM
               || (enable_guestfs_fuse_workaround && errno == ENOENT))
        {
          if (!g_file_copy (src, tmp_dest, flags,
                            cancellable, NULL, NULL, error))
            goto out;
        }
      else
        {
          _set_error_from_errno (error);
          goto out;
        }
    }
      
  if (sync_data)
    {
      /* Now, we need to fdatasync */
      if (!gs_file_sync_data (tmp_dest, cancellable, error))
        goto out;
    }

  if (!gs_file_rename (tmp_dest, dest, cancellable, error))
    goto out;

  ret = TRUE;
  *out_try_again = FALSE;
 out:
  g_clear_pointer (&tmp_name, g_free);
  g_clear_object (&tmp_dest);
  return ret;
}

static gboolean
linkcopy_internal (GFile          *src,
                   GFile          *dest,
                   GFileCopyFlags  flags,
                   gboolean        sync_data,
                   GCancellable   *cancellable,
                   GError        **error)
{
  gboolean ret = FALSE;
  gboolean dest_exists;
  int i;
  gboolean enable_guestfs_fuse_workaround;
  struct stat src_stat;
  struct stat dest_stat;
  GFile *dest_parent = NULL;

  flags |= G_FILE_COPY_NOFOLLOW_SYMLINKS;

  g_return_val_if_fail ((flags & (G_FILE_COPY_BACKUP | G_FILE_COPY_TARGET_DEFAULT_PERMS)) == 0, FALSE);

  dest_parent = g_file_get_parent (dest);

  if (lstat (gs_file_get_path_cached (src), &src_stat) == -1)
    {
      int errsv = errno;
      g_set_error_literal (error, G_IO_ERROR, g_io_error_from_errno (errno),
                           g_strerror (errsv));
      goto out;
    }

  if (lstat (gs_file_get_path_cached (dest), &dest_stat) == -1)
    dest_exists = FALSE;
  else
    dest_exists = TRUE;
  
  if (((flags & G_FILE_COPY_OVERWRITE) == 0) && dest_exists)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_EXISTS,
                           "File exists");
      goto out;
    }

  /* Work around the behavior of link() where it's a no-op if src and
   * dest are the same.
   */
  if (dest_exists &&
      src_stat.st_dev == dest_stat.st_dev &&
      src_stat.st_ino == dest_stat.st_ino)
    {
      ret = TRUE;
      goto out;
    }

  enable_guestfs_fuse_workaround = getenv ("LIBGSYSTEM_ENABLE_GUESTFS_FUSE_WORKAROUND") != NULL;

  /* 128 attempts seems reasonable... */
  for (i = 0; i < 128; i++)
    {
      gboolean tryagain;

      if (!linkcopy_internal_attempt (src, dest, dest_parent,
                                      flags, sync_data,
                                      enable_guestfs_fuse_workaround,
                                      &tryagain,
                                      cancellable, error))
        goto out;

      if (!tryagain)
        break;
    }

  ret = TRUE;
 out:
  g_clear_object (&dest_parent);
  return ret;

}

/**
 * gs_file_linkcopy:
 * @src: Source file
 * @dest: Destination file
 * @flags: flags
 * @cancellable:
 * @error:
 *
 * First tries to use the UNIX link() call, but if the files are on
 * separate devices, fall back to copying via g_file_copy().
 *
 * The given @flags have different semantics than those documented
 * when hardlinking is used.  Specifically, both
 * #G_FILE_COPY_TARGET_DEFAULT_PERMS and #G_FILE_COPY_BACKUP are not
 * supported.  #G_FILE_COPY_NOFOLLOW_SYMLINKS treated as if it was
 * always given - if you want to follow symbolic links, you will need
 * to resolve them manually.
 *
 * Beware - do not use this function if @src may be modified, and it's
 * undesirable for the changes to also be reflected in @dest.  The
 * best use of this function is in the case where @src and @dest are
 * read-only, or where @src is a temporary file, and you want to put
 * it in the final place.
 */
gboolean
gs_file_linkcopy (GFile          *src,
                  GFile          *dest,
                  GFileCopyFlags  flags,
                  GCancellable   *cancellable,
                  GError        **error)
{
  return linkcopy_internal (src, dest, flags, FALSE, cancellable, error);
}

/**
 * gs_file_linkcopy_sync_data:
 * @src: Source file
 * @dest: Destination file
 * @flags: flags
 * @cancellable:
 * @error:
 *
 * This function is similar to gs_file_linkcopy(), except it also uses
 * gs_file_sync_data() to ensure that @dest is in stable storage
 * before it is moved into place.
 */
gboolean
gs_file_linkcopy_sync_data (GFile          *src,
                            GFile          *dest,
                            GFileCopyFlags  flags,
                            GCancellable   *cancellable,
                            GError        **error)
{
  return linkcopy_internal (src, dest, flags, TRUE, cancellable, error);
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
  static GQuark _file_path_quark = 0;

  if (G_UNLIKELY (_file_path_quark) == 0)
    _file_path_quark = g_quark_from_static_string ("gsystem-file-path");

  path = g_object_get_qdata ((GObject*)file, _file_path_quark);
  if (!path)
    {
      path = g_file_get_path (file);
      g_assert (path != NULL);
      g_object_set_qdata_full ((GObject*)file, _file_path_quark, (char*)path, (GDestroyNotify)g_free);
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
  static GQuark _file_name_quark = 0;

  if (G_UNLIKELY (_file_name_quark) == 0)
    _file_name_quark = g_quark_from_static_string ("gsystem-file-name");

  name = g_object_get_qdata ((GObject*)file, _file_name_quark);
  if (!name)
    {
      name = g_file_get_basename (file);
      g_object_set_qdata_full ((GObject*)file, _file_name_quark, (char*)name, (GDestroyNotify)g_free);
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
      _set_error_from_errno (error);
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
      _set_error_from_errno (error);
      return FALSE;
    }
  return TRUE;
}

/**
 * gs_file_chown:
 * @path: Path to file
 * @owner: UNIX owner
 * @group: UNIX group
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Merely wraps UNIX chown().
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
gs_file_chown (GFile          *path,
               guint32         owner,
               guint32         group,
               GCancellable   *cancellable,
               GError        **error)
{
  gboolean ret = FALSE;
  int res;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  do
    res = chown (gs_file_get_path_cached (path), owner, group);
  while (G_UNLIKELY (res != 0 && errno == EINTR));

  if (res < 0)
    {
      _set_error_from_errno (error);
      goto out;
    }

  ret = TRUE;
 out:
  return ret;
}

/**
 * gs_file_chmod:
 * @path: Path to file
 * @mode: UNIX mode
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Merely wraps UNIX chmod().
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
gs_file_chmod (GFile          *path,
               guint           mode,
               GCancellable   *cancellable,
               GError        **error)
{
  gboolean ret = FALSE;
  int res;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  do
    res = chmod (gs_file_get_path_cached (path), mode);
  while (G_UNLIKELY (res != 0 && errno == EINTR));

  if (res < 0)
    {
      _set_error_from_errno (error);
      goto out;
    }

  ret = TRUE;
 out:
  return ret;
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
  GFile *parent = NULL;

  if (!g_file_make_directory (dir, cancellable, &temp_error))
    {
      if (with_parents &&
          g_error_matches (temp_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
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
  g_clear_object (&parent);
  return ret;
}

/**
 * gs_file_ensure_directory_mode:
 * @dir: Path to create as directory
 * @mode: Create directory with these permissions
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Wraps UNIX mkdir() function with support for @cancellable, and
 * uses @error instead of errno.
 */
gboolean
gs_file_ensure_directory_mode (GFile         *dir,
                               guint          mode,
                               GCancellable  *cancellable,
                               GError       **error)
{
  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  if (mkdir (gs_file_get_path_cached (dir), mode) == -1 && errno != EEXIST)
    {
      _set_error_from_errno (error);
      return FALSE;
    }
  return TRUE;
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

