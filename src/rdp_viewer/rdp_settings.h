//
// Created by solomin on 07.05.2021.
//

#ifndef VEIL_CONNECT_RDP_SETTINGS_H
#define VEIL_CONNECT_RDP_SETTINGS_H

#include <gtk/gtk.h>

typedef struct{

    gboolean is_remote_app;
    gchar *remote_app_name;
    gchar *remote_app_options;

} VeilRdpSettings;

void rdp_settings_clear(VeilRdpSettings *rdp_settings);

#endif //VEIL_CONNECT_RDP_SETTINGS_H
