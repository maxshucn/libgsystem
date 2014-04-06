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

#ifndef __GSYSTEM_FILE_TREEWALK_H__
#define __GSYSTEM_FILE_TREEWALK_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define GS_TYPE_FILE_TREE_WALK         (gs_file_tree_walk_get_type ())
#define GS_FILE_TREE_WALK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GS_TYPE_FILE_TREE_WALK, GSFileTreeWalk))
#define GS_IS_FILE_TREE_WALK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GS_TYPE_FILE_TREE_WALK))

typedef struct _GSFileTreeWalk GSFileTreeWalk;

GType gs_file_tree_walk_get_type (void) G_GNUC_CONST;

typedef enum {
  GS_FILE_TREE_WALK_FLAG_DEPTH = 1 << 0,
  GS_FILE_TREE_WALK_FLAG_NOXDEV = 1 << 1
} GSFileTreeWalkFlags;

GSFileTreeWalk *gs_file_tree_walk_open (GFile                   *path,
                                        GSFileTreeWalkFlags      flags,
                                        GCancellable            *cancellable,
                                        GError                 **error);

GSFileTreeWalk *gs_file_tree_walk_open_at (int                   dfd,
                                           GSFileTreeWalkFlags   flags,
                                           GCancellable         *cancellable,
                                           GError              **error);

gboolean gs_file_tree_walk_next (GSFileTreeWalk        *self,
                                 GFileType             *out_file_type);

int gs_file_tree_walk_get_dirfd (GSFileTreeWalk   *self);

const char *gs_file_tree_walk_get_name (GSFileTreeWalk   *self);

char *gs_file_tree_walk_get_relpath (GSFileTreeWalk   *self);

G_END_DECLS

#endif
