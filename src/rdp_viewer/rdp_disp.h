/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef FREERDP_CLIENT_X11_DISP_H
#define FREERDP_CLIENT_X11_DISP_H

#include <gtk/gtk.h>

#include <freerdp/types.h>
#include <freerdp/client/disp.h>

typedef struct
{
    void *ex_context;
    DispClientContext *disp;
    BOOL haveXRandr;
    int eventBase, errorBase;
    int lastSentWidth, lastSentHeight;
    UINT64 lastSentDate;
    int targetWidth, targetHeight;
    BOOL activated;
    BOOL fullscreen;
    UINT16 lastSentDesktopOrientation;
    UINT32 lastSentDesktopScaleFactor;
    UINT32 lastSentDeviceScaleFactor;
} RdpDispContext;


BOOL disp_init(DispClientContext *disp, void *ex_context);
BOOL disp_uninit(RdpDispContext *xfDisp, DispClientContext *disp);

RdpDispContext* disp_new(void *context);
void disp_free(RdpDispContext* xfDisp);

void disp_update_layout(void *context, UINT32 targetWidth, UINT32 targetHeight);

#endif /* FREERDP_CLIENT_X11_DISP_H */
