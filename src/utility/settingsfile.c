/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include "settingsfile.h"

static gchar *ini_file_path = NULL;
static GKeyFile *keyfile = NULL;

static void open_ini_file()
{
    if (keyfile == NULL) {
        GError *error = NULL;
        keyfile = g_key_file_new();

        if(!g_key_file_load_from_file(keyfile, get_ini_file_name(),
                                      G_KEY_FILE_KEEP_COMMENTS |
                                      G_KEY_FILE_KEEP_TRANSLATIONS,
                                      &error))
        {
            if (error)
                g_debug("%s", error->message);
        }

        g_clear_error(&error);
    }
}

GKeyFile *get_keyfile()
{
    open_ini_file();
    return keyfile;
}

const gchar *
get_ini_file_name()
{
    if (ini_file_path == NULL) {
        // create app dir
        const gchar *user_config_dir = g_get_user_config_dir();
        gchar *app_data_dir = g_build_filename(user_config_dir, APPLICATION_NAME, NULL);
        g_mkdir_with_parents(app_data_dir, 0755);
        g_free(app_data_dir);

        ini_file_path = g_build_filename(user_config_dir,
                                         APPLICATION_NAME, "veil_client_settings.ini", NULL);

        if (!g_file_test(ini_file_path, G_FILE_TEST_EXISTS)) {
            // create file
            FILE *fp = fopen(ini_file_path, "ab"); fclose(fp);
        }

        g_info("ini_file_path %s", ini_file_path);
    }
    return ini_file_path;
}

void close_ini_file()
{
    if (ini_file_path)
        g_free(ini_file_path);

    if (keyfile)
        g_key_file_free(keyfile);
}

/// str
gchar *
read_str_from_ini_file(const gchar *group_name,  const gchar *key)
{
    gchar *str_value = NULL;

    open_ini_file();
    if (keyfile)
        str_value = g_key_file_get_string(keyfile, group_name, key, NULL);

    return str_value;
}

gchar *read_str_from_ini_file_with_def(const gchar *group_name,  const gchar *key, const gchar *default_val)
{
    gchar *val = read_str_from_ini_file(group_name, key);
    if (val == NULL)
        val = g_strdup(default_val);

    return val;
}

// Возвращает дефолтное значение, если не найдено искомое по ключу. Требуется осводить память возвращаемого указателя
gchar *read_str_from_ini_file_default(const gchar *group_name,  const gchar *key, const gchar *default_str)
{
    gchar *str_value = read_str_from_ini_file(group_name, key);
    if (str_value)
        return str_value;

    return g_strdup(default_str);
}


void
write_str_to_ini_file(const gchar *group_name,  const gchar *key, const gchar *str_value)
{
    if(str_value == NULL)
        return;

    open_ini_file();
    if (keyfile) {
        g_key_file_set_value(keyfile, group_name, key, str_value);
        g_key_file_save_to_file(keyfile, get_ini_file_name(), NULL);
    }
}

///integer
gint
read_int_from_ini_file(const gchar *group_name,  const gchar *key, gint def_value)
{
    GError *error = NULL;
    gint value = 0;

    open_ini_file();
    if (keyfile) {
        value = g_key_file_get_integer(keyfile, group_name, key, &error);

        if (error) {
            value = def_value;
            g_info("%s", error->message);
        }

        g_clear_error(&error);
    } else {
        value = def_value;
    }

    return value;
}


void
write_int_to_ini_file(const gchar *group_name,  const gchar *key, gint value)
{
    open_ini_file();
    if (keyfile) {
        g_key_file_set_integer(keyfile, group_name, key, value);
        g_key_file_save_to_file(keyfile, get_ini_file_name(), NULL);
    }
}
