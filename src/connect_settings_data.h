/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_CONNECT_SETTINGS_DATA_H
#define VEIL_CONNECT_CONNECT_SETTINGS_DATA_H

#include "vdi_session.h"
#include "remote-viewer-util.h"
#include "rdp_settings.h"

typedef struct{

    gchar *user;
    gchar *password;
    gchar *disposable_password; // 2fa

    // General
    gchar *domain;
    gchar *ip;
    int port;

    gboolean is_ldap;
    gboolean is_connect_to_prev_pool;
    gboolean to_save_pswd;

    VdiVmRemoteProtocol remote_protocol_type;

    gchar *vm_verbose_name;

    // RDP specific
    VeilRdpSettings rdp_settings;

} ConnectSettingsData;

#endif //VEIL_CONNECT_CONNECT_SETTINGS_DATA_H
