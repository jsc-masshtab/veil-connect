/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <config.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#ifdef G_OS_WIN32
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#endif

#include "rdp_viewer.h"
#include "remote-viewer-util.h"


typedef struct
{
    GMainLoop *loop;

    GPid pid;
    gboolean is_launched;

} NativeRdpData;

#ifdef _WIN32
static void
windows_rdp_launcher_cb_child_watch( GPid  pid, gint status, NativeRdpData *data)
{
    g_info("FINISHED. %s Status: %i", __func__, status);
    g_spawn_close_pid(pid);

    shutdown_loop(data->loop);
}

static void
on_ws_cmd_received(gpointer data G_GNUC_UNUSED, const gchar *cmd, NativeRdpData *native_rdp_data)
{
    if (g_strcmp0(cmd, "DISCONNECT") == 0) {
        TerminateProcess(native_rdp_data->pid, 0);
    }
}

static void
append_rdp_data(FILE *destFile, const gchar *param_name, const gchar *param_value)
{
    if (param_value == NULL)
        return;

    gchar *full_address = g_strdup_printf("%s:%s\n", param_name, param_value);
    fputs(full_address, destFile);
    g_free(full_address);
}
#endif


void
launch_windows_rdp_client(const VeilRdpSettings *p_rdp_settings)
{
#ifdef __linux__
    (void)p_rdp_settings;
#elif defined _WIN32
    //create rdp file based on template
    //open template for reading and take its content
    FILE *sourceFile;
    FILE *destFile;

    const char *rdp_template_filename = "rdp_data/rdp_template_file.txt";
    sourceFile = fopen(rdp_template_filename,"r");
    if (sourceFile == NULL) {
        g_info("Unable to open file rdp_data/rdp_template_file.txt");
        return;
    }

    gchar *app_data_dir = get_windows_app_data_location();
    gchar *app_rdp_data_dir = g_strdup_printf("%s/rdp_data", app_data_dir);
    g_mkdir_with_parents(app_rdp_data_dir, 0755);

    g_autofree gchar *rdp_data_file_name = NULL;
    rdp_data_file_name = g_strdup_printf("%s/rdp_file.rdp", app_rdp_data_dir);
    g_free(app_rdp_data_dir);
    g_free(app_data_dir);

    convert_string_from_utf8_to_locale(&rdp_data_file_name);
    destFile = fopen(rdp_data_file_name, "w");

    /* fopen() return NULL if unable to open file in given mode. */
    if (destFile == NULL)
    {
        // Unable to open
        g_info("\nUnable to open file %s.", rdp_data_file_name);
        g_info("Please check if file exists and you have read/write privilege.");
        fclose(sourceFile);
        return;
    }
    // Copy file    char ch;
    copy_file_content(sourceFile, destFile);

    // append unique data
    append_rdp_data(destFile, "full address:s", p_rdp_settings->ip);
    append_rdp_data(destFile, "username:s", p_rdp_settings->user_name);
    append_rdp_data(destFile, "domain:s", p_rdp_settings->domain);

    if (p_rdp_settings->is_remote_app) {
        append_rdp_data(destFile, "remoteapplicationmode:i", "1");
        append_rdp_data(destFile,"remoteapplicationprogram:s", p_rdp_settings->remote_app_program);
        append_rdp_data(destFile,"remoteapplicationcmdline:s", p_rdp_settings->remote_app_options);
    } else {
        append_rdp_data(destFile, "remoteapplicationmode:i", "0");
    }

    append_rdp_data(destFile, "use multimon:i", p_rdp_settings->is_multimon ? "1" : "0");
    append_rdp_data(destFile, "redirectsmartcards:i", p_rdp_settings->redirectsmartcards ? "1" : "0");
    append_rdp_data(destFile, "redirectprinters:i", p_rdp_settings->redirectprinters ? "1" : "0");
    append_rdp_data(destFile, "allow font smoothing:i",
                    p_rdp_settings->disable_rdp_fonts ? "0" : "1");
    append_rdp_data(destFile, "disable themes:i", p_rdp_settings->disable_rdp_themes ? "1" : "0");

    /* Close files to release resources */
    fclose(sourceFile);
    fclose(destFile);

    NativeRdpData data = {};

    // launch process
    gchar *argv[3] = {};
    int index = 0;
    argv[index] = g_strdup("mstsc");
    argv[++index] = g_strdup(rdp_data_file_name);

    GError *error = NULL;

    data.is_launched = g_spawn_async(NULL, argv, NULL,
                                     G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL,
                                     NULL, &data.pid, &error);

    for (guint i = 0; i < (sizeof(argv) / sizeof(gchar *)); i++) {
        g_free(argv[i]);
    }

    if (!data.is_launched) {
        g_warning("mstsc SPAWN FAILED");
        if (error) {
            g_warning("%s", error->message);
            g_clear_error(&error);
        }
        return;
    }

    // disconnect signal
    gulong ws_cmd_received_handle = g_signal_connect(get_vdi_session_static(), "ws-cmd-received",
                                                        G_CALLBACK(on_ws_cmd_received), &data);

    // stop process callback
    g_child_watch_add(data.pid, (GChildWatchFunc)windows_rdp_launcher_cb_child_watch, &data);

    // launch event loop
    create_loop_and_launch(&data.loop);

    g_signal_handler_disconnect(get_vdi_session_static(), ws_cmd_received_handle);

#endif
}
