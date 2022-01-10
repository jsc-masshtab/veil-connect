/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_CONN_INFO_DIALOG_H
#define VEIL_CONNECT_CONN_INFO_DIALOG_H

#include "net_speedometer.h"

typedef struct {
    GtkBuilder *builder;

    GtkWidget *dialog;
    GtkWidget *data_await_spinner;

    GtkWidget *vm_conn_time_label;
    GtkWidget *vdi_conn_time_label;
    GtkWidget *protocol_label;
    GtkWidget *read_speed_label;
    GtkWidget *write_speed_label;
    GtkWidget *avg_rtt_label;
    GtkWidget *network_loss_label;

} ConnInfoDialog;

ConnInfoDialog *conn_info_dialog_create(void);
void conn_info_dialog_destroy(ConnInfoDialog *self);

void conn_info_dialog_show(ConnInfoDialog *self, GtkWindow *parent_window);

void conn_info_dialog_update_vm_conn_time(ConnInfoDialog *self, const gchar *time);
void conn_info_dialog_update(ConnInfoDialog *self, VmRemoteProtocol protocol, NetworkStatsData *nw_data);
//void conn_info_dialog_reset(ConnInfoDialog *self);

#endif //VEIL_CONNECT_CONN_INFO_DIALOG_H
