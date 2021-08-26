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
    virt_viewer_util_init(APPLICATION_NAME_WITH_SPACES);
    g_info("APP VERSION %s FREERDP_VERSION %s", VERSION, FREERDP_VERSION_FULL);
    g_info("Build data time: %s %s", __DATE__, __TIME__);

    RemoteViewer *remote_viewer = remote_viewer_new();
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
