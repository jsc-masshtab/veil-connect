/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_SETTINGS_DATA_H
#define VEIL_CONNECT_SETTINGS_DATA_H

#include <winpr/wtypes.h>

#include "vdi_session.h"
#include "remote-viewer-util.h"

typedef enum{
    GLOBAL_APP_MODE_VDI, // Режим подключения к VDI
    GLOBAL_APP_MODE_DIRECT, // Режим подключения к ВМ напрямую
    GLOBAL_APP_MODE_CONTROLLER // Режим подключения к контроллеру (ECP)
} GlobalAppMode; // Режим работы приложения

typedef enum{
    X2GO_APP_QT_CLIENT,
    X2GO_APP_PYHOCA_CLI
} X2goApplication;


typedef struct{

    gboolean is_spice_client_cursor_visible;
    gboolean full_screen;
    gchar *monitor_mapping;

} VeilSpiceSettings;


typedef struct{

    // connect data
    gchar *user_name;
    gchar *password;

    gchar *domain;
    gchar *ip;
    int port;

    gchar *rdp_pixel_format_str;
    UINT32 rdp_fps;

    gboolean is_rdp_vid_comp_used;
    gchar *rdp_vid_comp_codec;

    gchar *shared_folders_str;

    // remote app
    gboolean is_remote_app;
    gchar *remote_app_program;
    gchar *remote_app_options;

    gboolean is_multimon;
    gboolean full_screen;
    gchar *selectedmonitors; // список через запятую номеров используемых мониторов.
    // В случает is_multimon == false берется первый указаннй номер (если он валиден)

    gboolean redirectsmartcards;
    gboolean redirectprinters;
    gboolean redirect_microphone;

    gchar *alternate_shell;

    gboolean disable_rdp_fonts;
    gboolean disable_rdp_themes;
    gboolean disable_rdp_decorations;

    gboolean allow_desktop_composition;

    gboolean is_rdp_network_assigned;
    gchar *rdp_network_type;

    gboolean is_sec_protocol_assigned;
    gchar *sec_protocol_type;

    gchar *usb_devices;

    gboolean use_rdp_file;
    gchar *rdp_settings_file;

    // RD Gateway
    gboolean use_gateway;
    gchar *gateway_address;

    gboolean freerdp_debug_log_enabled;

} VeilRdpSettings;


typedef struct{
    X2goApplication app_type;

    gchar *x2go_session_type;

    gboolean conn_type_assigned;
    gchar *x2go_conn_type;

    gboolean full_screen;

    gchar *compress_method;

} VeilX2GoSettings;


typedef struct{
    // General settings
    gchar *user;
    gchar *password;

    gchar *domain;
    gchar *ip;
    int port;

    gboolean is_connect_to_prev_pool;
    gboolean to_save_pswd;
    gboolean redirect_time_zone;

    gchar *vm_verbose_name;

    // RDP specific
    VeilRdpSettings rdp_settings;
    // Spice specific
    VeilSpiceSettings spice_settings;
    // X2Go
    VeilX2GoSettings x2Go_settings;

    // Sevice settings
    GlobalAppMode global_app_mode;
    gchar *windows_updates_url;
    int vm_await_timeout;

    // Параметры актуальные тольео во время работы приложения (не гужно сохранять в файл)
    gboolean not_connected_to_prev_pool_yet; // Подключалось ли приложение автоматом к предыдущему пулу при старте

    VmRemoteProtocol protocol_in_direct_app_mode;

} ConnectSettingsData;


void settings_data_read_all(ConnectSettingsData *data);
void settings_data_save_all(ConnectSettingsData *data);
void settings_data_clear(ConnectSettingsData *data);

// SPICE
void spice_settings_read(VeilSpiceSettings *spice_settings);
void spice_settings_write(VeilSpiceSettings *spice_settings);
void spice_settings_clear(VeilSpiceSettings *spice_settings);

// RDP
void rdp_settings_set_connect_data(VeilRdpSettings *rdp_settings, const gchar *user_name, const gchar *password,
                                   const gchar *domain, const gchar *ip, int port);

void rdp_settings_read_ini_file(VeilRdpSettings *rdp_settings, gboolean rewrite_remote_app_settings);
void rdp_settings_read_standard_file(VeilRdpSettings *rdp_settings, const gchar *file_name);
void rdp_settings_write(VeilRdpSettings *rdp_settings);

void rdp_settings_clear(VeilRdpSettings *rdp_settings);

// X2Go
void x2go_settings_read(VeilX2GoSettings *x2go_settings);
void x2go_settings_write(VeilX2GoSettings *x2go_settings);
void x2go_settings_clear(VeilX2GoSettings *x2go_settings);

#endif //VEIL_CONNECT_SETTINGS_DATA_H
