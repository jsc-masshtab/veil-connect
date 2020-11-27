//
// Created by solomin on 21.08.19.
//


#ifndef WS_VDI_CLIENT_H
#define WS_VDI_CLIENT_H

#include <libsoup/soup-session.h>
#include <libsoup/soup-message.h>
#include <glib.h>

typedef gboolean (*WsDataReceivedCallback) (gboolean is_vdi_online);

typedef struct{
    SoupSession *ws_soup_session;
    WsDataReceivedCallback ws_is_connected_callback;

    gchar* vdi_url;

    SoupWebsocketConnection *ws_conn;
    SoupMessage *ws_msg;

    gboolean is_running;

    GCancellable *cancel_job;

    gint reconnect_event_source_id;

} VdiWsClient;


void vdi_ws_client_start(VdiWsClient *ws_vdi_client, const gchar *vdi_ip, int vdi_port);
void vdi_ws_client_stop(VdiWsClient *ws_vdi_client);
void vdi_ws_client_set_is_connected_callback(VdiWsClient *vdi_ws_client,
        WsDataReceivedCallback ws_is_connected_callback);

SoupWebsocketState vdi_ws_client_get_conn_state(VdiWsClient *ws_vdi_client);

// Сообщить  vdi серверу что произошло подключение кв вм/либо отключение от вм
// vm_id == NULL отключение от вм
void vdi_ws_client_notify_vm_changed(VdiWsClient *ws_vdi_client, const gchar *vm_id);

#endif // WS_VDI_CLIENT_H
