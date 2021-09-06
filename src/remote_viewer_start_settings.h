/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef REMOTE_VIEWER_START_SETTINGS_H
#define REMOTE_VIEWER_START_SETTINGS_H

#include <gtk/gtk.h>

#include <winpr/wtypes.h>

#include "settings_data.h"

#include "remote-viewer.h"

GtkResponseType remote_viewer_start_settings_dialog(RemoteViewer *p_remote_viewer, GtkWindow *parent);


#endif // REMOTE_VIEWER_START_SETTINGS_H
