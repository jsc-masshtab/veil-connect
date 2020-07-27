/*
 * Veil VDI thin client
 * Based on virt-viewer and freerdp
 *
 */

#ifdef __linux__
#include <X11/Xlib.h>
#endif

#include <fcntl.h>

#include <config.h>
#include <locale.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <glib/gi18n.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <stdlib.h>

#include "settingsfile.h"
#include "remote-viewer.h"
#include "remote-viewer-util.h"
#include "crashhandler.h"
#include "vdi_api_session.h"

void
setup_logging()
{
    // get ts
    gint64 cur_ts = g_get_real_time();

    GDateTime *datetime = g_date_time_new_now_local();
    gchar *data_time_string = g_date_time_format(datetime, "%c");
    g_date_time_unref(datetime);

    // log dir dipends on OS
#ifdef __linux__
    const gchar *log_dir = "log/";
#elif _WIN32
    const gchar *locap_app_data_path = g_getenv("LOCALAPPDATA");

    // create log dir in local
    gchar *log_dir = g_strdup_printf("%s/%s/log", locap_app_data_path, PACKAGE);
    g_mkdir_with_parents(log_dir, 0755);
#endif

    // crash handler
    gchar *bt_file_name = g_strconcat(log_dir, "/", data_time_string, "_backtrace.txt", NULL);
    install_crash_handler(bt_file_name);

    //error output
    gchar *stderr_file_name = g_strconcat(log_dir, "/", data_time_string, "_stderr.txt", NULL);
    FILE *err_file_desc = freopen(stderr_file_name, "w", stderr);
    (void)err_file_desc;

    //stdout output
    gchar *stdout_file_name = g_strconcat(log_dir, "/", data_time_string, "_stdout.txt", NULL);
    FILE *out_file_desc = freopen(stdout_file_name, "w", stdout);
    (void)out_file_desc; // it would be polite to close file descriptors

    // free memory
    g_free(data_time_string);
    g_free(bt_file_name);
    g_free(stderr_file_name);
    g_free(stdout_file_name);
#ifdef _WIN32
    g_free(log_dir);
#endif
}

int
main(int argc, char **argv)
{
#ifdef NDEBUG // logging errors and traceback in release mode
    setup_logging();
#else
#endif
    // disable stdout buffering
    setbuf(stdout, NULL);

#ifdef __linux__
    XInitThreads();
#endif

    // print version
    printf("APP VERSION %s\n", VERSION);

    // start session
    start_vdi_session();

    // start app
    int ret = 1;
    GApplication *app = NULL;
    virt_viewer_util_init("Veil Connect");
    app = G_APPLICATION(remote_viewer_new());

    ret = g_application_run(app, argc, argv);

    // free resources
    stop_vdi_session();
    g_object_unref(app);
    free_ini_file_name();

    return ret;
}
//
/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
