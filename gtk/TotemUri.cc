#include "TotemUri.h"




static void
totem_ensure_dir (const char *path)
{
	if (g_file_test (path, G_FILE_TEST_IS_DIR) != FALSE)
		return;

	g_mkdir_with_parents (path, 0700);
}

const char *
totem_dot_config_dir (void)
{
	static char *totem_dir = NULL;

	if (totem_dir != NULL) {
		totem_ensure_dir (totem_dir);
		return totem_dir;
	}

	totem_dir = g_build_filename (g_get_user_config_dir(),
				      "totem",
				      NULL);

	totem_ensure_dir (totem_dir);

	return (const char *)totem_dir;
}

const char *
totem_data_dot_dir (void)
{
	static char *totem_dir = NULL;

	if (totem_dir != NULL) {
		totem_ensure_dir (totem_dir);
		return totem_dir;
	}

	totem_dir = g_build_filename (g_get_user_data_dir (),
				      "totem",
				      NULL);

	totem_ensure_dir (totem_dir);

	return (const char *)totem_dir;
}
