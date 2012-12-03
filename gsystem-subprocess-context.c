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

#include "libgsystem.h"

/**
 * SECTION:gssubprocess
 * @title: GSSubprocess Context
 * @short_description: Environment options for launching a child process
 *
 * This class contains a set of options for launching child processes,
 * such as where its standard input and output will be directed, the
 * argument list, the environment, and more.
 *
 * While the #GSSubprocess class has high level functions covering
 * popular cases, use of this class allows access to more advanced
 * options.  It can also be used to launch multiple subprocesses with
 * a similar configuration.
 *
 * Since: 2.36
 */

#include "config.h"

#include "gsystem-subprocess-context-private.h"
#include "gsystem-subprocess.h"

#include <string.h>

typedef GObjectClass GSSubprocessContextClass;

G_DEFINE_TYPE (GSSubprocessContext, gs_subprocess_context, G_TYPE_OBJECT);

enum
{
  PROP_0,
  PROP_ARGV,
  N_PROPS
};

static GParamSpec *gs_subprocess_context_pspecs[N_PROPS];

GSSubprocessContext *
gs_subprocess_context_new (gchar           **argv)
{
  g_return_val_if_fail (argv != NULL && argv[0] != NULL, NULL);

  return g_object_new (GS_TYPE_SUBPROCESS_CONTEXT,
		       "argv", argv,
		       NULL);
}

GSSubprocessContext *
gs_subprocess_context_newv (const gchar  *first_arg,
                           ...)
{
  GSSubprocessContext *result;
  va_list args;

  g_return_val_if_fail (first_arg != NULL, NULL);

  va_start (args, first_arg);
  result = gs_subprocess_context_newa (first_arg, args);
  va_end (args);
  
  return result;
}

GSSubprocessContext *
gs_subprocess_context_newa (const gchar *first_arg,
                           va_list      args)
{
  GSSubprocessContext *result;
  GPtrArray *argv;

  g_return_val_if_fail (first_arg != NULL, NULL);

  argv = g_ptr_array_new ();
  do
    g_ptr_array_add (argv, (gchar*)first_arg);
  while ((first_arg = va_arg (args, const gchar *)) != NULL);
  g_ptr_array_add (argv, NULL);

  result = gs_subprocess_context_new ((gchar**)argv->pdata);
  
  return result;
}

#ifdef G_OS_UNIX
GSSubprocessContext *
gs_subprocess_context_new_argv0 (const gchar      *argv0,
                                gchar           **argv)
{
  GSSubprocessContext *result;
  GPtrArray *real_argv;
  gchar **iter;
  
  g_return_val_if_fail (argv0 != NULL, NULL);
  g_return_val_if_fail (argv != NULL && argv[0] != NULL, NULL);
  
  real_argv = g_ptr_array_new ();
  g_ptr_array_add (real_argv, (gchar*)argv0);
  for (iter = argv; *iter; iter++)
    g_ptr_array_add (real_argv, (gchar*) *iter);
  g_ptr_array_add (real_argv, NULL);

  result = g_object_new (GS_TYPE_SUBPROCESS_CONTEXT,
                         "argv", real_argv->pdata,
                         NULL);
  result->has_argv0 = TRUE;

  return result;
}
#endif

static void
gs_subprocess_context_init (GSSubprocessContext  *self)
{
  self->stdin_fd = -1;
  self->stdout_fd = -1;
  self->stderr_fd = -1;
  self->stdout_disposition = GS_SUBPROCESS_STREAM_DISPOSITION_INHERIT;
  self->stderr_disposition = GS_SUBPROCESS_STREAM_DISPOSITION_INHERIT;
}

static void
gs_subprocess_context_finalize (GObject *object)
{
  GSSubprocessContext *self = GS_SUBPROCESS_CONTEXT (object);

  g_strfreev (self->argv);
  g_strfreev (self->envp);
  g_free (self->cwd);

  g_free (self->stdin_path);
  g_free (self->stdout_path);
  g_free (self->stderr_path);

  if (G_OBJECT_CLASS (gs_subprocess_context_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (gs_subprocess_context_parent_class)->finalize (object);
}

static void
gs_subprocess_context_set_property (GObject      *object,
				   guint         prop_id,
				   const GValue *value,
				   GParamSpec   *pspec)
{
  GSSubprocessContext *self = GS_SUBPROCESS_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_ARGV:
      self->argv = (gchar**) g_value_dup_boxed (value);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gs_subprocess_context_get_property (GObject    *object,
				   guint       prop_id,
				   GValue     *value,
				   GParamSpec *pspec)
{
  GSSubprocessContext *self = GS_SUBPROCESS_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_ARGV:
      g_value_set_boxed (value, self->argv);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
gs_subprocess_context_class_init (GSSubprocessContextClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = gs_subprocess_context_finalize;
  gobject_class->get_property = gs_subprocess_context_get_property;
  gobject_class->set_property = gs_subprocess_context_set_property;

  /**
   * GSSubprocessContext:argv:
   *
   * Array of arguments passed to child process; must have at least
   * one element.  The first element has special handling - if it is
   * an not absolute path ( as determined by g_path_is_absolute() ),
   * then the system search path will be used.  See
   * %G_SPAWN_SEARCH_PATH.
   * 
   * Note that in order to use the Unix-specific argv0 functionality,
   * you must use the setter function
   * gs_subprocess_context_set_args_and_argv0().  For more information
   * about this, see %G_SPAWN_FILE_AND_ARGV_ZERO.
   *
   * Since: 2.36
   */
  gs_subprocess_context_pspecs[PROP_ARGV] = g_param_spec_boxed ("argv", "Arguments", "Arguments for child process", G_TYPE_STRV,
							       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, gs_subprocess_context_pspecs);
}

/* Environment */

void
gs_subprocess_context_set_environment (GSSubprocessContext           *self,
				      gchar                       **environ)
{
  g_strfreev (self->envp);
  self->envp = g_strdupv (environ);
}

void
gs_subprocess_context_set_cwd (GSSubprocessContext           *self,
			      const gchar                  *cwd)
{
  g_free (self->cwd);
  self->cwd = g_strdup (cwd);
}

void
gs_subprocess_context_set_keep_descriptors (GSSubprocessContext           *self,
					   gboolean                      keep_descriptors)

{
  self->keep_descriptors = keep_descriptors ? 1 : 0;
}

void
gs_subprocess_context_set_search_path (GSSubprocessContext           *self,
				      gboolean                      search_path,
				      gboolean                      search_path_from_envp)
{
  self->search_path = search_path ? 1 : 0;
  self->search_path_from_envp = search_path_from_envp ? 1 : 0;
}

void
gs_subprocess_context_set_stdin_disposition (GSSubprocessContext           *self,
					    GSSubprocessStreamDisposition  disposition)
{
  g_return_if_fail (disposition != GS_SUBPROCESS_STREAM_DISPOSITION_STDERR_MERGE);
  self->stdin_disposition = disposition;
}

void
gs_subprocess_context_set_stdout_disposition (GSSubprocessContext           *self,
					     GSSubprocessStreamDisposition  disposition)
{
  g_return_if_fail (disposition != GS_SUBPROCESS_STREAM_DISPOSITION_STDERR_MERGE);
  self->stdout_disposition = disposition;
}

void
gs_subprocess_context_set_stderr_disposition (GSSubprocessContext           *self,
					     GSSubprocessStreamDisposition  disposition)
{
  self->stderr_disposition = disposition;
}

#ifdef G_OS_UNIX
void
gs_subprocess_context_set_stdin_file_path (GSSubprocessContext           *self,
					  const gchar                  *path)
{
  self->stdin_disposition = GS_SUBPROCESS_STREAM_DISPOSITION_NULL;
  g_free (self->stdin_path);
  self->stdin_path = g_strdup (path);
}

void
gs_subprocess_context_set_stdin_fd        (GSSubprocessContext           *self,
					  gint                          fd)
{
  self->stdin_disposition = GS_SUBPROCESS_STREAM_DISPOSITION_NULL;
  self->stdin_fd = fd;
}

void
gs_subprocess_context_set_stdout_file_path (GSSubprocessContext           *self,
					   const gchar                  *path)
{
  self->stdout_disposition = GS_SUBPROCESS_STREAM_DISPOSITION_NULL;
  g_free (self->stdout_path);
  self->stdout_path = g_strdup (path);
}

void
gs_subprocess_context_set_stdout_fd (GSSubprocessContext           *self,
				    gint                          fd)
{
  self->stdout_disposition = GS_SUBPROCESS_STREAM_DISPOSITION_NULL;
  self->stdout_fd = fd;
}

void
gs_subprocess_context_set_stderr_file_path (GSSubprocessContext           *self,
					   const gchar                  *path)
{
  self->stderr_disposition = GS_SUBPROCESS_STREAM_DISPOSITION_NULL;
  g_free (self->stderr_path);
  self->stderr_path = g_strdup (path);
}

void
gs_subprocess_context_set_stderr_fd        (GSSubprocessContext           *self,
					   gint                          fd)
{
  self->stderr_disposition = GS_SUBPROCESS_STREAM_DISPOSITION_NULL;
  self->stderr_fd = fd;
}
#endif

#ifdef G_OS_UNIX
void
gs_subprocess_context_set_child_setup (GSSubprocessContext           *self,
				      GSpawnChildSetupFunc          child_setup,
				      gpointer                      user_data)
{
  self->child_setup_func = child_setup;
  self->child_setup_data = user_data;
}
#endif