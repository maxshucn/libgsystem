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

/**
 * SECTION:gsfiletreewalk
 * @title: GSFileTreeWalk
 * @short_description: Recurse over a directory tree
 *
 * While #GFileEnumerator provides an API to iterate over one
 * directory, in many cases one wants to operate recursively.
 * This API is designed to do that.
 *
 * In addition, a #GSFileTreeWalk contains Unix-native API such as
 * file descriptor based enumeration.
 */

#include "config.h"

#define _GSYSTEM_NO_LOCAL_ALLOC
#include "libgsystem.h"
#include "gsystem-file-tree-walk.h"
#include "gsystem-enum-types.h"
#include <glib-unix.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>

/* Taken from systemd/src/shared/util.h */
union dirent_storage {
        struct dirent dent;
        guint8 storage[offsetof(struct dirent, d_name) +
                        ((NAME_MAX + 1 + sizeof(long)) & ~(sizeof(long) - 1))];
};

typedef GObjectClass GSFileTreeWalkClass;

struct _GSFileTreeWalk
{
  GObject parent;

  GSFileTreeWalkFlags flags;
  int origin_dfd;
  struct stat origin_stbuf;

  guint owns_dfd : 1;
};

G_DEFINE_TYPE (GSFileTreeWalk, gs_file_tree_walk, G_TYPE_OBJECT);

enum
{
  PROP_0,
  PROP_FLAGS,
  N_PROPS
};

static GParamSpec *gs_file_tree_walk_pspecs[N_PROPS];

static void
gs_file_tree_walk_init (GSFileTreeWalk  *self)
{
  self->origin_dfd = -1;
}

static void
gs_file_tree_walk_finalize (GObject *object)
{
  GSFileTreeWalk *self = GS_FILE_TREE_WALK (object);

  if (self->owns_dfd && self->origin_dfd != -1)
    (void) close (self->origin_dfd);

  if (G_OBJECT_CLASS (gs_file_tree_walk_class)->finalize != NULL)
    G_OBJECT_CLASS (gs_file_tree_walk_class)->finalize (object);
}

static void
gs_file_tree_walk_class_init (GSFileTreeWalkClass *class)
{
  /**
   * GSFileTreeWalk:flags:
   */
  gs_file_tree_walk_pspecs[PROP_FLAGS] = g_param_spec_flags ("flags", "", "",
                                                             GS_TYPE_FILE_TREE_WALK_FLAGS,
                                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                             G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, gs_file_tree_walk_pspecs);
}

static void
gs_file_tree_walk_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GSFileTreeWalk *self = GS_FILE_TREE_WALK (object);

  switch (prop_id)
    {
    case PROP_FLAGS:
      self->flags = g_value_get_flags (value);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gs_file_tree_walk_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GSFileTreeWalk *self = GS_FILE_TREE_WALK (object);

  switch (prop_id)
    {
    case PROP_FLAGS:
      g_value_set_flags (value, self->flags);
      break;

    default:
      g_assert_not_reached ();
    }
}

/**
 * gs_file_tree_walk_open:
 * @path: Directory to enumerate
 * @flags: Flags controlling enumeration
 * @cancellable: Cancellable
 * @error: Error
 *
 * Returns: (transfer full): A new directory enumerator
 */
GSFileTreeWalk *
gs_file_tree_walk_open (GFile                   *path,
                        GSFileTreeWalkFlags      flags,
                        GCancellable            *cancellable,
                        GError                 **error)
{
  gs_unref_object GSFileTreeWalk *ftw = g_object_new (GS_TYPE_FILE_TREE_WALK, "flags", flags, NULL);

  ftw->owns_dfd = TRUE;
  
  if (!gs_file_open_dir_fd (path, &ftw->origin_dfd,
                            cancellable, error))
    return NULL;

  return g_object_ref (ftw);
}

/**
 * gs_file_tree_walk_open_at:
 * @dfd: Directory file descriptor
 * @flags: Flags controlling enumeration
 * @cancellable: Cancellable
 * @error: Error
 *
 * Returns: (transfer full): A new directory enumerator
 */
GSFileTreeWalk *
gs_file_tree_walk_open_at (int                   dfd,
                           GSFileTreeWalkFlags   flags,
                           GCancellable         *cancellable,
                           GError              **error)
{
  gs_unref_object GSFileTreeWalk *ftw = g_object_new (GS_TYPE_FILE_TREE_WALK, "flags", flags, NULL);

  ftw->origin_dfd = dfd;
  if (fstat (ftw->origin_dfd, &ftw->origin_stbuf) != 0)
    {
      int errsv = errno;
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "fstat: %s", g_strerror (errsv));
      return NULL;
    }

  return g_object_ref (ftw);
}

/**
 * gs_file_tree_walk_next:
 * @self: Self
 * @out_file_type: (out): File type, may be %G_FILE_TYPE_UNKNOWN
 *
 * Returns: %TRUE if there are more files to traverse
 */
gboolean
gs_file_tree_walk_next (GSFileTreeWalk        *self,
                        GFileType             *out_file_type)
{
}

int gs_file_tree_walk_get_dirfd (GSFileTreeWalk   *self);

const char *gs_file_tree_walk_get_name (GSFileTreeWalk   *self);

char *gs_file_tree_walk_get_relpath (GSFileTreeWalk   *self);
