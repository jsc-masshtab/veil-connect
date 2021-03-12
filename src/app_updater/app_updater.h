/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_APP_UPDATER_H
#define VEIL_CONNECT_APP_UPDATER_H

#include <gtk/gtk.h>

#include "remote-viewer-util.h"

#define TYPE_APP_UPDATER         ( app_updater_get_type( ) )
#define APP_UPDATER( obj )       ( G_TYPE_CHECK_INSTANCE_CAST( (obj), TYPE_APP_UPDATER, AppUpdater ) )
#define IS_APP_UPDATER( obj )        ( G_TYPE_CHECK_INSTANCE_TYPE( (obj), TYPE_APP_UPDATER ) )
#define APP_UPDATER_CLASS( klass )   ( G_TYPE_CHECK_CLASS_CAST( (klass), TYPE_APP_UPDATER, AppUpdaterClass ) )
#define IS_APP_UPDATER_CLASS( klass )    ( G_TYPE_CHECK_CLASS_TYPE( (klass), TYPE_APP_UPDATER ) )
#define APP_UPDATER_GET_CLASS( obj ) ( G_TYPE_INSTANCE_GET_CLASS( (obj), TYPE_APP_UPDATER, AppUpdaterClass ) )

typedef struct _AppUpdater      AppUpdater;
typedef struct _AppUpdaterClass AppUpdaterClass;

struct _AppUpdater
{
    GObject parent;

    gchar *_cur_status_msg;
    gchar *_admin_password;
    gchar *_last_standard_output;
    gchar *_last_standard_error;
    gchar *_windows_releases_url; // url to windows releases storage

    int _is_checking_updates;
    int _is_getting_updates;
    gint _last_exit_status;

    LINUX_DISTRO _linux_distro;

    GMutex priv_members_mutex;
};

struct _AppUpdaterClass
{
    GObjectClass parent_class;

    /* signals */
    void (*status_msg_changed)(AppUpdater *self, const gchar *msg);
    void (*state_changed)(AppUpdater *self, int is_working);
    void (*updates_checked)(AppUpdater *self, int is_available);
};

GType app_updater_get_type( void ) G_GNUC_CONST;

AppUpdater *app_updater_new(void);

gchar *app_updater_get_cur_status_msg(AppUpdater *self);
int app_updater_is_getting_updates(AppUpdater *self);
void app_updater_set_admin_password(AppUpdater *self, const gchar *admin_password);
gchar *app_updater_get_admin_password(AppUpdater *self);

void app_updater_set_windows_releases_url(AppUpdater *self, const gchar *windows_releases_url);
gchar *app_updater_get_windows_releases_url(AppUpdater *self);

// execute tasks
void app_updater_execute_task_check_updates(AppUpdater *self);
void app_updater_execute_task_get_updates(AppUpdater *self);

gchar *app_updater_get_last_process_output(AppUpdater *self);



#endif //VEIL_CONNECT_APP_UPDATER_H
