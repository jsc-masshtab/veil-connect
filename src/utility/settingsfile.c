//
// Created by Solomin on 18.07.19.
//

#include "settingsfile.h"

static gchar *ini_file_path = NULL;

const gchar *
get_ini_file_name()
{
    if (ini_file_path == NULL) {
#ifdef __linux__
        ini_file_path = g_strdup("veil_client_settings.ini");
#elif _WIN32
        const gchar *locap_app_data_path = g_getenv("LOCALAPPDATA");
        // create log dir in local
        gchar *app_data_dir = g_strdup_printf("%s/%s/", locap_app_data_path, PACKAGE);
        g_mkdir_with_parents(app_data_dir, 0755);
        // ini file full path
        ini_file_path = g_strdup_printf("%s/veil_client_settings.ini", app_data_dir);
        g_free(app_data_dir);

        // prefill (maybe temp) if file didnt exist
        if (!g_file_test(ini_file_path, G_FILE_TEST_EXISTS)) {
            write_str_to_ini_file("RDPSettings", "rdp_pixel_format", "BGRA16");
            write_str_to_ini_file("RDPSettings", "rdp_args", "");
        }
#endif
    }
    return ini_file_path;
}

void free_ini_file_name()
{
    g_free(ini_file_path);
}

// Это конечно не оптимально открывать файл каждый раз чтоб записать или получить одно значение.
// Но потери производительности не существенны, зато существенно облегчение работы с ini.
/// str
gchar *
read_str_from_ini_file(const gchar *group_name,  const gchar *key)
{
    GError *error = NULL;
    gchar *str_value = NULL;

    GKeyFile *keyfile = g_key_file_new();

    if(!g_key_file_load_from_file(keyfile, get_ini_file_name(),
                                  G_KEY_FILE_KEEP_COMMENTS |
                                  G_KEY_FILE_KEEP_TRANSLATIONS,
                                  &error))
    {
        if (error)
            g_debug("%s", error->message);
    }
    else
    {
        str_value = g_key_file_get_string(keyfile, group_name, key, NULL);
    }

    g_clear_error(&error);
    g_key_file_free(keyfile);

    return str_value;
}


void
write_str_to_ini_file(const gchar *group_name,  const gchar *key, const gchar *str_value)
{
    if(str_value == NULL)
        return;

    GError *error = NULL;

    GKeyFile *keyfile = g_key_file_new();

    if(!g_key_file_load_from_file(keyfile, get_ini_file_name(),
                                  G_KEY_FILE_KEEP_COMMENTS |
                                  G_KEY_FILE_KEEP_TRANSLATIONS,
                                  &error))
    {
        if (error)
            g_debug("%s", error->message);
    }
    else
    {
        g_key_file_set_value(keyfile, group_name, key, str_value);
    }

    g_clear_error(&error);
    g_key_file_save_to_file(keyfile, get_ini_file_name(), NULL);
    g_key_file_free(keyfile);
}

///integer
gint
read_int_from_ini_file(const gchar *group_name,  const gchar *key, gint def_value)
{
    GError *error = NULL;
    gint value = 0;
    GKeyFile *keyfile = g_key_file_new();

    if(!g_key_file_load_from_file(keyfile, get_ini_file_name(),
                                  G_KEY_FILE_KEEP_COMMENTS |
                                  G_KEY_FILE_KEEP_TRANSLATIONS,
                                  &error))
    {
        if (error)
            g_debug("%s", error->message);
    }
    else
    {
        g_clear_error(&error);
        value = g_key_file_get_integer(keyfile, group_name, key, &error);

        if (error)
            value = def_value;
    }

    g_clear_error(&error);
    g_key_file_free(keyfile);

    return value;
}


void
write_int_to_ini_file(const gchar *group_name,  const gchar *key, gint value)
{
    GError *error = NULL;
    GKeyFile *keyfile = g_key_file_new();

    if(!g_key_file_load_from_file(keyfile, get_ini_file_name(),
                                  G_KEY_FILE_KEEP_COMMENTS |
                                  G_KEY_FILE_KEEP_TRANSLATIONS,
                                  &error))
    {
        if (error)
            g_debug("%s", error->message);
    }
    else
    {
        g_key_file_set_integer(keyfile, group_name, key, value);
    }

    g_clear_error(&error);
    g_key_file_save_to_file(keyfile, get_ini_file_name(), NULL);
    g_key_file_free(keyfile);
}
