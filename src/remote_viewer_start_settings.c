/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <stdio.h>
#include <ctype.h>
#include <glib/gstdio.h>

#include <freerdp/codec/color.h>

#include "remote-viewer-util.h"
#include "settingsfile.h"

#include "usb_selector_widget.h"

#include "remote_viewer_start_settings.h"

extern gboolean opt_manual_mode;

typedef struct{

    GMainLoop *loop;
    GtkResponseType dialog_window_response;

    GtkBuilder *builder;
    GtkWidget *window;

    GtkWidget *domain_entry;
    GtkWidget *address_entry;
    GtkWidget *port_spinbox;
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

    // Service
    GtkWidget *btn_archive_logs;
    GtkWidget *log_location_label;

    GtkWidget *btn_get_app_updates;
    GtkWidget *btn_open_doc;
    GtkWidget *check_updates_spinner;
    GtkWidget *check_updates_label;
    GtkWidget *windows_updates_url_entry;

    GtkWidget *direct_connect_mode_check_btn;

    // control buttons
    GtkWidget *bt_cancel;
    GtkWidget *bt_ok;

    ConnectSettingsData *p_conn_data;
    RemoteViewer *p_remote_viewer;

    // signal handler ids
    gulong on_direct_connect_mode_check_btn_toggled_id;

} ConnectSettingsDialogData;

// D ???????????? ???? ?????????????????? ?????????????????????? ?? VDI ??????????????. ?? ???????????? ???????????? ???????????? ?????????????????????? ?? ????
// GUI ?? ???????????? ?????????????? ?????????? ?????????????????? ??????????????
static void update_gui_according_to_connect_mode(ConnectSettingsDialogData *dialog_data,
                                                     gboolean _opt_manual_mode)
{
    gtk_widget_set_sensitive(dialog_data->ldap_check_btn, !_opt_manual_mode);
    gtk_widget_set_sensitive(dialog_data->conn_to_prev_pool_checkbutton, !_opt_manual_mode);
    gtk_widget_set_visible(dialog_data->remote_protocol_combobox, _opt_manual_mode);

    gtk_widget_set_visible(dialog_data->btn_choose_rdp_file, _opt_manual_mode);
    gtk_widget_set_visible(dialog_data->rdp_file_name_entry, _opt_manual_mode);
    gtk_widget_set_visible(dialog_data->use_rdp_file_check_btn, _opt_manual_mode);
}

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

// ?????????????????? ?? entry ?????????? ??????????, ?????? ?????? ???????????????? ???????????????????? ????????????
static void
make_entry_red(GtkWidget *entry)
{
    gtk_widget_set_name(entry, "network_entry_with_errors");
}

/*Take data from GUI. Return true if there were no errors otherwise - false  */
// ?? ?????????????? ???????? ?????????????? id ??????, ???????????? ?????????????????? ?? ???????? ?????????????? ?????? ??????????????, ?????????????? ???????????????????????? ?????? css.
static gboolean
fill_p_conn_data_from_gui(ConnectSettingsData *p_conn_data, ConnectSettingsDialogData *dialog_data)
{
    gboolean is_ok = TRUE;
    const gchar *pattern = "^$|[??-????-??????a-zA-Z0-9]+[??-????-??????a-zA-Z0-9.\\-_+ ]*$";

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

    p_conn_data->port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dialog_data->port_spinbox));
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

static void
on_h264_used_check_btn_toggled(GtkToggleButton *h264_used_check_btn, gpointer user_data)
{
    ConnectSettingsDialogData *dialog_data = (ConnectSettingsDialogData *)user_data;
    if (dialog_data->rdp_h264_codec_combobox) {
        gboolean is_h264_used_check_btn_toggled = gtk_toggle_button_get_active(h264_used_check_btn);
        gtk_widget_set_sensitive(dialog_data->rdp_h264_codec_combobox, is_h264_used_check_btn_toggled);
    }
}

static void
on_rdp_network_check_btn_toggled(GtkToggleButton *rdp_network_check_btn, gpointer user_data)
{
    ConnectSettingsDialogData *dialog_data = (ConnectSettingsDialogData *)user_data;
    gboolean is_rdp_network_check_btn_toggled = gtk_toggle_button_get_active(rdp_network_check_btn);
    gtk_widget_set_sensitive(dialog_data->rdp_network_type_combobox, is_rdp_network_check_btn_toggled);
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
                                                     "????????????",
                                                     GTK_RESPONSE_CANCEL,
                                                     "??????????????",
                                                     GTK_RESPONSE_ACCEPT,
                                                     NULL);

    gint res = gtk_dialog_run (GTK_DIALOG (dialog));

    // add new folder
    if (res == GTK_RESPONSE_ACCEPT) {
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
on_use_rdp_file_check_btn_toggled(GtkToggleButton *button, ConnectSettingsDialogData *dialog_data)
{
    gboolean use_rdp_file = gtk_toggle_button_get_active(button);
    gtk_widget_set_sensitive(dialog_data->btn_choose_rdp_file, use_rdp_file);
    gtk_widget_set_sensitive(dialog_data->rdp_file_name_entry, use_rdp_file);
}

static void
btn_btn_choose_rdp_file_clicked(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialogData *dialog_data)
{
    GtkWidget *dialog = gtk_file_chooser_dialog_new ("Open File",
                                                     GTK_WINDOW(dialog_data->window),
                                                     GTK_FILE_CHOOSER_ACTION_OPEN,
                                                     "????????????",
                                                     GTK_RESPONSE_CANCEL,
                                                     "??????????????",
                                                     GTK_RESPONSE_ACCEPT,
                                                     NULL);

    gint res = gtk_dialog_run (GTK_DIALOG (dialog));

    if (res == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
        gchar *file_name = gtk_file_chooser_get_filename(chooser);
        if (file_name) {
            gchar *rdp_settings_file_mod = replace_str(file_name, "\\", "/");

            gtk_entry_set_text(GTK_ENTRY(dialog_data->rdp_file_name_entry), rdp_settings_file_mod);
            gtk_widget_set_tooltip_text(dialog_data->rdp_file_name_entry, rdp_settings_file_mod);
            g_free(file_name);
            g_free(rdp_settings_file_mod);
        }
    }

    gtk_widget_destroy(dialog);
}

static void
btn_archive_logs_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialogData *dialog_data)
{
    gchar *log_dir = get_log_dir_path();

#ifdef G_OS_UNIX
    gchar *tar_cmd = g_strdup_printf("tar -czvf log.tar.gz %s", log_dir);

    int res = system(tar_cmd);

    // show log dir on gui
    gchar *cur_dir = g_get_current_dir();
    gtk_label_set_text(GTK_LABEL(dialog_data->log_location_label), cur_dir);
    free_memory_safely(&cur_dir);
#elif _WIN32
    gchar *app_data_dir = get_windows_app_data_location();
    gchar *tar_cmd = g_strdup_printf("cd \"%s\" && tar.exe -czvf log.tar.gz \"%s\"", app_data_dir, log_dir);
    //
    int res = system(tar_cmd);

    gtk_label_set_text(GTK_LABEL(dialog_data->log_location_label), app_data_dir);
    g_free(app_data_dir);
#endif
    (void)res;

    g_free(tar_cmd);
    g_free(log_dir);
}

static void
on_app_updater_status_msg_changed(gpointer data G_GNUC_UNUSED, const gchar *msg,
                                  ConnectSettingsDialogData *dialog_data)
{
    g_info("%s", (const char *)__func__);
    gtk_label_set_text(GTK_LABEL(dialog_data->check_updates_label), msg);
}

static void
on_app_updater_status_changed(gpointer data G_GNUC_UNUSED,
                              int isworking,
                              ConnectSettingsDialogData *dialog_data)
{
    g_info("%s isworking: %i", (const char *)__func__, isworking);
    if (isworking) {
        gtk_spinner_start((GtkSpinner *) dialog_data->check_updates_spinner);
    }
    else {
        gtk_spinner_stop((GtkSpinner *) dialog_data->check_updates_spinner);

        // show the last output if there was an error
        AppUpdater *app_updater = dialog_data->p_remote_viewer->app_updater;
        gchar *last_process_output = app_updater_get_last_process_output(app_updater);

        if (app_updater->_last_exit_status != 0 && last_process_output ) {

            GtkBuilder *builder = remote_viewer_util_load_ui("text_msg_form.ui");
            GtkWidget *text_msg_dialog = get_widget_from_builder(builder, "text_msg_dialog");
            gtk_dialog_add_button(GTK_DIALOG(text_msg_dialog), "????", GTK_RESPONSE_OK);

            GtkWidget *header_text_label = get_widget_from_builder(builder, "header_text_label");
            gtk_label_set_text(GTK_LABEL(header_text_label), "?????????????? ???????????????????? ???????????????????? ?? ????????????????");

            GtkWidget *main_text_view = get_widget_from_builder(builder, "main_text_view");

            GtkTextBuffer *main_text_view_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(main_text_view));
            // output of the last process
            g_info("%s last_process_output: %s", (const char *)__func__, last_process_output);
            gtk_text_buffer_set_text(main_text_view_buffer, last_process_output, -1);

            gtk_window_set_transient_for(GTK_WINDOW(text_msg_dialog), GTK_WINDOW(dialog_data->window));
            gtk_window_set_default_size(GTK_WINDOW(text_msg_dialog), 500, 500);
            gtk_window_set_resizable(GTK_WINDOW(text_msg_dialog), FALSE);

            gtk_dialog_run(GTK_DIALOG(text_msg_dialog));
            g_object_unref(builder);
            gtk_widget_destroy(text_msg_dialog);
        }

        free_memory_safely(&last_process_output);
    }
    gtk_widget_set_sensitive(dialog_data->btn_get_app_updates, !isworking);
}

// ???? ?????????????? ?????????????? enter ???? password_entry ???????????????? ???????? ?????????? ???????????? ?? ?????? ?????????????? ??????????????????????
static void
on_password_entry_activated(GtkEntry *entry G_GNUC_UNUSED, GtkWidget *ask_pass_dialog)
{
    gtk_widget_hide(ask_pass_dialog);
}

static void
btn_get_app_updates_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialogData *dialog_data)
{
    g_info("%s", (const char *)__func__);
    AppUpdater *app_updater = dialog_data->p_remote_viewer->app_updater;
    if (app_updater_is_getting_updates(app_updater))
        return;

#ifdef __linux__
    // get sudo pass (?????????? ???????????????? ???????????? ?????? ?????????? ???????????? sudo)
    GtkBuilder *builder = remote_viewer_util_load_ui("ask_pass_form.ui");
    GtkWidget *ask_pass_dialog = get_widget_from_builder(builder, "ask_pass_dialog");
    gtk_dialog_add_button(GTK_DIALOG(ask_pass_dialog), "????", GTK_RESPONSE_OK);
    gtk_dialog_add_button(GTK_DIALOG(ask_pass_dialog), "????????????", GTK_RESPONSE_CANCEL);
    gtk_dialog_set_default_response(GTK_DIALOG(ask_pass_dialog), GTK_RESPONSE_OK);
    gtk_window_set_transient_for(GTK_WINDOW(ask_pass_dialog), GTK_WINDOW(dialog_data->window));

    GtkWidget *password_entry = get_widget_from_builder(builder, "password_entry");
    g_signal_connect(password_entry, "activate", G_CALLBACK(on_password_entry_activated), ask_pass_dialog);

    int result = gtk_dialog_run(GTK_DIALOG(ask_pass_dialog));
    switch(result) {
        case GTK_RESPONSE_OK:
        case GTK_RESPONSE_NONE: // default
            g_info("GTK_RESPONSE_OK");
            const gchar *password = gtk_entry_get_text(GTK_ENTRY(password_entry));
            app_updater_set_admin_password(app_updater, password);
            break;
        default:
            break;
    }
    g_object_unref(builder);
    gtk_widget_destroy(ask_pass_dialog);

    // check if password provided
    gchar *app_updater_admin_password = app_updater_get_admin_password(app_updater);
    if( app_updater_admin_password == NULL) {
        gtk_label_set_text(GTK_LABEL(dialog_data->check_updates_label), "???????????????????? ???????????? ????????????");
        return;
    }
    free_memory_safely(&app_updater_admin_password);

    // start updating routine
    app_updater_execute_task_get_updates(app_updater);
#elif _WIN32
    app_updater_execute_task_get_updates(app_updater);
#else
    gtk_label_set_text(GTK_LABEL(dialog_data->check_updates_label), "???? ?????????????????????? ?????? ?????????????? ????");
#endif
}

static void
btn_open_doc_clicked_cb()
{
    gtk_show_uri_on_window(NULL, VEIL_CONNECT_DOC_SITE, GDK_CURRENT_TIME, NULL);
}

static void
on_select_usb_btn_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialogData *dialog_data)
{
    usb_selector_widget_show_and_start_loop(dialog_data->usb_selector_widget, GTK_WINDOW(dialog_data->window));
}

static void
on_direct_connect_mode_check_btn_toggled(GtkToggleButton *check_btn, gpointer user_data)
{
    g_info("%s", (const char*)__func__);
    ConnectSettingsDialogData *dialog_data = (ConnectSettingsDialogData *)user_data;
    gboolean _opt_manual_mode = gtk_toggle_button_get_active(check_btn);
    update_gui_according_to_connect_mode(dialog_data, _opt_manual_mode);

    // ?????????????????????????? ?????????????????? ????????
    if (_opt_manual_mode)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog_data->port_spinbox), 3389);
    else
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog_data->port_spinbox), 443);
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
         // ???????????????????????? ???????? ??????????????, ???????? ???????????????????? ???????????????????????? ????????
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
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog_data->port_spinbox), p_conn_data->port);
    // ldap
    gboolean is_ldap_btn_checked = p_conn_data->is_ldap;
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->ldap_check_btn, is_ldap_btn_checked);
    // Connect to prev pool
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->conn_to_prev_pool_checkbutton,
                                 p_conn_data->is_connect_to_prev_pool);
    // pswd
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->save_password_checkbtn, p_conn_data->to_save_pswd);

    if (dialog_data->remote_protocol_combobox) {
        // ?????????? ?? id ??????????????????
        const gchar *protocol_str = vdi_session_remote_protocol_to_str(p_conn_data->remote_protocol_type);
        gtk_combo_box_set_active_id((GtkComboBox *) dialog_data->remote_protocol_combobox, protocol_str);
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

    if (dialog_data->rdp_h264_codec_combobox) {
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
                                         (gint) get_default_h264_codec());
            }
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
    gchar *remote_app_program = read_str_from_ini_file("RDPSettings", "remote_app_program");
    if (remote_app_program) {
        gtk_entry_set_text(GTK_ENTRY(dialog_data->remote_app_name_entry), remote_app_program);
        g_free(remote_app_program);
    }
    gchar *remote_app_options = read_str_from_ini_file("RDPSettings", "remote_app_options");
    if (remote_app_options) {
        gtk_entry_set_text(GTK_ENTRY(dialog_data->remote_app_options_entry), remote_app_options);
        g_free(remote_app_options);
    }

    gboolean is_rdp_network_assigned = read_int_from_ini_file("RDPSettings", "is_rdp_network_assigned", 0);
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->rdp_network_check_btn, is_rdp_network_assigned);
    gtk_widget_set_sensitive(dialog_data->rdp_network_type_combobox, is_rdp_network_assigned);
    if (is_rdp_network_assigned) {
        gchar *rdp_network_type = read_str_from_ini_file("RDPSettings", "rdp_network_type");
        if (rdp_network_type) {
            gtk_combo_box_set_active_id(GTK_COMBO_BOX(dialog_data->rdp_network_type_combobox), rdp_network_type);
            g_free(rdp_network_type);
        }
    }

    gboolean rdp_decorations = read_int_from_ini_file("RDPSettings", "disable_rdp_decorations", 0);
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->rdp_decorations_check_btn, rdp_decorations);
    gboolean rdp_fonts = read_int_from_ini_file("RDPSettings", "disable_rdp_fonts", 0);
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->rdp_fonts_check_btn, rdp_fonts);
    gboolean rdp_themes = read_int_from_ini_file("RDPSettings", "disable_rdp_themes", 0);
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->rdp_themes_check_btn, rdp_themes);

    gchar *usb_devices = read_str_from_ini_file("RDPSettings", "usb_devices");
    usb_selector_widget_set_selected_usb_str(dialog_data->usb_selector_widget, usb_devices);
    g_free(usb_devices);

    gboolean use_rdp_file = read_int_from_ini_file("RDPSettings", "use_rdp_file", 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog_data->use_rdp_file_check_btn), use_rdp_file);
    gchar *rdp_settings_file = read_str_from_ini_file("RDPSettings", "rdp_settings_file");
    if (rdp_settings_file) {
        gtk_entry_set_text(GTK_ENTRY(dialog_data->rdp_file_name_entry), rdp_settings_file);
        g_free(rdp_settings_file);
    }
    gtk_widget_set_sensitive(dialog_data->btn_choose_rdp_file, use_rdp_file);
    gtk_widget_set_sensitive(dialog_data->rdp_file_name_entry, use_rdp_file);

    // Service settings
    gchar *cur_url = app_updater_get_windows_releases_url(dialog_data->p_remote_viewer->app_updater);
    gchar *windows_updates_url = read_str_from_ini_file_default("ServiceSettings",
                                                                "windows_updates_url", cur_url);
    free_memory_safely(&cur_url);
    if (windows_updates_url) {
        gtk_entry_set_text(GTK_ENTRY(dialog_data->windows_updates_url_entry), windows_updates_url);
        g_free(windows_updates_url);
    }

    gulong entry_handler_id = dialog_data->on_direct_connect_mode_check_btn_toggled_id;
    g_signal_handler_block(dialog_data->direct_connect_mode_check_btn, entry_handler_id); // to prevent callback
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->direct_connect_mode_check_btn, opt_manual_mode);
    g_signal_handler_unblock(dialog_data->direct_connect_mode_check_btn, entry_handler_id);
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
    gint port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dialog_data->port_spinbox));
    write_int_to_ini_file(paramToFileGrpoup, "port", port);
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
        const gchar *protocol_str = gtk_combo_box_get_active_id((GtkComboBox*)dialog_data->remote_protocol_combobox);
        VdiVmRemoteProtocol protocol = vdi_session_str_to_remote_protocol(protocol_str);
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

    if (dialog_data->rdp_h264_codec_combobox) {
        gchar *rdp_h264_codec_str =
                gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(dialog_data->rdp_h264_codec_combobox));
        write_str_to_ini_file("RDPSettings", "rdp_h264_codec", rdp_h264_codec_str);
        free_memory_safely(&rdp_h264_codec_str);
    }

    const gchar *shared_folders_str = gtk_entry_get_text(GTK_ENTRY(dialog_data->rdp_shared_folders_entry));
    // ???????????????????? ?????? ???????????? ???????? ?? ?????????????????????????? ??????????????????????????, ?????????????? ???? ???????????? ???????????? ????????????????
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
    write_str_to_ini_file("RDPSettings", "remote_app_program",
                          gtk_entry_get_text(GTK_ENTRY(dialog_data->remote_app_name_entry)));
    write_str_to_ini_file("RDPSettings", "remote_app_options",
                          gtk_entry_get_text(GTK_ENTRY(dialog_data->remote_app_options_entry)));

    gboolean is_rdp_network_assigned = gtk_toggle_button_get_active((GtkToggleButton *)
                                                                            dialog_data->rdp_network_check_btn);
    write_int_to_ini_file("RDPSettings", "is_rdp_network_assigned", is_rdp_network_assigned);
    const gchar *rdp_network_type =
            gtk_combo_box_get_active_id(GTK_COMBO_BOX(dialog_data->rdp_network_type_combobox));
    write_str_to_ini_file("RDPSettings", "rdp_network_type", rdp_network_type);

    gboolean rdp_decorations = gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->rdp_decorations_check_btn);
    write_int_to_ini_file("RDPSettings", "disable_rdp_decorations", rdp_decorations);
    gboolean rdp_fonts = gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->rdp_fonts_check_btn);
    write_int_to_ini_file("RDPSettings", "disable_rdp_fonts", rdp_fonts);
    gboolean rdp_themes = gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->rdp_themes_check_btn);
    write_int_to_ini_file("RDPSettings", "disable_rdp_themes", rdp_themes);

    gchar *usb_devices = usb_selector_widget_get_selected_usb_str(dialog_data->usb_selector_widget);
    if (usb_devices) {
        write_str_to_ini_file("RDPSettings", "usb_devices", usb_devices);
        g_free(usb_devices);
    } else {
        write_str_to_ini_file("RDPSettings", "usb_devices", "");
    }

    gboolean use_rdp_file = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog_data->use_rdp_file_check_btn));
    write_int_to_ini_file("RDPSettings", "use_rdp_file", use_rdp_file);
    const gchar *rdp_settings_file = gtk_entry_get_text(GTK_ENTRY(dialog_data->rdp_file_name_entry));
    gchar *rdp_settings_file_mod = replace_str(rdp_settings_file, "\\", "/");
    write_str_to_ini_file("RDPSettings", "rdp_settings_file", rdp_settings_file_mod);
    g_free(rdp_settings_file_mod);

    // Service settings
    write_str_to_ini_file("ServiceSettings", "windows_updates_url",
                          gtk_entry_get_text(GTK_ENTRY(dialog_data->windows_updates_url_entry)));

    opt_manual_mode = gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->direct_connect_mode_check_btn);
    write_int_to_ini_file("General", "opt_manual_mode", opt_manual_mode);
}

GtkResponseType remote_viewer_start_settings_dialog(RemoteViewer *p_remote_viewer,
                                                    ConnectSettingsData *p_conn_data, GtkWindow *parent)
{
    ConnectSettingsDialogData dialog_data;
    memset(&dialog_data, 0, sizeof(ConnectSettingsDialogData));

    dialog_data.p_remote_viewer = p_remote_viewer;
    dialog_data.p_conn_data = p_conn_data;
    dialog_data.dialog_window_response = GTK_RESPONSE_OK;

    // gui widgets
    dialog_data.builder = remote_viewer_util_load_ui("start_settings_form.ui");

    dialog_data.window = get_widget_from_builder(dialog_data.builder, "start-settings-window");
    // main buttons
    dialog_data.bt_cancel = get_widget_from_builder(dialog_data.builder, "btn_cancel");
    dialog_data.bt_ok = get_widget_from_builder(dialog_data.builder, "btn_ok");

    // main settings
    dialog_data.domain_entry = get_widget_from_builder(dialog_data.builder, "domain-entry");
    dialog_data.address_entry = get_widget_from_builder(dialog_data.builder, "connection-address-entry");
    dialog_data.port_spinbox = get_widget_from_builder(dialog_data.builder, "port_spinbox");
    dialog_data.ldap_check_btn = get_widget_from_builder(dialog_data.builder, "ldap-button");
    dialog_data.conn_to_prev_pool_checkbutton = get_widget_from_builder(dialog_data.builder, "connect-to-prev-button");
    dialog_data.save_password_checkbtn = get_widget_from_builder(dialog_data.builder, "save_password_btn");
    dialog_data.remote_protocol_combobox = get_widget_from_builder(dialog_data.builder, "remote_protocol_combobox");

    // spice settings
    dialog_data.client_cursor_visible_checkbutton =
            get_widget_from_builder(dialog_data.builder, "menu-show-client-cursor");

    // rdp settings
    dialog_data.rdp_image_pixel_format_combobox =
            get_widget_from_builder(dialog_data.builder, "rdp_image_pixel_format_combobox");
    dialog_data.rdp_fps_spin_btn = get_widget_from_builder(dialog_data.builder, "rdp_fps_spin_btn");

    dialog_data.is_h264_used_check_btn = get_widget_from_builder(dialog_data.builder, "is_h264_used_check_btn");
    dialog_data.rdp_h264_codec_combobox = get_widget_from_builder(dialog_data.builder, "rdp_h264_codec_combobox");
#ifdef _WIN32 // ???? Windows ???? ???????? ???????????? ????????????, ?????? ?????? ???????????????? ???????????? 1 (AVC420)
    gtk_widget_destroy(dialog_data.rdp_h264_codec_combobox);
    dialog_data.rdp_h264_codec_combobox = NULL;
#endif
    dialog_data.rdp_shared_folders_entry = get_widget_from_builder(dialog_data.builder, "rdp_shared_folders_entry");

    dialog_data.btn_add_remote_folder = get_widget_from_builder(dialog_data.builder, "btn_add_remote_folder");
    dialog_data.btn_remove_remote_folder = get_widget_from_builder(dialog_data.builder, "btn_remove_remote_folder");

    dialog_data.is_multimon_check_btn = get_widget_from_builder(dialog_data.builder, "is_multimon_check_btn");
    dialog_data.redirect_printers_check_btn =
            get_widget_from_builder(dialog_data.builder, "redirect_printers_check_btn");

    dialog_data.remote_app_check_btn = get_widget_from_builder(dialog_data.builder, "remote_app_check_btn");
    dialog_data.remote_app_name_entry = get_widget_from_builder(dialog_data.builder, "remote_app_name_entry");
    dialog_data.remote_app_options_entry = get_widget_from_builder(dialog_data.builder, "remote_app_options_entry");

    dialog_data.rdp_network_check_btn = get_widget_from_builder(dialog_data.builder, "rdp_network_check_btn");
    dialog_data.rdp_network_type_combobox = get_widget_from_builder(dialog_data.builder, "rdp_network_type_combobox");

    dialog_data.rdp_decorations_check_btn = get_widget_from_builder(dialog_data.builder, "rdp_decorations_check_btn");
    dialog_data.rdp_fonts_check_btn = get_widget_from_builder(dialog_data.builder, "rdp_fonts_check_btn");
    dialog_data.rdp_themes_check_btn = get_widget_from_builder(dialog_data.builder, "rdp_themes_check_btn");

    dialog_data.select_usb_btn = get_widget_from_builder(dialog_data.builder, "select_usb_btn");

    dialog_data.use_rdp_file_check_btn = get_widget_from_builder(dialog_data.builder, "use_rdp_file_check_btn");
    dialog_data.btn_choose_rdp_file = get_widget_from_builder(dialog_data.builder, "btn_choose_rdp_file");
    dialog_data.rdp_file_name_entry = get_widget_from_builder(dialog_data.builder, "rdp_file_name_entry");

    dialog_data.usb_selector_widget = usb_selector_widget_new();
    gtk_widget_destroy(dialog_data.usb_selector_widget->tk_address_entry);
    gtk_widget_destroy(dialog_data.usb_selector_widget->tk_address_header);
    usb_selector_widget_enable_auto_toggle(dialog_data.usb_selector_widget);

    // Service functions
    dialog_data.btn_archive_logs = get_widget_from_builder(dialog_data.builder, "btn_archive_logs");
    dialog_data.log_location_label = get_widget_from_builder(dialog_data.builder, "log_location_label");
    gtk_label_set_selectable(GTK_LABEL(dialog_data.log_location_label), TRUE);

    dialog_data.btn_get_app_updates = get_widget_from_builder(dialog_data.builder, "btn_get_app_updates");
    dialog_data.check_updates_spinner = get_widget_from_builder(dialog_data.builder, "check_updates_spinner");
    dialog_data.check_updates_label = get_widget_from_builder(dialog_data.builder, "check_updates_label");
    dialog_data.windows_updates_url_entry = get_widget_from_builder(dialog_data.builder, "windows_updates_url_entry");
    dialog_data.btn_open_doc = get_widget_from_builder(dialog_data.builder, "btn_open_doc");

    // ?? ???????? ???????????? ?????????? ?????????????????????? ?????????????? ???????????????????? ??????????.  Setup gui
    AppUpdater *app_updater = dialog_data.p_remote_viewer->app_updater;
    if (app_updater_is_getting_updates(app_updater)) {

        gchar *app_updater_cur_status_msg = app_updater_get_cur_status_msg(app_updater);
        gtk_label_set_text(GTK_LABEL(dialog_data.check_updates_label), app_updater_cur_status_msg);
        free_memory_safely(&app_updater_cur_status_msg);
        gtk_spinner_start((GtkSpinner *) dialog_data.check_updates_spinner);
        gtk_widget_set_sensitive(dialog_data.btn_get_app_updates, FALSE);
    }

    dialog_data.direct_connect_mode_check_btn = get_widget_from_builder(
            dialog_data.builder, "direct_connect_mode_check_btn");

    // Signals
    g_signal_connect_swapped(dialog_data.window, "delete-event", G_CALLBACK(window_deleted_cb), &dialog_data);
    g_signal_connect(dialog_data.bt_cancel, "clicked", G_CALLBACK(cancel_button_clicked_cb), &dialog_data);
    g_signal_connect(dialog_data.bt_ok, "clicked", G_CALLBACK(ok_button_clicked_cb), &dialog_data);
    g_signal_connect(dialog_data.is_h264_used_check_btn, "toggled",
                     G_CALLBACK(on_h264_used_check_btn_toggled), &dialog_data);
    g_signal_connect(dialog_data.rdp_network_check_btn, "toggled",
                     G_CALLBACK(on_rdp_network_check_btn_toggled), &dialog_data);
    g_signal_connect(dialog_data.btn_add_remote_folder, "clicked",
                     G_CALLBACK(btn_add_remote_folder_clicked_cb), &dialog_data);
    g_signal_connect(dialog_data.btn_remove_remote_folder, "clicked",
                     G_CALLBACK(btn_remove_remote_folder_clicked_cb), &dialog_data);
    g_signal_connect(dialog_data.use_rdp_file_check_btn, "toggled",
                     G_CALLBACK(on_use_rdp_file_check_btn_toggled), &dialog_data);
    g_signal_connect(dialog_data.btn_choose_rdp_file, "clicked",
                     G_CALLBACK(btn_btn_choose_rdp_file_clicked), &dialog_data);

    g_signal_connect(dialog_data.btn_archive_logs, "clicked",
                     G_CALLBACK(btn_archive_logs_clicked_cb), &dialog_data);
    g_signal_connect(dialog_data.remote_app_check_btn, "toggled", G_CALLBACK(on_remote_app_check_btn_toggled),
                     &dialog_data);
    g_signal_connect(dialog_data.btn_get_app_updates, "clicked", G_CALLBACK(btn_get_app_updates_clicked_cb),
                     &dialog_data);
    g_signal_connect(dialog_data.btn_open_doc, "clicked", G_CALLBACK(btn_open_doc_clicked_cb),
                     &dialog_data);
    g_signal_connect(dialog_data.select_usb_btn, "clicked", G_CALLBACK(on_select_usb_btn_clicked_cb), &dialog_data);
    dialog_data.on_direct_connect_mode_check_btn_toggled_id =
            g_signal_connect(dialog_data.direct_connect_mode_check_btn, "toggled",
                             G_CALLBACK(on_direct_connect_mode_check_btn_toggled), &dialog_data);

    gulong st_msg_hdle = g_signal_connect(p_remote_viewer->app_updater, "status-msg-changed",
                                          G_CALLBACK(on_app_updater_status_msg_changed),&dialog_data);
    gulong state_hdle = g_signal_connect(p_remote_viewer->app_updater, "state-changed",
                                         G_CALLBACK(on_app_updater_status_changed),&dialog_data);

    // read from file
    fill_p_conn_data_from_ini_file(p_conn_data);
    fill_connect_settings_gui(&dialog_data, p_conn_data);

    // show window
    gtk_window_set_transient_for(GTK_WINDOW(dialog_data.window), parent);
    gtk_window_set_position(GTK_WINDOW(dialog_data.window), GTK_WIN_POS_CENTER);
    gtk_widget_show_all(dialog_data.window);
    update_gui_according_to_connect_mode(&dialog_data, opt_manual_mode);
#ifndef  _WIN32
    gtk_widget_hide(dialog_data.windows_updates_url_entry);
#endif
    gtk_window_set_resizable(GTK_WINDOW(dialog_data.window), FALSE);

    create_loop_and_launch(&dialog_data.loop);

    // write to file if response is GTK_RESPONSE_OK
    save_data_to_ini_file(&dialog_data);

    // disconnect signals from external sources
    g_signal_handler_disconnect(p_remote_viewer->app_updater, st_msg_hdle);
    g_signal_handler_disconnect(p_remote_viewer->app_updater, state_hdle);

    // clear
    usb_selector_widget_free(dialog_data.usb_selector_widget);
    g_object_unref(dialog_data.builder);
    gtk_widget_destroy(dialog_data.window);

    return dialog_data.dialog_window_response;
}

void fill_p_conn_data_from_ini_file(ConnectSettingsData *p_conn_data)
{
    g_info("get_ini_file_name: %s", get_ini_file_name());

    // Main settings
    const gchar *group_name = get_cur_ini_param_group();
    // domain
    gchar *domain = read_str_from_ini_file(group_name, "domain");
    update_string_safely(&p_conn_data->domain, domain);
    free_memory_safely(&domain);
    // ip
    gchar *ip = read_str_from_ini_file(group_name, "ip");
    update_string_safely(&p_conn_data->ip, ip);
    free_memory_safely(&ip);
    // port
    p_conn_data->port = read_int_from_ini_file(group_name, "port", 443);
    // ldap
    p_conn_data->is_ldap = read_int_from_ini_file("RemoteViewerConnect", "is_ldap_btn_checked", 0);
    // Connect to prev pool
    p_conn_data->is_connect_to_prev_pool =
            read_int_from_ini_file("RemoteViewerConnect", "is_conn_to_prev_pool_btn_checked", 0);
    // pswd
    p_conn_data->to_save_pswd = read_int_from_ini_file(group_name, "to_save_pswd", 1);

    // remote protocol
    gint remote_protocol_type = read_int_from_ini_file("General", "cur_remote_protocol_index", VDI_SPICE_PROTOCOL);
    p_conn_data->remote_protocol_type = (VdiVmRemoteProtocol)remote_protocol_type;

    opt_manual_mode = read_int_from_ini_file("General", "opt_manual_mode", 0);
}
