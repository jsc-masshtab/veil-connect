/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "remote-viewer-util.h"
#include "conn_info_dialog.h"
#include "vdi_session.h"

static gchar *convert_speed_to_kbytes_str(gulong speed_in_bytes)
{
    // "%.2f Кбайт"
    return g_strdup_printf(_("%.2f Kbyte "), (float)speed_in_bytes / 1024.0);
}

ConnInfoDialog *conn_info_dialog_create()
{
    ConnInfoDialog *self = calloc(1, sizeof(ConnInfoDialog));

    self->builder = remote_viewer_util_load_ui("connection_info_dialog.glade");
    self->dialog = get_widget_from_builder(self->builder, "connection_info_dialog");
    gtk_dialog_add_button(GTK_DIALOG(self->dialog), "Ок", GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(self->dialog), GTK_RESPONSE_OK);

    self->data_await_spinner = get_widget_from_builder(self->builder, "data_await_spinner");

    self->vdi_conn_time_label = get_widget_from_builder(self->builder, "vdi_conn_time_label");
    self->vm_conn_time_label = get_widget_from_builder(self->builder, "vm_conn_time_label");
    self->protocol_label = get_widget_from_builder(self->builder, "protocol_label");
    self->read_speed_label = get_widget_from_builder(self->builder, "read_speed_label");
    self->write_speed_label = get_widget_from_builder(self->builder, "write_speed_label");
    self->avg_rtt_label = get_widget_from_builder(self->builder, "avg_rtt_label");
    self->network_loss_label = get_widget_from_builder(self->builder, "network_loss_label");

    // Connection time. Can be corrected later in the code
    g_autofree gchar *cur_time = NULL;
    cur_time = get_current_readable_time();
    conn_info_dialog_update_vm_conn_time(self, cur_time);

    return self;
}

void conn_info_dialog_destroy(ConnInfoDialog *self)
{
    gtk_widget_hide(self->dialog);
    gtk_widget_destroy(self->dialog);
    g_object_unref(G_OBJECT(self->builder));
}

void conn_info_dialog_show(ConnInfoDialog *self, GtkWindow *parent_window)
{
    gtk_window_set_transient_for(GTK_WINDOW(self->dialog), parent_window);
    gtk_spinner_start(GTK_SPINNER(self->data_await_spinner));

    gtk_dialog_run(GTK_DIALOG(self->dialog));
    gtk_widget_hide(self->dialog);
}

void conn_info_dialog_update_vm_conn_time(ConnInfoDialog *self, const gchar *time)
{
    gtk_label_set_text(GTK_LABEL(self->vm_conn_time_label), time);
}

void conn_info_dialog_update(ConnInfoDialog *self, VmRemoteProtocol protocol, NetworkStatsData *nw_data)
{
    // stop data awaiting spinner
    gtk_spinner_stop(GTK_SPINNER(self->data_await_spinner));

    // conn time
    VdiWsClient *ws_client = vdi_session_get_ws_client();
    gtk_label_set_text(GTK_LABEL(self->vdi_conn_time_label), vdi_ws_client_get_conn_time(ws_client));

    // quality
    g_autofree gchar *avg_rtt_str = NULL;
    avg_rtt_str = g_strdup_printf(_("%.2f ms"), nw_data->avg_rtt); // "%.2f мс"
    gtk_label_set_text(GTK_LABEL(self->avg_rtt_label), avg_rtt_str);

    g_autofree gchar *network_loss_str = NULL;
    network_loss_str = g_strdup_printf("%i", nw_data->loss_percentage);
    gtk_label_set_text(GTK_LABEL(self->network_loss_label), network_loss_str);

    // protocol
    gtk_label_set_text(GTK_LABEL(self->protocol_label), vdi_session_remote_protocol_to_str(protocol));

    // speed
    if (protocol == SPICE_PROTOCOL || protocol == SPICE_DIRECT_PROTOCOL) {
        g_autofree gchar *read_speed_str = NULL;
        read_speed_str = convert_speed_to_kbytes_str(nw_data->spice_total_read_speed);
        gtk_label_set_text(GTK_LABEL(self->read_speed_label), read_speed_str);
        gtk_label_set_text(GTK_LABEL(self->write_speed_label), "-");

    } else if (protocol == RDP_PROTOCOL || protocol == RDP_WINDOWS_NATIVE_PROTOCOL) {
        g_autofree gchar *read_speed_str = NULL;
        read_speed_str = convert_speed_to_kbytes_str(nw_data->rdp_read_speed);
        gtk_label_set_text(GTK_LABEL(self->read_speed_label), read_speed_str);
        g_autofree gchar *write_speed_str = NULL;
        write_speed_str = convert_speed_to_kbytes_str(nw_data->rdp_write_speed);
        gtk_label_set_text(GTK_LABEL(self->write_speed_label), write_speed_str);
    }
}

void conn_info_dialog_reset(ConnInfoDialog *self)
{
    gtk_label_set_text(GTK_LABEL(self->vm_conn_time_label), "");
    gtk_label_set_text(GTK_LABEL(self->vdi_conn_time_label), "");
    gtk_label_set_text(GTK_LABEL(self->avg_rtt_label), "");
    gtk_label_set_text(GTK_LABEL(self->network_loss_label), "");
    gtk_label_set_text(GTK_LABEL(self->protocol_label), "");
    gtk_label_set_text(GTK_LABEL(self->read_speed_label), "-");
    gtk_label_set_text(GTK_LABEL(self->write_speed_label), "-");
}
