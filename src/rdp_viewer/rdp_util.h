/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef RDP_UTIL_H
#define RDP_UTIL_H


#ifndef WRONG_FREERDP_ARGUMENTS
#define WRONG_FREERDP_ARGUMENTS 0x1A0A
#endif

#include <gtk/gtk.h>

const gchar *rdp_util_error_to_str(UINT32 rdp_error);


#endif // RDP_UTIL_H
