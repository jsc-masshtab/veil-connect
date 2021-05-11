/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef RDK_VIEWER_H
#define RDK_VIEWER_H

#include <gtk/gtk.h>

#include "remote-viewer.h"
#include "remote-viewer-util.h"
#include "rdp_settings.h"

RemoteViewerState rdp_viewer_start(RemoteViewer *app,
        const gchar *user_name, const gchar *password, gchar *domain, gchar *ip, int port,
                                   VeilRdpSettings *p_rdp_settings);

#endif /* RDK_VIEWER_H */
