/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <glib/gi18n.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "remote-viewer-util.h"
#include "settingsfile.h"
#include "settings_data.h"
#include "x2go_launcher.h"
#include "vdi_event.h"


#define MAX_PARAM_AMOUNT 30


static void x2go_launcher_clear(X2goLauncher *self)
{
    // Release resources
    g_free(self->qt_client_data.user);
    g_free(self->qt_client_data.password);

    if (self->send_signal_upon_job_finish)
        g_signal_emit_by_name(self, "job-finished");
}

// Вызывается когда процесс завершился
static void
x2go_launcher_cb_child_watch( GPid pid, gint status, X2goLauncher *self )
{
    g_info("FINISHED. %s Status: %i", __func__, status);
    /* Close pid */
    g_spawn_close_pid( pid );
    self->qt_client_data.is_launched = FALSE;
    self->qt_client_data.pid = 0;

    x2go_launcher_clear(self);

    vdi_event_vm_changed_notify(vdi_session_get_current_vm_id(), VDI_EVENT_TYPE_VM_DISCONNECTED);
}

static gboolean x2go_launcher_fill_settings_file(X2goLauncher *self, const gchar *x2go_data_file_name)
{
    const ConnectSettingsData *conn_data = self->p_conn_data;

    GKeyFile *x2go_keyfile = g_key_file_new();
    GError *error = NULL;

    if(!g_key_file_load_from_file(x2go_keyfile, x2go_data_file_name,
                                  G_KEY_FILE_KEEP_COMMENTS |
                                  G_KEY_FILE_KEEP_TRANSLATIONS,
                                  &error)) {
        if (error)
            g_warning("%s", error->message);
        g_clear_error(&error);
        return FALSE;
    }

    const gchar *group_name = "X2GoSession";

    if (x2go_keyfile == NULL) {
        g_warning("x2go_keyfile == NULL");
        return FALSE;
    }
    if (conn_data->ip)
        g_key_file_set_value(x2go_keyfile, group_name, "host", conn_data->ip);
    if (self->qt_client_data.user)
        g_key_file_set_value(x2go_keyfile, group_name, "user", self->qt_client_data.user);
    if (conn_data->x2Go_settings.x2go_session_type)
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

static gboolean x2go_launcher_launch_process(X2goLauncher *self, gchar **error_msg)
{
    // Create base settings file
    // Open template settings file
    const char *x2go_template_filename = "x2go_data/x2go_sessions";
    FILE *sourceFile = fopen(x2go_template_filename,"r");
    if (sourceFile == NULL) {
        *error_msg = g_strdup_printf(_("Unable to open %s"), x2go_template_filename);
        return FALSE;
    }
    // g_get_user_data_dir
    const gchar *user_config_dir = g_get_user_config_dir();
    g_autofree gchar *app_x2go_data_dir = NULL;
    app_x2go_data_dir = g_build_filename(user_config_dir, APP_FILES_DIRECTORY_NAME, "x2go_data", NULL);
    g_mkdir_with_parents(app_x2go_data_dir, 0755);

    g_autofree gchar *x2go_data_file_name = NULL;
    x2go_data_file_name = g_build_filename(app_x2go_data_dir, "x2go_sessions", NULL);
    g_info("app_x2go_data_dir: %s", x2go_data_file_name);

    convert_string_from_utf8_to_locale(&x2go_data_file_name);

    FILE *destFile = fopen(x2go_data_file_name, "w");

    if (destFile == NULL) {
        // Unable to open
        fclose(sourceFile);
        *error_msg = g_strdup_printf(_("\nUnable to open file %s."), x2go_data_file_name);
        return FALSE;
    }
    // Copy file
    copy_file_content(sourceFile, destFile);
    fclose(destFile);

    // Fill settings file
    if(!x2go_launcher_fill_settings_file(self, x2go_data_file_name)) {
        *error_msg = g_strdup_printf(_("Unable to prepare x2go config file"));
        return FALSE;
    }

    // Parameters
    gchar *argv[MAX_PARAM_AMOUNT] = {};
    int index = 0;
#ifdef G_OS_WIN32
    argv[index] = g_strdup("C:\\Program Files (x86)\\x2goclient\\x2goclient.exe");
#elif __linux__
    argv[index] = g_strdup("x2goclient");
#endif
    argv[++index] = g_strdup("--no-menu");
    argv[++index] = g_strdup("--close-disconnect");
    argv[++index] = g_strdup("--session=X2GoSession");
    argv[++index] = g_strdup_printf("--session-conf=%s", x2go_data_file_name);

    /* Spawn child process */
    GError *error = NULL;
    self->qt_client_data.is_launched = g_spawn_async(NULL, argv, NULL,
            G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH , NULL,
            NULL, &self->qt_client_data.pid, &error);

    for (guint i = 0; i < (sizeof(argv) / sizeof(gchar *)); i++) {
        g_free(argv[i]);
    }

    if(!self->qt_client_data.is_launched || self->qt_client_data.pid <= 0) {
        if (error) {
            g_warning("%s", error->message);
            g_clear_error(&error);
        }
        *error_msg = g_strdup_printf(_("Failed to start process x2goclient"));
        return FALSE;
    }

    vdi_event_vm_changed_notify(vdi_session_get_current_vm_id(), VDI_EVENT_TYPE_VM_CONNECTED);

    /* Add watch function to catch termination of the process. This function
     * will clean any remnants of process. */
    g_child_watch_add(self->qt_client_data.pid, (GChildWatchFunc)x2go_launcher_cb_child_watch, self);

    return TRUE;
}

static gboolean x2go_launcher_setup_client()
{
    g_autofree gchar *x2goclient_settings_file = NULL;
    x2goclient_settings_file = g_build_filename(g_get_home_dir(), ".x2goclient", "settings", NULL);
    g_info("x2goclient_settings_file: %s", x2goclient_settings_file);
    GKeyFile *x2goclient_keyfile = g_key_file_new();
    GError *error = NULL;

    if(!g_key_file_load_from_file(x2goclient_keyfile, x2goclient_settings_file,
                                  G_KEY_FILE_KEEP_COMMENTS |
                                  G_KEY_FILE_KEEP_TRANSLATIONS,
                                  &error)) {
        if (error)
            g_warning("%s", error->message);
        g_clear_error(&error);
        g_key_file_free(x2goclient_keyfile);
        return FALSE;
    }

    g_key_file_set_boolean(x2goclient_keyfile, "trayicon", "noclose", FALSE);
    g_key_file_save_to_file(x2goclient_keyfile, x2goclient_settings_file, NULL);

    g_key_file_free(x2goclient_keyfile);

    return TRUE;
}

void x2go_launcher_start_qt_client(X2goLauncher *self, const gchar *user, const gchar *password)
{
    self->qt_client_data.user = g_strdup(user);
    self->qt_client_data.password = g_strdup(password);

    // При закрытии окна процесс клиента x2go не завершается, из-за чего пользователь не
    // может вернутся к вбору пулов. Поэтому ключаем настройку для заввершения процесса при
    // закрытии окна
    x2go_launcher_setup_client();

    // Process
    g_autofree gchar *error_msg = NULL;
    if (!x2go_launcher_launch_process(self, &error_msg)) {
        g_warning("%s", error_msg);
        show_msg_box_dialog(NULL, error_msg);
        g_signal_emit_by_name(self, "job-finished");
    }
}
