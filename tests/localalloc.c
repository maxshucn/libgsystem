#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libgsystem.h>
#include <glib-unix.h>

static void
test_localalloc (void)
{
  gs_free char *str = g_strdup ("str");
  gs_free char *nullstr = NULL;
  gs_unref_object GFile *obj = g_file_new_for_path ("obj");
  gs_unref_object GFile *nullobj = NULL;
  gs_unref_variant GVariant *v = g_variant_new_strv (NULL, 0);
  gs_free_variant_iter GVariantIter *viter = g_variant_iter_new (v);
  gs_unref_variant_builder GVariantBuilder *vbuilder = g_variant_builder_new (G_VARIANT_TYPE("a{sv}"));
  gs_unref_array GArray *arr = g_array_new (TRUE, FALSE, 42);
  gs_unref_ptrarray GPtrArray *parr = g_ptr_array_new ();
  gs_unref_hashtable GHashTable *hash = g_hash_table_new (NULL, NULL);
  gs_free_list GList *list = g_list_append (NULL, (gpointer)42);
  gs_free_slist GSList *slist = g_slist_append (NULL, (gpointer)42);
  gs_free_checksum GChecksum *csum = g_checksum_new (G_CHECKSUM_SHA256);
  gs_unref_bytes GBytes *bytes = g_bytes_new ("hello", 5);
  gs_strfreev char **strv = g_get_environ ();
  gs_free_error GError *err = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED, "oops %s", "darn!");
  gs_unref_keyfile GKeyFile *keyfile = g_key_file_new ();
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/localalloc/all", test_localalloc);

  return g_test_run ();
}
