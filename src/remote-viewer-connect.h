/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef REMOTE_VIEWER_CONNECT_H
#define REMOTE_VIEWER_CONNECT_H

#include <gtk/gtk.h>

#include "vdi_session.h"
#include "settings_data.h"
#include "app_updater.h"

#define TYPE_REMOTE_VIEWER_CONNECT ( remote_viewer_connect_get_type( ) )
#define REMOTE_VIEWER_CONNECT( obj ) ( G_TYPE_CHECK_INSTANCE_CAST( (obj), \
    TYPE_REMOTE_VIEWER_CONNECT, RemoteViewerConnect ) )


typedef enum
{
    AUTH_GUI_DEFAULT_STATE,
    AUTH_GUI_CONNECT_TRY_STATE

} AuthDialogState;

typedef struct
{
    GObject parent;

    gboolean is_active;

    GtkBuilder *builder;

    ConnectSettingsData *p_conn_data;
    AppUpdater *p_app_updater;

    // gui elements
    GtkWidget *login_entry;
    GtkWidget *password_entry;
    GtkWidget *disposable_password_entry;
    GtkWidget *check_btn_2fa_password;
    GtkWidget *ldap_check_btn;

    GtkWidget *window;
    GtkWidget *connect_spinner;
    GtkWidget *message_display_label;
    GtkWidget *header_label;
    GtkWidget *new_version_available_image;

    GtkWidget *settings_button;
    GtkWidget *connect_button;
    GtkWidget *btn_cancel_auth;

    GtkWidget *app_name_label;

    GtkWidget *global_application_mode_image;

    AuthDialogState auth_dialog_state;

    gulong updates_checked_handle;

} RemoteViewerConnect;

typedef struct
{
    GObjectClass parent_class;

    /* signals */
    void (*show_vm_selector_requested)(RemoteViewerConnect *self);
    void (*connect_to_vm_requested)(RemoteViewerConnect *self);
    void (*quit_requested)(RemoteViewerConnect *self);

} RemoteViewerConnectClass;

GType remote_viewer_connect_get_type( void ) G_GNUC_CONST;

RemoteViewerConnect *remote_viewer_connect_new(void);
void remote_viewer_connect_show(RemoteViewerConnect *self, ConnectSettingsData *conn_data,
                                AppUpdater *app_updater);
void remote_viewer_connect_raise(RemoteViewerConnect *self);

#endif /* REMOTE_VIEWER_CONNECT_H */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
