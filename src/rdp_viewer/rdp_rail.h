/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_RDP_RAIL_H
#define VEIL_CONNECT_RDP_RAIL_H

#include <freerdp/client/rail.h>

#include "rdp_client.h"

int rdp_rail_init(ExtendedRdpContext* xfc, RailClientContext* rail);
int rdp_rail_uninit(ExtendedRdpContext* xfc, RailClientContext* rail);

const gchar *rail_error_to_string(UINT16 rail_error);

#endif //VEIL_CONNECT_RDP_RAIL_H
