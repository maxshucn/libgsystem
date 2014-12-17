#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libgsystem.h>
#include <glib-unix.h>

static void
test_shutil_rm_rf_noent (void)
{
  GError *error = NULL;
  GFile *noent = g_file_new_for_path ("noent");

  (void) gs_shutil_rm_rf (noent, NULL, &error);
  g_assert_no_error (error);
}

static void
test_shutil_rm_rf_file (void)
{
  GError *error = NULL;
  GFile *empty = g_file_new_for_path ("empty");

  (void) g_file_replace_contents (empty, "", 0, NULL, FALSE, 0, NULL, NULL, &error);
  g_assert_no_error (error);

  g_assert (g_file_query_exists (empty, NULL));

  (void) gs_shutil_rm_rf (empty, NULL, &error);
  g_assert_no_error (error);

  g_assert (!g_file_query_exists (empty, NULL));
}

static void
test_shutil_rm_rf_dir (void)
{
  GError *error = NULL;
  GFile *empty = g_file_new_for_path ("empty");

  (void) g_file_make_directory (empty, NULL, &error);
  g_assert_no_error (error);

  g_assert (g_file_query_exists (empty, NULL));

  (void) gs_shutil_rm_rf (empty, NULL, &error);
  g_assert_no_error (error);

  g_assert (!g_file_query_exists (empty, NULL));
}

static void
test_shutil_rm_rf_random (void)
{
  GError *error = NULL;
  GFile *testdir = g_file_new_for_path ("testdir");
  guint i = 0, depth = 0;
  const guint maxdepth = 20;
  DIR *cwddir = NULL;
  int r;

  (void) g_file_make_directory (testdir, NULL, &error);
  g_assert_no_error (error);

  cwddir = opendir ("testdir");

  for (i = 0; i < 255; i++)
    {
      guint8 op = (guint8)g_random_int_range (0, 5);
      gs_free char *name = NULL;
      
      name = g_strdup_printf ("%02X", g_random_int_range (0, G_MAXUINT8));
    again:
      switch (op)
	{
	case 0:
	  r = mkdirat (dirfd (cwddir), name, 0755);
	  if (r != 0)
	    continue;
	  break;
	case 1:
	  if (depth < maxdepth)
	    {
	      int newfd;
	      r = mkdirat (dirfd (cwddir), name, 0755);
	      if (r != 0)
		continue;
	    
	      (void) gs_file_open_dir_fd_at (dirfd (cwddir), name, &newfd, NULL, &error);
	      g_assert_no_error (error);
	    
	      (void) closedir (cwddir);
	      depth++;
	      cwddir = fdopendir (newfd);
	    }
	  else
	    {
	      op = 2;
	      goto again;
	    }
	  break;
	case 2:
	  if (depth > 0)
	    {
	      DIR *newdir = opendir ("..");
	      (void) closedir (cwddir);
	      cwddir = newdir;
	      depth--;
	    }
	  else
	    {
	      op = 1;
	      goto again;
	    }
	  break;
	case 3:
	  {
	    int fd = openat (dirfd (cwddir), name, O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0644);
	    if (fd != -1)
	      (void) close (fd);
	  }
	  break;
	case 4:
	  {
	    (void) symlinkat ("notarget", dirfd (cwddir), name);
	  }
	}
    }

  (void) closedir (cwddir);

  system ("ls -lR testdir");

  (void) gs_shutil_rm_rf (testdir, NULL, &error);

  g_assert (!g_file_query_exists (testdir, NULL));
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/shutil/rmrf-noent", test_shutil_rm_rf_noent);
  g_test_add_func ("/shutil/rmrf-file", test_shutil_rm_rf_file);
  g_test_add_func ("/shutil/rmrf-dir", test_shutil_rm_rf_file);
  g_test_add_func ("/shutil/rmrf-random", test_shutil_rm_rf_random);

  return g_test_run ();
}
