/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GTK GUI
 * Solomin a.solomin@mashtab.otg
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/gdi/gfx.h>

#include <freerdp/client/rdpei.h>
#include <freerdp/client/tsmf.h>
#include <freerdp/client/rail.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/client/encomsp.h>

#include "rdp_channels.h"
#include "rdp_client.h"
#include "rdp_rail.h"

#include "remote-viewer-util.h"

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
rdp_encomsp_participant_created(EncomspClientContext* context,
                               const ENCOMSP_PARTICIPANT_CREATED_PDU* participantCreated)
{
	/* WINPR_UNUSED(context);
	WINPR_UNUSED(participantCreated); */
	return CHANNEL_RC_OK;
}

static void rdp_encomsp_init(ExtendedRdpContext* ex_context, EncomspClientContext* encomsp)
{
	ex_context->encomsp = encomsp;
	encomsp->custom = (void*)ex_context;
    encomsp->ParticipantCreated = (pcEncomspParticipantCreated)rdp_encomsp_participant_created;
}

static void rdp_encomsp_uninit(ExtendedRdpContext* ex_context, EncomspClientContext* encomsp)
{
	if (encomsp)
	{
		encomsp->custom = NULL;
		encomsp->ParticipantCreated = NULL;
	}

	if (ex_context)
		ex_context->encomsp = NULL;
}

void rdp_OnChannelConnectedEventHandler(void* context, ChannelConnectedEventArgs* e)
{
    ExtendedRdpContext* ex_context = (ExtendedRdpContext*)context;
    g_info("%s Channel %s", (const char*)__func__, e->name);

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		ex_context->rdpei = (RdpeiClientContext*)e->pInterface;
	}
	else if (strcmp(e->name, TSMF_DVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		gdi_graphics_pipeline_init(ex_context->context.gdi, (RdpgfxClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
        //e->pInterface;
        rdp_rail_init(ex_context, (RailClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
	    g_info("strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0");
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
        rdp_encomsp_init(ex_context, (EncomspClientContext*)e->pInterface);
	}
}

void rdp_OnChannelDisconnectedEventHandler(void* context, ChannelDisconnectedEventArgs* e)
{
    ExtendedRdpContext* ex_context = (ExtendedRdpContext*)context;

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
		ex_context->rdpei = NULL;
	}
	else if (strcmp(e->name, TSMF_DVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		gdi_graphics_pipeline_uninit(ex_context->context.gdi, (RdpgfxClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, RAIL_SVC_CHANNEL_NAME) == 0)
	{
        rdp_rail_uninit(ex_context, (RailClientContext*)e->pInterface);
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
        rdp_encomsp_uninit(ex_context, (EncomspClientContext*)e->pInterface);
	}
}
