/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_RDP_CLIPBOARD_H
#define VEIL_CONNECT_RDP_CLIPBOARD_H


#include "rdp_client.h"

typedef struct {
    ExtendedRdpContext *ex_context;
    CliprdrClientContext *context;
    int			requestedFormatId;

    UINT32			format;
    gulong			clipboard_handler;

    GMutex transfer_clip_mutex;
    GCond transfer_clip_cond;
    gboolean is_transfered;
    gboolean waiting_for_transfered_data;

} RdpClipboard;

void rdp_cliprdr_init(ExtendedRdpContext *ex_context, CliprdrClientContext *cliprdr);
void rdp_cliprdr_uninit(ExtendedRdpContext *ex_context, CliprdrClientContext* cliprdr);

#endif //VEIL_CONNECT_RDP_CLIPBOARD_H
