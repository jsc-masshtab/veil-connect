/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef REMOTE_VIEWER_CONNECT_H
#define REMOTE_VIEWER_CONNECT_H

#include <gtk/gtk.h>

#include "vdi_session.h"
#include "connect_settings_data.h"
#include "remote-viewer.h"


GtkResponseType remote_viewer_connect_dialog(RemoteViewer *remote_viewer, ConnectSettingsData *connect_settings_data);

#endif /* REMOTE_VIEWER_CONNECT_H */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
