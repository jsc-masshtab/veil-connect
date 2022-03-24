/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_CONTROLLER_MANAGER_H
#define VEIL_CONNECT_CONTROLLER_MANAGER_H

#include <glib.h>
#include <gtk/gtk.h>

#include "settings_data.h"

#define TYPE_CONTROLLER_MANAGER  ( controller_manager_get_type( ) )
#define CONTROLLER_MANAGER( obj ) ( G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_CONTROLLER_MANAGER, ControllerManager) )

typedef struct{
    GObject parent;

    gboolean is_active;

    GtkBuilder *builder;
    GtkWidget *window;

    GtkWidget *btn_update;
    GtkWidget *btn_logout;
    GtkWidget *btn_cancel_requests;

    GtkWidget *vm_list_tree_view;
    GtkWidget *vm_verbose_name_entry;
    GtkWidget *remote_protocol_combobox;
    GtkWidget *status_label;
    GtkWidget *main_vm_spinner;

    GtkListStore *vm_list_store;

    ConnectionInfo ci;

    ConnectSettingsData *p_conn_data;

} ControllerManager;

typedef struct
{
    GObjectClass parent_class;

    /* signals */
    void (*logged_out)(ControllerManager *self, const gchar *reason_phrase);
    void (*connect_to_vm_requested)(ControllerManager *self);
    void (*quit_requested)(ControllerManager *self);

} ControllerManagerClass;


GType controller_manager_get_type( void ) G_GNUC_CONST;

void controller_manager_show(ControllerManager *self, ConnectSettingsData *conn_data);
void controller_manager_finish_job(ControllerManager *self);

ControllerManager *controller_manager_new(void);



#endif //VEIL_CONNECT_CONTROLLER_MANAGER_H
