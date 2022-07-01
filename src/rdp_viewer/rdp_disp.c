/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <winpr/sysinfo.h>

#include "rdp_disp.h"
#include "rdp_client.h"


static void disp_OnActivated(void* context G_GNUC_UNUSED, ActivatedEventArgs* e G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
}

static void disp_OnGraphicsReset(void* context G_GNUC_UNUSED, GraphicsResetEventArgs* e G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
}

static void disp_OnTimer(void* context G_GNUC_UNUSED, TimerEventArgs* e G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
}

BOOL disp_init(DispClientContext* disp, void *ex_context)
{
    RdpDispContext* rdp_disp = disp_new(ex_context);

	if (!rdp_disp->ex_context || !disp)
		return FALSE;

    rdpSettings* settings = ((ExtendedRdpContext *)rdp_disp->ex_context)->context.settings;

	if (!settings)
		return FALSE;

	rdp_disp->disp = disp;
	disp->custom = (void*)rdp_disp;

	return TRUE;
}

BOOL disp_uninit(RdpDispContext* rdp_disp, DispClientContext* disp G_GNUC_UNUSED)
{
	if (!rdp_disp)
		return FALSE;

	rdp_disp->disp = NULL;
    disp_free(rdp_disp);
	return TRUE;
}

RdpDispContext* disp_new(void *context)
{
    RdpDispContext* rd_disp;

    ExtendedRdpContext *ex_context = (ExtendedRdpContext *)context;

    if (!ex_context || !ex_context->context.settings || !ex_context->context.pubSub)
        return NULL;

    rd_disp = calloc(1, sizeof(RdpDispContext));

    if (!rd_disp)
        return NULL;

    rd_disp->ex_context = ex_context;
    ex_context->rdp_disp = rd_disp;

    rd_disp->lastSentWidth = rd_disp->targetWidth = ex_context->context.settings->DesktopWidth;
    rd_disp->lastSentHeight = rd_disp->targetHeight = ex_context->context.settings->DesktopHeight;

    PubSub_SubscribeActivated(ex_context->context.pubSub, disp_OnActivated);
    PubSub_SubscribeGraphicsReset(ex_context->context.pubSub, disp_OnGraphicsReset);
    PubSub_SubscribeTimer(ex_context->context.pubSub, disp_OnTimer);
    return rd_disp;
}

void disp_free(RdpDispContext* rdp_disp)
{
    if (!rdp_disp)
        return;

    ExtendedRdpContext *ex_context = (ExtendedRdpContext *)rdp_disp->ex_context;
    if (ex_context)
    {
        PubSub_UnsubscribeActivated(ex_context->context.pubSub, disp_OnActivated);
        PubSub_UnsubscribeGraphicsReset(ex_context->context.pubSub, disp_OnGraphicsReset);
        PubSub_UnsubscribeTimer(ex_context->context.pubSub, disp_OnTimer);
    }

    free(rdp_disp);
}

void disp_update_layout(void *context, UINT32 targetWidth, UINT32 targetHeight)
{
    ExtendedRdpContext *ex_context = (ExtendedRdpContext *)context;

    rdpSettings* settings = ex_context->context.settings;
    if (settings->SmartSizingWidth != targetWidth ||
        settings->SmartSizingHeight != targetHeight)
    {
        DISPLAY_CONTROL_MONITOR_LAYOUT layout = { 0 };

        layout.Flags = DISPLAY_CONTROL_MONITOR_PRIMARY;
        layout.Top = layout.Left = 0;
        layout.Width = targetWidth;
        layout.Height = targetHeight;
        layout.Orientation = settings->DesktopOrientation;
        layout.DesktopScaleFactor = settings->DesktopScaleFactor;
        layout.DeviceScaleFactor = settings->DeviceScaleFactor;
        layout.PhysicalWidth = targetWidth;
        layout.PhysicalHeight = targetHeight;

        if (ex_context->rdp_disp) {
            if (IFCALLRESULT(CHANNEL_RC_OK, ex_context->rdp_disp->disp->SendMonitorLayout,
                             ex_context->rdp_disp->disp, 1, &layout) != CHANNEL_RC_OK) {
                g_warning("SendMonitorLayout failed.");
            }
        }
        settings->SmartSizingWidth = targetWidth;
        settings->SmartSizingHeight = targetHeight;
    }
}