//
// Created by ubuntu on 17.09.2020.
//

#ifndef VEIL_CONNECT_RDP_RAIL_H
#define VEIL_CONNECT_RDP_RAIL_H

#include <freerdp/client/rail.h>

#include "rdp_client.h"

int rdp_rail_init(ExtendedRdpContext* xfc, RailClientContext* rail);
int rdp_rail_uninit(ExtendedRdpContext* xfc, RailClientContext* rail);

#endif //VEIL_CONNECT_RDP_RAIL_H
