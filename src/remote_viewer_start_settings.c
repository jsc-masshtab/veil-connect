#include <stdio.h>
#include <ctype.h>

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
    GtkWidget *ldap_checkbutton;
    GtkWidget *conn_to_prev_pool_checkbutton;
    GtkWidget *remote_protocol_combobox;

    // spice settings
    GtkWidget *client_cursor_visible_checkbutton;

    // RDP settings
    GtkWidget *rdp_image_pixel_format_combobox;
    GtkWidget *rdp_fps_spin_btn;

    GtkWidget *is_h264_used_check_btn;
    GtkWidget *rdp_h264_codec_entry;
    GtkWidget *rdp_shared_folders_entry;

    GtkWidget *btn_add_remote_folder;
    GtkWidget *btn_remove_remote_folder;

    // Service
    GtkWidget *btn_archive_logs;

    // control buttons
    GtkWidget *bt_cancel;
    GtkWidget *bt_ok;

    ConnectSettingsData *p_connect_settings_data;

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

/*Take data from GUI. Return true if there were no errors otherwise - false  */
// У виджета есть понятие id имя, котоое уникально и есть понятие имя виджета, которое используется для css.
static gboolean
fill_connect_settings_data_from_gui(ConnectSettingsData *connect_settings_data, ConnectSettingsDialogData *dialog_data)
{
    gboolean is_ok = TRUE;
    const gchar *pattern = "^$|[а-яА-ЯёЁa-zA-Z0-9]+[а-яА-ЯёЁa-zA-Z0-9.\\-_+ ]*$";

    // check domain name and set
    const gchar *domain_gui_str = gtk_entry_get_text(GTK_ENTRY(dialog_data->domain_entry));
    gboolean is_matched = g_regex_match_simple(pattern, domain_gui_str,G_REGEX_CASELESS,G_REGEX_MATCH_ANCHORED);

    if (is_matched) {
        gtk_widget_set_name(dialog_data->domain_entry, "domain-entry");
        free_memory_safely(&connect_settings_data->domain);
        connect_settings_data->domain = g_strdup(domain_gui_str);
    } else {
        gtk_widget_set_name(dialog_data->domain_entry, "network_entry_with_errors");
        is_ok = FALSE;
    }

    // check ip and set
    const gchar *address_gui_str = gtk_entry_get_text(GTK_ENTRY(dialog_data->address_entry));

    is_matched = g_regex_match_simple(pattern, address_gui_str,G_REGEX_CASELESS,G_REGEX_MATCH_ANCHORED);
    //g_info("%s is_matched %i\n", (const char *)__func__, is_matched);

    if (is_matched) {
        gtk_widget_set_name(dialog_data->address_entry, "connection-address-entry");
        free_memory_safely(&connect_settings_data->ip);
        connect_settings_data->ip = g_strdup(address_gui_str);
    } else {
        gtk_widget_set_name(dialog_data->address_entry, "network_entry_with_errors");
        is_ok = FALSE;
    }

    // check if the port is within the correct range and set
    const gchar *port_str = gtk_entry_get_text(GTK_ENTRY(dialog_data->port_entry));
    int port_int = atoi(port_str);
    if ( (g_strcmp0(port_str, "") == 0) || (port_int >= 0 && port_int <= 65535) ) {
        gtk_widget_set_name(dialog_data->port_entry, "connection-port-entry");
        connect_settings_data->port = port_int;
    } else {
        gtk_widget_set_name(dialog_data->port_entry, "network_entry_with_errors");
        is_ok = FALSE;
    }

    connect_settings_data->is_ldap = gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->ldap_checkbutton);
    connect_settings_data->is_connect_to_prev_pool =
            gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->conn_to_prev_pool_checkbutton);

    if (dialog_data->remote_protocol_combobox)
        connect_settings_data->remote_protocol_type = (VdiVmRemoteProtocol)(
                gtk_combo_box_get_active((GtkComboBox*)dialog_data->remote_protocol_combobox));

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
    gtk_widget_set_sensitive(dialog_data->rdp_h264_codec_entry, is_h264_used_check_btn_toggled);
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
btn_archive_logs_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialogData *dialog_data)
{
    gchar *log_dir = get_log_dir_path();
    g_info("%s", __func__);

#ifdef __linux__
    gchar *tar_cmd = g_strdup_printf("tar -czvf log.tar.gz %s", log_dir);
    system(tar_cmd);
#elif _WIN32
    const gchar *locap_app_data_path = g_getenv("LOCALAPPDATA");
    gchar *app_data_dir = g_strdup_printf("%s/%s", locap_app_data_path, PACKAGE);

    gchar *tar_cmd = g_strdup_printf("tar -czvf log.tar.gz %s -С %s", log_dir, app_data_dir);
    g_free(app_data_dir);

    system(tar_cmd);
#endif

    g_free(tar_cmd);
    g_free(log_dir);
}

static void
ok_button_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialogData *dialog_data)
{
    // fill connect_settings_data from gui
    gboolean is_success = fill_connect_settings_data_from_gui(dialog_data->p_connect_settings_data, dialog_data);

    // Close the window if settings are ok
    if (is_success) {
        dialog_data->dialog_window_response = GTK_RESPONSE_OK;
        shutdown_loop(dialog_data->loop);
    }
}

static void
fill_connect_settings_gui(ConnectSettingsDialogData *dialog_data, ConnectSettingsData *connect_settings_data)
{
    /// General settings
    // domain
    if (connect_settings_data->domain) {
        gtk_entry_set_text(GTK_ENTRY(dialog_data->domain_entry), connect_settings_data->domain);
    }
    // ip
    if (connect_settings_data->ip) {
        gtk_entry_set_text(GTK_ENTRY(dialog_data->address_entry), connect_settings_data->ip);
    }
    // port.
    int port_from_config_file = connect_settings_data->port;
    if (port_from_config_file != 0) {
        gchar *port_str = g_strdup_printf("%i", port_from_config_file);
        gtk_entry_set_text(GTK_ENTRY(dialog_data->port_entry), port_str);
        free_memory_safely(&port_str);
    } else {
        gtk_entry_set_text(GTK_ENTRY(dialog_data->port_entry), "");
    }
    // ldap
    gboolean is_ldap_btn_checked = connect_settings_data->is_ldap;
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->ldap_checkbutton, is_ldap_btn_checked);
    // Connect to prev pool
    gboolean is_conn_to_prev_pool_btn_checked = connect_settings_data->is_connect_to_prev_pool;
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->conn_to_prev_pool_checkbutton,
                                 is_conn_to_prev_pool_btn_checked);
    // remote protocol
    gint remote_protocol_type = (gint)connect_settings_data->remote_protocol_type;
    if (dialog_data->remote_protocol_combobox)
        gtk_combo_box_set_active((GtkComboBox*)dialog_data->remote_protocol_combobox, remote_protocol_type);

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

    gboolean is_rdp_h264_used = read_int_from_ini_file("RDPSettings", "is_rdp_h264_used", FALSE);
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->is_h264_used_check_btn, is_rdp_h264_used);
    gtk_widget_set_sensitive(dialog_data->rdp_h264_codec_entry, is_rdp_h264_used);

    gchar *rdp_h264_codec = read_str_from_ini_file("RDPSettings", "rdp_h264_codec");
    if (rdp_h264_codec)
        gtk_entry_set_text(GTK_ENTRY(dialog_data->rdp_h264_codec_entry), rdp_h264_codec);
    free_memory_safely(&rdp_h264_codec);

    gchar *shared_folders_str = read_str_from_ini_file("RDPSettings", "rdp_shared_folders");
    if (shared_folders_str)
        gtk_entry_set_text(GTK_ENTRY(dialog_data->rdp_shared_folders_entry), shared_folders_str);
    free_memory_safely(&shared_folders_str);
}

static void
save_data_to_ini_file(ConnectSettingsDialogData *dialog_data)
{
    if (dialog_data->dialog_window_response != GTK_RESPONSE_OK)
        return;

    const gchar *paramToFileGrpoup = opt_manual_mode ? "RemoteViewerConnectManual" : "RemoteViewerConnect";
    // domain
    write_str_to_ini_file(paramToFileGrpoup, "domain", gtk_entry_get_text(GTK_ENTRY(dialog_data->domain_entry)));
    // ip port
    write_str_to_ini_file(paramToFileGrpoup, "ip", gtk_entry_get_text(GTK_ENTRY(dialog_data->address_entry)));
    write_str_to_ini_file(paramToFileGrpoup, "port", gtk_entry_get_text(GTK_ENTRY(dialog_data->port_entry)));
    // ldap
    gboolean is_ldap_btn_checked = gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->ldap_checkbutton);
    write_int_to_ini_file(paramToFileGrpoup, "is_ldap_btn_checked", is_ldap_btn_checked);
    // prev pool
    gboolean is_conn_to_prev_pool_btn_checked =
            gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->conn_to_prev_pool_checkbutton);
    write_int_to_ini_file(paramToFileGrpoup, "is_conn_to_prev_pool_btn_checked", is_conn_to_prev_pool_btn_checked);

    if (dialog_data->remote_protocol_combobox) {
        gint cur_remote_protocol_index = gtk_combo_box_get_active((GtkComboBox*)dialog_data->remote_protocol_combobox);
        write_int_to_ini_file("General", "cur_remote_protocol_index", cur_remote_protocol_index);
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

    const gchar *rdp_h264_codec_str = gtk_entry_get_text(GTK_ENTRY(dialog_data->rdp_h264_codec_entry));
    write_str_to_ini_file("RDPSettings", "rdp_h264_codec", rdp_h264_codec_str);

    const gchar *shared_folders_str = gtk_entry_get_text(GTK_ENTRY(dialog_data->rdp_shared_folders_entry));
    write_str_to_ini_file("RDPSettings", "rdp_shared_folders", shared_folders_str);
}

GtkResponseType remote_viewer_start_settings_dialog(ConnectSettingsData *connect_settings_data, GtkWindow *parent)
{
    ConnectSettingsDialogData dialog_data;
    memset(&dialog_data, 0, sizeof(ConnectSettingsDialogData));

    dialog_data.p_connect_settings_data = connect_settings_data;
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
    dialog_data.ldap_checkbutton = get_widget_from_builder(dialog_data.builder, "ldap-button");
    gtk_widget_set_sensitive(dialog_data.ldap_checkbutton, !opt_manual_mode);
    dialog_data.conn_to_prev_pool_checkbutton = get_widget_from_builder(dialog_data.builder, "connect-to-prev-button");
    gtk_widget_set_sensitive(dialog_data.conn_to_prev_pool_checkbutton, !opt_manual_mode);
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
    dialog_data.rdp_h264_codec_entry = get_widget_from_builder(dialog_data.builder, "rdp_h264_codec_entry");

    dialog_data.rdp_shared_folders_entry = get_widget_from_builder(dialog_data.builder, "rdp_shared_folders_entry");

    dialog_data.btn_add_remote_folder = get_widget_from_builder(dialog_data.builder, "btn_add_remote_folder");
    dialog_data.btn_remove_remote_folder = get_widget_from_builder(dialog_data.builder, "btn_remove_remote_folder");

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

    // read from file
    fill_connect_settings_data_from_ini_file(connect_settings_data);
    fill_connect_settings_gui(&dialog_data, connect_settings_data);

    // show window
    gtk_window_set_transient_for(GTK_WINDOW(dialog_data.window), parent);
    gtk_window_set_position(GTK_WINDOW(dialog_data.window), GTK_WIN_POS_CENTER);
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

void
fill_connect_settings_data_from_ini_file(ConnectSettingsData *connect_settings_data)
{
    // Main settings
    const gchar *paramToFileGrpoup = opt_manual_mode ? "RemoteViewerConnectManual" : "RemoteViewerConnect";
    // domain
    free_memory_safely(&connect_settings_data->domain);
    connect_settings_data->domain = read_str_from_ini_file(paramToFileGrpoup, "domain");
    //g_info("%s connect_settings_data->domain %s\n", (const char *)__func__, connect_settings_data->domain);
    // ip
    free_memory_safely(&connect_settings_data->ip);
    connect_settings_data->ip = read_str_from_ini_file(paramToFileGrpoup, "ip");
    // port
    connect_settings_data->port = read_int_from_ini_file(paramToFileGrpoup, "port", 443);
    // ldap
    connect_settings_data->is_ldap = read_int_from_ini_file("RemoteViewerConnect", "is_ldap_btn_checked", 0);
    // Connect to prev pool
    connect_settings_data->is_connect_to_prev_pool =
            read_int_from_ini_file("RemoteViewerConnect", "is_conn_to_prev_pool_btn_checked", 0);
    // remote protocol
    gint remote_protocol_type = read_int_from_ini_file("General",
            "cur_remote_protocol_index", VDI_SPICE_PROTOCOL);
    connect_settings_data->remote_protocol_type = (VdiVmRemoteProtocol)remote_protocol_type;
}

//void free_connect_settings_data(ConnectSettingsData *connect_settings_data)
//{
//    free_memory_safely(&connect_settings_data->ip);
//    free(connect_settings_data);
//}
