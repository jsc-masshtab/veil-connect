/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "remote-viewer-util.h"
#include "settingsfile.h"
#include "x2go_launcher.h"


#define MAX_PARAM_AMOUNT 30

typedef struct
{
    GMainLoop *loop;

    GPid pid;
    gboolean is_launched;

    const ConnectSettingsData *p_data;

} X2goData;

// Вызывается когда процесс завершился
static void
x2go_launcher_cb_child_watch( GPid  pid, gint status, X2goData *data )
{
    g_info("FINISHED. %s Status: %i", __func__, status);
    /* Close pid */
    g_spawn_close_pid( pid );
    data->is_launched = FALSE;
    data->pid = 0;

    shutdown_loop(data->loop);
}

static gboolean x2go_launcher_fill_settings_file(X2goData *data, const gchar *x2go_data_file_name)
{
    const ConnectSettingsData *conn_data = data->p_data;

    GKeyFile *x2go_keyfile = g_key_file_new();
    GError *error = NULL;

    if(!g_key_file_load_from_file(x2go_keyfile, x2go_data_file_name,
                                  G_KEY_FILE_KEEP_COMMENTS |
                                  G_KEY_FILE_KEEP_TRANSLATIONS,
                                  &error)) {
        if (error)
            g_debug("%s", error->message);
        g_clear_error(&error);
        return FALSE;
    }

    const gchar *group_name = "X2GoSession";
    g_key_file_set_value(x2go_keyfile, group_name, "host", conn_data->ip);
    g_key_file_set_value(x2go_keyfile, group_name, "user", conn_data->user);
    g_key_file_set_value(x2go_keyfile, group_name, "command", conn_data->x2Go_settings.x2go_session_type);
    g_key_file_set_integer(x2go_keyfile, group_name, "fullscreen", conn_data->x2Go_settings.full_screen);
    gint speed;
    if (g_strcmp0(conn_data->x2Go_settings.x2go_conn_type, "modem") == 0)
        speed = 0;
    else if (g_strcmp0(conn_data->x2Go_settings.x2go_conn_type, "isdn") == 0)
        speed = 1;
    else if (g_strcmp0(conn_data->x2Go_settings.x2go_conn_type, "adsl") == 0)
        speed = 2;
    else if (g_strcmp0(conn_data->x2Go_settings.x2go_conn_type, "wan") == 0)
        speed = 3;
    else
        speed = 4;
    g_key_file_set_integer(x2go_keyfile, group_name, "speed", speed);
    g_key_file_save_to_file(x2go_keyfile, x2go_data_file_name, NULL);
    g_key_file_free(x2go_keyfile);

    return TRUE;
}

static void x2go_launcher_launch_process(X2goData *data)
{
    // Create base settings file
    const char *x2go_template_filename = "x2go_data/x2go_sessions";
    FILE *sourceFile = fopen(x2go_template_filename,"r");
    if (sourceFile == NULL) {
        g_info("Unable to open %s", x2go_template_filename);
        return;
    }

    g_autofree gchar *app_data_dir = NULL;
    app_data_dir = get_windows_app_data_location();
    g_autofree gchar *app_x2go_data_dir = NULL;
    app_x2go_data_dir = g_strdup_printf("%s/x2go_data", app_data_dir);
    g_mkdir_with_parents(app_x2go_data_dir, 0755);

    g_autofree gchar *x2go_data_file_name = NULL;
    x2go_data_file_name = g_strdup_printf("%s/x2go_sessions", app_x2go_data_dir);

    convert_string_from_utf8_to_locale(&x2go_data_file_name);

    FILE *destFile = fopen(x2go_data_file_name, "w");

    if (destFile == NULL) {
        // Unable to open
        g_info("\nUnable to open file %s.", x2go_data_file_name);
        fclose(sourceFile);
        return;
    }
    // Copy file
    copy_file_content(sourceFile, destFile);
    fclose(destFile);

    // Fill settings file
    if(!x2go_launcher_fill_settings_file(data, x2go_data_file_name))
        return;

    // Parameters
    gchar *argv[MAX_PARAM_AMOUNT] = {};
    int index = 0;
    argv[index] = g_strdup("C:\\Program Files (x86)\\x2goclient\\x2goclient.exe");
    argv[++index] = g_strdup("--no-menu");
    argv[++index] = g_strdup("--close-disconnect");
    argv[++index] = g_strdup("--session=X2GoSession");
    argv[++index] = g_strdup_printf("--session-conf=%s", x2go_data_file_name);

    /* Spawn child process */
    GError *error = NULL;
    data->is_launched = g_spawn_async(NULL, argv, NULL,
            G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH , NULL,
            NULL, &data->pid, &error);

    for (guint i = 0; i < (sizeof(argv) / sizeof(gchar *)); i++) {
        g_free(argv[i]);
    }

    if(!data->is_launched || data->pid <= 0) {
        g_warning("X2GO SPAWN FAILED");
        if (error) {
            g_warning("%s", error->message);
            g_clear_error(&error);
        }
        return;
    }

    /* Add watch function to catch termination of the process. This function
     * will clean any remnants of process. */
    g_child_watch_add(data->pid, (GChildWatchFunc)x2go_launcher_cb_child_watch, data);
}

void x2go_launcher_start(const ConnectSettingsData *con_data)
{
    X2goData data = {};
    data.p_data = con_data;

    // Process
    x2go_launcher_launch_process(&data);

    create_loop_and_launch(&data.loop);

    // Release resources
}
