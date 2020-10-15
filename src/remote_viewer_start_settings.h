#ifndef REMOTE_VIEWER_START_SETTINGS_H
#define REMOTE_VIEWER_START_SETTINGS_H

#include <gtk/gtk.h>

#include <winpr/wtypes.h>

#include "connect_settings_data.h"


GtkResponseType remote_viewer_start_settings_dialog(ConnectSettingsData *p_conn_data, GtkWindow *parent);
void fill_p_conn_data_from_ini_file(ConnectSettingsData *p_conn_data);


#endif // REMOTE_VIEWER_START_SETTINGS_H



