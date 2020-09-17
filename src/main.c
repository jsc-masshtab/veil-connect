/*
 * Veil Connect
 * Based on virt-viewer and freerdp
 *
 */

#ifdef __linux__
#include <X11/Xlib.h>
#endif

#include <config.h>
#include <locale.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>

#include "settingsfile.h"
#include "remote-viewer.h"
#include "remote-viewer-util.h"
#include "crashhandler.h"
#include "vdi_session.h"
#include "usbredir_controller.h"

void
setup_logging()
{
    gchar *log_dir = get_log_dir_path();
    // create log dir
    g_mkdir_with_parents(log_dir, 0755);

    // Return if we cant write to log dir
    if (g_access(log_dir, W_OK) != 0) {
        printf("Log directory is protected from writing\n");
        g_free(log_dir);
        return;
    }

    // get ts
    GDateTime *datetime = g_date_time_new_now_local();
    gchar *data_time_string = g_date_time_format(datetime, "%Y_%m_%d___%H_%M_%S");
    // printf("data_time_string %s", data_time_string);
    g_date_time_unref(datetime);

    // crash handler
    gchar *bt_file_name = g_strconcat(log_dir, "/", data_time_string, "_backtrace.txt", NULL);
    convert_string_from_utf8_to_locale(&bt_file_name);
    install_crash_handler(bt_file_name);

    //error output
    gchar *stderr_file_name = g_strconcat(log_dir, "/", data_time_string, "_stderr.txt", NULL);
    convert_string_from_utf8_to_locale(&stderr_file_name);
    FILE *err_file_desc = freopen(stderr_file_name, "w", stderr);
    (void)err_file_desc;

    //stdout output
    gchar *stdout_file_name = g_strconcat(log_dir, "/", data_time_string, "_stdout.txt", NULL);
    convert_string_from_utf8_to_locale(&stdout_file_name);
    FILE *out_file_desc = freopen(stdout_file_name, "w", stdout);
    (void)out_file_desc; // it would be polite to close file descriptors

    // free memory
    g_free(data_time_string);
    g_free(bt_file_name);
    g_free(stderr_file_name);
    g_free(stdout_file_name);
    g_free(log_dir);
}

int
main(int argc, char **argv)
{
#ifdef NDEBUG // logging errors and traceback in release mode
    setup_logging();
#endif
    // disable stdout buffering
    setbuf(stdout, NULL);

#ifdef __linux__
    XInitThreads();
#endif
    // init usb redir
    usbredir_controller_init();

    // start session
    vdi_session_create();

    // start app
    GApplication *app = NULL;
    virt_viewer_util_init("Veil Connect");
    g_info("APP VERSION %s", VERSION);

    app = G_APPLICATION(remote_viewer_new());

    int ret = g_application_run(app, argc, argv);

    // free resources
    vdi_session_destroy();
    g_object_unref(app);
    free_ini_file_name();
    usbredir_controller_deinit();

    return ret;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
