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

const gchar *rail_error_to_string(UINT16 rail_error);
const gchar *rdp_util_error_to_str(UINT32 rdp_error);
gboolean is_disconnect_intentional(UINT32 last_error);
// Create full error message. Allocate memory. Используется после дисконнекта
gchar *rdp_util_get_full_error_msg(UINT32 last_rdp_error, UINT32 rail_rdp_error);

#endif // RDP_UTIL_H
