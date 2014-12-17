#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libgsystem.h>

static void
test_shutil_rm_rf_noent (void)
{
  GError *error = NULL;
  GFile *noent = g_file_new_for_path ("noent");

  (void) gs_shutil_rm_rf (noent, NULL, &error);
  g_assert_no_error (error);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/shutil/rmrf-noent", test_shutil_rm_rf_noent);

  return g_test_run ();
}
