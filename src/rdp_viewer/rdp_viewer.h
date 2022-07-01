/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef RDP_VIEWER_H
#define RDP_VIEWER_H

#include <gtk/gtk.h>

#include "remote-viewer-util.h"
#include "rdp_client.h"
#include "net_speedometer.h"
#include "settings_data.h"
#include "messenger.h"


#define TYPE_RDP_VIEWER ( rdp_viewer_get_type( ) )
#define RDP_VIEWER( obj ) ( G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_RDP_VIEWER, RdpViewer) )

typedef struct {
    GObject parent;

    ExtendedRdpContext *ex_context;
    NetSpeedometer *net_speedometer;

    GThread *job_thread;
} RdpViewer;

typedef struct {
    GObjectClass parent_class;

    /* signals */
    void (*job_finished)(RdpViewer *self);
    void (*quit_requested)(RdpViewer *self);

} RdpViewerClass;

GType rdp_viewer_get_type( void ) G_GNUC_CONST;

RdpViewer *rdp_viewer_new(void);

void rdp_viewer_start(RdpViewer *self, ConnectSettingsData *conn_data, VeilMessenger *veil_messenger,
        VeilRdpSettings *p_rdp_settings);

void rdp_viewer_stop(RdpViewer *rdp_viewer, const gchar *signal_upon_job_finish, gboolean exit_if_cant_abort);

#endif /* RDP_VIEWER_H */
