//
// Created by ubuntu on 17.09.2020.
//

#include "rdp_rail.h"

static UINT rdp_rail_server_start_cmd(RailClientContext* context)
{
    UINT status;
    RAIL_EXEC_ORDER exec = { 0 };
    RAIL_SYSPARAM_ORDER sysparam = { 0 };
    RAIL_CLIENT_STATUS_ORDER clientStatus = { 0 };
    ExtendedRdpContext* ex_context = (ExtendedRdpContext*)context->custom;
    rdpSettings* settings = ex_context->context.settings;
    clientStatus.flags = TS_RAIL_CLIENTSTATUS_ALLOWLOCALMOVESIZE;

    if (settings->AutoReconnectionEnabled)
        clientStatus.flags |= TS_RAIL_CLIENTSTATUS_AUTORECONNECT;

    clientStatus.flags |= TS_RAIL_CLIENTSTATUS_ZORDER_SYNC;
    clientStatus.flags |= TS_RAIL_CLIENTSTATUS_WINDOW_RESIZE_MARGIN_SUPPORTED;
    clientStatus.flags |= TS_RAIL_CLIENTSTATUS_APPBAR_REMOTING_SUPPORTED;
    clientStatus.flags |= TS_RAIL_CLIENTSTATUS_POWER_DISPLAY_REQUEST_SUPPORTED;
    clientStatus.flags |= TS_RAIL_CLIENTSTATUS_BIDIRECTIONAL_CLOAK_SUPPORTED;
    status = context->ClientInformation(context, &clientStatus);

    if (status != CHANNEL_RC_OK)
        return status;

    if (settings->RemoteAppLanguageBarSupported)
    {
        RAIL_LANGBAR_INFO_ORDER langBarInfo;
        langBarInfo.languageBarStatus = 0x00000008; /* TF_SFT_HIDDEN */
        status = context->ClientLanguageBarInfo(context, &langBarInfo);

        /* We want the language bar, but the server might not support it. */
        switch (status)
        {
            case CHANNEL_RC_OK:
            case ERROR_BAD_CONFIGURATION:
                break;
            default:
                return status;
        }
    }

    sysparam.params = 0;
    sysparam.params |= SPI_MASK_SET_HIGH_CONTRAST;
    sysparam.highContrast.colorScheme.string = NULL;
    sysparam.highContrast.colorScheme.length = 0;
    sysparam.highContrast.flags = 0x7E;
    sysparam.params |= SPI_MASK_SET_MOUSE_BUTTON_SWAP;
    sysparam.mouseButtonSwap = FALSE;
    sysparam.params |= SPI_MASK_SET_KEYBOARD_PREF;
    sysparam.keyboardPref = FALSE;
    sysparam.params |= SPI_MASK_SET_DRAG_FULL_WINDOWS;
    sysparam.dragFullWindows = FALSE;
    sysparam.params |= SPI_MASK_SET_KEYBOARD_CUES;
    sysparam.keyboardCues = FALSE;
    sysparam.params |= SPI_MASK_SET_WORK_AREA;
    sysparam.workArea.left = 0;
    sysparam.workArea.top = 0;
    sysparam.workArea.right = settings->DesktopWidth;
    sysparam.workArea.bottom = settings->DesktopHeight;
    sysparam.dragFullWindows = FALSE;
    status = context->ClientSystemParam(context, &sysparam);

    if (status != CHANNEL_RC_OK)
        return status;

    exec.RemoteApplicationProgram = settings->RemoteApplicationProgram;
    exec.RemoteApplicationWorkingDir = settings->ShellWorkingDirectory;
    exec.RemoteApplicationArguments = settings->RemoteApplicationCmdLine;
    return context->ClientExecute(context, &exec);
}

static UINT rdp_rail_server_system_param(RailClientContext* context,
                                        const RAIL_SYSPARAM_ORDER* sysparam)
{
    return CHANNEL_RC_OK;
}

static UINT rdp_rail_server_handshake(RailClientContext* context,
                                     const RAIL_HANDSHAKE_ORDER* handshake)
{
    return rdp_rail_server_start_cmd(context);
}

static UINT rdp_rail_server_handshake_ex(RailClientContext* context,
                                        const RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
    return rdp_rail_server_start_cmd(context);
}

int rdp_rail_init(ExtendedRdpContext* ex_rdp_context, RailClientContext* rail)
{
    rdpContext* context = (rdpContext*)ex_rdp_context;

    if (!ex_rdp_context || !rail)
        return 0;

    //ex_rdp_context->rail = rail;
    // xf_rail_register_update_callbacks(context->update);
    rail->custom = (void*)ex_rdp_context;


    //rail->ServerExecuteResult = xf_rail_server_execute_result;
    rail->ServerSystemParam = rdp_rail_server_system_param;
    rail->ServerHandshake = rdp_rail_server_handshake;
    rail->ServerHandshakeEx = rdp_rail_server_handshake_ex;

    
    //rail->ServerLocalMoveSize = xf_rail_server_local_move_size;
    //rail->ServerMinMaxInfo = xf_rail_server_min_max_info;
    //rail->ServerLanguageBarInfo = xf_rail_server_language_bar_info;
    //rail->ServerGetAppIdResponse = xf_rail_server_get_appid_response;


    //ex_rdp_context->railWindows = HashTable_New(TRUE);
    //
    //if (!ex_rdp_context->railWindows)
    //    return 0;
    //
    //ex_rdp_context->railWindows->keyCompare = rail_window_key_equals;
    //ex_rdp_context->railWindows->hash = rail_window_key_hash;
    //ex_rdp_context->railWindows->valueFree = rail_window_free;
    //ex_rdp_context->railIconCache = RailIconCache_New(ex_rdp_context->context.settings);
    //
    //if (!ex_rdp_context->railIconCache)
    //{
    //    HashTable_Free(ex_rdp_context->railWindows);
    //    return 0;
    //}


    return 1;
}

int rdp_rail_uninit(ExtendedRdpContext* ex_rdp_context, RailClientContext* rail)
{

}