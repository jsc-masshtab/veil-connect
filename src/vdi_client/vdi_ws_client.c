#include <stdio.h>
#include <libsoup/soup-session.h>
#include <libsoup/soup-message.h>
#include <libsoup/soup-websocket.h>
#include <libsoup/soup.h>
#include <gio/gio.h>

#include "remote-viewer-util.h"

#include "config.h"
#include "vdi_ws_client.h"
#include "settingsfile.h"
#include "vdi_session.h"

#define WS_RECONNECT_TIMEOUT 5000

// static functions declarations
static gboolean vdi_ws_client_ws_connect(VdiWsClient *vdi_ws_client);
static void vdi_ws_client_ws_reconnect(VdiWsClient *vdi_ws_client);

// implementations
//static void vdi_ws_client_on_pong(SoupWebsocketConnection *self,
//               GBytes                  *message,
//               gpointer                 user_data) {
//
//    g_info("!!!!!on_pong");
//}

// ws сообщения от брокера. Ожидаем json
static void vdi_ws_client_on_message(SoupWebsocketConnection *ws_conn G_GNUC_UNUSED, gint type, GBytes *message,
        gpointer data G_GNUC_UNUSED)
{
    g_info("!!!!!on_message");
    if (type == SOUP_WEBSOCKET_DATA_TEXT) { // we are expecting json
        gsize sz;
        const gchar *string_msg = g_bytes_get_data(message, &sz);
        g_info("Received text data: %s\n", string_msg);

        if (!string_msg)
            return;

        // parse msg
        JsonParser *parser = json_parser_new();
        JsonObject *root_object = get_root_json_object(parser, string_msg);
        if (!root_object) {
            g_object_unref(parser);
            return;
        }
        // type
        // data - в данный момент присылается полученоое от вейла сообщение
        const gchar *msg_type = json_object_get_string_member_safely(root_object, "msg_type");
        if (g_strcmp0(msg_type, "data") == 0) {
            // object
            JsonObject *vm_member_object = json_object_get_object_member_safely(root_object, "object");
            int power_state = json_object_get_int_member_safely(vm_member_object, "user_power_state");
            vdi_session_vm_state_change_notify(power_state);

        } else if (g_strcmp0(msg_type, "control") == 0) {
            const gchar *cmd = json_object_get_string_member_safely(root_object, "cmd");
            vdi_session_ws_cmd_received_notify(cmd);
        }

        g_object_unref(parser);

    } else if (type == SOUP_WEBSOCKET_DATA_BINARY) {

        g_info("Received binary data (not shown)\n");

    } else {
        g_info("Invalid data type: %d\n", type);
    }
}

static void vdi_ws_client_on_close(SoupWebsocketConnection *ws_conn G_GNUC_UNUSED, VdiWsClient *vdi_ws_client)
{
    g_info("WebSocket connection closed\n");
    vdi_session_ws_conn_change_notify(FALSE);
    vdi_ws_client_ws_reconnect(vdi_ws_client);
}

static void vdi_ws_client_on_connection(SoupSession *session, GAsyncResult *res, VdiWsClient *vdi_ws_client)
{
    g_info("!!!!!on_connection");
    GError *error = NULL;

    vdi_ws_client->ws_conn = soup_session_websocket_connect_finish(session, res, &error);
    if (error) {
        vdi_ws_client->ws_conn = NULL;
        g_info("Error: %s\n", error->message);
        g_error_free(error);

        vdi_ws_client_ws_reconnect(vdi_ws_client);

    } else {
        g_object_set(vdi_ws_client->ws_conn, "keepalive-interval", 10, NULL);

        g_signal_connect(vdi_ws_client->ws_conn, "message", G_CALLBACK(vdi_ws_client_on_message), NULL);
        g_signal_connect(vdi_ws_client->ws_conn, "closed", G_CALLBACK(vdi_ws_client_on_close), vdi_ws_client);
        //g_signal_connect(vdi_ws_client->ws_conn, "pong", G_CALLBACK(vdi_ws_client_on_pong), NULL);

        // Authentication on server. Needs wss or everybody will see the token
        // Вместе с auth заодно пошлем информацию о тк
        // vm_id - это id машины к которой щас подключены может быть NULL, что значит мы ни к чему
        // не подключены
        // tk_os - os машины, где запущен тк
        gchar *vm_id_json = string_to_json_value(vdi_session_get_current_vm_id());
        gchar *tk_os = string_to_json_value(util_get_os_name());
        gchar *auth_data = g_strdup_printf("{"
                                           "\"msg_type\": \"AUTH\", "
                                           "\"token\": \"%s\", "
                                           "\"veil_connect_version\": \"%s\", "
                                           "\"vm_id\": %s, "
                                           "\"tk_os\": %s"
                                           "}",
                                           vdi_session_get_token(),
                                           VERSION,
                                           vm_id_json,
                                           tk_os);
        soup_websocket_connection_send_text(vdi_ws_client->ws_conn, auth_data);
        g_free(auth_data);
        g_free(vm_id_json);
        g_free(tk_os);
    }
    // notify gui
    vdi_session_ws_conn_change_notify(error == NULL);
}

static gboolean vdi_ws_client_ws_connect(VdiWsClient *vdi_ws_client)
{
    soup_session_websocket_connect_async(
            vdi_ws_client->ws_soup_session,
            vdi_ws_client->ws_msg,
            "veil_connect_trusted_origin", NULL, vdi_ws_client->cancel_job,
            (GAsyncReadyCallback)vdi_ws_client_on_connection,
            vdi_ws_client
    );

    vdi_ws_client->reconnect_event_source_id = 0;
    return G_SOURCE_REMOVE;
}

static void vdi_ws_client_ws_reconnect(VdiWsClient *vdi_ws_client)
{
    // Reconnect if its not reconnecting yet
    //g_info("di_ws_client->is_running %i vdi_ws_client->reconnect_event_source_id %i",
    //       vdi_ws_client->is_running, vdi_ws_client->reconnect_event_source_id);
    if(vdi_ws_client->is_running && vdi_ws_client->reconnect_event_source_id == 0)
        vdi_ws_client->reconnect_event_source_id =
            g_timeout_add(WS_RECONNECT_TIMEOUT, (GSourceFunc)vdi_ws_client_ws_connect, vdi_ws_client);
}

void vdi_ws_client_start(VdiWsClient *vdi_ws_client, const gchar *vdi_ip, int vdi_port)
{
    g_info("%s vdi_ws_client->is_running %i", (const char *)__func__, vdi_ws_client->is_running);
    if (vdi_ws_client->is_running)
        return;

    gboolean ssl_strict = FALSE;
    vdi_ws_client->ws_soup_session = soup_session_new_with_options("idle-timeout", 25, "timeout", 25,
                                                                   "ssl-strict", ssl_strict, NULL);

    const gchar *protocol = (vdi_port == 443) ? "wss" : "ws";

    gboolean is_nginx_vdi_prefix_disabled = read_int_from_ini_file("General", "is_nginx_vdi_prefix_disabled", 0);
    if (is_nginx_vdi_prefix_disabled)
        vdi_ws_client->vdi_url = g_strdup_printf("%s://%s:%i/ws/client/vdi_server_check",
                                                 protocol, vdi_ip, vdi_port);
    else
        vdi_ws_client->vdi_url = g_strdup_printf("%s://%s:%i/%s/ws/client/vdi_server_check",
                                                 protocol, vdi_ip, vdi_port, NGINX_VDI_API_PREFIX);

    // msg
    // hand shake preparation
    vdi_ws_client->ws_msg = soup_message_new("GET", vdi_ws_client->vdi_url);
    if (vdi_ws_client->ws_msg == NULL) {
        g_info("%s Cant parse message", (const char *)__func__);
        return;
    }

    vdi_ws_client->is_running = TRUE;

    vdi_ws_client->cancel_job = g_cancellable_new();
    vdi_ws_client_ws_connect(vdi_ws_client);
}

void vdi_ws_client_stop(VdiWsClient *vdi_ws_client)
{
    g_info("vdi_ws_client_stop is_running %i", vdi_ws_client->is_running);
    if (!vdi_ws_client->is_running)
        return;

    vdi_ws_client->is_running = FALSE;

    // cancel reconnecting if its active
    if (vdi_ws_client->reconnect_event_source_id)
        g_source_remove(vdi_ws_client->reconnect_event_source_id);

    g_cancellable_cancel(vdi_ws_client->cancel_job);

    if (vdi_ws_client->ws_conn) {
        SoupWebsocketState ws_state = soup_websocket_connection_get_state(vdi_ws_client->ws_conn);
        if (ws_state == SOUP_WEBSOCKET_STATE_OPEN)
            soup_websocket_connection_close(vdi_ws_client->ws_conn, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);
    }

    soup_session_abort(vdi_ws_client->ws_soup_session);

    // free memory
    if (vdi_ws_client->ws_conn)
        g_object_unref(vdi_ws_client->ws_conn);
    g_object_unref(vdi_ws_client->ws_msg);
    g_object_unref(vdi_ws_client->cancel_job);
    g_object_unref(vdi_ws_client->ws_soup_session);

    g_free(vdi_ws_client->vdi_url);

    memset(vdi_ws_client, 0, sizeof(VdiWsClient));
    g_info("%s", (const char *)__func__);
}

SoupWebsocketState vdi_ws_client_get_conn_state(VdiWsClient *ws_vdi_client)
{
    if (ws_vdi_client->ws_conn)
        return soup_websocket_connection_get_state(ws_vdi_client->ws_conn);
    else
        return SOUP_WEBSOCKET_STATE_CLOSED;
}

void vdi_ws_client_send_vm_changed(VdiWsClient *ws_vdi_client, const gchar *vm_id)
{
    if (!ws_vdi_client->ws_conn)
        return;
    //g_info("vm_id: %s", vm_id);
    gchar *vm_id_json = string_to_json_value(vm_id);
    gchar *tk_data = g_strdup_printf("{"
                                     "\"msg_type\": \"UPDATED\", "
                                     "\"event\": \"vm_changed\", "
                                     "\"vm_id\": %s"
                                     "}",
                                     vm_id_json);
    soup_websocket_connection_send_text(ws_vdi_client->ws_conn, tk_data);
    g_free(tk_data);
    g_free(vm_id_json);
}

void vdi_ws_client_send_user_gui(VdiWsClient *ws_vdi_client)
{
    g_info((const char *)__func__);
    if (!ws_vdi_client->ws_conn)
        return;

    const gchar *tk_data = ("{"
                            "\"msg_type\": \"UPDATED\", "
                            "\"event\": \"user_gui\""
                            "}");
    soup_websocket_connection_send_text(ws_vdi_client->ws_conn, tk_data);
}