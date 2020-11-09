#include <stdio.h>
#include <ctype.h>
#include <glib/gstdio.h>

#include <freerdp/codec/color.h>

#include "remote-viewer-util.h"
#include "settingsfile.h"

#include "remote_viewer_start_settings.h"

extern gboolean opt_manual_mode;

typedef struct{

    GMainLoop *loop;
    GtkResponseType dialog_window_response;

    GtkBuilder *builder;
    GtkWidget *window;

    GtkWidget *domain_entry;
    GtkWidget *address_entry;
    GtkWidget *port_entry;
    GtkWidget *ldap_check_btn;
    GtkWidget *conn_to_prev_pool_checkbutton;
    GtkWidget *save_password_checkbtn;
    GtkWidget *remote_protocol_combobox;

    // spice settings
    GtkWidget *client_cursor_visible_checkbutton;

    // RDP settings
    GtkWidget *rdp_image_pixel_format_combobox;
    GtkWidget *rdp_fps_spin_btn;

    GtkWidget *is_h264_used_check_btn;
    GtkWidget *rdp_h264_codec_combobox;
    GtkWidget *rdp_shared_folders_entry;

    GtkWidget *btn_add_remote_folder;
    GtkWidget *btn_remove_remote_folder;

    GtkWidget *is_multimon_check_btn;
    GtkWidget *redirect_printers_check_btn;

    GtkWidget *remote_app_check_btn;
    GtkWidget *remote_app_name_entry;
    GtkWidget *remote_app_options_entry;

    // Service
    GtkWidget *btn_archive_logs;

    // control buttons
    GtkWidget *bt_cancel;
    GtkWidget *bt_ok;

    ConnectSettingsData *p_conn_data;

} ConnectSettingsDialogData;

static gboolean
window_deleted_cb(ConnectSettingsDialogData *dialog_data)
{
    dialog_data->dialog_window_response = GTK_RESPONSE_CLOSE;
    shutdown_loop(dialog_data->loop);
    return TRUE;
}

static void
cancel_button_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialogData *dialog_data)
{
    dialog_data->dialog_window_response = GTK_RESPONSE_CANCEL;
    shutdown_loop(dialog_data->loop);
}

// Применяем к entry стиль такой, что она выглядет содержащей ошибку
static void
make_entry_red(GtkWidget *entry)
{
    gtk_widget_set_name(entry, "network_entry_with_errors");
}

/*Take data from GUI. Return true if there were no errors otherwise - false  */
// У виджета есть понятие id имя, котоое уникально и есть понятие имя виджета, которое используется для css.
static gboolean
fill_p_conn_data_from_gui(ConnectSettingsData *p_conn_data, ConnectSettingsDialogData *dialog_data)
{
    gboolean is_ok = TRUE;
    const gchar *pattern = "^$|[а-яА-ЯёЁa-zA-Z0-9]+[а-яА-ЯёЁa-zA-Z0-9.\\-_+ ]*$";

    // check domain name and set
    const gchar *domain_gui_str = gtk_entry_get_text(GTK_ENTRY(dialog_data->domain_entry));
    gboolean is_matched = g_regex_match_simple(pattern, domain_gui_str,G_REGEX_CASELESS,G_REGEX_MATCH_ANCHORED);

    if (is_matched) {
        gtk_widget_set_name(dialog_data->domain_entry, "domain-entry");
        update_string_safely(&p_conn_data->domain, domain_gui_str);
    } else {
        make_entry_red(dialog_data->domain_entry);
        is_ok = FALSE;
    }

    // check ip and set
    const gchar *address_gui_str = gtk_entry_get_text(GTK_ENTRY(dialog_data->address_entry));

    is_matched = g_regex_match_simple(pattern, address_gui_str,G_REGEX_CASELESS,G_REGEX_MATCH_ANCHORED);
    //g_info("%s is_matched %i\n", (const char *)__func__, is_matched);

    if (is_matched) {
        gtk_widget_set_name(dialog_data->address_entry, "connection-address-entry");
        update_string_safely(&p_conn_data->ip, address_gui_str);
    } else {
        make_entry_red(dialog_data->address_entry);
        is_ok = FALSE;
    }

    // check if the port is within the correct range and set
    const gchar *port_str = gtk_entry_get_text(GTK_ENTRY(dialog_data->port_entry));
    int port_int = atoi(port_str);
    if ( (g_strcmp0(port_str, "") == 0) || (port_int >= 0 && port_int <= 65535) ) {
        gtk_widget_set_name(dialog_data->port_entry, "connection-port-entry");
        p_conn_data->port = port_int;
    } else {
        make_entry_red(dialog_data->port_entry);
        is_ok = FALSE;
    }

    p_conn_data->is_ldap = gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->ldap_check_btn);
    p_conn_data->is_connect_to_prev_pool =
            gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->conn_to_prev_pool_checkbutton);

    if (dialog_data->remote_protocol_combobox) {
        gchar *current_protocol_str = gtk_combo_box_text_get_active_text(
                (GtkComboBoxText*)dialog_data->remote_protocol_combobox);
        p_conn_data->remote_protocol_type = vdi_session_str_to_remote_protocol(current_protocol_str);
        free_memory_safely(&current_protocol_str);
    }

    return is_ok;
}

// Перехватывается событие ввода текста. Игнорируется, если есть нецифры
static void
on_insert_text_event(GtkEditable *editable, const gchar *text, gint length,
        gint *position G_GNUC_UNUSED, gpointer data G_GNUC_UNUSED)
{
    int i;

    for (i = 0; i < length; ++i) {
        if (!isdigit(text[i])) {
            g_signal_stop_emission_by_name(G_OBJECT(editable), "insert-text");
            return;
        }
    }
}

static void
on_h264_used_check_btn_toggled(GtkToggleButton *h264_used_check_btn, gpointer user_data)
{
    ConnectSettingsDialogData *dialog_data = (ConnectSettingsDialogData *)user_data;
    gboolean is_h264_used_check_btn_toggled = gtk_toggle_button_get_active(h264_used_check_btn);
    gtk_widget_set_sensitive(dialog_data->rdp_h264_codec_combobox, is_h264_used_check_btn_toggled);
}

static void
on_remote_app_check_btn_toggled(GtkToggleButton *remote_app_check_btn, gpointer user_data)
{
    ConnectSettingsDialogData *dialog_data = (ConnectSettingsDialogData *)user_data;

    gboolean is_remote_app_check_btn_toggled = gtk_toggle_button_get_active(remote_app_check_btn);
    gtk_widget_set_sensitive(dialog_data->remote_app_name_entry, is_remote_app_check_btn_toggled);
    gtk_widget_set_sensitive(dialog_data->remote_app_options_entry, is_remote_app_check_btn_toggled);
}

/*
static void
shared_folders_icon_released(GtkEntry            *entry,
                               GtkEntryIconPosition icon_pos,
                               GdkEvent            *event,
                               gpointer             user_data)
{
    ConnectSettingsDialogData *dialog_data = (ConnectSettingsDialogData *)user_data;


    //folders_selector_widget_get_folders(GTK_WINDOW(dialog_data->window));


    GtkWidget *popup_window;
    popup_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (popup_window), "Pop Up window");

    gtk_window_set_transient_for(GTK_WINDOW (popup_window), GTK_WINDOW(dialog_data->window));
    gtk_window_set_position (GTK_WINDOW (popup_window),GTK_WIN_POS_CENTER);
    gtk_widget_show_all (popup_window);
}*/

static void
btn_add_remote_folder_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialogData *dialog_data)
{
    // get folder name
    GtkWidget *dialog = gtk_file_chooser_dialog_new ("Open File",
                                          GTK_WINDOW(dialog_data->window),
                                          GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                          "Отмена",
                                          GTK_RESPONSE_CANCEL,
                                          "Выбрать",
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);

    gint res = gtk_dialog_run (GTK_DIALOG (dialog));

    // add new folder
    if (res == GTK_RESPONSE_ACCEPT)
    {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
        gchar *added_folder = gtk_file_chooser_get_filename(chooser);

#ifdef _WIN32
        gchar *windows_style_path = replace_str(added_folder, "\\", "/");
        free_memory_safely(&added_folder);
        added_folder = windows_style_path;
#endif
        g_info("added folder: %s", added_folder);

        if (added_folder) {
            const gchar *current_folders_str = gtk_entry_get_text(GTK_ENTRY(dialog_data->rdp_shared_folders_entry));

            if (current_folders_str && *current_folders_str != '\0') {
                gchar *new_folders_str = g_strdup_printf("%s;%s", current_folders_str, added_folder);
                gtk_entry_set_text(GTK_ENTRY(dialog_data->rdp_shared_folders_entry), new_folders_str);
                g_free(new_folders_str);
            }
            else {
                gtk_entry_set_text(GTK_ENTRY(dialog_data->rdp_shared_folders_entry), added_folder);
            }
        }

        free_memory_safely(&added_folder);
    }

    gtk_widget_destroy(dialog);
}

static void
btn_remove_remote_folder_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialogData *dialog_data)
{
    // clear rdp_shared_folders_entry
    gtk_entry_set_text(GTK_ENTRY(dialog_data->rdp_shared_folders_entry), "");
}

static void
btn_archive_logs_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialogData *dialog_data G_GNUC_UNUSED)
{
    gchar *log_dir = get_log_dir_path();
    g_info("%s", __func__);

#ifdef __linux__
    gchar *tar_cmd = g_strdup_printf("tar -czvf log.tar.gz %s", log_dir);
    int res = system(tar_cmd);
#elif _WIN32
    gchar *app_data_dir = get_windows_app_data_location();

    gchar *tar_cmd = g_strdup_printf("tar.exe -czvf log.tar.gz %s -C %s", log_dir, app_data_dir);
    g_free(app_data_dir);

    int res = system(tar_cmd);
#endif
    (void)res;

    g_free(tar_cmd);
    g_free(log_dir);
}

static void
ok_button_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialogData *dialog_data)
{
    // fill p_conn_data from gui
    gboolean is_success = fill_p_conn_data_from_gui(dialog_data->p_conn_data, dialog_data);

   /* // Check some RDP settings
    const gchar *shared_folders_str = gtk_entry_get_text(GTK_ENTRY(dialog_data->rdp_shared_folders_entry));
    gchar **shared_folders_array = g_strsplit(shared_folders_str, ";", 10);

    gchar **shared_folder;
    for (shared_folder = shared_folders_array; *shared_folder; shared_folder++) {
        // Подсвечиваем поле красным, если обнаружили некорректный путь
        if(g_access(*shared_folder, R_OK) != 0) {
            make_entry_red(dialog_data->rdp_shared_folders_entry);
            is_success = FALSE;
            break;
        }
    }
    g_strfreev(shared_folders_array);*/

    // Close the window if settings are ok
    if (is_success) {
        dialog_data->dialog_window_response = GTK_RESPONSE_OK;
        shutdown_loop(dialog_data->loop);
    }
}

static void
fill_connect_settings_gui(ConnectSettingsDialogData *dialog_data, ConnectSettingsData *p_conn_data)
{
    /// General settings
    // domain
    if (p_conn_data->domain) {
        gtk_entry_set_text(GTK_ENTRY(dialog_data->domain_entry), p_conn_data->domain);
    }
    // ip
    if (p_conn_data->ip) {
        gtk_entry_set_text(GTK_ENTRY(dialog_data->address_entry), p_conn_data->ip);
    }
    // port.
    int port_from_config_file = p_conn_data->port;
    if (port_from_config_file != 0) {
        gchar *port_str = g_strdup_printf("%i", port_from_config_file);
        gtk_entry_set_text(GTK_ENTRY(dialog_data->port_entry), port_str);
        free_memory_safely(&port_str);
    } else {
        gtk_entry_set_text(GTK_ENTRY(dialog_data->port_entry), "");
    }
    // ldap
    gboolean is_ldap_btn_checked = p_conn_data->is_ldap;
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->ldap_check_btn, is_ldap_btn_checked);
    // Connect to prev pool
    gboolean is_conn_to_prev_pool_btn_checked = p_conn_data->is_connect_to_prev_pool;
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->conn_to_prev_pool_checkbutton,
                                 is_conn_to_prev_pool_btn_checked);
    // pswd
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->save_password_checkbtn, p_conn_data->to_save_pswd);

    if (dialog_data->remote_protocol_combobox) {
        // индекс 0 - спайс индекс 1 - рдп
        gint index = (p_conn_data->remote_protocol_type == VDI_SPICE_PROTOCOL) ? 0 : 1;
        gtk_combo_box_set_active((GtkComboBox *) dialog_data->remote_protocol_combobox, index);
    }

    /// Spice settings
    gboolean is_spice_client_cursor_visible =
            read_int_from_ini_file("SpiceSettings", "is_spice_client_cursor_visible", FALSE);
    gtk_toggle_button_set_active((GtkToggleButton*)dialog_data->client_cursor_visible_checkbutton,
                                     is_spice_client_cursor_visible);

    /// RDP settings
    gchar *rdp_pixel_format_str = read_str_from_ini_file("RDPSettings", "rdp_pixel_format");
    UINT32 freerdp_pix_index = (g_strcmp0(rdp_pixel_format_str, "BGRA32") == 0) ? 1 : 0;
    free_memory_safely(&rdp_pixel_format_str);
    gtk_combo_box_set_active((GtkComboBox*)dialog_data->rdp_image_pixel_format_combobox, freerdp_pix_index);

    UINT32 rdp_fps = CLAMP(read_int_from_ini_file("RDPSettings", "rdp_fps", 30), 1, 60);
    gtk_spin_button_set_value((GtkSpinButton*) dialog_data->rdp_fps_spin_btn, (gdouble)rdp_fps);

    gboolean is_rdp_h264_used = read_int_from_ini_file("RDPSettings", "is_rdp_h264_used", TRUE);
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->is_h264_used_check_btn, is_rdp_h264_used);
    gtk_widget_set_sensitive(dialog_data->rdp_h264_codec_combobox, is_rdp_h264_used);

    if (is_rdp_h264_used) {
        gchar *rdp_h264_codec = read_str_from_ini_file("RDPSettings", "rdp_h264_codec");
        if (rdp_h264_codec) {
            g_strstrip(rdp_h264_codec);
            gtk_combo_box_set_active(GTK_COMBO_BOX(dialog_data->rdp_h264_codec_combobox),
                    string_to_h264_codec(rdp_h264_codec));
            free_memory_safely(&rdp_h264_codec);
        } else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(dialog_data->rdp_h264_codec_combobox),
                    (gint)get_default_h264_codec());
        }
    }

    gchar *shared_folders_str = read_str_from_ini_file("RDPSettings", "rdp_shared_folders");
    if (shared_folders_str)
        gtk_entry_set_text(GTK_ENTRY(dialog_data->rdp_shared_folders_entry), shared_folders_str);
    free_memory_safely(&shared_folders_str);

    gboolean is_rdp_multimon = read_int_from_ini_file("RDPSettings", "is_multimon", FALSE);
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->is_multimon_check_btn, is_rdp_multimon);

    gboolean redirect_printers = read_int_from_ini_file("RDPSettings", "redirect_printers", FALSE);
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->redirect_printers_check_btn, redirect_printers);

    gboolean is_remote_app = read_int_from_ini_file("RDPSettings", "is_remote_app", 0);
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->remote_app_check_btn, is_remote_app);
    gchar *remote_app_name = read_str_from_ini_file("RDPSettings", "remote_app_name");
    if (remote_app_name) {
        gtk_entry_set_text(GTK_ENTRY(dialog_data->remote_app_name_entry), remote_app_name);
        g_free(remote_app_name);
    }
    gchar *remote_app_options = read_str_from_ini_file("RDPSettings", "remote_app_options");
    if (remote_app_options) {
        gtk_entry_set_text(GTK_ENTRY(dialog_data->remote_app_options_entry), remote_app_options);
        g_free(remote_app_options);
    }
}

static void
save_data_to_ini_file(ConnectSettingsDialogData *dialog_data)
{
    if (dialog_data->dialog_window_response != GTK_RESPONSE_OK)
        return;

    const gchar *paramToFileGrpoup = get_cur_ini_param_group();
    // domain
    write_str_to_ini_file(paramToFileGrpoup, "domain", gtk_entry_get_text(GTK_ENTRY(dialog_data->domain_entry)));
    // ip port
    write_str_to_ini_file(paramToFileGrpoup, "ip", gtk_entry_get_text(GTK_ENTRY(dialog_data->address_entry)));
    write_str_to_ini_file(paramToFileGrpoup, "port", gtk_entry_get_text(GTK_ENTRY(dialog_data->port_entry)));
    // ldap
    gboolean is_ldap_btn_checked = gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->ldap_check_btn);
    write_int_to_ini_file(paramToFileGrpoup, "is_ldap_btn_checked", is_ldap_btn_checked);
    // prev pool
    gboolean is_conn_to_prev_pool_btn_checked =
            gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->conn_to_prev_pool_checkbutton);
    write_int_to_ini_file(paramToFileGrpoup, "is_conn_to_prev_pool_btn_checked", is_conn_to_prev_pool_btn_checked);
    // pswd
    dialog_data->p_conn_data->to_save_pswd = gtk_toggle_button_get_active(
            (GtkToggleButton *)dialog_data->save_password_checkbtn);
    write_int_to_ini_file(paramToFileGrpoup, "to_save_pswd", dialog_data->p_conn_data->to_save_pswd);

    if (dialog_data->remote_protocol_combobox) {
        gint remote_protocol_combobox_index =
                gtk_combo_box_get_active((GtkComboBox*)dialog_data->remote_protocol_combobox);
        // индекс 0 - спайс индекс 1 - рдп
        VdiVmRemoteProtocol protocol = (remote_protocol_combobox_index == 0) ? VDI_SPICE_PROTOCOL : VDI_RDP_PROTOCOL;
        write_int_to_ini_file("General", "cur_remote_protocol_index", protocol);
    }

    /// Spice debug cursor enabling
    gboolean is_spice_client_cursor_visible =
            gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->client_cursor_visible_checkbutton);
    set_client_spice_cursor_visible(is_spice_client_cursor_visible);

    /// RDP settings
    gchar *rdp_pixel_format_str = gtk_combo_box_text_get_active_text(
            (GtkComboBoxText *)dialog_data->rdp_image_pixel_format_combobox);
    write_str_to_ini_file("RDPSettings", "rdp_pixel_format", rdp_pixel_format_str);
    free_memory_safely(&rdp_pixel_format_str);

    gint fps = (gint)gtk_spin_button_get_value((GtkSpinButton *)dialog_data->rdp_fps_spin_btn);
    write_int_to_ini_file("RDPSettings", "rdp_fps", fps);

    gboolean is_rdp_h264_used = gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->is_h264_used_check_btn);
    write_int_to_ini_file("RDPSettings", "is_rdp_h264_used", is_rdp_h264_used);

    gchar *rdp_h264_codec_str =
            gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(dialog_data->rdp_h264_codec_combobox));
    write_str_to_ini_file("RDPSettings", "rdp_h264_codec", rdp_h264_codec_str);
    free_memory_safely(&rdp_h264_codec_str);

    const gchar *shared_folders_str = gtk_entry_get_text(GTK_ENTRY(dialog_data->rdp_shared_folders_entry));
    // Пользовать мог ввести путь с виндусятскими разделителями, поэтому на всякий случай заменяем
    gchar *folder_name = replace_str(shared_folders_str, "\\", "/");
    write_str_to_ini_file("RDPSettings", "rdp_shared_folders", folder_name);
    free_memory_safely(&folder_name);

    gboolean is_rdp_multimon = gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->is_multimon_check_btn);
    write_int_to_ini_file("RDPSettings", "is_multimon", is_rdp_multimon);

    gboolean redirect_printers = gtk_toggle_button_get_active(
            (GtkToggleButton *)dialog_data->redirect_printers_check_btn);
    write_int_to_ini_file("RDPSettings", "redirect_printers", redirect_printers);

    gboolean is_remote_app = gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->remote_app_check_btn);
    write_int_to_ini_file("RDPSettings", "is_remote_app", is_remote_app);
    write_str_to_ini_file("RDPSettings", "remote_app_name",
            gtk_entry_get_text(GTK_ENTRY(dialog_data->remote_app_name_entry)));
    write_str_to_ini_file("RDPSettings", "remote_app_options",
                          gtk_entry_get_text(GTK_ENTRY(dialog_data->remote_app_options_entry)));

} // remote_app_name_entry      remote_app_options_entry

GtkResponseType remote_viewer_start_settings_dialog(ConnectSettingsData *p_conn_data, GtkWindow *parent)
{
    ConnectSettingsDialogData dialog_data;
    memset(&dialog_data, 0, sizeof(ConnectSettingsDialogData));

    dialog_data.p_conn_data = p_conn_data;
    dialog_data.dialog_window_response = GTK_RESPONSE_OK;

    // gui widgets
    dialog_data.builder = remote_viewer_util_load_ui("start_settings_form.ui");

    dialog_data.window = get_widget_from_builder(dialog_data.builder, "start-settings-window");
    // main buttons
    dialog_data.bt_cancel = get_widget_from_builder(dialog_data.builder, "btn_cancel");
    dialog_data.bt_ok = get_widget_from_builder(dialog_data.builder, "btn_ok");

    // main settinfs
    dialog_data.domain_entry = get_widget_from_builder(dialog_data.builder, "domain-entry");
    dialog_data.address_entry = get_widget_from_builder(dialog_data.builder, "connection-address-entry");
    dialog_data.port_entry = get_widget_from_builder(dialog_data.builder, "connection-port-entry");
    dialog_data.ldap_check_btn = get_widget_from_builder(dialog_data.builder, "ldap-button");
    gtk_widget_set_sensitive(dialog_data.ldap_check_btn, !opt_manual_mode);
    dialog_data.conn_to_prev_pool_checkbutton = get_widget_from_builder(dialog_data.builder, "connect-to-prev-button");
    gtk_widget_set_sensitive(dialog_data.conn_to_prev_pool_checkbutton, !opt_manual_mode);
    dialog_data.save_password_checkbtn = get_widget_from_builder(dialog_data.builder, "save_password_btn");
    dialog_data.remote_protocol_combobox =get_widget_from_builder(dialog_data.builder, "combobox_remote_protocol");
    if (!opt_manual_mode) {
        gtk_widget_destroy(dialog_data.remote_protocol_combobox);
        dialog_data.remote_protocol_combobox = NULL;
    }

    // spice settings
    dialog_data.client_cursor_visible_checkbutton =
            get_widget_from_builder(dialog_data.builder, "menu-show-client-cursor");

    // rdp settings
    dialog_data.rdp_image_pixel_format_combobox =
            get_widget_from_builder(dialog_data.builder, "rdp_image_pixel_format_combobox");
    dialog_data.rdp_fps_spin_btn = get_widget_from_builder(dialog_data.builder, "rdp_fps_spin_btn");

    dialog_data.is_h264_used_check_btn = get_widget_from_builder(dialog_data.builder, "is_h264_used_check_btn");
    dialog_data.rdp_h264_codec_combobox = get_widget_from_builder(dialog_data.builder, "rdp_h264_codec_combobox");

    dialog_data.rdp_shared_folders_entry = get_widget_from_builder(dialog_data.builder, "rdp_shared_folders_entry");

    dialog_data.btn_add_remote_folder = get_widget_from_builder(dialog_data.builder, "btn_add_remote_folder");
    dialog_data.btn_remove_remote_folder = get_widget_from_builder(dialog_data.builder, "btn_remove_remote_folder");

    dialog_data.is_multimon_check_btn = get_widget_from_builder(dialog_data.builder, "is_multimon_check_btn");
    dialog_data.redirect_printers_check_btn =
            get_widget_from_builder(dialog_data.builder, "redirect_printers_check_btn");

    dialog_data.remote_app_check_btn = get_widget_from_builder(dialog_data.builder, "remote_app_check_btn");
    dialog_data.remote_app_name_entry = get_widget_from_builder(dialog_data.builder, "remote_app_name_entry");
    dialog_data.remote_app_options_entry = get_widget_from_builder(dialog_data.builder, "remote_app_options_entry");

    // Service functions
    dialog_data.btn_archive_logs = get_widget_from_builder(dialog_data.builder, "btn_archive_logs");

    // Signals
    g_signal_connect_swapped(dialog_data.window, "delete-event", G_CALLBACK(window_deleted_cb), &dialog_data);
    g_signal_connect(dialog_data.bt_cancel, "clicked", G_CALLBACK(cancel_button_clicked_cb), &dialog_data);
    g_signal_connect(dialog_data.bt_ok, "clicked", G_CALLBACK(ok_button_clicked_cb), &dialog_data);
    g_signal_connect(dialog_data.port_entry, "insert-text", G_CALLBACK(on_insert_text_event), NULL);
    g_signal_connect(dialog_data.is_h264_used_check_btn, "toggled", G_CALLBACK(on_h264_used_check_btn_toggled),
            &dialog_data);
    g_signal_connect(dialog_data.btn_add_remote_folder, "clicked",
                     G_CALLBACK(btn_add_remote_folder_clicked_cb), &dialog_data);
    g_signal_connect(dialog_data.btn_remove_remote_folder, "clicked",
                     G_CALLBACK(btn_remove_remote_folder_clicked_cb), &dialog_data);
    g_signal_connect(dialog_data.btn_archive_logs, "clicked",
                     G_CALLBACK(btn_archive_logs_clicked_cb), &dialog_data);
    g_signal_connect(dialog_data.remote_app_check_btn, "toggled", G_CALLBACK(on_remote_app_check_btn_toggled),
                     &dialog_data);

    // read from file
    fill_p_conn_data_from_ini_file(p_conn_data);
    fill_connect_settings_gui(&dialog_data, p_conn_data);

    // show window
    gtk_window_set_transient_for(GTK_WINDOW(dialog_data.window), parent);
    gtk_window_set_position(GTK_WINDOW(dialog_data.window), GTK_WIN_POS_CENTER);
    //gtk_window_resize(GTK_WINDOW(dialog_data.window),         width,          height);
    gtk_widget_show_all(dialog_data.window);
    gtk_window_set_resizable(GTK_WINDOW(dialog_data.window), FALSE);

    create_loop_and_launch(&dialog_data.loop);

    // write to file if response is GTK_RESPONSE_OK
    save_data_to_ini_file(&dialog_data);

    // clear
    g_object_unref(dialog_data.builder);
    gtk_widget_destroy(dialog_data.window);

    return dialog_data.dialog_window_response;
}

void fill_p_conn_data_from_ini_file(ConnectSettingsData *p_conn_data)
{
    // Main settings
    const gchar *paramToFileGrpoup = get_cur_ini_param_group();
    // domain
    gchar *domain = read_str_from_ini_file(paramToFileGrpoup, "domain");
    update_string_safely(&p_conn_data->domain, domain);
    free_memory_safely(&domain);
    // ip
    gchar *ip = read_str_from_ini_file(paramToFileGrpoup, "ip");
    update_string_safely(&p_conn_data->ip, ip);
    free_memory_safely(&ip);
    // port
    p_conn_data->port = read_int_from_ini_file(paramToFileGrpoup, "port", 443);
    // ldap
    p_conn_data->is_ldap = read_int_from_ini_file("RemoteViewerConnect", "is_ldap_btn_checked", 0);
    // Connect to prev pool
    p_conn_data->is_connect_to_prev_pool =
            read_int_from_ini_file("RemoteViewerConnect", "is_conn_to_prev_pool_btn_checked", 0);
    // pswd
    const gchar *group_name = get_cur_ini_param_group();
    p_conn_data->to_save_pswd = read_int_from_ini_file(group_name, "to_save_pswd", 1);

    // remote protocol
    gint remote_protocol_type = read_int_from_ini_file("General", "cur_remote_protocol_index", VDI_SPICE_PROTOCOL);
    p_conn_data->remote_protocol_type = (VdiVmRemoteProtocol)remote_protocol_type;
}
