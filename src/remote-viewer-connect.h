/*
 * Veil VDI thin client
 * Based on virt-viewer and freerdp
 *
 */

#ifndef REMOTE_VIEWER_CONNECT_H
#define REMOTE_VIEWER_CONNECT_H

#include <gtk/gtk.h>

#include "vdi_session.h"
#include "connect_settings_data.h"

GtkResponseType remote_viewer_connect_dialog(ConnectSettingsData *connect_settings_data);

#endif /* REMOTE_VIEWER_CONNECT_H */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
