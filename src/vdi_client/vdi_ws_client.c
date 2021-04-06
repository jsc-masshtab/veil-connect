/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

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
static void vdi_ws_client_ws_reconnect_if_allowed(VdiWsClient *vdi_ws_client);

// implementations
//static void vdi_ws_client_on_pong(SoupWebsocketConnection *self,
//               GBytes                  *message,
//               gpointer                 user_data) {
//
//    g_info("!!!!!on_pong");
//}

// ws сообщения от брокера. Ожидаем json
static void vdi_ws_client_on_message(SoupWebsocketConnection *ws_conn G_GNUC_UNUSED, gint type, GBytes *message,
                                     VdiWsClient *vdi_ws_client)
{
    g_info("%s: on_message", (const char *)__func__);
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
        const gchar *msg_type = json_object_get_string_member_safely(root_object, "msg_type");

        if (g_strcmp0(msg_type, "data") == 0) {
            // ретранслированное от вейла сообщение
            // object
            JsonObject *vm_member_object = json_object_get_object_member_safely(root_object, "object");
            int power_state = json_object_get_int_member_safely(vm_member_object, "user_power_state");
            vdi_session_vm_state_change_notify(power_state);

        } else if (g_strcmp0(msg_type, "control") == 0) {
            // команда от администратора  VeiL VDI
            const gchar *cmd = json_object_get_string_member_safely(root_object, "cmd");
            // Если пришла команда отключиться, то не делаем попытки рекконекта
            if (g_strcmp0(cmd, "DISCONNECT") == 0)
                vdi_ws_client->reconnect_if_conn_lost = FALSE;
            vdi_session_ws_cmd_received_notify(cmd);
        } else if (g_strcmp0(msg_type, "text_msg") == 0) {
            // текстовое сообщение от администратора  VeiL VDI
            const gchar *sender_name = json_object_get_string_member_safely(root_object, "sender_name");
            const gchar *text_message = json_object_get_string_member_safely(root_object, "message");
            vdi_session_text_msg_received_notify(sender_name, text_message);
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
    gushort close_code = soup_websocket_connection_get_close_code(ws_conn);
    g_info("WebSocket connection closed: close_code: %i\n", close_code);
    vdi_session_ws_conn_change_notify(FALSE);
    vdi_ws_client_ws_reconnect_if_allowed(vdi_ws_client);
}

static void vdi_ws_client_send_text(VdiWsClient *ws_vdi_client, const char *text)
{
    SoupWebsocketState ws_state = vdi_ws_client_get_conn_state(ws_vdi_client);
    if (ws_state == SOUP_WEBSOCKET_STATE_OPEN)
        soup_websocket_connection_send_text(ws_vdi_client->ws_conn, text);
}

static void vdi_ws_client_on_connection(SoupSession *session, GAsyncResult *res, VdiWsClient *vdi_ws_client)
{
    g_info("!!!!!on_connection");
    GError *error = NULL;

    vdi_ws_client->ws_conn = soup_session_websocket_connect_finish(session, res, &error);
    if (error) {
        if (vdi_ws_client->ws_conn) {
            g_object_unref(vdi_ws_client->ws_conn);
            vdi_ws_client->ws_conn = NULL;
        }
        g_info("Error: %s\n", error->message);
        g_error_free(error);

        vdi_ws_client_ws_reconnect_if_allowed(vdi_ws_client);

    } else {
        g_object_set(vdi_ws_client->ws_conn, "keepalive-interval", 10, NULL);

        g_signal_connect(vdi_ws_client->ws_conn, "message", G_CALLBACK(vdi_ws_client_on_message), vdi_ws_client);
        g_signal_connect(vdi_ws_client->ws_conn, "closed", G_CALLBACK(vdi_ws_client_on_close), vdi_ws_client);
        //g_signal_connect(vdi_ws_client->ws_conn, "pong", G_CALLBACK(vdi_ws_client_on_pong), NULL);
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

static void vdi_ws_client_ws_reconnect_if_allowed(VdiWsClient *vdi_ws_client)
{
    // Reconnect if its not reconnecting yet
    //g_info("di_ws_client->is_running %i vdi_ws_client->reconnect_event_source_id %i",
    //       vdi_ws_client->is_running, vdi_ws_client->reconnect_event_source_id);
    if(vdi_ws_client->reconnect_if_conn_lost && vdi_ws_client->is_running &&
    vdi_ws_client->reconnect_event_source_id == 0) {
        vdi_ws_client->reconnect_event_source_id =
                g_timeout_add(WS_RECONNECT_TIMEOUT, (GSourceFunc) vdi_ws_client_ws_connect, vdi_ws_client);
    }
}

void vdi_ws_client_start(VdiWsClient *vdi_ws_client, const gchar *vdi_ip, int vdi_port)
{
    g_info("%s vdi_ws_client->is_running %i", (const char *)__func__, vdi_ws_client->is_running);
    if (vdi_ws_client->is_running)
        return;

    gboolean ssl_strict = FALSE;
    vdi_ws_client->ws_soup_session = soup_session_new_with_options("idle-timeout", 25, "timeout", 25,
                                                                   "ssl-strict", ssl_strict, NULL);

    // build URL
    const gchar *protocol = determine_http_protocol_by_port(vdi_port);

    g_autofree gchar *base_url = NULL;
    gboolean is_proxy_not_used = read_int_from_ini_file("General", "is_proxy_not_used", FALSE);
    if (is_proxy_not_used)
        base_url = g_strdup_printf("%s://%s:%i/ws/client", protocol, vdi_ip, vdi_port);
    else
        base_url = g_strdup_printf("%s://%s:%i/api/ws/client", protocol, vdi_ip, vdi_port);

    // add query parameters
    // Token for authentication on server.
    // Вместе с auth заодно пошлем информацию о тк
    // vm_id - это id машины к которой щас подключены
    // tk_os - os машины, где запущен тк
    //GArray *query_params_dyn_array = g_array_new(FALSE, FALSE, sizeof(gchar *));
    //g_array_append_val(query_params_dyn_array, rdp_param);

    g_autofree gchar *base_query_pars = NULL;
    base_query_pars = g_strdup_printf("?token=%s&"
                                      "is_conn_init_by_user=%i&"
                                      "veil_connect_version=%s&"
                                      "tk_os=%s",
                                      vdi_session_get_token(),
                                      vdi_ws_client->is_connect_initiated_by_user,
                                      VERSION,
                                      util_get_os_name());
    vdi_ws_client->is_connect_initiated_by_user = FALSE; // reset

    g_autofree gchar *query_pars = NULL;
    if (vdi_session_get_current_vm_id())
        query_pars = g_strdup_printf("%s&vm_id=%s", base_query_pars, vdi_session_get_current_vm_id());
    else
        query_pars = g_strdup(base_query_pars);

    vdi_ws_client->vdi_url = g_strdup_printf("%s/%s", base_url, query_pars);

    // msg
    // handshake preparation
    vdi_ws_client->ws_msg = soup_message_new("GET", vdi_ws_client->vdi_url);
    if (vdi_ws_client->ws_msg == NULL) {
        g_info("%s Cant parse message", (const char *)__func__);
        return;
    }

    vdi_ws_client->is_running = TRUE;
    vdi_ws_client->reconnect_if_conn_lost = TRUE;

    vdi_ws_client->cancel_job = g_cancellable_new();
    vdi_ws_client_ws_connect(vdi_ws_client);
}

void vdi_ws_client_stop(VdiWsClient *vdi_ws_client)
{
    g_info("vdi_ws_client_stop is_running %i", vdi_ws_client->is_running);
    if (!vdi_ws_client->is_running)
        return;

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

    free_memory_safely(&vdi_ws_client->vdi_url);

    vdi_ws_client->is_running = FALSE;
    vdi_ws_client->reconnect_if_conn_lost = FALSE;
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

    // Generate
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "msg_type");
    json_builder_add_string_value(builder, "UPDATED");

    json_builder_set_member_name(builder, "event");
    json_builder_add_string_value(builder, "vm_changed");

    json_builder_set_member_name(builder, "vm_id");
    json_builder_add_string_value(builder, vm_id);

    json_builder_set_member_name(builder, "connection_type");
    VdiVmRemoteProtocol protocol = vdi_session_get_current_remote_protocol();
    if (vm_id == NULL || protocol == VDI_ANOTHER_REMOTE_PROTOCOL)
        json_builder_add_string_value(builder, NULL);
    else
        json_builder_add_string_value(builder, vdi_session_remote_protocol_to_str(protocol));

    json_builder_set_member_name(builder, "is_connection_secure");
    // RDP соединения шифруются, другие нет.
    gboolean is_connection_secure = (protocol == VDI_RDP_PROTOCOL || protocol == VDI_RDP_WINDOWS_NATIVE_PROTOCOL);
    json_builder_add_boolean_value(builder, is_connection_secure);

    json_builder_end_object(builder);

    JsonGenerator *gen = json_generator_new();
    JsonNode * root = json_builder_get_root(builder);
    json_generator_set_root(gen, root);
    gchar *tk_data = json_generator_to_data(gen, NULL);
    g_info("%s: %s", (const char *)__func__, tk_data);

    // Send
    vdi_ws_client_send_text(ws_vdi_client, tk_data);

    // Free
    g_free(tk_data);
    json_node_free(root);
    g_object_unref(gen);
    g_object_unref(builder);
}

void vdi_ws_client_send_user_gui(VdiWsClient *ws_vdi_client)
{
    //g_info((const char *)__func__);
    if (!ws_vdi_client->ws_conn)
        return;

    const gchar *tk_data = ("{"
                            "\"msg_type\": \"UPDATED\", "
                            "\"event\": \"user_gui\""
                            "}");
    vdi_ws_client_send_text(ws_vdi_client, tk_data);
}
/*
void vdi_ws_client_send_text_msg(VdiWsClient *ws_vdi_client, const gchar *text_msg)
{
    if (!ws_vdi_client->ws_conn)
        return;

    // Generate
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "msg_type");
    json_builder_add_string_value(builder, "text_msg");

    json_builder_set_member_name(builder, "message");
    json_builder_add_string_value(builder, text_msg);

    json_builder_end_object (builder);

    JsonGenerator *gen = json_generator_new();
    JsonNode * root = json_builder_get_root(builder);
    json_generator_set_root(gen, root);
    gchar *tk_data = json_generator_to_data(gen, NULL);
    g_info("%s: %s", (const char *)__func__, tk_data);

    // Send
    vdi_ws_client_send_text(ws_vdi_client, tk_data);

    // Free
    g_free(tk_data);
    json_node_free(root);
    g_object_unref(gen);
    g_object_unref(builder);
}*/
