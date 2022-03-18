/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef REMOTE_VIEWER_START_SETTINGS_H
#define REMOTE_VIEWER_START_SETTINGS_H

#include <gtk/gtk.h>

#include <winpr/wtypes.h>

#include <stdio.h>
#include <ctype.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <freerdp/codec/color.h>

#include "remote-viewer-util.h"
#include "settingsfile.h"

#include "usb_selector_widget.h"

#include "veil_logger.h"
#include "controller_client/controller_session.h"
#include "settings_data.h"
#include "app_updater.h"

//#include "remote-viewer.h"

typedef struct {

    GMainLoop *loop;
    GtkResponseType dialog_window_response;

    GtkBuilder *builder;
    GtkWidget *window;

    GtkWidget *domain_entry;
    GtkWidget *address_entry;
    GtkWidget *port_spinbox;
    GtkWidget *conn_to_prev_pool_checkbutton;
    GtkWidget *pass_through_auth_btn;
    GtkWidget *save_password_checkbtn;
    GtkWidget *remote_protocol_combobox;
    GtkWidget *redirect_time_zone_check_btn;

    // spice settings
    GtkWidget *client_cursor_visible_checkbutton;
    GtkWidget *spice_full_screen_check_btn;
    GtkWidget *spice_monitor_mapping_entry;
    GtkWidget *btn_show_monitor_config_spice;

    GtkWidget *spice_usb_auto_connect_filter_entry;
    GtkWidget *spice_usb_redirect_on_connect_entry;
    GtkWidget *btn_show_usb_filter_tooltip_spice_0;
    GtkWidget *btn_show_usb_filter_tooltip_spice_1;

    // RDP settings
    GtkWidget *rdp_image_pixel_format_combobox;
    GtkWidget *rdp_fps_spin_btn;

    GtkWidget *is_rdp_vid_comp_used_check_btn;
    GtkWidget *rdp_codec_combobox;
    GtkWidget *rdp_shared_folders_entry;

    GtkWidget *btn_add_remote_folder;
    GtkWidget *btn_remove_remote_folder;

    GtkWidget *is_multimon_check_btn;
    GtkWidget *rdp_full_screen_check_btn;
    GtkWidget *rdp_selected_monitors_entry;
    GtkWidget *btn_show_monitor_config_rdp;

    GtkWidget *redirect_printers_check_btn;
    GtkWidget *rdp_redirect_microphone_check_btn;

    GtkWidget *remote_app_check_btn;
    GtkWidget *remote_app_name_entry;
    GtkWidget *remote_app_options_entry;

    GtkWidget *rdp_sec_protocol_check_btn;
    GtkWidget *sec_type_combobox;

    GtkWidget *rdp_network_check_btn;
    GtkWidget *rdp_network_type_combobox;

    GtkWidget *rdp_decorations_check_btn;
    GtkWidget *rdp_fonts_check_btn;
    GtkWidget *rdp_themes_check_btn;

    GtkWidget *select_usb_btn;

    GtkWidget *use_rdp_file_check_btn;
    GtkWidget *btn_choose_rdp_file;
    GtkWidget *rdp_file_name_entry;

    UsbSelectorWidget *usb_selector_widget;

    GtkWidget *use_rd_gateway_check_btn;
    GtkWidget *gateway_address_entry;

    GtkWidget *rdp_log_debug_info_check_btn;

    // X2GO settings
    GtkWidget *x2go_app_combobox;

    GtkWidget *x2go_session_type_combobox;

    GtkWidget *x2go_conn_type_check_btn;
    GtkWidget *x2go_conn_type_combobox;

    GtkWidget *x2go_full_screen_check_btn;

    GtkWidget *x2go_compress_method_combobox;

    // Service
    GtkWidget *btn_archive_logs;
    GtkWidget *log_location_label;

    GtkWidget *btn_get_app_updates;
    GtkWidget *btn_open_doc;
    GtkWidget *check_updates_spinner;
    GtkWidget *check_updates_label;
    GtkWidget *windows_updates_url_entry;

    GtkWidget *app_mode_combobox;
    GtkWidget *vm_await_time_spinbox;
    GtkWidget *unique_app_check_btn;

    // control buttons
    GtkWidget *bt_cancel;
    GtkWidget *bt_ok;

    // signal handler ids
    gulong on_app_mode_combobox_changed_id;

    ConnectSettingsData *p_conn_data;
    AppUpdater *p_app_updater;

} ConnectSettingsDialog;


GtkResponseType remote_viewer_start_settings_dialog(ConnectSettingsDialog *self,
                                                    ConnectSettingsData *conn_data,
                                                    AppUpdater *app_updater,
                                                    GtkWindow *parent);


#endif // REMOTE_VIEWER_START_SETTINGS_H
