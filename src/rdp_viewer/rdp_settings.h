//
// Created by solomin on 07.05.2021.
//

#ifndef VEIL_CONNECT_RDP_SETTINGS_H
#define VEIL_CONNECT_RDP_SETTINGS_H

#include <gtk/gtk.h>

typedef struct{

    // connect data
    gchar *user_name;
    gchar *password;
    gchar *domain;
    gchar *ip;
    int port;

    // remote app
    gboolean is_remote_app;
    gchar *remote_app_program;
    gchar *remote_app_options;

    gboolean is_multimon;
    gboolean redirectsmartcards;
    gboolean redirectprinters;

    gchar *alternate_shell;

    gboolean disable_rdp_fonts;
    gboolean disable_rdp_themes;
    gboolean disable_rdp_decorations;

    gboolean allow_desktop_composition;

    gboolean is_rdp_network_assigned;
    gchar *rdp_network_type;

    gboolean is_sec_protocol_assigned;
    gchar *sec_protocol_type;

} VeilRdpSettings;

void rdp_settings_set_connect_data(VeilRdpSettings *rdp_settings, const gchar *user_name, const gchar *password,
                                   const gchar *domain, const gchar *ip, int port);

//void rdp_settings_read_from_file(VeilRdpSettings *rdp_settings);
void rdp_settings_read_ini_file(VeilRdpSettings *rdp_settings, gboolean rewrite_remote_app_settings);
void rdp_settings_read_standard_file(VeilRdpSettings *rdp_settings, const gchar *file_name);

void rdp_settings_clear(VeilRdpSettings *rdp_settings);

#endif //VEIL_CONNECT_RDP_SETTINGS_H
