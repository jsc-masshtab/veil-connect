/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef RDP_DISPLAY_H
#define RDP_DISPLAY_H

#include <gtk/gtk.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/utils/signal.h>

#include "rdp_client.h"


#define TYPE_RDP_DISPLAY  (rdp_display_get_type ())
#define RDP_DISPLAY(obj)  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_RDP_DISPLAY, RdpDisplay))


typedef struct
{
    GtkDrawingArea parent_instance;

    ExtendedRdpContext *ex_context; // weak ref (not owning)
    GdkRectangle geometry;

} RdpDisplay;

typedef struct
{
    GtkDrawingAreaClass parent_class;
} RdpDisplayClass;

GType rdp_display_get_type(void) G_GNUC_CONST;

RdpDisplay *rdp_display_new(ExtendedRdpContext *ex_context, GdkRectangle geometry);

#endif // RDP_DISPLAY_H
