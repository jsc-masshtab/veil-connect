/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef NATIVE_RDP_LAUNCHER_H
#define NATIVE_RDP_LAUNCHER_H

#include <gtk/gtk.h>

#include "settings_data.h"


#define TYPE_NATIVE_RDP_LAUNCHER ( native_rdp_launcher_get_type( ) )
#define NATIVE_RDP_LAUNCHER( obj ) ( G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_NATIVE_RDP_LAUNCHER, NativeRdpLauncher) )

typedef struct
{
    GObject parent;

    GPid pid;
    gboolean is_launched;

    GtkWindow *parent_widget;

} NativeRdpLauncher;

typedef struct {
    GObjectClass parent_class;

    /* signals */
    void (*job_finished)(NativeRdpLauncher *self);

} NativeRdpLauncherClass;

GType native_rdp_launcher_get_type( void ) G_GNUC_CONST;

NativeRdpLauncher *native_rdp_launcher_new(void);

void native_rdp_launcher_start(NativeRdpLauncher *self, GtkWindow *parent, const VeilRdpSettings *p_rdp_settings);
void native_rdp_launcher_stop(NativeRdpLauncher *self);

#endif // NATIVE_RDP_LAUNCHER_H
