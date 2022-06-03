/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_LOUDPLAY_LAUNCHER_H
#define VEIL_CONNECT_LOUDPLAY_LAUNCHER_H

#include <gtk/gtk.h>

#include "settings_data.h"


#define TYPE_LOUDPLAY_LAUNCHER ( loudplay_launcher_get_type( ) )
#define LOUDPLAY_LAUNCHER( obj ) ( G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_LOUDPLAY_LAUNCHER, LoudplayLauncher) )

typedef struct
{
    GObject parent;

    GPid pid;
    gboolean is_launched;

    GtkWindow *parent_widget;

} LoudplayLauncher;

typedef struct {
    GObjectClass parent_class;

    /* signals */
    void (*job_finished)(LoudplayLauncher *self);

} LoudplayLauncherClass;

GType loudplay_launcher_get_type( void ) G_GNUC_CONST;

LoudplayLauncher *loudplay_launcher_new(void);


void loudplay_launcher_start(LoudplayLauncher *self, GtkWindow *parent, ConnectSettingsData *conn_data);
void loudplay_launcher_stop(LoudplayLauncher *self);

#endif //VEIL_CONNECT_LOUDPLAY_LAUNCHER_H
