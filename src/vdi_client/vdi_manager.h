//
// Created by Solomin on 13.06.19.
//

#ifndef VIRT_VIEWER_VEIL_VDI_MANAGER_H
#define VIRT_VIEWER_VEIL_VDI_MANAGER_H

#include <gtk/gtk.h>

#include "vdi_session.h"
#include "connect_settings_data.h"

GtkResponseType vdi_manager_dialog(GtkWindow *main_window, ConnectSettingsData *con_data);

#endif //VIRT_VIEWER_VEIL_VDI_MANAGER_H
