/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_NET_SPEEDOMETER_H
#define VEIL_CONNECT_NET_SPEEDOMETER_H

#include <glib.h>
#include <gtk/gtk.h>
#include <freerdp/freerdp.h>

#include "vdi_session.h"

#include "atomic_string.h"

#define TYPE_NET_SPEEDOMETER  ( net_speedometer_get_type( ) )
#define NET_SPEEDOMETER( obj ) ( G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_NET_SPEEDOMETER, NetSpeedometer) )

typedef struct _VirtViewerApp VirtViewerApp;

typedef struct{
    // quality. current data
    float min_rtt; // ms
    float avg_rtt;
    float max_rtt;
    int loss_percentage; // 0 - 100

    // spice speed. Спайс дает возможность узнать количество только полученных байтов
    SpiceReadBytes spice_read_bytes;
    SpiceReadBytes spice_read_speeds;
    gulong spice_total_read_speed; // суммарная скорость

    // rdp speed
    UINT64 rdp_read_bytes; // in
    UINT64 rdp_write_bytes; // out
    guint64 rdp_read_speed; // bytes per second
    guint64 rdp_write_speed; // bytes per second

} NetworkStatsData;

typedef struct{
    GObject parent;

    GThread *ping_job_thread;
    gboolean is_ping_job_running;

    guint stats_check_event_source_id;
    VirtViewerApp *p_virt_viewer_app;
    rdpRdp* p_rdp; // Pointer to a rdp_rdp structure used to keep RDP connection's parameters.

    AtomicString vm_ip;
    NetworkStatsData nw; // network data

    gboolean is_ip_reachable;

} NetSpeedometer;

typedef struct
{
    GObjectClass parent_class;

    /* signals */
    void (*stats_data_updated)(NetSpeedometer *self, VdiVmRemoteProtocol protocol, NetworkStatsData *nw_data);
    void (*address_changed)(NetSpeedometer *self);

} NetSpeedometerClass;

GType net_speedometer_get_type( void ) G_GNUC_CONST;

NetSpeedometer *net_speedometer_new(void);
void net_speedometer_update_vm_ip(NetSpeedometer *self, const gchar *vm_ip);

void net_speedometer_set_pointer_to_virt_viewer_app(NetSpeedometer *self, VirtViewerApp *app);
void net_speedometer_set_pointer_rdp_context(NetSpeedometer *self, rdpRdp *rdp);

#endif //VEIL_CONNECT_NET_SPEEDOMETER_H
