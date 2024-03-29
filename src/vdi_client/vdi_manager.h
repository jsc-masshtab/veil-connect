/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VIRT_VIEWER_VEIL_VDI_MANAGER_H
#define VIRT_VIEWER_VEIL_VDI_MANAGER_H

#include <gtk/gtk.h>

#include "settings_data.h"
#include "vdi_session.h"
#include "remote-viewer-connect.h"


#define TYPE_VDI_MANAGER         ( vdi_manager_get_type( ) )
#define VDI_MANAGER( obj )       ( G_TYPE_CHECK_INSTANCE_CAST( (obj), TYPE_VDI_MANAGER, VdiManager ) )
#define IS_VDI_MANAGER( obj )        ( G_TYPE_CHECK_INSTANCE_TYPE( (obj), TYPE_VDI_MANAGER ) )
#define VDI_MANAGER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_CAST( (klass), TYPE_VDI_MANAGER, VdiManagerClass ) )
#define IS_VDI_MANAGER_CLASS( klass )    ( G_TYPE_CHECK_CLASS_TYPE( (klass), TYPE_VDI_MANAGER ) )
#define VDI_MANAGER_GET_CLASS( obj ) ( G_TYPE_INSTANCE_GET_CLASS( (obj), TYPE_VDI_MANAGER, VdiManagerClass ) )


typedef struct _VdiManager      VdiManager;
typedef struct _VdiManagerClass VdiManagerClass;

struct _VdiManager
{
    GObject parent;

    gboolean is_active;

    GtkBuilder *builder;

    GtkWidget *window;

    GtkWidget *button_quit;
    GtkWidget *btn_update;
    GtkWidget *btn_cancel_requests;
    GtkWidget *btn_show_only_favorites;

    GtkWidget *vm_main_box;
    GtkWidget *gtk_flow_box;
    GtkWidget *status_label;
    GtkWidget *vm_prep_progress_bar;
    GtkWidget *main_vm_spinner;
    GtkWidget *label_is_vdi_online;

    GtkWidget *pool_options_menu;
    GtkWidget *favorite_pool_menu_item;
    gulong favorite_pool_menu_item_toggled_handle;

    GPtrArray *pool_widgets_array;

    gulong ws_conn_changed_handle;
    gulong auth_fail_detected_handle;
    gulong pool_entitlement_changed_handle;
    gulong vm_prep_progress_handle;

    int current_vm_request_id;

    gboolean is_favorites_supported_by_server;

    ConnectSettingsData *p_conn_data;
    ConnectSettingsDialog connect_settings_dialog;
};

struct _VdiManagerClass
{
    GObjectClass parent_class;

    /* signals */
    void (*logged_out)(VdiManager *self, const gchar *reason_phrase);
    void (*connect_to_vm_requested)(VdiManager *self);
    void (*quit_requested)(VdiManager *self);
};

GType vdi_manager_get_type( void ) G_GNUC_CONST;

void vdi_manager_show(VdiManager *self, ConnectSettingsData *conn_data);
void vdi_manager_raise(VdiManager *self);
void vdi_manager_finish_job(VdiManager *self);

VdiManager *vdi_manager_new(void);
#endif //VIRT_VIEWER_VEIL_VDI_MANAGER_H
