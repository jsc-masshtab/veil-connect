/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

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


const gchar *rdp_util_error_to_str(UINT32 rdp_error)
{
    if (rdp_error == WRONG_FREERDP_ARGUMENTS)
        return "Невалидные агрументы Freerdp";

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