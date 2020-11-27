//
// Created by solomin on 15.06.19.
//
#include <string.h>
#include <stdlib.h>

#include <libsoup/soup-session.h>
#include "remote-viewer-util.h"
#include <json-glib/json-glib.h>

#include "config.h"
#include "vdi_session.h"
#include "jsonhandler.h"
#include "settingsfile.h"

//#define RESPONSE_BUFFER_SIZE 200
#define OK_RESPONSE 200
#define ACCEPT_RESPONSE 202
#define BAD_REQUEST 400
#define AUTH_FAIL_RESPONSE 401

static VdiSession vdiSession;

static const gchar *bool_to_str(gboolean flag)
{
    return (flag ? "true" : "false");
}

static void free_session_memory()
{
    free_memory_safely(&vdiSession.vdi_username);
    free_memory_safely(&vdiSession.vdi_password);
    free_memory_safely(&vdiSession.vdi_ip);

    free_memory_safely(&vdiSession.api_url);
    free_memory_safely(&vdiSession.auth_url);
    free_memory_safely(&vdiSession.jwt);

    free_memory_safely(&vdiSession.current_pool_id);
    free_memory_safely(&vdiSession.current_vm_id);
    free_memory_safely(&vdiSession.current_vm_verbose_name);
    free_memory_safely(&vdiSession.current_controller_address);
}

static void setup_header_for_vdi_session_api_call(SoupMessage *msg)
{
    soup_message_headers_clear(msg->request_headers);
    gchar *authHeader = g_strdup_printf("jwt %s", vdiSession.jwt);
    //g_info("%s %s\n", (const char *)__func__, authHeader);
    soup_message_headers_append(msg->request_headers, "Authorization", authHeader);
    g_free(authHeader);
    soup_message_headers_append(msg->request_headers, "Content-Type", "application/json");
    soup_message_headers_append(msg->request_headers, "User-Agent", "thin-client");
}

static guint send_message(SoupMessage *msg)
{
    static int count = 0;
    g_info("Send_count: %i", ++count);

    guint status = soup_session_send_message(vdiSession.soup_session, msg);
    g_info("%s: Successfully sent. Response code: %i reason_phrase: %s",
            (const char *)__func__, status, msg->reason_phrase);
    return status;
}

// Вызывается в главном потоке  при смене jwt
static gboolean on_jwt_updated(gpointer data G_GNUC_UNUSED)
{
    vdi_ws_client_start(&vdiSession.vdi_ws_client, vdi_session_get_vdi_ip(), vdi_session_get_vdi_port());
    return G_SOURCE_REMOVE;
}

// Получаем токен
static gboolean vdi_api_session_get_token()
{
    g_info("%s", (const char *)__func__);

    if(vdiSession.auth_url == NULL)
        return FALSE;

    // create request message
    SoupMessage *msg = soup_message_new("POST", vdiSession.auth_url);
    if(msg == NULL) {
        g_info("%s Не удалось сформировать SoupMessage. Некорректный url?", (const char*)__func__);
        return FALSE;
    }

    // set header
    soup_message_headers_append(msg->request_headers, "Content-Type", "application/json");
    soup_message_headers_append(msg->request_headers, "User-Agent", "thin-client");

    // set body
    gchar *message_body_str = g_strdup_printf("{\"username\": \"%s\", \"password\": \"%s\", \"ldap\": %s}",
                                   vdiSession.vdi_username, vdiSession.vdi_password, bool_to_str(vdiSession.is_ldap));
    //g_info("%s  messageBodyStr %s", (const char *)__func__, messageBodyStr);
    soup_message_set_request(msg, "application/json",
                             SOUP_MEMORY_COPY, message_body_str, strlen_safely(message_body_str));
    g_free(message_body_str);

    // send message
    send_message(msg);

    // parse response
    g_info("msg->status_code %i", msg->status_code);
    //g_info("msg->response_body->data %s", msg->response_body->data);

    if(msg->status_code != OK_RESPONSE) {
        g_object_unref(msg);
        g_info("%s : Unable to get token", (const char *)__func__);
        return FALSE;
    }

    // extract reply
    JsonParser *parser = json_parser_new();

    ServerReplyType server_reply_type;
    JsonObject *reply_json_object = jsonhandler_get_data_or_errors_object(parser, msg->response_body->data,
            &server_reply_type);

    switch (server_reply_type) {
        case SERVER_REPLY_TYPE_DATA: {
            free_memory_safely(&vdiSession.jwt);
            vdiSession.jwt = g_strdup(json_object_get_string_member_safely(reply_json_object, "access_token"));
            // В основном потоке вызывается калбэк на смену токена
            g_timeout_add(50, (GSourceFunc)on_jwt_updated, NULL);

            g_object_unref(msg);
            g_object_unref(parser);
            return TRUE;
        }
        case SERVER_REPLY_TYPE_ERROR:
        case SERVER_REPLY_TYPE_UNKNOWN:
        default: {
            const gchar *message = json_object_get_string_member_safely(reply_json_object, "message");
            g_info("%s : Unable to get token. %s", (const char *)__func__, message);
            g_object_unref(msg);
            g_object_unref(parser);
            return FALSE;
        }
    }
}

// connect to reddis and subscribe for licence handling
static void vdi_api_session_register_for_license() {
    if (vdiSession.redis_client.is_subscribed)
        return;

    // do request to vdi server in order to get data fot Redis connection
    gchar *url_str = g_strdup_printf("%s/client/message_broker/", vdiSession.api_url);
    gchar *response_body_str = vdi_session_api_call("GET", url_str, NULL, NULL);
    // g_info("%s: response_body_str %s", (const char *) __func__, response_body_str);
    g_free(url_str);

    // parse the response
    JsonParser *parser = json_parser_new();

    ServerReplyType server_reply_type;
    JsonObject *data_member_object = jsonhandler_get_data_or_errors_object(parser, response_body_str,
                                                                           &server_reply_type);

    if (server_reply_type == SERVER_REPLY_TYPE_DATA) {

        vdi_redis_client_clear_connection_data(&vdiSession.redis_client);
        vdiSession.redis_client.adress = g_strdup(vdiSession.vdi_ip);
        vdiSession.redis_client.port = json_object_get_int_member_safely(data_member_object, "port");
        vdiSession.redis_client.password = g_strdup(
            g_strdup(json_object_get_string_member_safely(data_member_object, "password")));
        vdiSession.redis_client.channel = g_strdup(
            g_strdup(json_object_get_string_member_safely(data_member_object, "channel")));
        vdiSession.redis_client.db = json_object_get_int_member_safely(data_member_object, "db");

        // connect to Redis and subscribe to channel
        vdi_redis_client_init(&vdiSession.redis_client);
    }

    g_object_unref(parser);
}

void vdi_session_create()
{
    memset(&vdiSession, 0, sizeof(VdiSession));

    // create session
    gboolean ssl_strict = FALSE; // maybe read from ini file
    vdiSession.soup_session = soup_session_new_with_options("timeout", HTTP_RESPONSE_TIOMEOUT,
    "ssl-strict", ssl_strict, NULL);
    vdiSession.current_remote_protocol = VDI_SPICE_PROTOCOL; // by default
}

void vdi_session_destroy()
{
    // logout
    vdi_session_logout();

    vdi_session_cancell_pending_requests();

    // free memory
    g_object_unref(vdiSession.soup_session);
    free_session_memory();
}

const gchar *vdi_session_get_vdi_ip()
{
    return vdiSession.vdi_ip;
}

int vdi_session_get_vdi_port(void)
{
    return vdiSession.vdi_port;
}

const gchar *vdi_session_get_vdi_username(void)
{
    return vdiSession.vdi_username;
}

const gchar *vdi_session_get_vdi_password(void)
{
    return vdiSession.vdi_password;
}

const gchar *vdi_session_get_token(void)
{
    return vdiSession.jwt;
}

void vdi_session_cancell_pending_requests()
{
    soup_session_abort(vdiSession.soup_session);
}

void vdi_session_set_credentials(const gchar *username, const gchar *password, const gchar *ip,
                         int port, gboolean is_ldap)
{
    free_session_memory();

    vdiSession.vdi_username = g_strdup(username);
    vdiSession.vdi_password = g_strdup(password);
    vdiSession.vdi_ip = g_strdup(ip);
    vdiSession.vdi_port = port;

    const gchar *http_protocol = determine_http_protocol_by_port(port);

    gboolean is_nginx_vdi_prefix_disabled = read_int_from_ini_file("General", "is_nginx_vdi_prefix_disabled", 0);
    if (is_nginx_vdi_prefix_disabled)
        vdiSession.api_url = g_strdup_printf("%s://%s:%i", http_protocol, vdiSession.vdi_ip, vdiSession.vdi_port);
    else
        vdiSession.api_url = g_strdup_printf("%s://%s:%i/%s", http_protocol,
            vdiSession.vdi_ip, vdiSession.vdi_port, NGINX_VDI_API_PREFIX);


    vdiSession.auth_url = g_strdup_printf("%s/auth/", vdiSession.api_url);

    vdiSession.is_ldap = is_ldap;
}

void vdi_session_set_current_pool_id(const gchar *current_pool_id)
{
    free_memory_safely(&vdiSession.current_pool_id);
    vdiSession.current_pool_id = g_strdup(current_pool_id);
}

const gchar *vdi_session_get_current_pool_id()
{
    return vdiSession.current_pool_id;
}

const gchar *vdi_session_get_current_vm_id()
{
    return vdiSession.current_vm_id;
}

void vdi_session_set_current_remote_protocol(VdiVmRemoteProtocol remote_protocol)
{
    vdiSession.current_remote_protocol = remote_protocol;
}

VdiVmRemoteProtocol vdi_session_get_current_remote_protocol()
{
    return vdiSession.current_remote_protocol;
}

VdiVmRemoteProtocol vdi_session_str_to_remote_protocol(const gchar *protocol_str)
{
    VdiVmRemoteProtocol protocol = VDI_ANOTHER_REMOTE_PROTOCOL;
    if (g_strcmp0("SPICE", protocol_str) == 0)
        protocol = VDI_SPICE_PROTOCOL;
    else if (g_strcmp0("SPICE_DIRECT", protocol_str) == 0)
        protocol = VDI_SPICE_DIRECT_PROTOCOL;
    else if (g_strcmp0("RDP", protocol_str) == 0)
        protocol = VDI_RDP_PROTOCOL;
    else if (g_strcmp0("NATIVE_RDP", protocol_str) == 0)
        protocol = VDI_RDP_WINDOWS_NATIVE_PROTOCOL;

    return protocol;
}

const gchar *vdi_session_remote_protocol_str(VdiVmRemoteProtocol protocol)
{
    switch (protocol) {
        case VDI_SPICE_PROTOCOL:
            return "SPICE";
        case VDI_SPICE_DIRECT_PROTOCOL:
            return "SPICE_DIRECT";
        case VDI_RDP_PROTOCOL:
            return "RDP";
        case VDI_RDP_WINDOWS_NATIVE_PROTOCOL:
            return "NATIVE_RDP";
        case VDI_ANOTHER_REMOTE_PROTOCOL:
        default:
            return "UNKNOWN_PROTOCOL";
    }
}

// todo: в будущем перейти к заглавным буквам. Оставлено для совместимости со старыми версиями vdi
static const gchar *vdi_session_remote_protocol_to_str_old(VdiVmRemoteProtocol vm_remote_protocol)
{
    switch (vm_remote_protocol) {
        case VDI_SPICE_PROTOCOL:
            return "spice";
        case VDI_SPICE_DIRECT_PROTOCOL:
            return "spice_direct";
        case VDI_RDP_PROTOCOL:
            return "rdp";
        case VDI_RDP_WINDOWS_NATIVE_PROTOCOL:
            return "native_rdp";
        case VDI_ANOTHER_REMOTE_PROTOCOL:
        default:
            return "unknown_protocol";
    }
}

VdiWsClient *vdi_session_get_ws_client()
{
    return &vdiSession.vdi_ws_client;
}

const gchar *vdi_session_get_current_vm_name()
{
    return vdiSession.current_vm_verbose_name;
}

const gchar *vdi_session_get_current_controller_address()
{
    return vdiSession.current_controller_address;
}

gchar *vdi_session_api_call(const char *method, const char *uri_string, const gchar *body_str, int *resp_code)
{
    gchar *response_body_str = NULL;

    if (uri_string == NULL)
        return response_body_str;

    // get the token if we dont have it
    if (vdiSession.jwt == NULL)
        vdi_api_session_get_token();

    SoupMessage *msg = soup_message_new(method, uri_string);
    if (msg == NULL) // this may happen according to doc
        return response_body_str;

    // set header
    setup_header_for_vdi_session_api_call(msg);
    // set body
    if(body_str)
        soup_message_set_request(msg, "application/json",
                SOUP_MEMORY_COPY, body_str, strlen_safely(body_str));

    // start attempts
    const int max_attempt_count = 2;
    for(int attempt_count = 0; attempt_count < max_attempt_count; attempt_count++) {
        // send request.
        send_message(msg);
        g_info("msg->status_code: %i", msg->status_code);
        //g_info("msg->response_body: %s", msg->response_body->data);

        // При первой попытке если AUTH_FAIL_RESPONSE, то пробуем обновить токен и повторить
        if (msg->status_code == AUTH_FAIL_RESPONSE && attempt_count == 0) {
            vdi_api_session_get_token();
            gchar *auth_header = g_strdup_printf("jwt %s", vdiSession.jwt);
            soup_message_headers_replace(msg->request_headers, "Authorization", auth_header);
            g_free(auth_header);
            continue;
        }

        response_body_str = g_strdup(msg->response_body->data); // json_string_with_data. memory allocation!
        break;
    }

    g_object_unref(msg);

    if (resp_code)
        *resp_code = msg->status_code;
    return response_body_str;
}

void vdi_session_log_in(GTask       *task,
                   gpointer       source_object G_GNUC_UNUSED,
                   gpointer       task_data G_GNUC_UNUSED,
                   GCancellable  *cancellable G_GNUC_UNUSED)
{
    // get token
    free_memory_safely(&vdiSession.jwt);
    gboolean token_received = vdi_api_session_get_token();

    // register for licensing
    if (token_received)
        vdi_api_session_register_for_license();

    g_task_return_boolean(task, token_received);
}

void vdi_session_get_vdi_pool_data(GTask   *task,
                 gpointer       source_object G_GNUC_UNUSED,
                 gpointer       task_data G_GNUC_UNUSED,
                 GCancellable  *cancellable G_GNUC_UNUSED)
{
    //g_info("In %s :thread id = %lu\n", (const char *)__func__, pthread_self());
    gchar *url_str = g_strdup_printf("%s/client/pools", vdiSession.api_url);
    gchar *response_body_str = vdi_session_api_call("GET", url_str, NULL, NULL);
    g_free(url_str);

    g_task_return_pointer(task, response_body_str, NULL); // return pointer must be freed
}

void vdi_session_get_vm_from_pool(GTask       *task,
                    gpointer       source_object G_GNUC_UNUSED,
                    gpointer       task_data G_GNUC_UNUSED,
                    GCancellable  *cancellable G_GNUC_UNUSED)
{
    // register for licensing if its still not done
    vdi_api_session_register_for_license();

    // get vm from pool
    gchar *url_str = g_strdup_printf("%s/client/pools/%s", vdiSession.api_url, vdiSession.current_pool_id);
    // todo: use vdi_session_remote_protocol_str in future. Cant replace now cause some people use old vdi server
    gchar *bodyStr = g_strdup_printf("{\"remote_protocol\":\"%s\"}",
                                     vdi_session_remote_protocol_to_str_old(vdiSession.current_remote_protocol));

    gchar *response_body_str = vdi_session_api_call("POST", url_str, bodyStr, NULL);
    g_free(url_str);
    g_free(bodyStr);

    //response_body_str == NULL. didnt receive what we wanted
    if (!response_body_str) {
        g_task_return_pointer(task, NULL, NULL);
        return;
    }

    // parse response
    JsonParser *parser = json_parser_new();

    ServerReplyType server_reply_type;
    JsonObject *reply_json_object = jsonhandler_get_data_or_errors_object(parser, response_body_str,
            &server_reply_type);
    g_info("%s: server_reply_type %i", (const char *)__func__, server_reply_type);

    VdiVmData *vdi_vm_data = calloc(1, sizeof(VdiVmData));
    vdi_vm_data->server_reply_type = server_reply_type;

    if (server_reply_type == SERVER_REPLY_TYPE_DATA) {
        vdi_vm_data->vm_host = g_strdup(json_object_get_string_member_safely(reply_json_object, "host"));
        vdi_vm_data->vm_port = json_object_get_int_member_safely(reply_json_object, "port");
        vdi_vm_data->vm_password = g_strdup(json_object_get_string_member_safely(reply_json_object, "password"));
        vdi_vm_data->vm_verbose_name = g_strdup(json_object_get_string_member_safely(
                reply_json_object, "vm_verbose_name"));

        // save some data in vdiSession
        update_string_safely(&vdiSession.current_vm_verbose_name, vdi_vm_data->vm_verbose_name);
        const gchar *vm_controller_address =
                json_object_get_string_member_safely(reply_json_object, "vm_controller_address");
        update_string_safely(&vdiSession.current_controller_address, vm_controller_address);
        const gchar *vm_id = json_object_get_string_member_safely(reply_json_object, "vm_id");
        update_string_safely(&vdiSession.current_vm_id, vm_id);
        g_info("!!!vm_id: %s  %s", vm_id, vdiSession.current_vm_id);

        g_info("vm_host %s", vdi_vm_data->vm_host);
        g_info("vm_port %i", vdi_vm_data->vm_port);
        //g_info("vm_password %s", vdi_vm_data->vm_password);
        g_info("vm_verbose_name %s", vdi_vm_data->vm_verbose_name);
        g_info("current_controller_address %s", vdiSession.current_controller_address);
    }

    vdi_vm_data->message = g_strdup(json_object_get_string_member_safely(reply_json_object, "message"));
    g_info("%s: server_reply_type %s", (const char *)__func__, vdi_vm_data->message);

    // no point to parse if data is invalid
    g_object_unref(parser);
    free_memory_safely(&response_body_str);
    g_task_return_pointer(task, vdi_vm_data, NULL);
}

void vdi_session_do_action_on_vm(GTask      *task,
                  gpointer       source_object G_GNUC_UNUSED,
                  gpointer       task_data G_GNUC_UNUSED,
                  GCancellable  *cancellable G_GNUC_UNUSED)
{
    ActionOnVmData *action_on_vm_data = g_task_get_task_data(task);
    g_info("%s: %s", (const char *)__func__, action_on_vm_data->action_on_vm_str);

    // url
    gchar *url_str = g_strdup_printf("%s/client/pools/%s/%s", vdiSession.api_url,
                                    vdi_session_get_current_pool_id(), action_on_vm_data->action_on_vm_str);
    // body
    gchar *bodyStr;
    if(action_on_vm_data->is_action_forced)
        bodyStr = g_strdup_printf("{\"force\":true}");
    else
        bodyStr = g_strdup_printf("{\"force\":false}");

    gchar *response_body_str G_GNUC_UNUSED = vdi_session_api_call("POST", url_str, bodyStr, NULL);

    // free url and body
    free_memory_safely(&url_str);
    free_memory_safely(&bodyStr);
    // free ActionOnVmData
    vdi_api_session_free_action_on_vm_data(action_on_vm_data);
}

//void vdi_session_attach_tcp_usb(GTask         *task,
//                                gpointer       source_object,
//                                gpointer       task_data,
//                                GCancellable  *cancellable)
//{
//
//}

gboolean vdi_session_logout(void)
{
    // disconnect from license server(redis)
    vdi_redis_client_deinit(&vdiSession.redis_client);

    // stop websocket connection
    vdi_ws_client_stop(&vdiSession.vdi_ws_client);

    g_info("%s", (const char *)__func__);
    if (vdiSession.jwt) {
        gchar *url_str = g_strdup_printf("%s/logout", vdiSession.api_url);

        SoupMessage *msg = soup_message_new("POST", url_str);
        g_free(url_str);

        if (msg == NULL) {
            g_info("%s : Cant construct logout message", (const char *)__func__);
            return FALSE;

        } else {
            // set header
            setup_header_for_vdi_session_api_call(msg);
            // send
            g_object_set(vdiSession.soup_session, "timeout", 1, NULL);
            send_message(msg);
            g_object_set(vdiSession.soup_session, "timeout", HTTP_RESPONSE_TIOMEOUT, NULL);

            guint res_code = msg->status_code;
            g_object_unref(msg);

            if (res_code == OK_RESPONSE) {
                // logout was succesfull so we can foget the token
                free_memory_safely(&vdiSession.jwt);
                return TRUE;
            }
            else
                return FALSE;
        }

    } else {
        g_info("%s : No token info", (const char *)__func__);
        return FALSE;
    }
}

// Добавляет USB TCP и в случае успеха возвращает usb_uuid. NULL при неудаче
gchar *vdi_session_attach_usb(AttachUsbData *attach_usb_data)
{
    g_info("%s", (const char*)__func__);
    g_usleep(1000000); // temp
    gchar *usb_uuid = NULL;

    // url
    gchar *url_str = g_strdup_printf("%s/client/pools/%s/attach-usb", vdiSession.api_url,
                                     vdi_session_get_current_pool_id());

    // body
    gchar *body_str = g_strdup_printf("{\"host_address\":\"%s\",\"host_port\":%i}",
            attach_usb_data->host_address, attach_usb_data->host_port);
    g_info("%s send body_str: %s", (const char *)__func__, body_str);

    // request
    gchar *response_body_str = vdi_session_api_call("POST", url_str, body_str, NULL);

    // parse
    if (response_body_str) {
        JsonParser *parser = json_parser_new();

        JsonObject *root_object = get_root_json_object(parser, response_body_str);
        JsonArray *tcp_usb_devices_array = json_object_get_array_member_safely(root_object, "tcp_usb_devices");

        if (tcp_usb_devices_array) {
            guint tcp_usb_amount = MIN(json_array_get_length(tcp_usb_devices_array), 20);

            // Находим наш девайс
            for (int i = 0; i < (int) tcp_usb_amount; ++i) {
                JsonObject *array_element = json_array_get_object_element(tcp_usb_devices_array, i);

                const gchar *host_address = json_object_get_string_member_safely(array_element, "host");
                const gchar *host_port = json_object_get_string_member_safely(array_element, "service");
                g_info("TCP USB ARRAY %s %s ", host_address, host_port);
                if (g_strcmp0(host_address, attach_usb_data->host_address) == 0 &&
                    atoi(host_port) == attach_usb_data->host_port) {
                    usb_uuid = g_strdup(json_object_get_string_member_safely(array_element, "uuid"));
                    g_info("TCP USB Device found in reply. usb_uuid %s ", usb_uuid);
                    break;
                }
            }
        }

        g_info("%s response_body_str: %s", (const char *)__func__, response_body_str);

        g_object_unref(parser);
    }

    // Для  остановки основного потока текущего USB
    if (!usb_uuid) {
        g_info("%s Lower flag. *(attach_usb_data->p_running_flag) = 0 ", (const char*)__func__);
        *(attach_usb_data->p_running_flag) = 0;

    }

    // free
    free_memory_safely(&url_str);
    free_memory_safely(&body_str);
    free_memory_safely(&response_body_str);
    vdi_api_session_free_attach_usb_data(attach_usb_data);

    return usb_uuid;
}

// Убирает USB TCP и в случае успеха возвращает TRUE
gboolean vdi_session_detach_usb(DetachUsbData *detach_usb_data)
{
    g_info("%s", (const char*)__func__);
    gboolean status = FALSE;

    // url
    gchar *url_str = g_strdup_printf("%s/client/pools/%s/detach-usb", vdiSession.api_url,
                                     vdi_session_get_current_pool_id());

    // body
    gchar *body_str;
    if (detach_usb_data->remove_all) {
        body_str = g_strdup_printf("{\"remove_all\":true}");
    } else {
        body_str = g_strdup_printf("{\"usb_uuid\":\"%s\",\"remove_all\":false}", detach_usb_data->usb_uuid);
    }

    // request
    int resp_code;
    gchar *response_body_str = vdi_session_api_call("POST", url_str, body_str, &resp_code);

    if (resp_code == OK_RESPONSE || resp_code == ACCEPT_RESPONSE)
        status = TRUE;
    // parse
    /*
    if (response_body_str) {
        g_info("%s response_body_str: %s", (const char *)__func__, response_body_str);

        JsonParser *parser = json_parser_new();

        ServerReplyType server_reply_type;
        jsonhandler_get_data_or_errors_object(parser, response_body_str, &server_reply_type);
        if (server_reply_type == SERVER_REPLY_TYPE_DATA) {
            status = TRUE;
        }

        g_object_unref(parser);
    }
     */

    // free
    free_memory_safely(&url_str);
    free_memory_safely(&body_str);
    free_memory_safely(&response_body_str);
    vdi_api_session_free_detach_usb_data(detach_usb_data);

    return status;
}

void vdi_api_session_execute_task_do_action_on_vm(const gchar *actionStr, gboolean isForced)
{
    ActionOnVmData *action_on_vm_data = malloc(sizeof(ActionOnVmData));
    action_on_vm_data->action_on_vm_str = g_strdup(actionStr);
    action_on_vm_data->is_action_forced = isForced;

    execute_async_task(vdi_session_do_action_on_vm, NULL, action_on_vm_data, NULL);
}

void vdi_api_session_free_action_on_vm_data(ActionOnVmData *action_on_vm_data)
{
    free_memory_safely(&action_on_vm_data->action_on_vm_str);
    free(action_on_vm_data);
}

void vdi_api_session_free_vdi_vm_data(VdiVmData *vdi_vm_data)
{
    free_memory_safely(&vdi_vm_data->vm_host);
    free_memory_safely(&vdi_vm_data->vm_password);
    free_memory_safely(&vdi_vm_data->message);
    free_memory_safely(&vdi_vm_data->vm_verbose_name);
    free(vdi_vm_data);
}

void vdi_api_session_free_attach_usb_data(AttachUsbData *attach_usb_data)
{
    free_memory_safely(&attach_usb_data->host_address);
    free(attach_usb_data);
}

void vdi_api_session_free_detach_usb_data(DetachUsbData *detach_usb_data)
{
    free_memory_safely(&detach_usb_data->usb_uuid);
    free(detach_usb_data);
}
