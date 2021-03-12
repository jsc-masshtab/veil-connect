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

#include "remote-viewer-util.h"

RemoteViewerState rdp_viewer_start(const gchar *usename, const gchar *password, gchar *domain, gchar *ip, int port);

#endif /* RDK_VIEWER_H */
