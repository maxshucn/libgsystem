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

#define _GSYSTEM_NO_LOCAL_ALLOC
#include "libgsystem.h"
#include "gsystem-glib-compat.h"
#include <glib/gstdio.h>
#include <gio/gunixinputstream.h>
#include <gio/gfiledescriptorbased.h>
#include <gio/gunixoutputstream.h>
#include <glib-unix.h>
#include <limits.h>
#include <dirent.h>

static int
close_nointr (int fd)
{
  int res;
  /* Note this is NOT actually a retry loop.
   * See: https://bugzilla.gnome.org/show_bug.cgi?id=682819
   */
  res = close (fd);
  /* Just ignore EINTR...on Linux, retrying is wrong. */
  if (res == EINTR)
    res = 0;
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
    {
      char *uri;
      uri = g_file_get_uri (file);
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                   "%s has no associated path", uri);
      g_free (uri);
      return NULL;
    }

  fd = _open_fd_noatime (path);
  if (fd < 0)
    {
      _set_error_from_errno (error);
      return NULL;
    }

  return g_unix_input_stream_new (fd, TRUE);
}

/**
 * gs_stream_fstat:
 * @stream: A stream containing a Unix file descriptor
 * @stbuf: Memory location to write stat buffer
 * @cancellable:
 * @error:
 *
 * Some streams created via libgsystem are #GUnixInputStream; these do
 * not support e.g. g_file_input_stream_query_info().  This function
 * allows dropping to the raw unix fstat() call for these types of
 * streams, while still conveniently wrapped with the normal GLib
 * handling of @cancellable and @error.
 */
gboolean
gs_stream_fstat (GFileDescriptorBased *stream,
                 struct stat          *stbuf,
                 GCancellable         *cancellable,
                 GError              **error)
{
  gboolean ret = FALSE;
  int fd;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    goto out;

  fd = g_file_descriptor_based_get_fd (stream);

  if (fstat (fd, stbuf) == -1)
    {
      _set_error_from_errno (error);
      goto out;
    }

  ret = TRUE;
 out:
  return ret;
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

#if GLIB_CHECK_VERSION(2,34,0)
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
#endif

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

/**
 * gs_file_create_with_uidgid:
 * @file: Path of file to create
 * @mode: Unix mode
 * @uid: Unix uid
 * @gid: Unix gid
 * @out_stream: (out) (transfer full) (allow-none): Output stream connected to file descriptor
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Create @file exclusively; it must not exist already.  Ensure the
 * returned file has mode @mode and has Unix owners corresponding to
 * the parameters @uid and @gid.
 *
 * The parameter @out_stream if provided, will be filled in with a
 * #GOutputStream connected to the file.
 *
 * Returns: %TRUE on success, %FALSE on error.
 */
gboolean
gs_file_create_with_uidgid (GFile          *file,
                            int             mode,
                            uid_t           uid,
                            gid_t           gid,
                            GOutputStream **out_stream,
                            GCancellable   *cancellable,
                            GError        **error)
{
  gboolean ret = FALSE;
  int fd;
  GOutputStream *ret_stream = NULL;
  static gsize uidgid_cached;
  static uid_t myuid;
  static uid_t mygid;

  /* Ok yes this is lame, but calling these two over and over shows up
   * in strace.  I like my straces to be clean, shoot me.
   */
  if (g_once_init_enter (&uidgid_cached))
    {
      myuid = getuid ();
      mygid = getgid ();
      g_once_init_leave (&uidgid_cached, 1);
    }

  fd = open_nointr (gs_file_get_path_cached (file), O_WRONLY | O_CREAT | O_EXCL, mode);
  if (fd < 0)
    {
      _set_error_from_errno (error);
      goto out;
    }

  if (uid != myuid || gid != mygid)
    {
      if (fchown (fd, uid, gid) < 0)
        {
          _set_error_from_errno (error);
          goto out;
        }
    }

  if (fchmod (fd, mode) < 0)
    {
      _set_error_from_errno (error);
      goto out;
    }
  
  ret_stream = g_unix_output_stream_new (fd, TRUE);
  
  ret = TRUE;
  gs_transfer_out_value (out_stream, &ret_stream);
 out:
  g_clear_object (&ret_stream);
  return ret;
}

/**
 * gs_file_create:
 * @file: Path to non-existent file
 * @mode: Unix access permissions
 * @out_stream: (out) (transfer full) (allow-none): Newly created output, or %NULL
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Like g_file_create(), except this function allows specifying the
 * access mode.  This allows atomically creating private files.
 */
gboolean
gs_file_create (GFile          *file,
                int             mode,
                GOutputStream **out_stream,
                GCancellable   *cancellable,
                GError        **error)
{
  gboolean ret = FALSE;
  int fd;
  GOutputStream *ret_stream = NULL;

  fd = open_nointr (gs_file_get_path_cached (file), O_WRONLY | O_CREAT | O_EXCL, mode);
  if (fd < 0)
    {
      _set_error_from_errno (error);
      goto out;
    }

  if (fchmod (fd, mode) < 0)
    {
      _set_error_from_errno (error);
      goto out;
    }
  
  ret_stream = g_unix_output_stream_new (fd, TRUE);
  
  ret = TRUE;
  gs_transfer_out_value (out_stream, &ret_stream);
 out:
  g_clear_object (&ret_stream);
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

/**
 * gsystem_fileutil_gen_tmp_name:
 * @prefix: (allow-none): String prepended to the result
 * @suffix: (allow-none): String suffixed to the result
 *
 * Generate a name suitable for use as a temporary file.  This
 * function does no I/O; it is not guaranteed that a file with that
 * name does not exist.
 */
char *
gsystem_fileutil_gen_tmp_name (const char *prefix,
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

/**
 * gs_file_open_in_tmpdir:
 * @tmpdir: Directory to place temporary file
 * @mode: Default mode (will be affected by umask)
 * @out_file: (out) (transfer full): Newly created file path
 * @out_stream: (out) (transfer full) (allow-none): Newly created output stream
 * @cancellable:
 * @error:
 *
 * Like g_file_open_tmp(), except the file will be created in the
 * provided @tmpdir, and allows specification of the Unix @mode, which
 * means private files may be created.  Return values will be stored
 * in @out_file, and optionally @out_stream.
 */
gboolean
gs_file_open_in_tmpdir (GFile             *tmpdir,
                        int                mode,
                        GFile            **out_file,
                        GOutputStream    **out_stream,
                        GCancellable      *cancellable,
                        GError           **error)
{
  gboolean ret = FALSE;
  const int max_attempts = 128;
  guint i;
  DIR *d = NULL;
  int dfd = -1;
  char *tmp_name = NULL;
  int fd;

  d = opendir (gs_file_get_path_cached (tmpdir));
  if (!d)
    {
      _set_error_from_errno (error);
      goto out;
    }
  dfd = dirfd (d);
  
  /* 128 attempts seems reasonable... */
  for (i = 0; i < max_attempts; i++)
    {
      g_free (tmp_name);
      tmp_name = gsystem_fileutil_gen_tmp_name (NULL, NULL);

      do
        fd = openat (dfd, tmp_name, O_WRONLY | O_CREAT | O_EXCL, mode);
      while (fd == -1 && errno == EINTR);
      if (fd < 0 && errno != EEXIST)
        {
          _set_error_from_errno (error);
          goto out;
        }
      break;
    }
  if (i == max_attempts)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Exhausted attempts to open temporary file");
      goto out;
    }

  ret = TRUE;
  *out_file = g_file_get_child (tmpdir, tmp_name);
  if (out_stream)
    *out_stream = g_unix_output_stream_new (fd, TRUE);
 out:
  if (d) (void) closedir (d);
  return ret;
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
  int res;
  char *tmp_name = NULL;
  GFile *tmp_dest = NULL;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    goto out;

  tmp_name = gsystem_fileutil_gen_tmp_name (NULL, NULL);
  tmp_dest = g_file_get_child (dest_parent, tmp_name);

  res = link (gs_file_get_path_cached (src), gs_file_get_path_cached (tmp_dest));
  if (res == -1)
    {
      if (errno == EEXIST)
        {
          /* Nothing, fall through */
          *out_try_again = TRUE;
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
      gboolean tryagain = FALSE;

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
 * gs_file_enumerator_iterate:
 * @direnum: an open #GFileEnumerator
 * @out_info: (out) (transfer none) (allow-none): Output location for the next #GFileInfo
 * @out_child: (out) (transfer none) (allow-none): Output location for the next #GFile, or %NULL
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * This is a version of g_file_enumerator_next_file() that's easier to
 * use correctly from C programs.  With g_file_enumerator_next_file(),
 * the gboolean return value signifies "end of iteration or error", which
 * requires allocation of a temporary #GError.
 *
 * In contrast, with this function, a %FALSE return from
 * gs_file_enumerator_iterate() <emphasis>always</emphasis> means
 * "error".  End of iteration is signaled by @out_info being %NULL.
 *
 * Another crucial difference is that the references for @out_info and
 * @out_child are owned by @direnum (they are cached as hidden
 * properties).  You must not unref them in your own code.  This makes
 * memory management significantly easier for C code in combination
 * with loops.
 *
 * Finally, this function optionally allows retrieving a #GFile as
 * well.
 *
 * The code pattern for correctly using gs_file_enumerator_iterate() from C
 * is:
 *
 * |[
 * direnum = g_file_enumerate_children (file, ...);
 * while (TRUE)
 *   {
 *     GFileInfo *info;
 *     if (!gs_file_enumerator_iterate (direnum, &info, NULL, cancellable, error))
 *       goto out;
 *     if (!info)
 *       break;
 *     ... do stuff with "info"; do not unref it! ...
 *   }
 * 
 * out:
 *   g_object_unref (direnum); // Note: frees the last @info
 * ]|
 */
gboolean
gs_file_enumerator_iterate (GFileEnumerator  *direnum,
                            GFileInfo       **out_info,
                            GFile           **out_child,
                            GCancellable     *cancellable,
                            GError          **error)
{
  gboolean ret = FALSE;
  GError *temp_error = NULL;

  static GQuark cached_info_quark;
  static GQuark cached_child_quark;
  static gsize quarks_initialized;

  g_return_val_if_fail (direnum != NULL, FALSE);
  g_return_val_if_fail (out_info != NULL, FALSE);

  if (g_once_init_enter (&quarks_initialized))
    {
      cached_info_quark = g_quark_from_static_string ("gsystem-cached-info");
      cached_child_quark = g_quark_from_static_string ("gsystem-cached-child");
      g_once_init_leave (&quarks_initialized, 1);
    }

  
  *out_info = g_file_enumerator_next_file (direnum, cancellable, &temp_error);
  if (out_child)
    *out_child = NULL;
  if (temp_error != NULL)
    {
      g_propagate_error (error, temp_error);
      goto out;
    }
  else if (*out_info != NULL)
    {
      g_object_set_qdata_full ((GObject*)direnum, cached_info_quark, *out_info, (GDestroyNotify)g_object_unref);
      if (out_child != NULL)
        {
          const char *name = g_file_info_get_name (*out_info);
          *out_child = g_file_get_child (g_file_enumerator_get_container (direnum), name);
          g_object_set_qdata_full ((GObject*)direnum, cached_child_quark, *out_child, (GDestroyNotify)g_object_unref);
        }
    }

  ret = TRUE;
 out:
  return ret;
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

static gboolean
chown_internal (GFile          *path,
                gboolean        dereference_links,
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
    if (dereference_links)
      res = chown (gs_file_get_path_cached (path), owner, group);
    else
      res = lchown (gs_file_get_path_cached (path), owner, group);
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
  return chown_internal (path, TRUE, owner, group, cancellable, error);
}

/**
 * gs_file_lchown:
 * @path: Path to file
 * @owner: UNIX owner
 * @group: UNIX group
 * @cancellable: a #GCancellable
 * @error: a #GError
 *
 * Merely wraps UNIX lchown().
 *
 * Returns: %TRUE on success, %FALSE on error
 */
gboolean
gs_file_lchown (GFile          *path,
                guint32         owner,
                guint32         group,
                GCancellable   *cancellable,
                GError        **error)
{
  return chown_internal (path, FALSE, owner, group, cancellable, error);
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

static int
path_common_directory (char *one,
                       char *two)
{
  int dir_index = 0;
  int i = 0;

  while (*one && *two)
    {
      if (*one != *two)
        break;
      if (*one == '/')
        dir_index = i + 1;

      one++;
      two++;
      i++;
    }

  return dir_index;
}

/**
 * gs_file_get_relpath:
 * @one: The first #GFile
 * @two: The second #GFile
 *
 * Like gs_file_get_relative_path(), but does not mandate that
 * the two files have any parent in common. This function will
 * instead insert "../" where appropriate.
 *
 * Returns: (transfer full): The relative path between the two.
 */
gchar *
gs_file_get_relpath (GFile *one,
                     GFile *two)
{
  gchar *simple_path;
  gchar *one_path, *one_suffix;
  gchar *two_path, *two_suffix;
  GString *path;
  int i;

  simple_path = g_file_get_relative_path (one, two);
  if (simple_path)
    return simple_path;

  one_path = g_file_get_path (one);
  two_path = g_file_get_path (two);

  i = path_common_directory (one_path, two_path);
  one_suffix = one_path + i;
  two_suffix = two_path + i;

  path = g_string_new ("");

  /* For every leftover path segment one has, append "../" so
   * that we reach the same directory. */
  while (*one_suffix)
    {
      g_string_append (path, "../");
      one_suffix = strchr (one_suffix, '/');
      if (one_suffix == NULL)
        break;
      one_suffix++;
    }

  /* And now append the leftover stuff on two's side. */
  g_string_append (path, two_suffix);

  g_free (one_path);
  g_free (two_path);

  return g_string_free (path, FALSE);
}

/**
 * gs_file_realpath:
 * @file: A #GFile
 *
 * Return a #GFile that contains the same path with symlinks
 * followed. That is, it's a #GFile whose path is the result
 * of calling realpath() on @file.
 *
 * Returns: (allow-none) (transfer full): A new #GFile or %NULL if @file is invalid
 */
GFile *
gs_file_realpath (GFile *file)
{
  gchar *path;
  gchar path_real[PATH_MAX];

  path = g_file_get_path (file);

  if (realpath ((const char *) path, path_real) == NULL)
    {
      g_free (path);
      return NULL;
    }

  g_free (path);
  return g_file_new_for_path (path_real);
}
