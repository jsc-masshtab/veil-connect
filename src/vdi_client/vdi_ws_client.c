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
#include "vdi_event_types.h"

#define WS_RECONNECT_TIMEOUT 5000


// static functions declarations
static gboolean vdi_ws_client_ws_connect(VdiWsClient *vdi_ws_client);
static void vdi_ws_client_ws_reconnect_if_allowed(VdiWsClient *vdi_ws_client);


static const gchar *vdi_ws_client_vi_event_to_str(VdiEventType vd_event_type)
{
    switch (vd_event_type) {
        case VDI_EVENT_TYPE_UNKNOWN:
            return "";
        case VDI_EVENT_TYPE_VM_CHANGED:
            return "vm_changed";
        case VDI_EVENT_TYPE_CONN_ERROR:
            return "vm_connection_error";
        default:
            return "";
    }
}

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

            const gchar *resource = json_object_get_string_member_safely(root_object, "resource");
            // ретранслированное от вейла сообщение
            if (g_strcmp0(resource, "/domains/") == 0) {
                // object
                JsonObject *vm_member_object = json_object_get_object_member_safely(root_object, "object");
                int power_state = json_object_get_int_member_safely(vm_member_object, "user_power_state");
                vdi_session_vm_state_change_notify(power_state);

            } else if (g_strcmp0(resource, "/events_thin_client/") == 0) {
                const gchar *event = json_object_get_string_member_safely(root_object, "event");
                if (g_strcmp0(event, "vm_preparation_progress") == 0) {
                    int request_id = json_object_get_int_member_safely(root_object, "request_id");
                    int progress = json_object_get_int_member_safely(root_object, "progress");
                    const gchar *msg = json_object_get_string_member_safely(root_object, "msg");
                    vdi_session_vm_prep_progress_received_notify(request_id, progress, msg);
                }
            }

        } else if (g_strcmp0(msg_type, "control") == 0) {
            // команда от администратора  VeiL VDI
            const gchar *cmd = json_object_get_string_member_safely(root_object, "cmd");
            // Если пришла команда отключиться, то не делаем попытки рекконекта
            if (g_strcmp0(cmd, "DISCONNECT") == 0) {
                vdi_session_logout();
                vdi_ws_client->reconnect_if_conn_lost = FALSE;
                vdi_session_ws_cmd_received_notify(cmd);
            }
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

static void vdi_ws_client_on_close(SoupWebsocketConnection *ws_conn, VdiWsClient *vdi_ws_client)
{
    gushort close_code = soup_websocket_connection_get_close_code(ws_conn);
    g_info("WebSocket connection closed: close_code: %i", close_code);

    if (vdi_ws_client_get_conn_state(vdi_ws_client) == SOUP_WEBSOCKET_STATE_CLOSED) {
        const char *close_data = soup_websocket_connection_get_close_data(ws_conn);
        g_info("WebSocket close data: %s", close_data);
    }
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

        // remember conn time

        free_memory_safely(&vdi_ws_client->conn_time);
        vdi_ws_client->conn_time = get_current_readable_time();
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
    vdi_ws_client->ws_soup_session = soup_session_new_with_options("idle-timeout", 21, "timeout", 21,
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
    //GArray *query_params_dyn_array = g_array_new(уFALSE, FALSE, sizeof(gchar *));
    //g_array_append_val(query_params_dyn_array, rdp_param);

    g_autofree gchar *base_query_pars = NULL;
    g_autofree gchar *token = NULL;
    token = vdi_session_get_token();
    base_query_pars = g_strdup_printf("?token=%s&"
                                      "is_conn_init_by_user=%i&"
                                      "veil_connect_version=%s&"
                                      "tk_os=%s",
                                      token,
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

    // set small timeout before closing
    g_object_set(vdi_ws_client->ws_soup_session, "idle-timeout", 2, "timeout", 2, NULL);

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
    vdi_ws_client->ws_conn = NULL;
    g_object_unref(vdi_ws_client->ws_msg);
    g_object_unref(vdi_ws_client->cancel_job);
    g_object_unref(vdi_ws_client->ws_soup_session);

    free_memory_safely(&vdi_ws_client->vdi_url);

    free_memory_safely(&vdi_ws_client->conn_time);

    vdi_ws_client->is_running = FALSE;
    vdi_ws_client->reconnect_if_conn_lost = FALSE;
    g_info("%s end of function", (const char *)__func__);
}

SoupWebsocketState vdi_ws_client_get_conn_state(VdiWsClient *ws_vdi_client)
{
    if (ws_vdi_client->ws_conn)
        return soup_websocket_connection_get_state(ws_vdi_client->ws_conn);
    else
        return SOUP_WEBSOCKET_STATE_CLOSED;
}

const gchar *vdi_ws_client_get_conn_time(VdiWsClient *ws_vdi_client)
{
    return ws_vdi_client->conn_time;
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
    json_builder_add_string_value(builder, vdi_ws_client_vi_event_to_str(VDI_EVENT_TYPE_VM_CHANGED));

    json_builder_set_member_name(builder, "vm_id");
    json_builder_add_string_value(builder, vm_id);

    json_builder_set_member_name(builder, "connection_type");
    VmRemoteProtocol protocol = vdi_session_get_current_remote_protocol();
    if (vm_id == NULL || protocol == ANOTHER_REMOTE_PROTOCOL)
        json_builder_add_null_value(builder);
    else
        json_builder_add_string_value(builder, vdi_session_remote_protocol_to_str(protocol));

    json_builder_set_member_name(builder, "is_connection_secure");
    if (vm_id == NULL) {
        json_builder_add_null_value(builder);
    }
    else {
        // RDP соединения шифруются, другие нет.
        gboolean is_connection_secure = (protocol == RDP_PROTOCOL || protocol == RDP_WINDOWS_NATIVE_PROTOCOL);
        json_builder_add_boolean_value(builder, is_connection_secure);
    }

    json_builder_end_object(builder);

    gchar *tk_data = json_generate_from_builder(builder);
    g_info("%s: %s", (const char *)__func__, tk_data);

    // Send
    vdi_ws_client_send_text(ws_vdi_client, tk_data);

    // Free
    g_free(tk_data);
    g_object_unref(builder);
}

void vdi_ws_client_send_conn_error(VdiWsClient *ws_vdi_client, guint32 conn_error_code, const gchar *conn_error_str)
{
    if (!ws_vdi_client->ws_conn)
        return;

    // Generate
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "msg_type");
    json_builder_add_string_value(builder, "UPDATED");

    json_builder_set_member_name(builder, "event");
    json_builder_add_string_value(builder, vdi_ws_client_vi_event_to_str(VDI_EVENT_TYPE_CONN_ERROR));

    json_builder_set_member_name(builder, "vm_id");
    const gchar *vm_id = vdi_session_get_current_vm_id();
    json_builder_add_string_value(builder, vm_id);

    json_builder_set_member_name(builder, "connection_type");
    VmRemoteProtocol protocol = vdi_session_get_current_remote_protocol();
    if (vm_id == NULL || protocol == ANOTHER_REMOTE_PROTOCOL)
        json_builder_add_null_value(builder);
    else
        json_builder_add_string_value(builder, vdi_session_remote_protocol_to_str(protocol));

    json_builder_set_member_name(builder, "conn_error_code");
    json_builder_add_int_value(builder, conn_error_code);

    json_builder_set_member_name(builder, "conn_error_str");
    json_builder_add_string_value(builder, conn_error_str);

    json_builder_end_object(builder);

    gchar *tk_data = json_generate_from_builder(builder);
    g_info("%s: %s", (const char *)__func__, tk_data);

    // Send
    vdi_ws_client_send_text(ws_vdi_client, tk_data);

    // Free
    g_free(tk_data);
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

void vdi_ws_client_send_rdp_network_stats(VdiWsClient *ws_vdi_client, guint64 rdp_read_speed, guint64 rdp_write_speed,
                                          float min_rtt, float avg_rtt, float max_rtt, int loss_percentage)
{
    if (!ws_vdi_client->ws_conn)
        return;

    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "msg_type");
    json_builder_add_string_value(builder, "UPDATED");

    json_builder_set_member_name(builder, "event");
    json_builder_add_string_value(builder, "network_stats");

    json_builder_set_member_name(builder, "connection_type");
    json_builder_add_string_value(builder, "RDP");

    json_builder_set_member_name(builder, "read_speed");
    json_builder_add_int_value(builder, rdp_read_speed);

    json_builder_set_member_name(builder, "write_speed");
    json_builder_add_int_value(builder, rdp_write_speed);

    json_builder_set_member_name(builder, "min_rtt");
    json_builder_add_double_value(builder, min_rtt);

    json_builder_set_member_name(builder, "avg_rtt");
    json_builder_add_double_value(builder, avg_rtt);

    json_builder_set_member_name(builder, "max_rtt");
    json_builder_add_double_value(builder, max_rtt);

    json_builder_set_member_name(builder, "loss_percentage");
    json_builder_add_int_value(builder, loss_percentage);

    json_builder_end_object(builder);

    gchar *tk_data = json_generate_from_builder(builder);
    vdi_ws_client_send_text(ws_vdi_client, tk_data);

    // Free
    g_free(tk_data);
    g_object_unref(builder);
}

void vdi_ws_client_send_spice_network_stats(VdiWsClient *ws_vdi_client,
                                            SpiceReadBytes *spice_speeds, gulong total_read_speed,
                                            float min_rtt, float avg_rtt, float max_rtt, int loss_percentage)
{
    if (!ws_vdi_client->ws_conn)
        return;

    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "msg_type");
    json_builder_add_string_value(builder, "UPDATED");

    json_builder_set_member_name(builder, "event");
    json_builder_add_string_value(builder, "network_stats");

    json_builder_set_member_name(builder, "connection_type");
    json_builder_add_string_value(builder, "SPICE");

    json_builder_set_member_name(builder, "inputs");
    json_builder_add_int_value(builder, spice_speeds->bytes_inputs);

    json_builder_set_member_name(builder, "webdav");
    json_builder_add_int_value(builder, spice_speeds->bytes_webdav);

    json_builder_set_member_name(builder, "cursor");
    json_builder_add_int_value(builder, spice_speeds->bytes_cursor);

    json_builder_set_member_name(builder, "display");
    json_builder_add_int_value(builder, spice_speeds->bytes_display);

    json_builder_set_member_name(builder, "record");
    json_builder_add_int_value(builder, spice_speeds->bytes_record);

    json_builder_set_member_name(builder, "playback");
    json_builder_add_int_value(builder, spice_speeds->bytes_playback);

    json_builder_set_member_name(builder, "main");
    json_builder_add_int_value(builder, spice_speeds->bytes_main);

    json_builder_set_member_name(builder, "total");
    json_builder_add_int_value(builder, total_read_speed);

    json_builder_set_member_name(builder, "min_rtt");
    json_builder_add_double_value(builder, min_rtt);

    json_builder_set_member_name(builder, "avg_rtt");
    json_builder_add_double_value(builder, avg_rtt);

    json_builder_set_member_name(builder, "max_rtt");
    json_builder_add_double_value(builder, max_rtt);

    json_builder_set_member_name(builder, "loss_percentage");
    json_builder_add_int_value(builder, loss_percentage);

    json_builder_end_object(builder);

    gchar *tk_data = json_generate_from_builder(builder);
    vdi_ws_client_send_text(ws_vdi_client, tk_data);

    // Free
    g_free(tk_data);
    g_object_unref(builder);
}
