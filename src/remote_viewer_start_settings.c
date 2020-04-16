#include <stdio.h>
#include <ctype.h>

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

    GtkWidget *bt_cancel;
    GtkWidget *bt_ok;

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

static void
fill_connect_settings_data_from_gui(ConnectSettingsData *connect_settings_data, ConnectSettingsDialogData *dialog_data)
{
    if (dialog_data->dialog_window_response != GTK_RESPONSE_OK)
        return;

    free_memory_safely(&connect_settings_data->domain);
     connect_settings_data->domain = g_strdup(gtk_entry_get_text(GTK_ENTRY(dialog_data->domain_entry)));

   free_memory_safely(&connect_settings_data->ip);
    connect_settings_data->ip = g_strdup(gtk_entry_get_text(GTK_ENTRY(dialog_data->address_entry)));

    const gchar *port_str = gtk_entry_get_text(GTK_ENTRY(dialog_data->port_entry));
    connect_settings_data->port = atoi(port_str);

    connect_settings_data->is_ldap = gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->ldap_checkbutton);
    connect_settings_data->is_connect_to_prev_pool =
            gtk_toggle_button_get_active((GtkToggleButton *)dialog_data->conn_to_prev_pool_checkbutton);

    if (dialog_data->remote_protocol_combobox)
        connect_settings_data->remote_protocol_type = (VdiVmRemoteProtocol)(
                gtk_combo_box_get_active((GtkComboBox*)dialog_data->remote_protocol_combobox));

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
ok_button_clicked_cb(GtkButton *button G_GNUC_UNUSED, ConnectSettingsDialogData *dialog_data)
{
    dialog_data->dialog_window_response = GTK_RESPONSE_OK;
    shutdown_loop(dialog_data->loop);
}

static void
fill_connect_settings_dialog_data(ConnectSettingsDialogData *dialog_data, ConnectSettingsData *connect_settings_data)
{
    const gchar *paramToFileGrpoup = opt_manual_mode ? "RemoteViewerConnectManual" : "RemoteViewerConnect";

    // domain
    if (connect_settings_data->domain) {
        gtk_entry_set_text(GTK_ENTRY(dialog_data->domain_entry), connect_settings_data->domain);
    }
    // ip
    if (connect_settings_data->ip) {
        gtk_entry_set_text(GTK_ENTRY(dialog_data->address_entry), connect_settings_data->ip);
    }
    // port. TODO: why gtk entry for port?
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
}

GtkResponseType remote_viewer_start_settings_dialog(ConnectSettingsData *connect_settings_data, GtkWindow *parent)
{
    ConnectSettingsDialogData dialog_data;
    memset(&dialog_data, 0, sizeof(ConnectSettingsDialogData));

    dialog_data.dialog_window_response = GTK_RESPONSE_OK;

    // gui widgets
    dialog_data.builder = remote_viewer_util_load_ui("start_settings_form.ui");

    dialog_data.window = GTK_WIDGET(gtk_builder_get_object(dialog_data.builder, "start-settings-window"));
    dialog_data.bt_cancel = GTK_WIDGET(gtk_builder_get_object(dialog_data.builder, "btn_cancel"));
    dialog_data.bt_ok = GTK_WIDGET(gtk_builder_get_object(dialog_data.builder, "btn_ok"));
    dialog_data.domain_entry = GTK_WIDGET(gtk_builder_get_object(dialog_data.builder, "domain-entry"));
    dialog_data.address_entry = GTK_WIDGET(gtk_builder_get_object(dialog_data.builder, "connection-address-entry"));
    dialog_data.port_entry = GTK_WIDGET(gtk_builder_get_object(dialog_data.builder, "connection-port-entry"));
    dialog_data.ldap_checkbutton = GTK_WIDGET(gtk_builder_get_object(dialog_data.builder, "ldap-button"));
    gtk_widget_set_sensitive(dialog_data.ldap_checkbutton, !opt_manual_mode);
    dialog_data.conn_to_prev_pool_checkbutton =
            GTK_WIDGET(gtk_builder_get_object(dialog_data.builder, "connect-to-prev-button"));
    gtk_widget_set_sensitive(dialog_data.conn_to_prev_pool_checkbutton, !opt_manual_mode);
    dialog_data.remote_protocol_combobox =
            GTK_WIDGET(gtk_builder_get_object(dialog_data.builder, "combobox-remote-protocol"));
    if (!opt_manual_mode) {
        gtk_widget_destroy(dialog_data.remote_protocol_combobox);
        dialog_data.remote_protocol_combobox = NULL;
    }

    // Signals
    g_signal_connect_swapped(dialog_data.window, "delete-event", G_CALLBACK(window_deleted_cb), &dialog_data);
    g_signal_connect(dialog_data.bt_cancel, "clicked", G_CALLBACK(cancel_button_clicked_cb), &dialog_data);
    g_signal_connect(dialog_data.bt_ok, "clicked", G_CALLBACK(ok_button_clicked_cb), &dialog_data);
    g_signal_connect(dialog_data.port_entry, "insert-text", G_CALLBACK(on_insert_text_event), NULL);

    // read from file
    fill_connect_settings_data_from_ini_file(connect_settings_data);
    fill_connect_settings_dialog_data(&dialog_data, connect_settings_data);

    // show window
    gtk_window_set_transient_for(GTK_WINDOW(dialog_data.window), parent);
    gtk_window_set_position(GTK_WINDOW(dialog_data.window), GTK_WIN_POS_CENTER);
    gtk_widget_show_all(dialog_data.window);
    gtk_window_set_resizable(GTK_WINDOW(dialog_data.window), FALSE);

    create_loop_and_launch(&dialog_data.loop);

    // fill connect_settings_data from gui if response is GTK_RESPONSE_OK
    fill_connect_settings_data_from_gui(connect_settings_data, &dialog_data);
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
    const gchar *paramToFileGrpoup = opt_manual_mode ? "RemoteViewerConnectManual" : "RemoteViewerConnect";
    // domain
    free_memory_safely(&connect_settings_data->domain);
    connect_settings_data->domain = read_str_from_ini_file(paramToFileGrpoup, "domain");
    printf("%s connect_settings_data->domain %s\n", (const char *)__func__, connect_settings_data->domain);
    // ip
    free_memory_safely(&connect_settings_data->ip);
    connect_settings_data->ip = read_str_from_ini_file(paramToFileGrpoup, "ip");
    // port
    connect_settings_data->port = read_int_from_ini_file(paramToFileGrpoup, "port");
    // ldap
    connect_settings_data->is_ldap = read_int_from_ini_file("RemoteViewerConnect", "is_ldap_btn_checked");
    // Connect to prev pool
    connect_settings_data->is_connect_to_prev_pool =
            read_int_from_ini_file("RemoteViewerConnect", "is_conn_to_prev_pool_btn_checked");
    // remote protocol
    gint remote_protocol_type = read_int_from_ini_file("General", "cur_remote_protocol_index");
    connect_settings_data->remote_protocol_type = (VdiVmRemoteProtocol)remote_protocol_type;
}

//void free_connect_settings_data(ConnectSettingsData *connect_settings_data)
//{
//    free_memory_safely(&connect_settings_data->ip);
//    free(connect_settings_data);
//}