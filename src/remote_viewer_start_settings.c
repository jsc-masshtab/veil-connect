/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include "remote_viewer_start_settings.h"
#include "additional_addresses_widget.h"


// D режиме по умолчанию подключение к VDI серверу. В ручном режиме прямое подключение к ВМ
// GUI в разных режимах имеет небольшое отличие
static void update_gui_according_to_app_mode(ConnectSettingsDialog *dialog_data,
                                             GlobalAppMode global_app_mode)
{
    switch (global_app_mode) {
        case GLOBAL_APP_MODE_VDI: {
            gtk_widget_set_sensitive(dialog_data->btn_add_addresses, TRUE);
            gtk_widget_set_sensitive(dialog_data->conn_to_prev_pool_checkbutton, TRUE);
            gtk_widget_set_sensitive(dialog_data->redirect_time_zone_check_btn, TRUE);
            gtk_widget_set_visible(dialog_data->remote_protocol_combobox, FALSE);
            gtk_widget_set_visible(dialog_data->btn_choose_rdp_file, FALSE);
            gtk_widget_set_visible(dialog_data->rdp_file_name_entry, FALSE);
            gtk_widget_set_visible(dialog_data->use_rdp_file_check_btn, FALSE);
            break;
        }
        case GLOBAL_APP_MODE_DIRECT: {
            gtk_widget_set_sensitive(dialog_data->btn_add_addresses, FALSE);
            gtk_widget_set_sensitive(dialog_data->conn_to_prev_pool_checkbutton, FALSE);
            gtk_widget_set_sensitive(dialog_data->redirect_time_zone_check_btn, FALSE);
            gtk_widget_set_visible(dialog_data->remote_protocol_combobox, TRUE);
            gtk_widget_set_visible(dialog_data->btn_choose_rdp_file, TRUE);
            gtk_widget_set_visible(dialog_data->rdp_file_name_entry, TRUE);
            gtk_widget_set_visible(dialog_data->use_rdp_file_check_btn, TRUE);
            break;
        }
        case GLOBAL_APP_MODE_CONTROLLER: {
            gtk_widget_set_sensitive(dialog_data->btn_add_addresses, FALSE);
            gtk_widget_set_sensitive(dialog_data->conn_to_prev_pool_checkbutton, FALSE);
            gtk_widget_set_sensitive(dialog_data->redirect_time_zone_check_btn, FALSE);
            gtk_widget_set_visible(dialog_data->remote_protocol_combobox, FALSE);
            gtk_widget_set_visible(dialog_data->btn_choose_rdp_file, FALSE);
            gtk_widget_set_visible(dialog_data->rdp_file_name_entry, FALSE);
            gtk_widget_set_visible(dialog_data->use_rdp_file_check_btn, FALSE);
            break;
        }
        default:
            break;
    }
}

static gboolean
window_deleted_cb(ConnectSettingsDialog *dialog_data)
{
    dialog_data->dialog_window_response = GTK_RESPONSE_CLOSE;
    shutdown_loop(dialog_data->loop);
    return TRUE;
}

static void
cancel_button_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialog *dialog_data)
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

/*Return true if there were no errors otherwise - false  */
// У виджета есть понятие id имя, котоое уникально и есть понятие имя виджета, которое используется для css.
static gboolean
check_parameters(ConnectSettingsData *p_conn_data G_GNUC_UNUSED, ConnectSettingsDialog *dialog_data)
{
    gboolean is_ok = TRUE;
    const gchar *pattern = "^$|[а-яА-ЯёЁa-zA-Z0-9]+[а-яА-ЯёЁa-zA-Z0-9.\\-_+ ]*$";

    // check domain name and set
    const gchar *domain_gui_str = gtk_entry_get_text(GTK_ENTRY(dialog_data->domain_entry));
    gboolean is_matched = g_regex_match_simple(pattern, domain_gui_str,G_REGEX_CASELESS,G_REGEX_MATCH_ANCHORED);

    if (is_matched) {
        gtk_widget_set_name(dialog_data->domain_entry, "domain-entry");
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
    } else {
        make_entry_red(dialog_data->address_entry);
        is_ok = FALSE;
    }

    return is_ok;
}

static void
on_vid_comp_used_check_btn_toggled(GtkToggleButton *check_btn, gpointer user_data)
{
    ConnectSettingsDialog *dialog_data = (ConnectSettingsDialog *)user_data;
    if (dialog_data->rdp_codec_combobox) {
        gboolean is_vid_comp_used = gtk_toggle_button_get_active(check_btn);
        gtk_widget_set_sensitive(dialog_data->rdp_codec_combobox, is_vid_comp_used);
    }
}

static void
on_rdp_sec_protocol_check_btn_toggled(GtkToggleButton *check_btn, gpointer user_data)
{
    ConnectSettingsDialog *dialog_data = (ConnectSettingsDialog *)user_data;
    gboolean is_check_btn_toggled = gtk_toggle_button_get_active(check_btn);
    gtk_widget_set_sensitive(dialog_data->sec_type_combobox, is_check_btn_toggled);
}

static void
on_rdp_network_check_btn_toggled(GtkToggleButton *rdp_network_check_btn, gpointer user_data)
{
    ConnectSettingsDialog *dialog_data = (ConnectSettingsDialog *)user_data;
    gboolean is_rdp_network_check_btn_toggled = gtk_toggle_button_get_active(rdp_network_check_btn);
    gtk_widget_set_sensitive(dialog_data->rdp_network_type_combobox, is_rdp_network_check_btn_toggled);
}

//static void
//on_remote_app_check_btn_toggled(GtkToggleButton *remote_app_check_btn, gpointer user_data)
//{
//    ConnectSettingsDialog *dialog_data = (ConnectSettingsDialog *)user_data;
//
//    gboolean is_remote_app_check_btn_toggled = gtk_toggle_button_get_active(remote_app_check_btn);
//    gtk_widget_set_sensitive(dialog_data->remote_app_name_entry, is_remote_app_check_btn_toggled);
//    gtk_widget_set_sensitive(dialog_data->remote_app_options_entry, is_remote_app_check_btn_toggled);
//}

/*
static void
shared_folders_icon_released(GtkEntry            *entry,
                               GtkEntryIconPosition icon_pos,
                               GdkEvent            *event,
                               gpointer             user_data)
{
    ConnectSettingsDialog *dialog_data = (ConnectSettingsDialog *)user_data;


    //folders_selector_widget_get_folders(GTK_WINDOW(dialog_data->window));


    GtkWidget *popup_window;
    popup_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (popup_window), "Pop Up window");

    gtk_window_set_transient_for(GTK_WINDOW (popup_window), GTK_WINDOW(dialog_data->window));
    gtk_window_set_position (GTK_WINDOW (popup_window),GTK_WIN_POS_CENTER);
    gtk_widget_show_all (popup_window);
}*/

static void
btn_add_remote_folder_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialog *dialog_data)
{
    // get folder name
    GtkWidget *dialog = gtk_file_chooser_dialog_new (_("Open File"),
                                                     GTK_WINDOW(dialog_data->window),
                                                     GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                     _("Cancel"),
                                                     GTK_RESPONSE_CANCEL,
                                                     _("Choose"),
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
btn_remove_remote_folder_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialog *dialog_data)
{
    // clear rdp_shared_folders_entry
    gtk_entry_set_text(GTK_ENTRY(dialog_data->rdp_shared_folders_entry), "");
}

static void
on_use_rdp_file_check_btn_toggled(GtkToggleButton *button, ConnectSettingsDialog *dialog_data)
{
    gboolean use_rdp_file = gtk_toggle_button_get_active(button);
    gtk_widget_set_sensitive(dialog_data->btn_choose_rdp_file, use_rdp_file);
    gtk_widget_set_sensitive(dialog_data->rdp_file_name_entry, use_rdp_file);
}

static void
on_use_rd_gateway_check_btn_toggled(GtkToggleButton *button, ConnectSettingsDialog *dialog_data)
{
    gboolean is_btn_active = gtk_toggle_button_get_active(button);
    gtk_widget_set_sensitive(dialog_data->gateway_address_entry, is_btn_active);
}

static void
btn_choose_rdp_file_clicked(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialog *dialog_data)
{
    GtkWidget *dialog = gtk_file_chooser_dialog_new (_("Open File"),
                                                     GTK_WINDOW(dialog_data->window),
                                                     GTK_FILE_CHOOSER_ACTION_OPEN,
                                                     _("Cancel"),
                                                     GTK_RESPONSE_CANCEL,
                                                     _("Choose"),
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
on_conn_type_check_btn_toggled(GtkToggleButton *button, ConnectSettingsDialog *dialog_data)
{
    gboolean is_toggled = gtk_toggle_button_get_active(button);
    gtk_widget_set_sensitive(dialog_data->x2go_conn_type_combobox, is_toggled);
}

static void
btn_archive_logs_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialog *dialog_data)
{
    gchar *log_dir = get_log_dir_path();

#ifdef G_OS_UNIX
    gchar *compress_cmd = g_strdup_printf("tar -czvf log.tar.gz %s", log_dir);

    int res = system(compress_cmd);

    // show log dir on gui
    gchar *cur_dir = g_get_current_dir();
    gtk_label_set_text(GTK_LABEL(dialog_data->log_location_label), cur_dir);
    free_memory_safely(&cur_dir);
#elif _WIN32
    gchar *app_data_dir = get_windows_app_data_location();
    gchar *compress_cmd = g_strdup_printf("tar.exe --exclude=%s -a -c -f \"%s\\log.zip\" \"%s\"",
                                     veil_logger_get_log_start_date(), app_data_dir, log_dir);

    //gchar *compress_cmd = g_strdup_printf(
    //        "powershell Compress-Archive -Force -Path \"%s\" -DestinationPath \"%s\\log.zip\"",
    //        log_dir,
    //        app_data_dir);

    int res = system(compress_cmd); // powershell Compress-Archive -Force po po.zip

    gtk_label_set_text(GTK_LABEL(dialog_data->log_location_label), app_data_dir);
    g_free(app_data_dir);
#endif
    (void)res;

    g_free(compress_cmd);
    g_free(log_dir);
}

static void
on_app_updater_status_msg_changed(gpointer data G_GNUC_UNUSED, const gchar *msg,
                                  ConnectSettingsDialog *dialog_data)
{
    g_info("%s", (const char *)__func__);
    gtk_label_set_text(GTK_LABEL(dialog_data->check_updates_label), msg);
}

static void
on_app_updater_status_changed(gpointer data G_GNUC_UNUSED,
                              int isworking,
                              ConnectSettingsDialog *dialog_data)
{
    g_info("%s isworking: %i", (const char *)__func__, isworking);
    if (isworking) {
        gtk_spinner_start((GtkSpinner *) dialog_data->check_updates_spinner);
    }
    else {
        gtk_spinner_stop((GtkSpinner *) dialog_data->check_updates_spinner);

        // show the last output if there was an error
        AppUpdater *app_updater = dialog_data->p_app_updater;
        gchar *last_process_output = app_updater_get_last_process_output(app_updater);

        if (app_updater->_last_exit_status != 0 && last_process_output ) {

            GtkBuilder *builder = remote_viewer_util_load_ui("text_msg_form.glade");
            GtkWidget *text_msg_dialog = get_widget_from_builder(builder, "text_msg_dialog");
            gtk_dialog_add_button(GTK_DIALOG(text_msg_dialog), _("Ok"), GTK_RESPONSE_OK);

            GtkWidget *header_text_label = get_widget_from_builder(builder, "header_text_label");
            // Процесс обновления завершился с ошибками
            gtk_label_set_text(GTK_LABEL(header_text_label), _("The update process ended with errors"));

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

// По нажатию квавиши enter на password_entry скрываем окно ввода пароля и луп диалога превывается
static void
on_password_entry_activated(GtkEntry *entry G_GNUC_UNUSED, GtkWidget *ask_pass_dialog)
{
    gtk_widget_hide(ask_pass_dialog);
}

static void
btn_get_app_updates_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialog *dialog_data)
{
    g_info("%s", (const char *)__func__);
    AppUpdater *app_updater = dialog_data->p_app_updater;
    if (app_updater == NULL || app_updater_is_getting_updates(app_updater))
        return;

#ifdef __linux__
    // get sudo pass (Нужно показать виджет для ввода пароля sudo)
    GtkBuilder *builder = remote_viewer_util_load_ui("ask_pass_form.glade");
    GtkWidget *ask_pass_dialog = get_widget_from_builder(builder, "ask_pass_dialog");
    gtk_dialog_add_button(GTK_DIALOG(ask_pass_dialog), _("Ok"), GTK_RESPONSE_OK);
    gtk_dialog_add_button(GTK_DIALOG(ask_pass_dialog), _("Cancel"), GTK_RESPONSE_CANCEL);
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
        // "Необходимо ввести пароль"
        gtk_label_set_text(GTK_LABEL(dialog_data->check_updates_label), _("Password required"));
        return;
    }
    free_memory_safely(&app_updater_admin_password);

    // start updating routine
    app_updater_execute_task_get_updates(app_updater);
#elif _WIN32
    app_updater_execute_task_get_updates(app_updater);
#else
    gtk_label_set_text(GTK_LABEL(dialog_data->check_updates_label), "Не реализовано для текущей ОС");
#endif
}

static void
btn_open_doc_clicked_cb()
{
    gtk_show_uri_on_window(NULL, VEIL_CONNECT_DOC_SITE, GDK_CURRENT_TIME, NULL);
}

static void
on_select_usb_btn_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialog *dialog_data)
{
    usb_selector_widget_show_and_start_loop(dialog_data->usb_selector_widget, GTK_WINDOW(dialog_data->window));
}

static void
btn_show_monitor_config_clicked(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialog *dialog_data)
{
    util_show_monitor_config_window(GTK_WINDOW(dialog_data->window), gdk_display_get_default());
}

static void
btn_show_usb_filter_tooltip_spice_clicked(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialog *dialog_data) {
    show_msg_box_dialog(GTK_WINDOW(dialog_data->window),
                        _("Filter string format:\n\n"
                        "Filter consists of one or more rules. Where each rule has the form of:\n"
                        "\n"
                        "class ,vendor ,product ,version ,allow\n"
                        "\n"
                        "Use -1 for class /vendor /product /version to accept any value.\n"
                        "\n"
                        "And the rules themselves are concatenated like this:\n"
                        "\n"
                        "rule1 |rule2 |rule3\n"
                        "\n"
                        "The default setting filters out HID (class 0x03) USB devices from auto connect and auto"
                        " connects anything else. Note the explicit allow rule at the end, this is necessary since"
                        " by default all devices without a matching filter rule will not auto-connect.\n"
                        "\n"
                        "Filter strings in this format can be easily created with the RHEV-M USB filter editor tool.\n"
                        "\n"
                        "Default value: \"0x03,-1,-1,-1,0|-1,-1,-1,-1,1\""));
}

static void
btn_add_addresses_clicked(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialog *dialog_data)
{
    additional_addresses_widget_show(&get_vdi_session_static()->additional_addresses_list,
                                     &get_vdi_session_static()->multi_address_mode,
            dialog_data->p_conn_data, GTK_WINDOW(dialog_data->window));
}

static void
on_app_mode_combobox_changed(GtkComboBox *widget G_GNUC_UNUSED, gpointer user_data)
{
    g_info("%s", (const char*)__func__);
    ConnectSettingsDialog *dialog_data = (ConnectSettingsDialog *)user_data;
    const gchar *mode_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(dialog_data->app_mode_combobox));
    GlobalAppMode global_app_mode = (GlobalAppMode)atoi(mode_id);
    update_gui_according_to_app_mode(dialog_data, global_app_mode);

    // Устанавливаем дефолтный порт
    if (global_app_mode == GLOBAL_APP_MODE_DIRECT)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog_data->port_spinbox), 3389);
    else
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog_data->port_spinbox), 443);
}

static void
fill_gui(ConnectSettingsDialog *dialog_data)
{
    ConnectSettingsData *p_conn_data = dialog_data->p_conn_data;

    update_gui_according_to_app_mode(dialog_data, p_conn_data->global_app_mode);

    // Service settings
    gtk_entry_set_text(GTK_ENTRY(dialog_data->windows_updates_url_entry), p_conn_data->windows_updates_url);

    gulong handler_id = dialog_data->on_app_mode_combobox_changed_id;
    g_signal_handler_block(dialog_data->app_mode_combobox, handler_id); // to prevent callback

    g_autofree gchar *global_app_mode_str = NULL;
    global_app_mode_str = g_strdup_printf("%i", (int)p_conn_data->global_app_mode);
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(dialog_data->app_mode_combobox), global_app_mode_str);

    g_signal_handler_unblock(dialog_data->app_mode_combobox, handler_id);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog_data->vm_await_time_spinbox), p_conn_data->vm_await_timeout);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog_data->unique_app_check_btn), p_conn_data->unique_app);

    /// General settings
    // domain
    if (p_conn_data->domain)
        gtk_entry_set_text(GTK_ENTRY(dialog_data->domain_entry), p_conn_data->domain);

    switch (p_conn_data->global_app_mode) {
        case GLOBAL_APP_MODE_VDI: {
            if (vdi_session_get_vdi_ip())
                gtk_entry_set_text(GTK_ENTRY(dialog_data->address_entry), vdi_session_get_vdi_ip());
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog_data->port_spinbox),
                                      vdi_session_get_vdi_port());
            break;
        }
        case GLOBAL_APP_MODE_DIRECT: {
            if (p_conn_data->ip) {
                gtk_entry_set_text(GTK_ENTRY(dialog_data->address_entry), p_conn_data->ip);
            }
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog_data->port_spinbox), p_conn_data->port);
            break;
        }
        case GLOBAL_APP_MODE_CONTROLLER: {
            if (get_controller_session_static()->address.string)
                gtk_entry_set_text(GTK_ENTRY(dialog_data->address_entry),
                        get_controller_session_static()->address.string);
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog_data->port_spinbox),
                    get_controller_session_static()->port);
            break;
        }
    }

    // Connect to prev pool
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->conn_to_prev_pool_checkbutton,
                                 p_conn_data->connect_to_prev_pool);
    // pswd
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->save_password_checkbtn, p_conn_data->to_save_pswd);

    if (gtk_widget_get_visible(dialog_data->remote_protocol_combobox)) {
        // Текст и id совпадают
        const gchar *protocol_str = util_remote_protocol_to_str(p_conn_data->protocol_in_direct_app_mode);
        gtk_combo_box_set_active_id((GtkComboBox *) dialog_data->remote_protocol_combobox, protocol_str);
    }

    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->redirect_time_zone_check_btn,
            p_conn_data->redirect_time_zone);

    /// Spice settings
    gtk_toggle_button_set_active((GtkToggleButton*)dialog_data->client_cursor_visible_checkbutton,
                                 p_conn_data->spice_settings.is_spice_client_cursor_visible);
    gtk_toggle_button_set_active((GtkToggleButton*)dialog_data->spice_full_screen_check_btn,
                                 p_conn_data->spice_settings.full_screen);
    if (p_conn_data->spice_settings.monitor_mapping) {
        gtk_entry_set_text(GTK_ENTRY(dialog_data->spice_monitor_mapping_entry),
                           p_conn_data->spice_settings.monitor_mapping);
    }
    if (p_conn_data->spice_settings.usb_auto_connect_filter)
        gtk_entry_set_text(GTK_ENTRY(dialog_data->spice_usb_auto_connect_filter_entry),
                           p_conn_data->spice_settings.usb_auto_connect_filter);
    if (p_conn_data->spice_settings.usb_redirect_on_connect)
        gtk_entry_set_text(GTK_ENTRY(dialog_data->spice_usb_redirect_on_connect_entry),
                           p_conn_data->spice_settings.usb_redirect_on_connect);

    /// RDP settings
    UINT32 freerdp_pix_index = (g_strcmp0(p_conn_data->rdp_settings.rdp_pixel_format_str, "BGRA32") == 0) ? 1 : 0;
    gtk_combo_box_set_active((GtkComboBox*)dialog_data->rdp_image_pixel_format_combobox, freerdp_pix_index);

    UINT32 rdp_fps = CLAMP(p_conn_data->rdp_settings.rdp_fps, 1, 60);
    gtk_spin_button_set_value((GtkSpinButton*) dialog_data->rdp_fps_spin_btn, (gdouble)rdp_fps);

    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->is_rdp_vid_comp_used_check_btn,
                                 p_conn_data->rdp_settings.is_rdp_vid_comp_used);

    gtk_widget_set_sensitive(dialog_data->rdp_codec_combobox, p_conn_data->rdp_settings.is_rdp_vid_comp_used);
    if (p_conn_data->rdp_settings.is_rdp_vid_comp_used)
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(dialog_data->rdp_codec_combobox),
                p_conn_data->rdp_settings.rdp_vid_comp_codec);

    if (p_conn_data->rdp_settings.shared_folders_str)
        gtk_entry_set_text(GTK_ENTRY(dialog_data->rdp_shared_folders_entry),
                p_conn_data->rdp_settings.shared_folders_str);

    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->is_multimon_check_btn,
            p_conn_data->rdp_settings.is_multimon);
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->dynamic_resolution_check_btn,
                                 p_conn_data->rdp_settings.dynamic_resolution_enabled);
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->rdp_full_screen_check_btn,
                                 p_conn_data->rdp_settings.full_screen);
    if (p_conn_data->rdp_settings.selectedmonitors)
        gtk_entry_set_text(GTK_ENTRY(dialog_data->rdp_selected_monitors_entry),
                p_conn_data->rdp_settings.selectedmonitors);

    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->redirect_printers_check_btn,
            p_conn_data->rdp_settings.redirectprinters);
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->rdp_redirect_microphone_check_btn,
                                 p_conn_data->rdp_settings.redirect_microphone);

    //gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->remote_app_check_btn,
    //        p_conn_data->rdp_settings.is_remote_app);
    //if (p_conn_data->rdp_settings.remote_app_program)
    //    gtk_entry_set_text(GTK_ENTRY(dialog_data->remote_app_name_entry),
    //            p_conn_data->rdp_settings.remote_app_program);
    //if (p_conn_data->rdp_settings.remote_app_options) {
    //    gtk_entry_set_text(GTK_ENTRY(dialog_data->remote_app_options_entry),
    //            p_conn_data->rdp_settings.remote_app_options);
    //}
    gtk_combo_box_set_active(GTK_COMBO_BOX(dialog_data->remote_app_format_combobox),
                             (gint)p_conn_data->rdp_settings.remote_application_format);

    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->rdp_sec_protocol_check_btn,
            p_conn_data->rdp_settings.is_sec_protocol_assigned);
    gtk_widget_set_sensitive(dialog_data->sec_type_combobox, p_conn_data->rdp_settings.is_sec_protocol_assigned);

    if (p_conn_data->rdp_settings.is_sec_protocol_assigned && p_conn_data->rdp_settings.sec_protocol_type)
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(dialog_data->sec_type_combobox),
                p_conn_data->rdp_settings.sec_protocol_type);

    gboolean is_rdp_network_assigned = p_conn_data->rdp_settings.is_rdp_network_assigned;
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->rdp_network_check_btn, is_rdp_network_assigned);
    gtk_widget_set_sensitive(dialog_data->rdp_network_type_combobox, is_rdp_network_assigned);
    if (is_rdp_network_assigned && p_conn_data->rdp_settings.rdp_network_type) {
        gtk_combo_box_set_active_id(GTK_COMBO_BOX(dialog_data->rdp_network_type_combobox),
                p_conn_data->rdp_settings.rdp_network_type);
    }

    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->rdp_decorations_check_btn,
            p_conn_data->rdp_settings.disable_rdp_decorations);
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->rdp_fonts_check_btn,
            p_conn_data->rdp_settings.disable_rdp_fonts);
    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->rdp_themes_check_btn,
            p_conn_data->rdp_settings.disable_rdp_themes);

    usb_selector_widget_set_selected_usb_str(dialog_data->usb_selector_widget, p_conn_data->rdp_settings.usb_devices);

    gboolean use_rdp_file = p_conn_data->rdp_settings.use_rdp_file;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog_data->use_rdp_file_check_btn), use_rdp_file);
    if (p_conn_data->rdp_settings.rdp_settings_file)
        gtk_entry_set_text(GTK_ENTRY(dialog_data->rdp_file_name_entry), p_conn_data->rdp_settings.rdp_settings_file);
    gtk_widget_set_sensitive(dialog_data->btn_choose_rdp_file, use_rdp_file);
    gtk_widget_set_sensitive(dialog_data->rdp_file_name_entry, use_rdp_file);

    gboolean use_gateway = p_conn_data->rdp_settings.use_gateway;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog_data->use_rd_gateway_check_btn), use_gateway);
    if (p_conn_data->rdp_settings.gateway_address)
        gtk_entry_set_text(GTK_ENTRY(dialog_data->gateway_address_entry), p_conn_data->rdp_settings.gateway_address);

    gtk_toggle_button_set_active((GtkToggleButton *)dialog_data->rdp_log_debug_info_check_btn,
                                 p_conn_data->rdp_settings.freerdp_debug_log_enabled);

    // X2Go Settings
    gtk_combo_box_set_active(GTK_COMBO_BOX(dialog_data->x2go_app_combobox),
                             (gint)p_conn_data->x2Go_settings.app_type);

    gtk_combo_box_set_active_id(GTK_COMBO_BOX(dialog_data->x2go_session_type_combobox),
            p_conn_data->x2Go_settings.x2go_session_type);

    gboolean conn_type_assigned = p_conn_data->x2Go_settings.conn_type_assigned;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog_data->x2go_conn_type_check_btn), conn_type_assigned);
    gtk_widget_set_sensitive(dialog_data->x2go_conn_type_combobox, conn_type_assigned);
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(dialog_data->x2go_conn_type_combobox),
            p_conn_data->x2Go_settings.x2go_conn_type);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog_data->x2go_full_screen_check_btn),
            p_conn_data->x2Go_settings.full_screen);

    gtk_combo_box_set_active_id(GTK_COMBO_BOX(dialog_data->x2go_compress_method_combobox),
                                p_conn_data->x2Go_settings.compress_method);

    // Loudplay
    gtk_entry_set_text(GTK_ENTRY(dialog_data->loudplay_client_path_entry),
                       p_conn_data->loudplay_config->loudplay_client_path);

    if (dialog_data->loudplay_config_gui)
        gtk_widget_destroy(dialog_data->loudplay_config_gui);
    dialog_data->loudplay_config_gui = gobject_gui_generate_gui(G_OBJECT(p_conn_data->loudplay_config));
    gtk_box_pack_end(GTK_BOX(dialog_data->loudplay_box), dialog_data->loudplay_config_gui, TRUE, TRUE, 6);
    gtk_widget_show_all(dialog_data->loudplay_config_gui);
}

static void
take_from_gui(ConnectSettingsDialog *dialog_data)
{
    ConnectSettingsData *conn_data = dialog_data->p_conn_data;

    // Service settings
    update_string_safely(&conn_data->windows_updates_url,
                         gtk_entry_get_text(GTK_ENTRY(dialog_data->windows_updates_url_entry)));
    conn_data->global_app_mode = (GlobalAppMode)atoi(
            gtk_combo_box_get_active_id(GTK_COMBO_BOX(dialog_data->app_mode_combobox)));
    conn_data->vm_await_timeout =
            gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dialog_data->vm_await_time_spinbox));
    conn_data->unique_app = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog_data->unique_app_check_btn));
    // Main
    // domain
    update_string_safely(&conn_data->domain, gtk_entry_get_text(GTK_ENTRY(dialog_data->domain_entry)));
    const gchar *ip = gtk_entry_get_text(GTK_ENTRY(dialog_data->address_entry));
    int port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dialog_data->port_spinbox));

    switch (conn_data->global_app_mode) {
        case GLOBAL_APP_MODE_VDI: {
            vdi_session_set_conn_data(ip, port);
            break;
        }
        case GLOBAL_APP_MODE_DIRECT: {
            update_string_safely(&conn_data->ip, ip);
            conn_data->port = port;
            break;
        }
        case GLOBAL_APP_MODE_CONTROLLER: {
            controller_session_set_conn_data(ip, port);
            break;
        }
    }

    // prev pool
    conn_data->connect_to_prev_pool =
            gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->conn_to_prev_pool_checkbutton);
    // pswd
    conn_data->to_save_pswd = gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->save_password_checkbtn);

    if (gtk_widget_get_visible(dialog_data->remote_protocol_combobox)) {
        const gchar *protocol_str = gtk_combo_box_get_active_id((GtkComboBox*)dialog_data->remote_protocol_combobox);
        conn_data->protocol_in_direct_app_mode = util_str_to_remote_protocol(protocol_str);
    }

    conn_data->redirect_time_zone = gtk_toggle_button_get_active(
            (GtkToggleButton *)dialog_data->redirect_time_zone_check_btn);

    /// Spice debug cursor enabling
    conn_data->spice_settings.is_spice_client_cursor_visible =
            gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->client_cursor_visible_checkbutton);
    conn_data->spice_settings.full_screen =
            gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->spice_full_screen_check_btn);
    update_string_safely(&conn_data->spice_settings.monitor_mapping,
                         gtk_entry_get_text(GTK_ENTRY(dialog_data->spice_monitor_mapping_entry)));
    update_string_safely(&conn_data->spice_settings.usb_auto_connect_filter,
                         gtk_entry_get_text(GTK_ENTRY(dialog_data->spice_usb_auto_connect_filter_entry)));
    update_string_safely(&conn_data->spice_settings.usb_redirect_on_connect,
                         gtk_entry_get_text(GTK_ENTRY(dialog_data->spice_usb_redirect_on_connect_entry)));

    /// RDP settings
    update_string_safely(&conn_data->rdp_settings.rdp_pixel_format_str,
            gtk_combo_box_text_get_active_text((GtkComboBoxText *)dialog_data->rdp_image_pixel_format_combobox));

    conn_data->rdp_settings.rdp_fps = (gint)gtk_spin_button_get_value((GtkSpinButton *)dialog_data->rdp_fps_spin_btn);

    conn_data->rdp_settings.is_rdp_vid_comp_used = gtk_toggle_button_get_active(
            (GtkToggleButton *)dialog_data->is_rdp_vid_comp_used_check_btn);

    if (dialog_data->rdp_codec_combobox) {
        const gchar *rdp_vid_comp_codec_str =
                gtk_combo_box_get_active_id(GTK_COMBO_BOX(dialog_data->rdp_codec_combobox));
        update_string_safely(&conn_data->rdp_settings.rdp_vid_comp_codec, rdp_vid_comp_codec_str);
    }

    const gchar *shared_folders_str = gtk_entry_get_text(GTK_ENTRY(dialog_data->rdp_shared_folders_entry));
    // Пользовать мог ввести путь с виндусятскими разделителями, поэтому на всякий случай заменяем
    gchar *folder_name = replace_str(shared_folders_str, "\\", "/");
    update_string_safely(&conn_data->rdp_settings.shared_folders_str, folder_name);
    free_memory_safely(&folder_name);

    conn_data->rdp_settings.is_multimon = gtk_toggle_button_get_active(
            (GtkToggleButton *)dialog_data->is_multimon_check_btn);
    conn_data->rdp_settings.dynamic_resolution_enabled = gtk_toggle_button_get_active(
            (GtkToggleButton *)dialog_data->dynamic_resolution_check_btn);
    conn_data->rdp_settings.full_screen = gtk_toggle_button_get_active(
            (GtkToggleButton *)dialog_data->rdp_full_screen_check_btn);
    update_string_safely(&conn_data->rdp_settings.selectedmonitors,
                         gtk_entry_get_text(GTK_ENTRY(dialog_data->rdp_selected_monitors_entry)));

    conn_data->rdp_settings.redirectprinters = gtk_toggle_button_get_active(
            (GtkToggleButton *)dialog_data->redirect_printers_check_btn);
    conn_data->rdp_settings.redirect_microphone = gtk_toggle_button_get_active(
            (GtkToggleButton *)dialog_data->rdp_redirect_microphone_check_btn);

    //conn_data->rdp_settings.is_remote_app =
    //        gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->remote_app_check_btn);
    //update_string_safely(&conn_data->rdp_settings.remote_app_program,
    //                      gtk_entry_get_text(GTK_ENTRY(dialog_data->remote_app_name_entry)));
    //update_string_safely(&conn_data->rdp_settings.remote_app_options,
    //                      gtk_entry_get_text(GTK_ENTRY(dialog_data->remote_app_options_entry)));
    //
    conn_data->rdp_settings.remote_application_format = (RemoteApplicationFormat)gtk_combo_box_get_active(
            GTK_COMBO_BOX(dialog_data->remote_app_format_combobox));

    conn_data->rdp_settings.is_sec_protocol_assigned = gtk_toggle_button_get_active((GtkToggleButton *)
                                                                            dialog_data->rdp_sec_protocol_check_btn);
    const gchar *sec_protocol_type =
            gtk_combo_box_get_active_id(GTK_COMBO_BOX(dialog_data->sec_type_combobox));
    update_string_safely(&conn_data->rdp_settings.sec_protocol_type, sec_protocol_type);
    //
    conn_data->rdp_settings.is_rdp_network_assigned = gtk_toggle_button_get_active((GtkToggleButton *)
                                                               dialog_data->rdp_network_check_btn);
    const gchar *rdp_network_type =
            gtk_combo_box_get_active_id(GTK_COMBO_BOX(dialog_data->rdp_network_type_combobox));
    update_string_safely(&conn_data->rdp_settings.rdp_network_type, rdp_network_type);

    conn_data->rdp_settings.disable_rdp_decorations =
            gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->rdp_decorations_check_btn);
    conn_data->rdp_settings.disable_rdp_fonts =
            gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->rdp_fonts_check_btn);
    conn_data->rdp_settings.disable_rdp_themes =
            gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->rdp_themes_check_btn);

    gchar *usb_devices = usb_selector_widget_get_selected_usb_str(dialog_data->usb_selector_widget);
    if (usb_devices) {
        update_string_safely(&conn_data->rdp_settings.usb_devices, usb_devices);
        g_free(usb_devices);
    } else {
        update_string_safely(&conn_data->rdp_settings.usb_devices, "");
    }

    conn_data->rdp_settings.use_rdp_file =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog_data->use_rdp_file_check_btn));
    const gchar *rdp_settings_file = gtk_entry_get_text(GTK_ENTRY(dialog_data->rdp_file_name_entry));
    gchar *rdp_settings_file_mod = replace_str(rdp_settings_file, "\\", "/");
    update_string_safely(&conn_data->rdp_settings.rdp_settings_file, rdp_settings_file_mod);
    g_free(rdp_settings_file_mod);

    conn_data->rdp_settings.use_gateway =
            gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->use_rd_gateway_check_btn);
    const gchar *gateway_address = gtk_entry_get_text(GTK_ENTRY(dialog_data->gateway_address_entry));
    update_string_safely(&conn_data->rdp_settings.gateway_address, gateway_address);

    conn_data->rdp_settings.freerdp_debug_log_enabled =
            gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->rdp_log_debug_info_check_btn);

    // X2Go settings
    conn_data->x2Go_settings.app_type = (X2goApplication)gtk_combo_box_get_active(
            GTK_COMBO_BOX(dialog_data->x2go_app_combobox));

    const gchar *x2go_session_type =
            gtk_combo_box_get_active_id(GTK_COMBO_BOX(dialog_data->x2go_session_type_combobox));
    update_string_safely(&conn_data->x2Go_settings.x2go_session_type, x2go_session_type);

    conn_data->x2Go_settings.conn_type_assigned = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            dialog_data->x2go_conn_type_check_btn));
    const gchar *x2go_conn_type = gtk_combo_box_get_active_id(GTK_COMBO_BOX(dialog_data->x2go_conn_type_combobox));
    update_string_safely(&conn_data->x2Go_settings.x2go_conn_type, x2go_conn_type);

    conn_data->x2Go_settings.full_screen = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            dialog_data->x2go_full_screen_check_btn));

    const gchar *x2go_compress_method = gtk_combo_box_get_active_id(
            GTK_COMBO_BOX(dialog_data->x2go_compress_method_combobox));
    update_string_safely(&conn_data->x2Go_settings.compress_method, x2go_compress_method);

    // Loudplay
    gobject_gui_fill_gobject_from_gui(G_OBJECT(conn_data->loudplay_config), dialog_data->loudplay_config_gui);
    const gchar *loudplay_client_path = gtk_entry_get_text(
            GTK_ENTRY(dialog_data->loudplay_client_path_entry));
    update_string_safely(&conn_data->loudplay_config->loudplay_client_path, loudplay_client_path);
}

static void
ok_button_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialog *dialog_data)
{
    // fill p_conn_data from gui
    gboolean is_success = check_parameters(dialog_data->p_conn_data, dialog_data);

    // Close the window if settings are ok
    if (is_success) {
        dialog_data->dialog_window_response = GTK_RESPONSE_OK;
        take_from_gui(dialog_data);
        shutdown_loop(dialog_data->loop);
    }
}

GtkResponseType remote_viewer_start_settings_dialog(ConnectSettingsDialog *self,
                                                    ConnectSettingsData *conn_data,
                                                    AppUpdater *app_updater,
                                                    GtkWindow *parent,
                                                    gboolean show_only_vm_conn_settings)
{
    self->p_conn_data = conn_data;
    self->p_app_updater = app_updater;
    self->dialog_window_response = GTK_RESPONSE_OK;

    // gui widgets
    self->builder = remote_viewer_util_load_ui("start_settings_form.glade");

    // hide general and service settings if required
    if (show_only_vm_conn_settings) {
        GtkWidget *general_settings_page = get_widget_from_builder(self->builder, "general_settings_page");
        gtk_widget_set_visible(general_settings_page, FALSE);
        gtk_widget_set_no_show_all(general_settings_page, TRUE);
        GtkWidget *service_page = get_widget_from_builder(self->builder, "service_page");
        gtk_widget_set_visible(service_page, FALSE);
        gtk_widget_set_no_show_all(service_page, TRUE);
    }

    self->window = get_widget_from_builder(self->builder, "start-settings-window");
    // main buttons
    self->bt_cancel = get_widget_from_builder(self->builder, "btn_cancel");
    self->bt_ok = get_widget_from_builder(self->builder, "btn_ok");

    // main settings
    self->domain_entry = get_widget_from_builder(self->builder, "domain-entry");
    self->address_entry = get_widget_from_builder(self->builder, "connection-address-entry");
    self->port_spinbox = get_widget_from_builder(self->builder, "port_spinbox");
    self->conn_to_prev_pool_checkbutton = get_widget_from_builder(self->builder, "connect-to-prev-button");
    self->save_password_checkbtn = get_widget_from_builder(self->builder, "save_password_btn");
    self->remote_protocol_combobox = get_widget_from_builder(self->builder, "remote_protocol_combobox");
    self->redirect_time_zone_check_btn =
            get_widget_from_builder(self->builder, "redirect_time_zone_check_btn");

    // spice settings
    self->client_cursor_visible_checkbutton =
            get_widget_from_builder(self->builder, "menu-show-client-cursor");
    self->spice_full_screen_check_btn =
            get_widget_from_builder(self->builder, "spice_full_screen_check_btn");
    self->spice_monitor_mapping_entry =
            get_widget_from_builder(self->builder, "spice_monitor_mapping_entry");
    self->btn_show_monitor_config_spice =
            get_widget_from_builder(self->builder, "btn_show_monitor_config_spice");
    self->spice_usb_auto_connect_filter_entry =
            get_widget_from_builder(self->builder, "spice_usb_auto_connect_filter_entry");
    self->spice_usb_redirect_on_connect_entry =
            get_widget_from_builder(self->builder, "spice_usb_redirect_on_connect_entry");
    self->btn_show_usb_filter_tooltip_spice_0 =
            get_widget_from_builder(self->builder, "btn_show_usb_filter_tooltip_spice_0");
    self->btn_show_usb_filter_tooltip_spice_1 =
            get_widget_from_builder(self->builder, "btn_show_usb_filter_tooltip_spice_1");

    // rdp settings
    self->rdp_image_pixel_format_combobox =
            get_widget_from_builder(self->builder, "rdp_image_pixel_format_combobox");
    self->rdp_fps_spin_btn = get_widget_from_builder(self->builder, "rdp_fps_spin_btn");

    self->is_rdp_vid_comp_used_check_btn =
            get_widget_from_builder(self->builder, "is_rdp_vid_comp_used_check_btn");
    self->rdp_codec_combobox = get_widget_from_builder(self->builder, "rdp_codec_combobox");
#ifdef _WIN32 // На Windows не работает AVC444
    gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(self->rdp_codec_combobox), 2);
#endif
    self->rdp_shared_folders_entry = get_widget_from_builder(self->builder, "rdp_shared_folders_entry");

    self->btn_add_remote_folder = get_widget_from_builder(self->builder, "btn_add_remote_folder");
    self->btn_remove_remote_folder = get_widget_from_builder(self->builder, "btn_remove_remote_folder");

    self->is_multimon_check_btn = get_widget_from_builder(self->builder, "is_multimon_check_btn");
    self->dynamic_resolution_check_btn = get_widget_from_builder(self->builder, "dynamic_resolution_check_btn");
    self->rdp_full_screen_check_btn =
            get_widget_from_builder(self->builder, "rdp_full_screen_check_btn");
    self->rdp_selected_monitors_entry =
            get_widget_from_builder(self->builder, "rdp_selected_monitors_entry");
    self->btn_show_monitor_config_rdp =
            get_widget_from_builder(self->builder, "btn_show_monitor_config_rdp");
    self->redirect_printers_check_btn =
            get_widget_from_builder(self->builder, "redirect_printers_check_btn");
    self->rdp_redirect_microphone_check_btn =
            get_widget_from_builder(self->builder, "rdp_redirect_microphone_check_btn");

    //self->remote_app_check_btn = get_widget_from_builder(self->builder, "remote_app_check_btn");
    //self->remote_app_name_entry = get_widget_from_builder(self->builder, "remote_app_name_entry");
    //self->remote_app_options_entry = get_widget_from_builder(self->builder, "remote_app_options_entry");
    self->remote_app_format_combobox = get_widget_from_builder(self->builder, "remote_app_format_combobox");

    self->rdp_sec_protocol_check_btn =
            get_widget_from_builder(self->builder, "rdp_sec_protocol_check_btn");
    self->sec_type_combobox = get_widget_from_builder(self->builder, "sec_type_combobox");

    self->rdp_network_check_btn = get_widget_from_builder(self->builder, "rdp_network_check_btn");
    self->rdp_network_type_combobox = get_widget_from_builder(self->builder, "rdp_network_type_combobox");

    self->rdp_decorations_check_btn = get_widget_from_builder(self->builder, "rdp_decorations_check_btn");
    self->rdp_fonts_check_btn = get_widget_from_builder(self->builder, "rdp_fonts_check_btn");
    self->rdp_themes_check_btn = get_widget_from_builder(self->builder, "rdp_themes_check_btn");

    self->select_usb_btn = get_widget_from_builder(self->builder, "select_usb_btn");

    self->use_rdp_file_check_btn = get_widget_from_builder(self->builder, "use_rdp_file_check_btn");
    self->btn_choose_rdp_file = get_widget_from_builder(self->builder, "btn_choose_rdp_file");
    self->rdp_file_name_entry = get_widget_from_builder(self->builder, "rdp_file_name_entry");

    g_autofree gchar *title = NULL;
    title = g_strdup_printf("RemoteFX USB  -  %s ", APPLICATION_NAME);
    self->usb_selector_widget = usb_selector_widget_new(title);
    gtk_widget_destroy(self->usb_selector_widget->tk_address_entry);
    gtk_widget_destroy(self->usb_selector_widget->tk_address_header);
    usb_selector_widget_enable_auto_toggle(self->usb_selector_widget);

    self->use_rd_gateway_check_btn = get_widget_from_builder(self->builder, "use_rd_gateway_check_btn");
    self->gateway_address_entry = get_widget_from_builder(self->builder, "gateway_address_entry");
    self->btn_add_addresses = get_widget_from_builder(self->builder, "btn_add_addresses");
    self->rdp_log_debug_info_check_btn = get_widget_from_builder(self->builder, "rdp_log_debug_info_check_btn");

    // X2Go settings
    self->x2go_app_combobox = get_widget_from_builder(self->builder, "x2go_app_combobox");
    self->x2go_session_type_combobox = get_widget_from_builder(self->builder,
            "x2go_session_type_combobox");

    self->x2go_conn_type_check_btn = get_widget_from_builder(self->builder, "x2go_conn_type_check_btn");
    self->x2go_conn_type_combobox = get_widget_from_builder(self->builder, "x2go_conn_type_combobox");

    self->x2go_full_screen_check_btn = get_widget_from_builder(self->builder,
            "x2go_full_screen_check_btn");

    self->x2go_compress_method_combobox =
            get_widget_from_builder(self->builder, "x2go_compress_method_combobox");
    //GtkTreeIter iter;
    //GtkTreeModel *list_store = gtk_combo_box_get_model(GTK_COMBO_BOX(self->x2go_compress_method_combobox));
    //gboolean valid = gtk_tree_model_get_iter_first (list_store, &iter);

    // Loudplay
    self->loudplay_client_path_entry = get_widget_from_builder(self->builder, "loudplay_client_path_entry");
    self->loudplay_box = get_widget_from_builder(self->builder, "loudplay_box");

    // Service functions
    self->btn_archive_logs = get_widget_from_builder(self->builder, "btn_archive_logs");
    self->log_location_label = get_widget_from_builder(self->builder, "log_location_label");
    gtk_label_set_selectable(GTK_LABEL(self->log_location_label), TRUE);

    self->btn_get_app_updates = get_widget_from_builder(self->builder, "btn_get_app_updates");
    self->check_updates_spinner = get_widget_from_builder(self->builder, "check_updates_spinner");
    self->check_updates_label = get_widget_from_builder(self->builder, "check_updates_label");
    self->windows_updates_url_entry = get_widget_from_builder(self->builder, "windows_updates_url_entry");
    self->btn_open_doc = get_widget_from_builder(self->builder, "btn_open_doc");

    // В этот момент может происходить процесс обновления софта.  Setup gui
    if (self->p_app_updater && app_updater_is_getting_updates(self->p_app_updater)) {

        gchar *app_updater_cur_status_msg = app_updater_get_cur_status_msg(self->p_app_updater);
        gtk_label_set_text(GTK_LABEL(self->check_updates_label), app_updater_cur_status_msg);
        free_memory_safely(&app_updater_cur_status_msg);
        gtk_spinner_start((GtkSpinner *) self->check_updates_spinner);
        gtk_widget_set_sensitive(self->btn_get_app_updates, FALSE);
    }

    self->app_mode_combobox = get_widget_from_builder(self->builder, "app_mode_combobox");
    self->vm_await_time_spinbox = get_widget_from_builder(self->builder, "vm_await_time_spinbox");
    self->unique_app_check_btn = get_widget_from_builder(self->builder, "unique_app_check_btn");

    // Signals
    g_signal_connect_swapped(self->window, "delete-event", G_CALLBACK(window_deleted_cb), self);
    g_signal_connect(self->bt_cancel, "clicked", G_CALLBACK(cancel_button_clicked_cb), self);
    g_signal_connect(self->bt_ok, "clicked", G_CALLBACK(ok_button_clicked_cb), self);
    g_signal_connect(self->is_rdp_vid_comp_used_check_btn, "toggled",
                     G_CALLBACK(on_vid_comp_used_check_btn_toggled), self);
    g_signal_connect(self->rdp_sec_protocol_check_btn, "toggled",
                     G_CALLBACK(on_rdp_sec_protocol_check_btn_toggled), self);
    g_signal_connect(self->rdp_network_check_btn, "toggled",
                     G_CALLBACK(on_rdp_network_check_btn_toggled), self);
    g_signal_connect(self->btn_add_remote_folder, "clicked",
                     G_CALLBACK(btn_add_remote_folder_clicked_cb), self);
    g_signal_connect(self->btn_remove_remote_folder, "clicked",
                     G_CALLBACK(btn_remove_remote_folder_clicked_cb), self);
    g_signal_connect(self->use_rdp_file_check_btn, "toggled",
                     G_CALLBACK(on_use_rdp_file_check_btn_toggled), self);
    g_signal_connect(self->btn_choose_rdp_file, "clicked",
                     G_CALLBACK(btn_choose_rdp_file_clicked), self);
    g_signal_connect(self->use_rd_gateway_check_btn, "toggled",
                     G_CALLBACK(on_use_rd_gateway_check_btn_toggled), self);

    g_signal_connect(self->x2go_conn_type_check_btn, "toggled",
                     G_CALLBACK(on_conn_type_check_btn_toggled), self);

    g_signal_connect(self->btn_archive_logs, "clicked",
                     G_CALLBACK(btn_archive_logs_clicked_cb), self);
    //g_signal_connect(self->remote_app_check_btn, "toggled", G_CALLBACK(on_remote_app_check_btn_toggled), self);
    g_signal_connect(self->btn_get_app_updates, "clicked", G_CALLBACK(btn_get_app_updates_clicked_cb),
                     self);
    g_signal_connect(self->btn_open_doc, "clicked", G_CALLBACK(btn_open_doc_clicked_cb),
                     self);
    g_signal_connect(self->select_usb_btn, "clicked", G_CALLBACK(on_select_usb_btn_clicked_cb), self);

    g_signal_connect(self->btn_show_monitor_config_spice, "clicked",
                     G_CALLBACK(btn_show_monitor_config_clicked), self);
    g_signal_connect(self->btn_show_monitor_config_rdp, "clicked",
                     G_CALLBACK(btn_show_monitor_config_clicked), self);
    g_signal_connect(self->btn_show_usb_filter_tooltip_spice_0, "clicked",
                     G_CALLBACK(btn_show_usb_filter_tooltip_spice_clicked), self);
    g_signal_connect(self->btn_show_usb_filter_tooltip_spice_1, "clicked",
                     G_CALLBACK(btn_show_usb_filter_tooltip_spice_clicked), self);
    g_signal_connect(self->btn_add_addresses, "clicked",
                     G_CALLBACK(btn_add_addresses_clicked), self);

    self->on_app_mode_combobox_changed_id =
            g_signal_connect(self->app_mode_combobox, "changed",
                             G_CALLBACK(on_app_mode_combobox_changed), self);

    gulong st_msg_hdle = 0, state_hdle = 0;
    if (self->p_app_updater) {
        st_msg_hdle = g_signal_connect(self->p_app_updater, "status-msg-changed",
                                              G_CALLBACK(on_app_updater_status_msg_changed), self);
        state_hdle = g_signal_connect(self->p_app_updater, "state-changed",
                                             G_CALLBACK(on_app_updater_status_changed), self);
    }

    // show window
    gtk_window_set_transient_for(GTK_WINDOW(self->window), parent);
    gtk_window_set_position(GTK_WINDOW(self->window), GTK_WIN_POS_CENTER);
    gtk_widget_show_all(self->window);
    fill_gui(self);
#ifndef  _WIN32
    gtk_widget_hide(self->windows_updates_url_entry);
#endif
    gtk_window_set_resizable(GTK_WINDOW(self->window), FALSE);

    create_loop_and_launch(&self->loop);

    // save
    settings_data_save_all(self->p_conn_data);

    // disconnect signals from external sources
    if (st_msg_hdle)
        g_signal_handler_disconnect(self->p_app_updater, st_msg_hdle);
    if (state_hdle)
        g_signal_handler_disconnect(self->p_app_updater, state_hdle);

    // clear
    usb_selector_widget_free(self->usb_selector_widget);
    gtk_widget_destroy(self->window);
    self->window = NULL;
    g_object_unref(self->builder);

    self->loudplay_config_gui = NULL;

    return self->dialog_window_response;
}

void remote_viewer_start_settings_finish_job(ConnectSettingsDialog *self)
{
    if (self->window)
        gtk_widget_hide(self->window);
    shutdown_loop(self->loop);
}
