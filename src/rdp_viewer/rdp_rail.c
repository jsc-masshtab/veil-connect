//
// Created by ubuntu on 17.09.2020.
//

//#include "freerdp/rail/rail.h"
#include <freerdp/version.h>

#include "rdp_rail.h"

#define RAIL_ERROR_ARRAY_SIZE 7
static const char* error_code_names[RAIL_ERROR_ARRAY_SIZE] = { "RAIL_EXEC_S_OK",
     "RAIL_EXEC_E_HOOK_NOT_LOADED (The server is not monitoring the current input desktop)",
     "RAIL_EXEC_E_DECODE_FAILED (The request PDU was malformed)",
     "RAIL_EXEC_E_NOT_IN_ALLOWLIST (The requested application was blocked by policy from being launched on the server)",
     "RAIL_EXEC_E_FILE_NOT_FOUND (The application or file path could not be found)",
     "RAIL_EXEC_E_FAIL (Wrong application name?)",
     "RAIL_EXEC_E_SESSION_LOCKED (The remote session is locked)" };

static UINT rdp_rail_server_start_cmd(RailClientContext* context)
{
    UINT status;
    RAIL_EXEC_ORDER exec = { 0 };
    RAIL_SYSPARAM_ORDER sysparam = { 0 };
    RAIL_CLIENT_STATUS_ORDER clientStatus = { 0 };
    ExtendedRdpContext* ex_context = (ExtendedRdpContext*)context->custom;
    rdpSettings* settings = ex_context->context.settings;

#if FREERDP_VERSION_MINOR == 0
    RAIL_HANDSHAKE_ORDER clientHandshake;
    clientHandshake.buildNumber = 0x00001DB0;
    context->ClientHandshake(context, &clientHandshake);

    clientStatus.flags = RAIL_CLIENTSTATUS_ALLOWLOCALMOVESIZE;

    if (settings->AutoReconnectionEnabled)
        clientStatus.flags |= RAIL_CLIENTSTATUS_AUTORECONNECT;
#endif
#if FREERDP_VERSION_MINOR >= 2
    clientStatus.flags = TS_RAIL_CLIENTSTATUS_ALLOWLOCALMOVESIZE;

    if (settings->AutoReconnectionEnabled)
        clientStatus.flags |= TS_RAIL_CLIENTSTATUS_AUTORECONNECT;

    clientStatus.flags |= TS_RAIL_CLIENTSTATUS_ZORDER_SYNC;
    clientStatus.flags |= TS_RAIL_CLIENTSTATUS_WINDOW_RESIZE_MARGIN_SUPPORTED;
    clientStatus.flags |= TS_RAIL_CLIENTSTATUS_APPBAR_REMOTING_SUPPORTED;
    clientStatus.flags |= TS_RAIL_CLIENTSTATUS_POWER_DISPLAY_REQUEST_SUPPORTED;
    clientStatus.flags |= TS_RAIL_CLIENTSTATUS_BIDIRECTIONAL_CLOAK_SUPPORTED;
#endif

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

static UINT rdp_rail_server_execute_result(RailClientContext* context,
                                          const RAIL_EXEC_RESULT_ORDER* execResult)
{
    ExtendedRdpContext* ex_context = (ExtendedRdpContext*)context->custom;

    if (execResult->execResult != RAIL_EXEC_S_OK) {
        g_info("RAIL exec error: execResult=%s NtError=0x%X\n",
               rail_error_to_string(execResult->execResult), execResult->rawResult);
        ex_context->rail_rdp_error = execResult->execResult;

        freerdp_abort_connect(ex_context->context.instance);

    } else {
        g_info("%s RAIL_EXEC_S_OK", (const char *)__func__);
    }

    return CHANNEL_RC_OK;
}

static UINT rdp_rail_server_system_param(RailClientContext* context G_GNUC_UNUSED,
                                        const RAIL_SYSPARAM_ORDER* sysparam G_GNUC_UNUSED)
{
    return CHANNEL_RC_OK;
}

static UINT rdp_rail_server_handshake(RailClientContext* context,
                                     const RAIL_HANDSHAKE_ORDER* handshake G_GNUC_UNUSED)
{
    return rdp_rail_server_start_cmd(context);
}

static UINT rdp_rail_server_handshake_ex(RailClientContext* context,
                                        const RAIL_HANDSHAKE_EX_ORDER* handshakeEx G_GNUC_UNUSED)
{
    return rdp_rail_server_start_cmd(context);
}

static UINT rdp_rail_server_min_max_info(RailClientContext* context G_GNUC_UNUSED,
                                        const RAIL_MINMAXINFO_ORDER* minMaxInfo G_GNUC_UNUSED)
{
    g_info("%s  windowId %i maxWidth %i maxHeight %i maxPosX %i maxPosY %i minTrackWidth %i "
           "minTrackHeight %i maxTrackWidth %i maxTrackHeight", (const char *)__func__,
           minMaxInfo->maxWidth, minMaxInfo->maxHeight,
           minMaxInfo->maxPosX, minMaxInfo->maxPosY, minMaxInfo->minTrackWidth,
           minMaxInfo->minTrackHeight, minMaxInfo->maxTrackWidth,
           minMaxInfo->maxTrackHeight);

    return CHANNEL_RC_OK;
}

static UINT rdp_rail_server_language_bar_info(RailClientContext* context G_GNUC_UNUSED,
                                             const RAIL_LANGBAR_INFO_ORDER* langBarInfo G_GNUC_UNUSED)
{
    return CHANNEL_RC_OK;
}

static UINT rdp_rail_server_get_appid_response(RailClientContext* context G_GNUC_UNUSED,
                                              const RAIL_GET_APPID_RESP_ORDER* getAppIdResp G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
    return CHANNEL_RC_OK;
}

static BOOL rdp_rail_window_create(rdpContext* context G_GNUC_UNUSED,
        const WINDOW_ORDER_INFO* orderInfo G_GNUC_UNUSED,
        const WINDOW_STATE_ORDER* windowState G_GNUC_UNUSED)
{
    //ExtendedRdpContext* ex_context = (ExtendedRdpContext*)context;

    UINT32 fieldFlags = orderInfo->fieldFlags;

    g_info("rdp_rail_window_create orderInfo->windowId: %i    fieldFlags: %i", orderInfo->windowId, fieldFlags);

    // Count open windows
    // fieldFlags & WINDOW_ORDER_STATE_NEW) && (fieldFlags & WINDOW_ORDER_TYPE_WINDOW
    // win 7    285335326  0x1101DF1E
    // win 8    285269790
    //if ((fieldFlags & WINDOW_ORDER_STATE_NEW) && (fieldFlags & WINDOW_ORDER_TYPE_WINDOW)) {
    //
    //    g_array_append_val(ex_context->app_windows_array, orderInfo->windowId);
    //    g_info("%s win_amount %i", (const char *) __func__, ex_context->app_windows_array->len);
    //
    //    g_info("windowState->numWindowRects: %i numVisibilityRects: %i showState: %i",
    //           windowState->numWindowRects, windowState->numVisibilityRects,
    //           windowState->showState);
    //}
    return TRUE;
}

static BOOL rdp_rail_window_update(rdpContext* context G_GNUC_UNUSED,
                                   const WINDOW_ORDER_INFO* orderInfo G_GNUC_UNUSED,
                                   const WINDOW_STATE_ORDER* windowState G_GNUC_UNUSED)
{
    //g_info("%s", (const char *)__func__);
    return TRUE;
}

static BOOL rdp_rail_window_delete(rdpContext* context G_GNUC_UNUSED, const WINDOW_ORDER_INFO* orderInfo G_GNUC_UNUSED)
{
    g_info("%s", (const char *) __func__);
    //ExtendedRdpContext* ex_context = (ExtendedRdpContext*)context;

    // orderInfo->fieldFlags & WINDOW_ORDER_TYPE_WINDOW
    /*if (orderInfo->fieldFlags == 553648128) {
        ex_context->app_windows_amount--;
        g_info("%s win_amount %i orderInfo->windowId: %i    fieldFlags: %i", (const char *) __func__,
               ex_context->app_windows_amount, orderInfo->windowId, orderInfo->fieldFlags);
    }*/

    //for(guint i = 0; i < ex_context->app_windows_array->len; ++i) {
    //    UINT32 window_id = g_array_index(ex_context->app_windows_array, UINT32, i);
    //    if (orderInfo->windowId == window_id) {
    //        g_array_remove_index_fast(ex_context->app_windows_array, i);
    //        break;
    //    }
    //}
    //
    //if (ex_context->app_windows_array->len == 0)
    //    freerdp_abort_connect(context->instance);
    return TRUE;
}

static BOOL rdp_rail_monitored_desktop(rdpContext* context G_GNUC_UNUSED,
        const WINDOW_ORDER_INFO* orderInfo G_GNUC_UNUSED,
        const MONITORED_DESKTOP_ORDER* monitoredDesktop G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
    return TRUE;
}

static BOOL rdp_rail_non_monitored_desktop(rdpContext* context G_GNUC_UNUSED,
        const WINDOW_ORDER_INFO* orderInfo G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
    return TRUE;
}

static void rdp_rail_register_update_callbacks(rdpUpdate* update)
{
    rdpWindowUpdate* window = update->window;
    window->WindowCreate = (pWindowCreate)rdp_rail_window_create;
    window->WindowUpdate = (pWindowUpdate)rdp_rail_window_update;
    window->WindowDelete = (pWindowDelete)rdp_rail_window_delete;
    //window->WindowIcon = xf_rail_window_icon;
    //window->WindowCachedIcon = xf_rail_window_cached_icon;
    //window->NotifyIconCreate = xf_rail_notify_icon_create;
    //window->NotifyIconUpdate = xf_rail_notify_icon_update;
    //window->NotifyIconDelete = xf_rail_notify_icon_delete;
    window->MonitoredDesktop = (pMonitoredDesktop)rdp_rail_monitored_desktop;
    window->NonMonitoredDesktop = (pNonMonitoredDesktop)rdp_rail_non_monitored_desktop;
}

int rdp_rail_init(ExtendedRdpContext* ex_rdp_context, RailClientContext* rail)
{
    rdpContext* context = (rdpContext*)ex_rdp_context;

    if (!ex_rdp_context || !rail)
        return 0;

    ex_rdp_context->rail = rail;
    rdp_rail_register_update_callbacks(context->update);
    rail->custom = (void*)ex_rdp_context;

    rail->ServerExecuteResult = rdp_rail_server_execute_result;
    rail->ServerSystemParam = rdp_rail_server_system_param;
    rail->ServerHandshake = rdp_rail_server_handshake;
    rail->ServerHandshakeEx = rdp_rail_server_handshake_ex;

    //rail->ServerLocalMoveSize = xf_rail_server_local_move_size;
    rail->ServerMinMaxInfo = rdp_rail_server_min_max_info;
    rail->ServerLanguageBarInfo = rdp_rail_server_language_bar_info;
    rail->ServerGetAppIdResponse = rdp_rail_server_get_appid_response;

    //ex_rdp_context->app_windows_array = g_array_new(FALSE, FALSE, sizeof(UINT32));

    return 1;
}

int rdp_rail_uninit(ExtendedRdpContext* ex_rdp_context, RailClientContext* rail)
{
    g_info("%s", (const char *)__func__);

    if (ex_rdp_context->rail) {
        ex_rdp_context->rail->custom = NULL;
        ex_rdp_context->rail = NULL;
    }

    //if (ex_rdp_context->app_windows_array) {
    //    g_array_free (ex_rdp_context->app_windows_array, TRUE);
    //    ex_rdp_context->app_windows_array = NULL;
    //}

    return 1;
}

const gchar *rail_error_to_string(UINT16 rail_error)
{
    if (rail_error >= 0 && rail_error < RAIL_ERROR_ARRAY_SIZE)
        return error_code_names[rail_error];
    else
        return "RAIL exec error: Unknown error";
}