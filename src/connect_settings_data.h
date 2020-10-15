//
// Created by ubuntu on 15.10.2020.
//

#ifndef VEIL_CONNECT_CONNECT_SETTINGS_DATA_H
#define VEIL_CONNECT_CONNECT_SETTINGS_DATA_H

#include "vdi_session.h"
#include "remote-viewer-util.h"

typedef struct{

    gchar *user;
    gchar *password;

    // General
    gchar *domain;
    gchar *ip;
    int port;

    gboolean is_ldap;
    gboolean is_connect_to_prev_pool;

    VdiVmRemoteProtocol remote_protocol_type;

    gchar *vm_verbose_name;

} ConnectSettingsData;

static void connect_settings_data_free(ConnectSettingsData *connect_settings_data)
{
    g_info("%s", (const char *)__func__);
    free_memory_safely(&connect_settings_data->user);
    free_memory_safely(&connect_settings_data->password);
    free_memory_safely(&connect_settings_data->domain);
    free_memory_safely(&connect_settings_data->ip);
    free_memory_safely(&connect_settings_data->vm_verbose_name);
}

#endif //VEIL_CONNECT_CONNECT_SETTINGS_DATA_H
