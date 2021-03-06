/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef FREERDP_CLIENT_SAMPLE_CHANNELS_H
#define FREERDP_CLIENT_SAMPLE_CHANNELS_H

#include <freerdp/freerdp.h>
#include <freerdp/client/channels.h>

//int rdp_on_channel_connected(freerdp* instance, const char* name, void* pInterface);
//int rdp_on_channel_disconnected(freerdp* instance, const char* name, void* pInterface);

void rdp_OnChannelConnectedEventHandler(void* context, ChannelConnectedEventArgs* e);
void rdp_OnChannelDisconnectedEventHandler(void* context, ChannelDisconnectedEventArgs* e);

#endif /* FREERDP_CLIENT_SAMPLE_CHANNELS_H */
