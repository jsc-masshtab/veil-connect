/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef WS_VDI_CLIENT_H
#define WS_VDI_CLIENT_H

#include <libsoup/soup-session.h>
#include <libsoup/soup-message.h>
#include <glib.h>


typedef struct{
    SoupSession *ws_soup_session;

    gchar* vdi_url;

    SoupWebsocketConnection *ws_conn;
    SoupMessage *ws_msg;

    gboolean is_running;
    gboolean is_connect_initiated_by_user; // Флаг необходим, чтобы различить автоконнект и ручной коннект пользователем
    gboolean reconnect_if_conn_lost; // Флаг делать ли авторекконект, если соединение потеряно

    GCancellable *cancel_job;

    gint reconnect_event_source_id;

} VdiWsClient;


void vdi_ws_client_start(VdiWsClient *ws_vdi_client, const gchar *vdi_ip, int vdi_port);
void vdi_ws_client_stop(VdiWsClient *ws_vdi_client);

SoupWebsocketState vdi_ws_client_get_conn_state(VdiWsClient *ws_vdi_client);

// Сообщить  vdi серверу что произошло подключение кв вм/либо отключение от вм
// vm_id == NULL отключение от вм
void vdi_ws_client_send_vm_changed(VdiWsClient *ws_vdi_client, const gchar *vm_id);
// Сообщить  vdi серверу что юзер взаимодействовал с gui
// В Qt очень просто можно установить фильтр ивентов сразу на все приложение, в gtk такого не нашел.
// Поэтому вызываем этот метод везде, где хотим оповестить сервер об активности клиета
void vdi_ws_client_send_user_gui(VdiWsClient *ws_vdi_client);
// Послать тексотовое сообщение администратору VeiL VDI
//void vdi_ws_client_send_text_msg(VdiWsClient *ws_vdi_client, const gchar *text_msg);

#endif // WS_VDI_CLIENT_H
