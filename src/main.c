/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <config.h>

#include <locale.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>

#ifdef G_OS_UNIX
#include <X11/Xlib.h>
#endif

#include <freerdp/version.h>

#include "settingsfile.h"
#include "remote-viewer.h"
#include "remote-viewer-util.h"
#include "veil_logger.h"


int
main(int argc, char **argv)
{
    veil_logger_setup();

    // disable stdout buffering
    setbuf(stdout, NULL);

#ifdef G_OS_UNIX
    XInitThreads();
#endif
    // start app
    virt_viewer_util_init(APPLICATION_NAME);
    g_info("APP VERSION %s FREERDP_VERSION %s", VERSION, FREERDP_VERSION_FULL);
    g_info("Build data time: %s %s", __DATE__, __TIME__);
    g_info("Ini file path: %s", get_ini_file_name());

    gboolean unique_app = read_int_from_ini_file("ServiceSettings", "unique_app", TRUE);
    RemoteViewer *remote_viewer = remote_viewer_new(unique_app);
    GApplication *app = G_APPLICATION(remote_viewer);

    int ret = g_application_run(app, argc, argv);

    // free resources
    remote_viewer_free_resources(remote_viewer);
    g_object_unref(remote_viewer);
    close_ini_file();
    veil_logger_free();

    return ret;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
