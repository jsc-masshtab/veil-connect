/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <freerdp/locale/keyboard.h>
#include <freerdp/scancode.h>
#include <freerdp/error.h>

#include "rdp_util.h"

#ifndef ERRCONNECT_ACCOUNT_DISABLED
#define ERRCONNECT_ACCOUNT_DISABLED 0x00000012
#endif
#ifndef ERRCONNECT_PASSWORD_MUST_CHANGE
#define ERRCONNECT_PASSWORD_MUST_CHANGE 0x00000013
#endif
#ifndef ERRCONNECT_LOGON_FAILURE
#define ERRCONNECT_LOGON_FAILURE 0x00000014
#endif
#ifndef ERRCONNECT_WRONG_PASSWORD
#define ERRCONNECT_WRONG_PASSWORD 0x00000015
#endif
#ifndef ERRCONNECT_ACCESS_DENIED
#define ERRCONNECT_ACCESS_DENIED 0x00000016
#endif
#ifndef ERRCONNECT_ACCOUNT_RESTRICTION
#define ERRCONNECT_ACCOUNT_RESTRICTION 0x00000017
#endif
#ifndef ERRCONNECT_ACCOUNT_LOCKED_OUT
#define ERRCONNECT_ACCOUNT_LOCKED_OUT 0x00000018
#endif
#ifndef ERRCONNECT_ACCOUNT_EXPIRED
#define ERRCONNECT_ACCOUNT_EXPIRED 0x00000019
#endif
#ifndef ERRCONNECT_LOGON_TYPE_NOT_GRANTED
#define ERRCONNECT_LOGON_TYPE_NOT_GRANTED 0x0000001A
#endif
#ifndef ERRCONNECT_NO_OR_MISSING_CREDENTIALS
#define ERRCONNECT_NO_OR_MISSING_CREDENTIALS 0x0000001B
#endif

#define RAIL_ERROR_ARRAY_SIZE 7
static const char* error_code_names[RAIL_ERROR_ARRAY_SIZE] = {
        "RAIL_EXEC_S_OK",
        "RAIL_EXEC_E_HOOK_NOT_LOADED (The server is not monitoring the current input desktop)",
        "RAIL_EXEC_E_DECODE_FAILED (The request PDU was malformed). Wrong app name?",
        "RAIL_EXEC_E_NOT_IN_ALLOWLIST (The requested application was blocked by policy from being launched on the server)",
        "RAIL_EXEC_E_FILE_NOT_FOUND (The application or file path could not be found)",
        "RAIL_EXEC_E_FAIL (Wrong application name?)",
        "RAIL_EXEC_E_SESSION_LOCKED (The remote session is locked)"
};

const gchar *rail_error_to_string(UINT16 rail_error)
{
    if (rail_error < RAIL_ERROR_ARRAY_SIZE)
        return error_code_names[rail_error];
    else
        return "RAIL exec error: Unknown error";
}

const gchar *rdp_util_error_to_str(UINT32 rdp_error)
{
    if (rdp_error == WRONG_FREERDP_ARGUMENTS)
        return "Невалидные агрументы FreeRDP";

    //rdp_error = (rdp_error >= 0x10000 && rdp_error < 0x00020000) ? (rdp_error & 0xFFFF) : rdp_error;
    if (rdp_error >= 0x10000 && rdp_error < 0x00020000) {
        rdp_error = (rdp_error & 0xFFFF);

        switch (rdp_error) {
            case ERRINFO_RPC_INITIATED_DISCONNECT:
                return "ERRINFO_RPC_INITIATED_DISCONNECT";
            case ERRINFO_RPC_INITIATED_LOGOFF:
                return "ERRINFO_RPC_INITIATED_LOGOFF";
            case ERRINFO_IDLE_TIMEOUT:
                return "ERRINFO_IDLE_TIMEOUT";
            case ERRINFO_LOGON_TIMEOUT:
                return "ERRINFO_LOGON_TIMEOUT";
            case ERRINFO_DISCONNECTED_BY_OTHER_CONNECTION:
                return "ERRINFO_DISCONNECTED_BY_OTHER_CONNECTION";
            case ERRINFO_OUT_OF_MEMORY:
                return "ERRINFO_OUT_OF_MEMORY";
            case ERRINFO_SERVER_DENIED_CONNECTION:
                return "ERRINFO_SERVER_DENIED_CONNECTION";
            case ERRINFO_SERVER_INSUFFICIENT_PRIVILEGES:
                return "ERRINFO_SERVER_INSUFFICIENT_PRIVILEGES";
            case ERRINFO_SERVER_FRESH_CREDENTIALS_REQUIRED:
                return "ERRINFO_SERVER_FRESH_CREDENTIALS_REQUIRED";
            case ERRINFO_RPC_INITIATED_DISCONNECT_BY_USER:
                return "ERRINFO_RPC_INITIATED_DISCONNECT_BY_USER";
            case ERRINFO_LOGOFF_BY_USER:
                return "ERRINFO_LOGOFF_BY_USER";
//        case ERRINFO_CLOSE_STACK_ON_DRIVER_NOT_READY:
//            return "ERRINFO_CLOSE_STACK_ON_DRIVER_NOT_READY";
//        case ERRINFO_SERVER_DWM_CRASH:
//            return "ERRINFO_SERVER_DWM_CRASH";
//        case ERRINFO_CLOSE_STACK_ON_DRIVER_FAILURE:
//            return "ERRINFO_CLOSE_STACK_ON_DRIVER_FAILURE";
//        case ERRINFO_CLOSE_STACK_ON_DRIVER_IFACE_FAILURE:
//            return "ERRINFO_CLOSE_STACK_ON_DRIVER_IFACE_FAILURE";
//        case ERRINFO_SERVER_WINLOGON_CRASH:
//            return "ERRINFO_SERVER_WINLOGON_CRASH";
//        case ERRINFO_SERVER_CSRSS_CRASH:
//            return "ERRINFO_SERVER_CSRSS_CRASH";
            default:
                return "";
        }
    }
    else {
        rdp_error = (rdp_error & 0xFFFF);

        switch (rdp_error) {
            case ERRCONNECT_PRE_CONNECT_FAILED:
                return "ERRCONNECT_PRE_CONNECT_FAILED";
            case ERRCONNECT_CONNECT_UNDEFINED:
                return "ERRCONNECT_CONNECT_UNDEFINED";
            case ERRCONNECT_POST_CONNECT_FAILED:
                return "ERRCONNECT_POST_CONNECT_FAILED";
            case ERRCONNECT_DNS_ERROR:
                return "ERRCONNECT_DNS_ERROR";
            case ERRCONNECT_DNS_NAME_NOT_FOUND:
                return "ERRCONNECT_DNS_NAME_NOT_FOUND";
            case ERRCONNECT_CONNECT_FAILED:
                return "ERRCONNECT_CONNECT_FAILED";
            case ERRCONNECT_MCS_CONNECT_INITIAL_ERROR:
                return "ERRCONNECT_MCS_CONNECT_INITIAL_ERROR";
            case ERRCONNECT_TLS_CONNECT_FAILED:
                return "ERRCONNECT_TLS_CONNECT_FAILED";
            case ERRCONNECT_AUTHENTICATION_FAILED:
                return "ERRCONNECT_AUTHENTICATION_FAILED";
            case ERRCONNECT_INSUFFICIENT_PRIVILEGES:
                return "ERRCONNECT_INSUFFICIENT_PRIVILEGES";
            case ERRCONNECT_CONNECT_CANCELLED:
                return "ERRCONNECT_CONNECT_CANCELLED";
            case ERRCONNECT_SECURITY_NEGO_CONNECT_FAILED:
                return "ERRCONNECT_SECURITY_NEGO_CONNECT_FAILED";
            case ERRCONNECT_CONNECT_TRANSPORT_FAILED:
                return "ERRCONNECT_CONNECT_TRANSPORT_FAILED";
            case ERRCONNECT_PASSWORD_EXPIRED:
                return "ERRCONNECT_PASSWORD_EXPIRED";
                /* For non-domain workstation where we can't contact a kerberos server */
            case ERRCONNECT_PASSWORD_CERTAINLY_EXPIRED:
                return "ERRCONNECT_PASSWORD_CERTAINLY_EXPIRED";
            case ERRCONNECT_CLIENT_REVOKED:
                return "ERRCONNECT_CLIENT_REVOKED";
            case ERRCONNECT_KDC_UNREACHABLE:
                return "ERRCONNECT_KDC_UNREACHABLE";
            case ERRCONNECT_ACCOUNT_DISABLED:
                return "ERRCONNECT_ACCOUNT_DISABLED";
            case ERRCONNECT_PASSWORD_MUST_CHANGE:
                return "ERRCONNECT_PASSWORD_MUST_CHANGE";
            case ERRCONNECT_LOGON_FAILURE:
                return "ERRCONNECT_LOGON_FAILURE";
            case ERRCONNECT_WRONG_PASSWORD:
                return "ERRCONNECT_WRONG_PASSWORD";
            case ERRCONNECT_ACCESS_DENIED:
                return "ERRCONNECT_ACCESS_DENIED";
            case ERRCONNECT_ACCOUNT_RESTRICTION:
                return "ERRCONNECT_ACCOUNT_RESTRICTION";
            case ERRCONNECT_ACCOUNT_LOCKED_OUT:
                return "ERRCONNECT_ACCOUNT_LOCKED_OUT";
            case ERRCONNECT_ACCOUNT_EXPIRED:
                return "ERRCONNECT_ACCOUNT_EXPIRED";
            case ERRCONNECT_LOGON_TYPE_NOT_GRANTED:
                return "ERRCONNECT_LOGON_TYPE_NOT_GRANTED";
            case ERRCONNECT_NO_OR_MISSING_CREDENTIALS:
                return "ERRCONNECT_NO_OR_MISSING_CREDENTIALS";
            default:
                return "";
        }
    }
}

gboolean is_disconnect_intentional(UINT32 last_error)
{
    gboolean is_stop_intentional = FALSE;
    if (last_error >= 0x10000 && last_error < 0x00020000) {
        if ((last_error & 0xFFFF) == ERRINFO_LOGOFF_BY_USER ||
            (last_error & 0xFFFF) == ERRINFO_RPC_INITIATED_DISCONNECT_BY_USER)
            is_stop_intentional = TRUE;
    }

    return is_stop_intentional;
}

gchar *rdp_util_get_full_error_msg(UINT32 last_rdp_error, UINT32 rail_rdp_error)
{
    gchar *final_msg = NULL;
    g_autofree gchar *rdp_err_msg = NULL;
    g_autofree gchar *rail_rdp_err_msg = NULL;
    // "Нет соединения. Код: 0x%X %s"
    rdp_err_msg = g_strdup_printf(_("No Connection. Code: 0x%X %s"), last_rdp_error,
                                  rdp_util_error_to_str(last_rdp_error));

    if (rail_rdp_error) {
        // Ошибка удаленного приложения. 0x%X  %s
        rail_rdp_err_msg = g_strdup_printf(_("Remote app error. 0x%X  %s"), rail_rdp_error,
                                           rail_error_to_string(rail_rdp_error));
    } else {
        rail_rdp_err_msg = g_strdup("");
    }

    final_msg = g_strdup_printf("%s %s", rdp_err_msg, rail_rdp_err_msg);
    return final_msg;
}