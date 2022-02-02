/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <stdlib.h>

#include <glib/gi18n.h>

#include "remote-viewer-util.h"
#include "controller_session.h"
#include "jsonhandler.h"

#define OK_RESPONSE 200
#define HTTP_RESPONSE_TIOMEOUT 20


static ControllerSession *controller_session_static = NULL;

G_DEFINE_TYPE( ControllerSession, controller_session, G_TYPE_OBJECT )


static void controller_session_setup_header(SoupMessage *msg)
{
    soup_message_headers_clear(msg->request_headers);
    g_autofree gchar *jwt_str = NULL;
    jwt_str = atomic_string_get(&controller_session_static->jwt);
    gchar *auth_header = g_strdup_printf("jwt %s", jwt_str);
    //g_info("%s %s\n", (const char *)__func__, authHeader);
    soup_message_headers_append(msg->request_headers, "Authorization", auth_header);
    g_free(auth_header);
    soup_message_headers_append(msg->request_headers, "Content-Type", "application/json");
}

static gchar *controller_session_api_call(const char *method,
        const char *uri_string, GHashTable* query, const gchar *body_str,
        int *resp_code)
{
    gchar *response_body_str = NULL;

    if (uri_string == NULL) {
        g_warning("%s :uri_string is NULL.", (const char *)__func__);
        return response_body_str;
    }

    // Check token
    g_autofree gchar *jwt_str = NULL;
    jwt_str = atomic_string_get(&controller_session_static->jwt);
    if (jwt_str == NULL) {
        g_warning("%s :Token is NULL. Cant execute request.", (const char *)__func__);
        return response_body_str;
    }

    // create msg
    SoupURI	*soup_uri = soup_uri_new(uri_string);
    if (soup_uri == NULL) {
        g_warning("%s :Invalid soup_uri. Cant execute request.", (const char *)__func__);
        return response_body_str;
    }

    // Set query params
    if (query)
        soup_uri_set_query_from_form(soup_uri, query);

    //char *path = soup_uri_to_string(soup_uri, TRUE); // temp

    SoupMessage *msg = soup_message_new_from_uri(method, soup_uri);


    if (soup_uri)
        soup_uri_free(soup_uri);
    if (msg == NULL) // this may happen according to doc
        return response_body_str;

    // set header
    controller_session_setup_header(msg);
    // set body
    if(body_str)
        soup_message_set_request(msg, "application/json", SOUP_MEMORY_COPY, body_str, strlen_safely(body_str));

    // send request.
    util_send_message(controller_session_static->soup_session, msg, uri_string);
    g_info("vdi_session_api_call: msg->status_code: %i", msg->status_code);
    //g_info("msg->response_body: %s", msg->response_body->data);
    // Повторяем логику как в браузере - завершаем соединение и выбрасываем пользователя к форме авторизации
    //if (msg->status_code == AUTH_FAIL_RESPONSE) {
    //    // request to go to the auth gui (like browsers do)
    //    gdk_threads_add_idle((GSourceFunc)auth_fail_detected, NULL);
    //}

    response_body_str = g_strdup(msg->response_body->data); // json_string_with_data. memory allocation!

    if (resp_code)
        *resp_code = msg->status_code;

    g_object_unref(msg);

    return response_body_str;
}

static void controller_session_free_memory(ControllerSession *self)
{
    atomic_string_set(&self->username, NULL);
    atomic_string_set(&self->password, NULL);
    atomic_string_set(&self->address, NULL);
    atomic_string_set(&self->jwt, NULL);
    atomic_string_set(&self->api_url, NULL);
    atomic_string_set(&self->last_error_phrase, NULL);
    atomic_string_set(&self->login_time, NULL);
}

static void controller_session_finalize(GObject *object)
{
    g_info("%s", (const char *)__func__);
    ControllerSession *self = CONTROLLER_SESSION(object);

    controller_session_free_memory(self);

    GObjectClass *parent_class = G_OBJECT_CLASS( controller_session_parent_class );
    ( *parent_class->finalize )( object );
}

static void controller_session_class_init(ControllerSessionClass *klass )
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = controller_session_finalize;
}

static void controller_session_init(ControllerSession *self G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
}

ControllerSession *get_controller_session_static()
{
    if (controller_session_static == NULL)
        controller_session_static = controller_session_new();

    return controller_session_static;
}

void controller_session_static_destroy()
{
    if (controller_session_static == NULL)
        return;

    g_object_unref(controller_session_static);
    controller_session_static = NULL;
}

ControllerSession *controller_session_new()
{
    g_info("%s", (const char *)__func__);
    ControllerSession *controller_session = CONTROLLER_SESSION( g_object_new( TYPE_CONTROLLER_SESSION, NULL ) );

    gboolean ssl_strict = FALSE;
    controller_session->soup_session = soup_session_new_with_options("timeout", HTTP_RESPONSE_TIOMEOUT,
                                                                     "ssl-strict", ssl_strict,
                                                                     NULL);

    atomic_string_init(&controller_session->username);
    atomic_string_init(&controller_session->password);
    atomic_string_init(&controller_session->address);
    atomic_string_init(&controller_session->jwt);
    atomic_string_init(&controller_session->api_url);
    atomic_string_init(&controller_session->last_error_phrase);
    atomic_string_init(&controller_session->login_time);

    return controller_session;
}

void controller_session_set_credentials(const gchar *username, const gchar *password)
{
    atomic_string_set(&controller_session_static->username, username);
    atomic_string_set(&controller_session_static->password, password);
}

void controller_session_set_conn_data(const gchar *ip, int port)
{
    atomic_string_set(&controller_session_static->address, ip);
    controller_session_static->port = port;
    const gchar *http_protocol = determine_http_protocol_by_port(port);

    g_autofree gchar *api_url = NULL;
    api_url = g_strdup_printf("%s://%s:%i", http_protocol, ip, port);
    atomic_string_set(&controller_session_static->api_url, api_url);
}

void controller_session_set_ldap(gboolean is_ldap)
{
    controller_session_static->is_ldap = is_ldap;
}

void controller_session_cancel_pending_requests()
{
    soup_session_abort(controller_session_static->soup_session);
}

VmRemoteProtocol controller_session_get_current_remote_protocol()
{
    return controller_session_static->current_remote_protocol;
}

void controller_session_set_current_remote_protocol(VmRemoteProtocol protocol)
{
    controller_session_static->current_remote_protocol = protocol;
}

gchar *controller_session_get_login_time()
{
    return atomic_string_get(&controller_session_static->login_time);
}

gchar *controller_session_extract_error_msg(SoupMessage *msg, JsonObject *reply_json_object)
{
    gchar *detail_msg = NULL;

    const gchar *message = reply_json_object ?
            json_object_get_string_member_safely(reply_json_object, "detail") : NULL;

    if (strlen_safely(message) != 0)
        detail_msg = g_strdup(message);
    else if (msg && strlen_safely(msg->reason_phrase) != 0)
        detail_msg = g_strdup(msg->reason_phrase);
    else // "Не удалось авторизоваться"
        detail_msg = g_strdup(_("ECP request failed"));

    atomic_string_set(&controller_session_static->last_error_phrase, detail_msg);

    g_warning("%s: %s", (const char *)__func__, detail_msg);

    return detail_msg;
}

const gchar *controller_session_extract_ipv4(JsonObject *reply_json_object)
{
    JsonObject *guest_utils_object =
            json_object_get_object_member_safely(reply_json_object, "guest_utils");
    if (guest_utils_object == NULL)
        return NULL;

    JsonArray *ipv4_array = json_object_get_array_member_safely(guest_utils_object, "ipv4");
    if (ipv4_array == NULL  || json_array_get_length(ipv4_array) == 0)
        return NULL;

    const gchar *ipv4_0 = json_array_get_string_element(ipv4_array, 0);
    return ipv4_0;
}

gboolean controller_session_logout()
{
    g_info("%s", (const char *)__func__);

    controller_session_cancel_pending_requests();

    g_autofree gchar *jwt_str = NULL;
    jwt_str = atomic_string_get(&controller_session_static->jwt);
    if (jwt_str) {
        g_autofree gchar *api_url = NULL;
        api_url = atomic_string_get(&controller_session_static->api_url);

        g_autofree gchar *url_str = NULL;
        url_str = g_strdup_printf("%s/logout/", api_url);

        // Send request
        g_object_set(controller_session_static->soup_session, "timeout", 1, NULL);
        int resp_code = 0;
        g_autofree gchar *response_body_str = NULL;
        response_body_str = controller_session_api_call("POST", url_str, NULL, NULL, &resp_code);
        g_object_set(controller_session_static->soup_session, "timeout", HTTP_RESPONSE_TIOMEOUT, NULL);
        (void)response_body_str;

        return (resp_code == OK_RESPONSE);
    }

    return TRUE;
}

void controller_session_log_in_task(GTask *task,
                                    gpointer source_object G_GNUC_UNUSED,
                                    gpointer task_data G_GNUC_UNUSED,
                                    GCancellable *cancellable G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);

    atomic_string_set(&controller_session_static->jwt, NULL);

    g_autofree gchar *api_url = NULL;
    api_url = atomic_string_get(&controller_session_static->api_url);

    g_autofree gchar *auth_url = NULL;
    auth_url = g_strdup_printf("%s/auth/", api_url);

    SoupMessage *msg = soup_message_new("POST", auth_url);
    if(msg == NULL) { // "Не удалось сформировать SoupMessage. %s"
        g_autofree gchar *text_msg = NULL;
        text_msg = g_strdup_printf(_("Failed to form SoupMessage. %s"), auth_url);
        g_warning("%s: %s", (const char*)__func__, text_msg);
        atomic_string_set(&controller_session_static->last_error_phrase, text_msg);
        g_task_return_boolean(task, FALSE);
        return;
    }

    // set header
    soup_message_headers_append(msg->request_headers, "Content-Type", "application/json");

    // set body
    g_autofree gchar *username = NULL;
    username = atomic_string_get(&controller_session_static->username);
    g_autofree gchar *password = NULL;
    password = atomic_string_get(&controller_session_static->password);
    g_autofree gchar *message_body_str = NULL;
    message_body_str = g_strdup_printf(
            "{\"username\": \"%s\", \"password\": \"%s\", \"ldap\": %s}",
            username,
            password,
            bool_to_str(controller_session_static->is_ldap));

    soup_message_set_request(msg, "application/json",
                             SOUP_MEMORY_COPY, message_body_str, strlen_safely(message_body_str));

    // send message
    util_send_message(controller_session_static->soup_session, msg, auth_url);

    // parse response
    g_info("%s: msg->status_code %i", (const char *)__func__, msg->status_code);
    JsonParser *parser = json_parser_new();

    ServerReplyType server_reply_type;
    JsonObject *reply_json_object = json_get_data_or_errors_object_ecp(parser, msg->response_body->data,
                                                                   &server_reply_type);

    if  (reply_json_object && server_reply_type == SERVER_REPLY_TYPE_DATA) {

        const gchar *jwt_str = json_object_get_string_member_safely(reply_json_object, "token");

        if (strlen_safely(jwt_str)) {
            // Remember login time
            gchar *login_time_str = NULL;
            login_time_str = get_current_readable_time();
            atomic_string_set(&controller_session_static->login_time, login_time_str);
            // Remember token
            atomic_string_set(&controller_session_static->jwt, jwt_str);
            g_task_return_boolean(task, TRUE);
        } else {
            atomic_string_set(&controller_session_static->last_error_phrase,
                    _("Failed to extract token. Probably unexpected msg format."));
            g_task_return_boolean(task, FALSE);
        }

    } else {
        g_autofree gchar *detail_msg = NULL;
        detail_msg = controller_session_extract_error_msg(msg, reply_json_object);
        (void)detail_msg;

        g_task_return_boolean(task, FALSE);
    }

    g_object_unref(msg);
    g_object_unref(parser);
}

void controller_session_get_vm_list_task(GTask *task,
                                         gpointer source_object G_GNUC_UNUSED,
                                         gpointer task_data,
                                         GCancellable *cancellable G_GNUC_UNUSED)
{
    g_autofree gchar *api_url_str = NULL;
    api_url_str = atomic_string_get(&controller_session_static->api_url);

    g_autofree gchar *url_str = NULL;
    url_str = g_strdup_printf("%s/api/domains/", api_url_str);

    VmListRequestData *vm_list_request_data = (VmListRequestData *)task_data;

    GHashTable *query = g_hash_table_new((GHashFunc)g_str_hash, (GEqualFunc)g_str_equal);
    if (strlen_safely(vm_list_request_data->name_filter))
        g_hash_table_insert(query, "name", vm_list_request_data->name_filter);
    if (strlen_safely(vm_list_request_data->ordering))
        g_hash_table_insert(query, "ordering", vm_list_request_data->ordering);
    g_hash_table_insert(query, "template", "False");
    g_autofree gchar *limit_str = NULL;
    limit_str = g_strdup_printf("%i", vm_list_request_data->limit);
    g_hash_table_insert(query, "limit", limit_str);

    gchar *response_body_str = controller_session_api_call("GET", url_str, query, NULL, NULL);

    // free
    g_hash_table_unref(query);
    controller_session_free_vm_list_request_data(vm_list_request_data);

    g_task_return_pointer(task, response_body_str, NULL); // return pointer must be freed
}

void controller_session_get_vm_data_task(GTask *task,
                                         gpointer source_object G_GNUC_UNUSED,
                                         gpointer task_data,
                                         GCancellable *cancellable G_GNUC_UNUSED)
{
    gchar *vm_id = (gchar *)task_data;
    if (vm_id == NULL)
        g_task_return_pointer(task, NULL, NULL);

    g_autofree gchar *api_url_str = NULL;
    api_url_str = atomic_string_get(&controller_session_static->api_url);

    g_autofree gchar *url_str = NULL;
    url_str = g_strdup_printf("%s/api/domains/%s/", api_url_str, vm_id);

    g_autofree gchar *response_body_str = NULL;
    response_body_str = controller_session_api_call("GET", url_str, NULL, NULL, NULL);

    // Parse
    JsonParser *parser = json_parser_new();
    ServerReplyType server_reply_type = SERVER_REPLY_TYPE_UNKNOWN;
    JsonObject *reply_json_object = json_get_data_or_errors_object_ecp(parser, response_body_str, &server_reply_type);

    VeilVmData *vdi_vm_data = calloc(1, sizeof(VeilVmData));
    vdi_vm_data->server_reply_type = server_reply_type;

    if (reply_json_object && server_reply_type == SERVER_REPLY_TYPE_DATA) {

        const gchar *vm_verbose_name = json_object_get_string_member_safely(reply_json_object, "verbose_name");
        vdi_vm_data->vm_verbose_name = g_strdup(vm_verbose_name);

        // update data according to protocol
        switch (controller_session_get_current_remote_protocol()) {
            case SPICE_PROTOCOL: {
                // В случае  spice адресом выступает адрес контроллера
                vdi_vm_data->vm_host = atomic_string_get(&get_controller_session_static()->address);
                const gchar *password = json_object_get_string_member_safely(
                        reply_json_object, "graphics_password");
                vdi_vm_data->vm_password = g_strdup(password);

                vdi_vm_data->vm_port = (int)json_object_get_int_member_safely(reply_json_object, "remote_access_port");
                if (vdi_vm_data->vm_port == 0) {
                    vdi_vm_data->message = g_strdup(
                            "Failed to fetch connection info. Possibly remote access is disabled.");
                    vdi_vm_data->server_reply_type = SERVER_REPLY_TYPE_ERROR;
                }
                break;
            }
            case SPICE_DIRECT_PROTOCOL: {
                const gchar *password = json_object_get_string_member_safely(
                        reply_json_object, "graphics_password");
                vdi_vm_data->vm_password = g_strdup(password);

                vdi_vm_data->vm_port = (int)json_object_get_int_member_safely(
                        reply_json_object, "real_remote_access_port");
                if (vdi_vm_data->vm_port == 0) {
                    vdi_vm_data->message = g_strdup(
                            _("Failed to fetch connection info. Possibly remote access is disabled."));
                    vdi_vm_data->server_reply_type = SERVER_REPLY_TYPE_ERROR;
                    break;
                }

                // В случае direct spice адресом выступает адрес сервера (node)
                JsonObject *node_object =
                        json_object_get_object_member_safely(reply_json_object, "node");
                if (node_object == NULL) {
                    vdi_vm_data->message = g_strdup("Format error: node_object == NULL.");
                    vdi_vm_data->server_reply_type = SERVER_REPLY_TYPE_ERROR;
                    break;
                }

                const gchar *node_id = json_object_get_string_member_safely(node_object, "id");

                // Make request to fetch node ip
                g_autofree gchar *url_node_str = NULL;
                url_node_str = g_strdup_printf("%s/api/nodes/%s/", api_url_str, node_id);
                g_autofree gchar *response_body_node_str = NULL;
                response_body_node_str = controller_session_api_call("GET", url_node_str, NULL, NULL, NULL);
                // Parse
                JsonParser *parser_node = json_parser_new();
                ServerReplyType server_reply_type_node = SERVER_REPLY_TYPE_UNKNOWN;
                JsonObject *reply_json_object_node = json_get_data_or_errors_object_ecp(
                        parser_node, response_body_node_str, &server_reply_type_node);

                if (reply_json_object_node && server_reply_type_node == SERVER_REPLY_TYPE_DATA) {
                    const gchar *node_ip = json_object_get_string_member_safely(
                            reply_json_object_node, "management_ip");
                    vdi_vm_data->vm_host = g_strdup(node_ip);
                } else {
                    const gchar *message = reply_json_object_node ?
                                           json_object_get_string_member_safely(reply_json_object_node, "detail") :
                                           NULL;
                    vdi_vm_data->message = g_strdup(message);
                    g_object_unref(parser_node);
                    break;
                }
                g_object_unref(parser_node);

                break;
            }
            case RDP_PROTOCOL:
            case RDP_NATIVE_PROTOCOL:
            case X2GO_PROTOCOL: {
                const gchar *ipv4_0 = controller_session_extract_ipv4(reply_json_object);
                vdi_vm_data->vm_host = g_strdup(ipv4_0);
                break;
            }
            default:
                break;
        }

    } else {
        const gchar *message = reply_json_object ?
                               json_object_get_string_member_safely(reply_json_object, "detail") : NULL;
        vdi_vm_data->message = g_strdup(message);
    }

    g_object_unref(parser);

    g_free(vm_id); // память была выделена в основном потоке
    g_task_return_pointer(task, vdi_vm_data, NULL); // return pointer must be freed
}

void controller_session_free_vm_list_request_data(VmListRequestData *vm_list_request_data)
{
    free_memory_safely(&vm_list_request_data->name_filter);
    free_memory_safely(&vm_list_request_data->ordering);
    free(vm_list_request_data);
}