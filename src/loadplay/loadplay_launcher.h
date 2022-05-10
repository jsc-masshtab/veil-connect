/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_LOADPLAY_LAUNCHER_H
#define VEIL_CONNECT_LOADPLAY_LAUNCHER_H

#include <gtk/gtk.h>

#include "settings_data.h"


#define TYPE_LOADPLAY_LAUNCHER ( loadplay_launcher_get_type( ) )
#define LOADPLAY_LAUNCHER( obj ) ( G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_LOADPLAY_LAUNCHER, LoadplayLauncher) )

typedef struct
{
    GObject parent;

    GPid pid;
    gboolean is_launched;

    GtkWindow *parent_widget;

} LoadplayLauncher;

typedef struct {
    GObjectClass parent_class;

    /* signals */
    void (*job_finished)(LoadplayLauncher *self);

} LoadplayLauncherClass;

GType loadplay_launcher_get_type( void ) G_GNUC_CONST;

LoadplayLauncher *loadplay_launcher_new(void);


void loadplay_launcher_start(LoadplayLauncher *self, GtkWindow *parent, ConnectSettingsData *conn_data);
void loadplay_launcher_stop(LoadplayLauncher *self);

#endif //VEIL_CONNECT_LOADPLAY_LAUNCHER_H
