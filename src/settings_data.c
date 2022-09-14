/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include<stdio.h>
#include<stdlib.h>

#include "remote-viewer-util.h"
#include "settingsfile.h"
#include "settings_data.h"
#include "controller_client/controller_session.h"

/*
 * Чтение всех настроек приложения
 */
void settings_data_read_all(ConnectSettingsData *data)
{
    data->was_read_from_file = TRUE;

    settings_data_clear(data);

    data->redirect_time_zone =
            read_int_from_ini_file("General", "redirect_time_zone", 0);

    // Direct connection to VM data
    const gchar *manual_conn_group = get_cur_ini_group_direct();
    data->user = read_str_from_ini_file(manual_conn_group, "username");
    data->password = read_str_from_ini_file(manual_conn_group, "password");

    data->ip = read_str_from_ini_file(manual_conn_group, "ip");
    data->port = read_int_from_ini_file(manual_conn_group, "port", 443);

    // connection to VDI
    {
        const gchar *param_group_vdi = get_cur_ini_group_vdi();
        g_autofree gchar *username = NULL;
        g_autofree gchar *password = NULL;
        g_autofree gchar *ip = NULL;
        username = read_str_from_ini_file(param_group_vdi, "username");
        password = read_str_from_ini_file(param_group_vdi, "password");
        ip = read_str_from_ini_file(param_group_vdi, "ip");

        GList *broker_addresses_list = read_string_list_from_ini_file(param_group_vdi, "broker_addresses_list");
        vdi_session_set_broker_addresses(broker_addresses_list);
        MultiAddressMode multi_address_mode = (MultiAddressMode)read_int_from_ini_file(
                param_group_vdi, "multi_address_mode", 0);
        vdi_session_set_multi_address_mode(multi_address_mode);

        int port = read_int_from_ini_file(param_group_vdi, "port", 443);
        gboolean is_ldap = read_int_from_ini_file(param_group_vdi, "is_ldap", 0);

        vdi_session_set_credentials(username, password, NULL);
        vdi_session_set_vdi_port(port);
        vdi_session_set_ldap(is_ldap);

        data->domain = read_str_from_ini_file(param_group_vdi, "domain");
        data->connect_to_prev_pool =
                read_int_from_ini_file(param_group_vdi, "connect_to_pool", 0);
        data->to_save_pswd = read_int_from_ini_file(param_group_vdi, "to_save_pswd", 1);
        data->pass_through_auth = read_int_from_ini_file(param_group_vdi, "pass_through_auth", 0);
    }

    // controller
    {
        const gchar *param_group_controller = get_cur_ini_group_controller();
        g_autofree gchar *username = NULL;
        g_autofree gchar *password = NULL;
        g_autofree gchar *ip = NULL;
        username = read_str_from_ini_file(param_group_controller, "username");
        password = read_str_from_ini_file(param_group_controller, "password");
        ip = read_str_from_ini_file(param_group_controller, "ip");
        int port = read_int_from_ini_file(param_group_controller, "port", 443);
        gboolean is_ldap = read_int_from_ini_file(param_group_controller, "is_ldap", 0);

        controller_session_set_credentials(username, password);
        controller_session_set_conn_data(ip, port);
        controller_session_set_ldap(is_ldap);

        data->domain = read_str_from_ini_file(param_group_controller, "domain");
        data->to_save_pswd = read_int_from_ini_file(param_group_controller, "to_save_pswd", 1);
    }

    // SPICE
    spice_settings_read(&data->spice_settings);
    // RDP
    rdp_settings_read_ini_file(&data->rdp_settings);
    // X2Go
    x2go_settings_read(&data->x2Go_settings);
    // Loudplay
    loudplay_settings_read(&data->loudplay_config);

    // Service
    data->global_app_mode = (GlobalAppMode)read_int_from_ini_file(
            "ServiceSettings", "global_app_mode", GLOBAL_APP_MODE_VDI);

    data->windows_updates_url = read_str_from_ini_file_default("ServiceSettings",
            "windows_updates_url", VEIL_CONNECT_WIN_RELEASE_URL);
    data->vm_await_timeout =
            CLAMP(read_int_from_ini_file("ServiceSettings", "vm_await_timeout", 65), 5, 10000);
    data->unique_app = read_int_from_ini_file("ServiceSettings", "unique_app", TRUE);

    // cur_remote_protocol_index
    VmRemoteProtocol protocol = (VmRemoteProtocol)read_int_from_ini_file("General",
            "cur_remote_protocol_index", SPICE_PROTOCOL);
    vdi_session_set_current_remote_protocol(protocol);
    data->protocol_in_direct_app_mode = protocol;
    controller_session_set_current_remote_protocol(protocol);
}

/*
 * Сохранение всех настроек приложения
 */
void settings_data_save_all(ConnectSettingsData *data)
{
    if (!data->was_read_from_file)
        return;

    // Main
    write_int_to_ini_file("General", "redirect_time_zone", data->redirect_time_zone);
    // Direct
    const gchar *manual_conn_group = get_cur_ini_group_direct();
    write_str_to_ini_file(manual_conn_group, "username", data->user);
    if (data->to_save_pswd)
        write_str_to_ini_file(manual_conn_group, "password", data->password);
    else
        write_str_to_ini_file(manual_conn_group, "password", "");
    write_str_to_ini_file(manual_conn_group, "ip", data->ip);
    write_int_to_ini_file(manual_conn_group, "port", data->port);

    // VDI
    const gchar *param_group_vdi = get_cur_ini_group_vdi();
    write_str_to_ini_file(param_group_vdi, "username", vdi_session_get_vdi_username());
    if (data->to_save_pswd)
        write_str_to_ini_file(param_group_vdi, "password", vdi_session_get_vdi_password());
    else
        write_str_to_ini_file(param_group_vdi, "password", "");
    // save addresses
    GList *addresses = get_vdi_session_static()->broker_addresses_list;
    write_string_list_to_ini_file_v2(param_group_vdi, "broker_addresses_list", addresses);
    write_int_to_ini_file(param_group_vdi, "multi_address_mode", get_vdi_session_static()->multi_address_mode);

    write_int_to_ini_file(param_group_vdi, "port", vdi_session_get_vdi_port());
    write_int_to_ini_file(param_group_vdi, "is_ldap", vdi_session_is_ldap());

    write_str_to_ini_file(param_group_vdi, "domain", data->domain);
    write_int_to_ini_file(param_group_vdi, "connect_to_pool", data->connect_to_prev_pool);
    write_int_to_ini_file(param_group_vdi, "to_save_pswd", data->to_save_pswd);
    write_int_to_ini_file(param_group_vdi, "pass_through_auth", data->pass_through_auth);

    // cur_remote_protocol_index
    switch (data->global_app_mode) {
        case GLOBAL_APP_MODE_VDI: {
            write_int_to_ini_file("General", "cur_remote_protocol_index",
                    vdi_session_get_current_remote_protocol());
            break;
        }
        case GLOBAL_APP_MODE_DIRECT: {
            write_int_to_ini_file("General", "cur_remote_protocol_index",
                                  data->protocol_in_direct_app_mode);
            break;
        }
        case GLOBAL_APP_MODE_CONTROLLER: {
            write_int_to_ini_file("General", "cur_remote_protocol_index",
                                  controller_session_get_current_remote_protocol());
            break;
        }
    }

    // Controller
    const gchar *param_group_controller = get_cur_ini_group_controller();
    write_str_to_ini_file(param_group_controller, "username", get_controller_session_static()->username.string);
    if (data->to_save_pswd)
        write_str_to_ini_file(param_group_controller, "password", get_controller_session_static()->password.string);
    else
        write_str_to_ini_file(param_group_controller, "password", "");
    write_str_to_ini_file(param_group_controller, "ip", get_controller_session_static()->address.string);
    write_int_to_ini_file(param_group_controller, "port", get_controller_session_static()->port);
    write_int_to_ini_file(param_group_controller, "is_ldap", get_controller_session_static()->is_ldap);

    write_str_to_ini_file(param_group_controller, "domain", data->domain);
    write_int_to_ini_file(param_group_controller, "to_save_pswd", data->to_save_pswd);

    // SPICE
    spice_settings_write(&data->spice_settings);
    // RDP
    rdp_settings_write(&data->rdp_settings);
    // X2Go
    x2go_settings_write(&data->x2Go_settings);
    // Loudplay
    loudplay_settings_write(data->loudplay_config);

    // Service
    write_int_to_ini_file("ServiceSettings", "global_app_mode", data->global_app_mode);
    write_str_to_ini_file("ServiceSettings", "windows_updates_url", data->windows_updates_url);
    write_int_to_ini_file("ServiceSettings", "vm_await_timeout", data->vm_await_timeout);
    write_int_to_ini_file("ServiceSettings", "unique_app", data->unique_app);

    g_key_file_save_to_file(get_ini_keyfile(), get_ini_file_name(), NULL);
}

void settings_data_clear(ConnectSettingsData *data)
{
    free_memory_safely(&data->user);
    free_memory_safely(&data->password);
    free_memory_safely(&data->domain);
    free_memory_safely(&data->ip);
    free_memory_safely(&data->vm_verbose_name);

    spice_settings_clear(&data->spice_settings);
    rdp_settings_clear(&data->rdp_settings);
    x2go_settings_clear(&data->x2Go_settings);
    loudplay_settings_clear(&data->loudplay_config);

    free_memory_safely(&data->windows_updates_url);
}

void spice_settings_read(VeilSpiceSettings *spice_settings)
{
    const gchar *spice_group = "SpiceSettings";

    spice_settings->is_spice_client_cursor_visible =
            read_int_from_ini_file(spice_group, "is_spice_client_cursor_visible", FALSE);
    set_client_spice_cursor_visible(spice_settings->is_spice_client_cursor_visible);

    spice_settings->full_screen = read_int_from_ini_file(spice_group, "full_screen", FALSE);

    g_autofree gchar *monitor_mapping = NULL;
    monitor_mapping = read_str_from_ini_file(spice_group, "monitor-mapping");
    update_string_safely(&spice_settings->monitor_mapping, monitor_mapping);

    g_autofree gchar *usb_auto_connect_filter = NULL;
    usb_auto_connect_filter = read_str_from_ini_file(spice_group, "usb_auto_connect_filter");
    update_string_safely(&spice_settings->usb_auto_connect_filter, usb_auto_connect_filter);

    g_autofree gchar *usb_redirect_on_connect = NULL;
    usb_redirect_on_connect = read_str_from_ini_file(spice_group, "usb_redirect_on_connect");
    update_string_safely(&spice_settings->usb_redirect_on_connect, usb_redirect_on_connect);
}

void spice_settings_write(VeilSpiceSettings *spice_settings)
{
    const gchar *spice_group = "SpiceSettings";

    write_int_to_ini_file(spice_group, "is_spice_client_cursor_visible",
            spice_settings->is_spice_client_cursor_visible);
    write_int_to_ini_file(spice_group, "full_screen", spice_settings->full_screen);
    write_str_to_ini_file(spice_group, "monitor-mapping", spice_settings->monitor_mapping);
    write_str_to_ini_file(spice_group, "usb_auto_connect_filter", spice_settings->usb_auto_connect_filter);
    write_str_to_ini_file(spice_group, "usb_redirect_on_connect", spice_settings->usb_redirect_on_connect);
}

void spice_settings_clear(VeilSpiceSettings *spice_settings)
{
    free_memory_safely(&spice_settings->monitor_mapping);
    free_memory_safely(&spice_settings->usb_auto_connect_filter);
    free_memory_safely(&spice_settings->usb_redirect_on_connect);
}

void rdp_settings_set_connect_data(VeilRdpSettings *rdp_settings, const gchar *user_name, const gchar *password,
                                   const gchar *domain, const gchar *ip, int port)
{
    update_string_safely(&rdp_settings->user_name, user_name);
    update_string_safely(&rdp_settings->password, password);
    update_string_safely(&rdp_settings->domain, domain);
    update_string_safely(&rdp_settings->ip, ip);

    rdp_settings->port = port;
}

/*
 * Чтение из ini
 */
void rdp_settings_read_ini_file(VeilRdpSettings *rdp_settings)
{
    const gchar *rdp_group = "RDPSettings";
    g_autofree gchar *rdp_pixel_format = NULL;
    rdp_pixel_format = read_str_from_ini_file_with_def(rdp_group, "rdp_pixel_format", "BGRA32");
    update_string_safely(&rdp_settings->rdp_pixel_format_str, rdp_pixel_format);

    rdp_settings->rdp_fps = CLAMP(read_int_from_ini_file(rdp_group, "rdp_fps", 30), 1, 60);

    rdp_settings->is_rdp_vid_comp_used = read_int_from_ini_file(rdp_group, "is_rdp_vid_comp_used", TRUE);
    g_autofree gchar *rdp_vid_comp_codec = NULL;
    rdp_vid_comp_codec = read_str_from_ini_file_with_def(rdp_group, "rdp_vid_comp_codec", "RFX");
    update_string_safely(&rdp_settings->rdp_vid_comp_codec, rdp_vid_comp_codec);

    g_autofree gchar *rdp_shared_folders = NULL;
    rdp_shared_folders = read_str_from_ini_file(rdp_group, "rdp_shared_folders");
    update_string_safely(&rdp_settings->shared_folders_str, rdp_shared_folders);

    //rdp_settings->is_remote_app = read_int_from_ini_file(rdp_group, "is_remote_app", 0);
    //g_autofree gchar *remote_app_program = NULL;
    //remote_app_program = read_str_from_ini_file(rdp_group, "remote_app_program");
    //update_string_safely(&rdp_settings->remote_app_program, remote_app_program);
    //g_autofree gchar *remote_app_options = NULL;
    //remote_app_options = read_str_from_ini_file(rdp_group, "remote_app_options");
    //update_string_safely(&rdp_settings->remote_app_options, remote_app_options);
    rdp_settings->remote_application_format = (RemoteApplicationFormat)read_int_from_ini_file(
            rdp_group, "remote_application_format", REMOTE_APPLICATION_FORMAT_ALIAS);

    rdp_settings->is_multimon = read_int_from_ini_file(rdp_group, "is_multimon", 0);
    rdp_settings->full_screen = read_int_from_ini_file(rdp_group, "full_screen", 1);
    g_autofree gchar *selectedmonitors = NULL;
    selectedmonitors = read_str_from_ini_file(rdp_group, "selectedmonitors");
    update_string_safely(&rdp_settings->selectedmonitors, selectedmonitors);
    rdp_settings->dynamic_resolution_enabled = read_int_from_ini_file(rdp_group, "dynamic_resolution_enabled", 0);

    rdp_settings->redirectsmartcards = read_int_from_ini_file(rdp_group, "redirectsmartcards", 1);
    rdp_settings->redirectprinters = read_int_from_ini_file(rdp_group, "redirect_printers", 1);
    rdp_settings->redirect_microphone = read_int_from_ini_file(rdp_group, "redirect_microphone", 1);

    g_autofree gchar *alternate_shell = NULL;
    alternate_shell = read_str_from_ini_file(rdp_group, "alternate_shell");
    update_string_safely(&rdp_settings->alternate_shell, alternate_shell);

    rdp_settings->disable_rdp_fonts = read_int_from_ini_file(rdp_group, "disable_rdp_fonts", 0);
    rdp_settings->disable_rdp_themes = read_int_from_ini_file(rdp_group, "disable_rdp_themes", 0);
    rdp_settings->disable_rdp_decorations = read_int_from_ini_file(rdp_group, "disable_rdp_decorations", 0);

    rdp_settings->allow_desktop_composition = read_int_from_ini_file(rdp_group, "allow_desktop_composition", 0);

    rdp_settings->is_rdp_network_assigned = read_int_from_ini_file(rdp_group, "is_rdp_network_assigned", 0);
    if (rdp_settings->is_rdp_network_assigned) {
        g_autofree gchar *rdp_network_type = NULL;
        rdp_network_type = read_str_from_ini_file(rdp_group, "rdp_network_type");
        update_string_safely(&rdp_settings->rdp_network_type, rdp_network_type);
    }

    rdp_settings->is_sec_protocol_assigned = read_int_from_ini_file(rdp_group, "is_sec_protocol_assigned", 0);
    if (rdp_settings->is_sec_protocol_assigned) {
        g_autofree gchar *sec_protocol_type = NULL;
        sec_protocol_type = read_str_from_ini_file(rdp_group, "sec_protocol_type");
        update_string_safely(&rdp_settings->sec_protocol_type, sec_protocol_type);
    }

    g_autofree gchar *usb_devices = NULL;
    usb_devices = read_str_from_ini_file(rdp_group, "usb_devices");
    update_string_safely(&rdp_settings->usb_devices, usb_devices);

    rdp_settings->use_rdp_file = read_int_from_ini_file(rdp_group, "use_rdp_file", 0);
    g_autofree gchar *rdp_settings_file = NULL;
    rdp_settings_file = read_str_from_ini_file(rdp_group, "rdp_settings_file");
    update_string_safely(&rdp_settings->rdp_settings_file, rdp_settings_file);

    rdp_settings->use_gateway = read_int_from_ini_file(rdp_group, "use_gateway", 0);
    g_autofree gchar *gateway_address = NULL;
    gateway_address = read_str_from_ini_file(rdp_group, "gateway_address");
    update_string_safely(&rdp_settings->gateway_address, gateway_address);

    rdp_settings->freerdp_debug_log_enabled = read_int_from_ini_file(rdp_group, "freerdp_debug_log_enabled", FALSE);

    rdp_settings->h264_bitrate = read_int_from_ini_file(rdp_group, "h264_bitrate", 10000000);
    rdp_settings->h264_framerate = read_int_from_ini_file(rdp_group, "h264_framerate", 30);

    rdp_settings->disable_username_mod = read_int_from_ini_file(rdp_group, "disable_username_mod", 0);

}

/*
 * Чтение настроек из стандартного RDP файла
 */
void rdp_settings_read_standard_file(VeilRdpSettings *rdp_settings, const gchar *file_name)
{
    // Если не указано, берем из ini
    g_autofree gchar *final_file_name = NULL;
    if (file_name)
        final_file_name = g_strdup(file_name);
    else
        final_file_name = read_str_from_ini_file("RDPSettings", "rdp_settings_file");
    convert_string_from_utf8_to_locale(&final_file_name);

    FILE *rdp_file = fopen(final_file_name, "r");
    if (rdp_file == NULL) {
        g_warning("%s: Cant open file %s", (const char *)__func__, final_file_name);
        return;
    }

    // reset parameters
    rdp_settings_clear(rdp_settings);

    // parse
    const guint buff_size = 1024
    ;
    char line[buff_size];

    const gchar *delimiter = ":";
    const guint tokens_num = 3;
    //fgets
    while (fgets(line, sizeof(line), rdp_file)) {
        printf("Retrieved line  %s\n", line);

        gchar **params_array = g_strsplit(line, delimiter, tokens_num);
        if (g_strv_length(params_array) == tokens_num) {
            gchar *name = params_array[0];
            gchar *value = params_array[2];
            remove_char(value, '\n');

            if (g_strcmp0(name, "domain") == 0) {
                update_string_safely(&rdp_settings->domain, value);
            } else if (g_strcmp0(name, "username") == 0) {
                update_string_safely(&rdp_settings->user_name, value);
            } else if (g_strcmp0(name, "full address") == 0) {
                update_string_safely(&rdp_settings->ip, value);

            } else if (g_strcmp0(name, "remoteapplicationmode") == 0) {
                rdp_settings->is_remote_app = atoi(value);
            } else if (g_strcmp0(name, "remoteapplicationprogram") == 0) {
                update_string_safely(&rdp_settings->remote_app_program, value);
            } else if (g_strcmp0(name, "remoteapplicationcmdline") == 0) {
                update_string_safely(&rdp_settings->remote_app_options, value);

            } else if (g_strcmp0(name, "use multimon") == 0) {
                rdp_settings->is_multimon = atoi(value);
            } else if (g_strcmp0(name, "selectedmonitors") == 0) {
                update_string_safely(&rdp_settings->selectedmonitors, value);

            }else if (g_strcmp0(name, "redirectsmartcards") == 0) {
                rdp_settings->redirectsmartcards = atoi(value);
            } else if (g_strcmp0(name, "redirectprinters") == 0) {
                rdp_settings->redirectprinters = atoi(value);

            } else if (g_strcmp0(name, "alternate shell") == 0) {
                update_string_safely(&rdp_settings->alternate_shell, value);
            } else if (g_strcmp0(name, "allow font smoothing") == 0) {
                rdp_settings->disable_rdp_fonts = !((gboolean)atoi(value));
            } else if (g_strcmp0(name, "disable themes") == 0) {
                rdp_settings->disable_rdp_themes = atoi(value);
            } else if (g_strcmp0(name, "allow desktop composition") == 0) {
                rdp_settings->allow_desktop_composition = atoi(value);
            }
        }

        g_strfreev(params_array);
    }

    fclose(rdp_file);
}

void rdp_settings_write(VeilRdpSettings *rdp_settings)
{
    const gchar *rdp_group = "RDPSettings";

    write_str_to_ini_file(rdp_group, "rdp_pixel_format", rdp_settings->rdp_pixel_format_str);
    write_int_to_ini_file(rdp_group, "rdp_fps", rdp_settings->rdp_fps);
    write_int_to_ini_file(rdp_group, "is_rdp_vid_comp_used", rdp_settings->is_rdp_vid_comp_used);
    write_str_to_ini_file(rdp_group, "rdp_vid_comp_codec", rdp_settings->rdp_vid_comp_codec);

    // Пользовать мог ввести путь с виндусятскими разделителями, поэтому на всякий случай заменяем
    gchar *folder_name = replace_str(rdp_settings->shared_folders_str, "\\", "/");
    write_str_to_ini_file(rdp_group, "rdp_shared_folders", folder_name);
    free_memory_safely(&folder_name);

    write_int_to_ini_file(rdp_group, "is_multimon", rdp_settings->is_multimon);
    write_int_to_ini_file(rdp_group, "full_screen", rdp_settings->full_screen);
    write_str_to_ini_file(rdp_group, "selectedmonitors", rdp_settings->selectedmonitors);
    write_int_to_ini_file(rdp_group, "dynamic_resolution_enabled", rdp_settings->dynamic_resolution_enabled);

    write_int_to_ini_file(rdp_group, "redirectsmartcards", rdp_settings->redirectsmartcards);
    write_int_to_ini_file(rdp_group, "redirect_printers", rdp_settings->redirectprinters);
    write_int_to_ini_file(rdp_group, "redirect_microphone", rdp_settings->redirect_microphone);

    //write_int_to_ini_file(rdp_group, "is_remote_app", rdp_settings->is_remote_app);
    //write_str_to_ini_file(rdp_group, "remote_app_program", rdp_settings->remote_app_program);
    //write_str_to_ini_file(rdp_group, "remote_app_options", rdp_settings->remote_app_options);
    write_int_to_ini_file(rdp_group, "remote_application_format", rdp_settings->remote_application_format);
    //
    write_int_to_ini_file(rdp_group, "is_sec_protocol_assigned", rdp_settings->is_sec_protocol_assigned);
    write_str_to_ini_file(rdp_group, "sec_protocol_type", rdp_settings->sec_protocol_type);
    //
    write_int_to_ini_file(rdp_group, "is_rdp_network_assigned", rdp_settings->is_rdp_network_assigned);
    write_str_to_ini_file(rdp_group, "rdp_network_type", rdp_settings->rdp_network_type);

    write_int_to_ini_file(rdp_group, "disable_rdp_decorations", rdp_settings->disable_rdp_decorations);
    write_int_to_ini_file(rdp_group, "disable_rdp_fonts", rdp_settings->disable_rdp_fonts);
    write_int_to_ini_file(rdp_group, "disable_rdp_themes", rdp_settings->disable_rdp_themes);

    write_int_to_ini_file(rdp_group, "allow_desktop_composition", rdp_settings->allow_desktop_composition);

    if (rdp_settings->usb_devices)
        write_str_to_ini_file(rdp_group, "usb_devices", rdp_settings->usb_devices);
    else
        write_str_to_ini_file(rdp_group, "usb_devices", "");

    write_int_to_ini_file(rdp_group, "use_rdp_file", rdp_settings->use_rdp_file);
    gchar *rdp_settings_file_mod = replace_str(rdp_settings->rdp_settings_file, "\\", "/");
    write_str_to_ini_file(rdp_group, "rdp_settings_file", rdp_settings_file_mod);
    g_free(rdp_settings_file_mod);

    write_int_to_ini_file(rdp_group, "use_gateway", rdp_settings->use_gateway);
    write_str_to_ini_file(rdp_group, "gateway_address", rdp_settings->gateway_address);

    write_int_to_ini_file(rdp_group, "freerdp_debug_log_enabled", rdp_settings->freerdp_debug_log_enabled);

    write_int_to_ini_file(rdp_group, "h264_bitrate", rdp_settings->h264_bitrate);
    write_int_to_ini_file(rdp_group, "h264_framerate", rdp_settings->h264_framerate);

    write_int_to_ini_file(rdp_group, "disable_username_mod", rdp_settings->disable_username_mod);
}

void rdp_settings_clear(VeilRdpSettings *rdp_settings)
{
    rdp_settings->port = 0;
    rdp_settings->is_remote_app = 0;

    free_memory_safely(&rdp_settings->rdp_pixel_format_str);
    free_memory_safely(&rdp_settings->rdp_vid_comp_codec);
    free_memory_safely(&rdp_settings->shared_folders_str);

    free_memory_safely(&rdp_settings->user_name);
    free_memory_safely(&rdp_settings->password);
    free_memory_safely(&rdp_settings->domain);
    free_memory_safely(&rdp_settings->ip);

    free_memory_safely(&rdp_settings->remote_app_program);
    free_memory_safely(&rdp_settings->remote_app_options);

    free_memory_safely(&rdp_settings->alternate_shell);

    free_memory_safely(&rdp_settings->rdp_network_type);
    free_memory_safely(&rdp_settings->sec_protocol_type);
    free_memory_safely(&rdp_settings->rdp_settings_file);
    free_memory_safely(&rdp_settings->usb_devices);

    free_memory_safely(&rdp_settings->gateway_address);
}

void x2go_settings_read(VeilX2GoSettings *x2go_settings)
{
    const gchar *x2go_group = "X2GoSettings";

    x2go_settings->app_type = (X2goApplication)read_int_from_ini_file(x2go_group, "app_type", X2GO_APP_QT_CLIENT);

    g_autofree gchar *session_type = NULL;
    session_type = read_str_from_ini_file_with_def(x2go_group, "session_type", "XFCE");
    update_string_safely(&x2go_settings->x2go_session_type, session_type);
    g_autofree gchar *conn_type = NULL;
    conn_type = read_str_from_ini_file_with_def(x2go_group, "conn_type", "modem");
    update_string_safely(&x2go_settings->x2go_conn_type, conn_type);
    x2go_settings->conn_type_assigned = read_int_from_ini_file(x2go_group, "conn_type_assigned", 0);
    x2go_settings->full_screen = read_int_from_ini_file(x2go_group, "full_screen", 1);

    g_autofree gchar *compress_method = NULL;
    compress_method = read_str_from_ini_file_with_def(x2go_group, "compress_method", "16m-jpeg");
    update_string_safely(&x2go_settings->compress_method, compress_method);
}

void x2go_settings_write(VeilX2GoSettings *x2go_settings)
{
    const gchar *x2go_group = "X2GoSettings";

    write_int_to_ini_file(x2go_group, "app_type", x2go_settings->app_type);

    write_str_to_ini_file(x2go_group, "session_type", x2go_settings->x2go_session_type);
    write_int_to_ini_file(x2go_group, "conn_type_assigned", x2go_settings->conn_type_assigned);
    write_str_to_ini_file(x2go_group, "conn_type", x2go_settings->x2go_conn_type);
    write_int_to_ini_file(x2go_group, "full_screen", x2go_settings->full_screen);
    write_str_to_ini_file(x2go_group, "compress_method", x2go_settings->compress_method);
}

void x2go_settings_clear(VeilX2GoSettings *x2go_settings)
{
    free_memory_safely(&x2go_settings->x2go_session_type);
    free_memory_safely(&x2go_settings->x2go_conn_type);
}

void loudplay_settings_read(LoudPlayConfig **p_loudplay_config)
{
    if (*p_loudplay_config)
        loudplay_settings_clear(p_loudplay_config);

    // Open file
    g_autofree gchar *file_name = NULL;
    file_name = loudplay_config_get_file_name();
    *p_loudplay_config = LOUDPLAY_CONFIG(json_handler_gobject_from_file(file_name, TYPE_LOUDPLAY_CONFIG));

    if (*p_loudplay_config == NULL) // Неудача в случае ошибки парсинга файла. Создаем дефолтный
        *p_loudplay_config = LOUDPLAY_CONFIG( g_object_new( TYPE_LOUDPLAY_CONFIG, NULL ) );

    // client path
    (*p_loudplay_config)->loudplay_client_path = read_str_from_ini_file_with_def(
            "LoudplaySettings", "loudplay_client_path",
            "C:\\job\\loudplay\\1.0.3\\1.0.3");
    (*p_loudplay_config)->is_client_path_relative =
            read_int_from_ini_file("LoudplaySettings", "is_client_path_relative", FALSE);
}

void loudplay_settings_write(LoudPlayConfig *loudplay_config)
{
    if (!loudplay_config)
        return;

    g_autofree gchar *file_name = NULL;
    file_name = loudplay_config_get_file_name();
    json_handler_gobject_to_file(file_name, (GObject *)loudplay_config);

    write_str_to_ini_file("LoudplaySettings", "loudplay_client_path",
                          loudplay_config->loudplay_client_path);
    write_int_to_ini_file("LoudplaySettings", "is_client_path_relative",
            loudplay_config->is_client_path_relative);
}

void loudplay_settings_clear(LoudPlayConfig **p_loudplay_config)
{
    if (!(*p_loudplay_config))
        return;

    g_object_unref(*p_loudplay_config);
    *p_loudplay_config = NULL;
}