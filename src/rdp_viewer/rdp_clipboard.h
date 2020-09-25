//
// Created by ubuntu on 24.09.2020.
//

#ifndef VEIL_CONNECT_RDP_CLIPBOARD_H
#define VEIL_CONNECT_RDP_CLIPBOARD_H

#include "rdp_client.h"

typedef struct {
    ExtendedRdpContext *ex_context;
    CliprdrClientContext *context;
    int			requestedFormatId;

    UINT32			format;
    gulong			clipboard_handler;

    gpointer		srv_data;
} RdpClipboard;

void rdp_cliprdr_init(ExtendedRdpContext *ex_context, CliprdrClientContext *cliprdr);
void rdp_cliprdr_uninit(ExtendedRdpContext *ex_context, CliprdrClientContext* cliprdr);

#endif //VEIL_CONNECT_RDP_CLIPBOARD_H
