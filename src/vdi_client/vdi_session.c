/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <winerror.h>
#include <rpc.h>
#include <io.h>
#include <fcntl.h>
#define SECURITY_WIN32
#include <security.h>
#include <ntsecapi.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>

#include <libsoup/soup-session.h>

#include <json-glib/json-glib.h>
#include <glib/gmacros.h>
#include <glib/gi18n.h>

#include "remote-viewer-util.h"
#include "config.h"
#include "vdi_session.h"
#include "jsonhandler.h"
#include "settingsfile.h"
#include "veil_time.h"

#define OK_RESPONSE 200
#define ACCEPT_RESPONSE 202
#define BAD_REQUEST 400
#define AUTH_FAIL_RESPONSE 401
#define HTTP_RESPONSE_TIOMEOUT 40

#define USER_AGENT "thin-client"

typedef unsigned long gss_uint32;
typedef gss_uint32      OM_uint32;


static VdiSession *vdi_session_static = NULL;

//#define VDI_SESSION_GET_PRIVATE( obj )  ( G_TYPE_INSTANCE_GET_PRIVATE( (obj), TYPE_VDI_SESSION, VdiSessionPrivate ) )

G_DEFINE_TYPE( VdiSession, vdi_session, G_TYPE_OBJECT )


static void vdi_session_finalize( GObject *self );

static void vdi_session_class_init( VdiSessionClass *klass )
{
    g_info("vdi_session_class_init");
    GObjectClass *gobject_class = G_OBJECT_CLASS( klass );

    gobject_class->finalize = vdi_session_finalize;

    // signals
    g_signal_new("vm-changed",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(VdiSessionClass, vm_changed),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE,
                 1,
                 G_TYPE_INT);

    g_signal_new("ws-conn-changed",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(VdiSessionClass, ws_conn_changed),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE,
                 1,
                 G_TYPE_INT);

    g_signal_new("ws-cmd-received",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(VdiSessionClass, ws_cmd_received),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE,
                 1,
                 G_TYPE_STRING);

    g_signal_new("text-msg-received",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(VdiSessionClass, text_msg_received),
                 NULL, NULL,
                 NULL,
                 G_TYPE_NONE,
                 2,
                 G_TYPE_STRING, G_TYPE_STRING);

    g_signal_new("auth-fail-detected",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(VdiSessionClass, auth_fail_detected),
                 NULL, NULL,
                 NULL,
                 G_TYPE_NONE,
                 0);

    g_signal_new("vm-prep-progress-received",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(VdiSessionClass, vm_prep_progress_received),
                 NULL, NULL,
                 NULL,
                 G_TYPE_NONE,
                 3,
                 G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING);

    g_signal_new("pool-entitlement-changed",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(VdiSessionClass, pool_entitlement_changed),
                 NULL, NULL,
                 NULL,
                 G_TYPE_NONE,
                 0);

    g_signal_new("login-state-changed",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(VdiSessionClass, login_state_changed),
                 NULL, NULL,
                 NULL,
                 G_TYPE_NONE,
                 2,
                 G_TYPE_INT, G_TYPE_STRING);
}

static void vdi_session_init( VdiSession *self G_GNUC_UNUSED)
{
    g_info("vdi_session_init");
}

static void vdi_session_finalize( GObject *self )
{
    g_info("vdi_session_finalize");

    GObjectClass *parent_class = G_OBJECT_CLASS( vdi_session_parent_class );
    ( *parent_class->finalize )( self );
}

VdiSession *vdi_session_new()
{
    g_info("vdi_session_new");
    VdiSession *vdi_session = VDI_SESSION( g_object_new( TYPE_VDI_SESSION, NULL ) );

    // create http session
    gboolean ssl_strict = FALSE; // maybe read from ini file
    vdi_session->soup_session = soup_session_new_with_options("timeout", HTTP_RESPONSE_TIOMEOUT,
                                                             "ssl-strict", ssl_strict, NULL);

    // init
    vdi_session->current_remote_protocol = SPICE_PROTOCOL; // by default

    vdi_session->vdi_username = NULL;
    vdi_session->vdi_password = NULL;
    vdi_session->disposable_password = NULL;
    vdi_session->multi_address_mode = MULTI_ADDRESS_MODE_FIRST;
    vdi_session->broker_addresses_list = NULL;

    vdi_session->api_url = NULL;

    atomic_string_init(&vdi_session->jwt);
    atomic_string_init(&vdi_session->vdi_version);

    vdi_session->pool_type = VDI_POOL_TYPE_UNKNOWN;
    vdi_session->current_pool_id = NULL;
    vdi_session->current_vm_id = NULL;
    vdi_session->current_vm_verbose_name = NULL;
    vdi_session->current_controller_address = NULL;

    vdi_session->user_permissions = (USER_PERMISSION_NO_PERMISSIONS | USER_PERMISSION_USB_REDIR |
            USER_PERMISSION_FOLDERS_REDIR | USER_PERMISSION_SHARED_CLIPBOARD_CLIENT_TO_GUEST |
            USER_PERMISSION_SHARED_CLIPBOARD_GUEST_TO_CLIENT);

    vdi_session->login_time = NULL;

    memset(&vdi_session->vdi_ws_client, 0, sizeof(VdiWsClient));
    vdi_session->vdi_ws_client.reconnect_if_conn_lost = TRUE;

    return vdi_session;
}

static void vdi_session_reset_current_data()
{
    free_memory_safely(&vdi_session_static->current_pool_id);
    free_memory_safely(&vdi_session_static->current_vm_id);
    free_memory_safely(&vdi_session_static->current_vm_verbose_name);
    free_memory_safely(&vdi_session_static->current_controller_address);
    free_memory_safely(&vdi_session_static->login_time);
}

static void free_session_memory()
{
    free_memory_safely(&vdi_session_static->vdi_username);
    free_memory_safely(&vdi_session_static->vdi_password);
    free_memory_safely(&vdi_session_static->disposable_password);
    if (vdi_session_static->broker_addresses_list) {
        g_list_free_full(vdi_session_static->broker_addresses_list, g_free);
        vdi_session_static->broker_addresses_list = NULL;
    }
    free_memory_safely(&vdi_session_static->current_logged_address);

    free_memory_safely(&vdi_session_static->api_url);

    atomic_string_set(&vdi_session_static->jwt, NULL);
    atomic_string_set(&vdi_session_static->vdi_version, NULL);

    vdi_session_reset_current_data();
}

static void setup_header_for_vdi_session_api_call(SoupMessage *msg, GHashTable *request_headers)
{
    soup_message_headers_clear(msg->request_headers);
    g_autofree gchar *jwt_str = NULL;
    jwt_str = atomic_string_get(&vdi_session_static->jwt);
    gchar *auth_header = g_strdup_printf("jwt %s", jwt_str);
    //g_info("%s %s\n", (const char *)__func__, auth_header);
    soup_message_headers_append(msg->request_headers, "Authorization", auth_header);
    g_free(auth_header);
    soup_message_headers_append(msg->request_headers, "Content-Type", "application/json");
    soup_message_headers_append(msg->request_headers, "User-Agent", USER_AGENT);

    // Additional headers
    if (request_headers) {
        GHashTableIter iter;
        gpointer key, value;

        g_hash_table_iter_init(&iter, request_headers);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            soup_message_headers_append(msg->request_headers, (const char *)key, (const char *)value);
        }
    }
}

static guint send_message(SoupMessage *msg, const char *uri_string)
{
    static int count = 0;
    g_info("Send_count: %i", ++count);

    guint status = soup_session_send_message(vdi_session_static->soup_session, msg);
    g_info("%s: Response code: %i  reason_phrase: %s  uri_string: %s",
            (const char *)__func__, status, msg->reason_phrase, uri_string);
    return status;
}

// Вызывается в главном потоке  при смене jwt
static gboolean vdi_api_session_restart_vdi_ws_client(gpointer data G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
    vdi_ws_client_stop(&vdi_session_static->vdi_ws_client);
    vdi_ws_client_start(&vdi_session_static->vdi_ws_client,
            vdi_session_static->current_logged_address,
            vdi_session_get_vdi_port());

    return G_SOURCE_REMOVE;
}

// Parse response and fill login_data
static void vdi_session_parse_login_response(SoupMessage *msg, LoginData *login_data)
{
    // parse response
    g_info("msg->status_code %i", msg->status_code);
    free_memory_safely(&login_data->reply_msg);

    // extract reply
    JsonParser *parser = json_parser_new();

    ServerReplyType server_reply_type;
    JsonObject *reply_json_object = json_get_data_or_errors_object(parser, msg->response_body->data,
                                                                   &server_reply_type);

    switch (server_reply_type) {
        case SERVER_REPLY_TYPE_DATA: {
            const gchar *domain = json_object_get_string_member_safely(reply_json_object, "domain");
            login_data->domain = g_strdup(domain);

            const gchar *jwt_str = json_object_get_string_member_safely(reply_json_object, "access_token");
            atomic_string_set(&vdi_session_static->jwt, jwt_str);
            break;
        }
        case SERVER_REPLY_TYPE_ERROR:
        case SERVER_REPLY_TYPE_UNKNOWN:
        default: {
            const gchar *message = json_object_get_string_member_safely(reply_json_object, "message");
            //g_info("%s : Unable to get token. %s %s", (const char *)__func__, message, msg->reason_phrase);
            if (strlen_safely(message) != 0)
                login_data->reply_msg = g_strdup(message);
            else if (msg->reason_phrase != NULL && strlen_safely(msg->reason_phrase) != 0)
                login_data->reply_msg = g_strdup(msg->reason_phrase);
            else // "Не удалось авторизоваться"
                login_data->reply_msg = g_strdup(_("Failed to login"));

            break;
        }
    }

    g_object_unref(parser);
}

// allocate memory
static gchar *vdi_session_form_api_url(const gchar *address, int vdi_port)
{
    const gchar *http_protocol = determine_http_protocol_by_port(vdi_port);

    gboolean is_proxy_not_used = read_int_from_ini_file("General", "is_proxy_not_used", FALSE);
    // В штатном режиме подключаемся к прокси серверу

    gchar *api_url;
    if (is_proxy_not_used)
        api_url = g_strdup_printf("%s://%s:%i", http_protocol, address, vdi_port);
    else
        api_url = g_strdup_printf("%s://%s:%i/api", http_protocol, address, vdi_port);

    return api_url;
}

// Получаем токен
static gboolean vdi_session_auth_request(const gchar *address, LoginData *login_data)
{
    atomic_string_set(&vdi_session_static->jwt, NULL);

    g_autofree gchar *api_url = NULL;
    api_url = vdi_session_form_api_url(address, vdi_session_static->vdi_port);
    update_string_safely(&vdi_session_static->api_url, api_url);

    vdi_api_session_clear_login_data(login_data);
    g_info("%s", (const char *)__func__);

    if(vdi_session_static->api_url == NULL)
        return FALSE;

    g_autofree gchar *auth_url = NULL;
    auth_url = g_strdup_printf("%s/auth/", vdi_session_static->api_url);

    // create request message
    SoupMessage *msg = soup_message_new("POST", auth_url);
    if(msg == NULL) { // "Не удалось сформировать SoupMessage. %s"
        login_data->reply_msg = g_strdup_printf(_("Failed to form SoupMessage. %s"), auth_url);
        g_info("%s: %s", (const char*)__func__, login_data->reply_msg);
        return FALSE;
    }

    // set header
    soup_message_headers_append(msg->request_headers, "Content-Type", "application/json");
    soup_message_headers_append(msg->request_headers, "User-Agent", USER_AGENT);

    // set body
    // todo: use glib to create json msg
    gchar *message_body_str = g_strdup_printf(
            "{\"username\": \"%s\", \"password\": \"%s\", \"code\": \"%s\", "
            "\"ldap\": %s, \"veil_connect_version\": \"%s\"}",
            vdi_session_static->vdi_username,
            vdi_session_static->vdi_password,
            vdi_session_static->disposable_password,
            bool_to_str(vdi_session_static->is_ldap),
            PACKAGE_VERSION);
    //g_info("%s  messageBodyStr %s", (const char *)__func__, messageBodyStr);
    soup_message_set_request(msg, "application/json",
                             SOUP_MEMORY_COPY, message_body_str, strlen_safely(message_body_str));
    g_free(message_body_str);

    // send message
    send_message(msg, auth_url);

    vdi_session_parse_login_response(msg, login_data);
    g_object_unref(msg);

    //
    if (vdi_session_static->jwt.string) {
        update_string_safely(&vdi_session_static->current_logged_address, address);
        return TRUE;
    } else {
        g_info("Failed to login at address %s. %s", address, login_data->reply_msg);
    }

    return FALSE;
}

// Get server version
static void vdi_session_request_version()
{
    if (vdi_session_static->jwt.string == NULL)
        return;

    gchar *url_str = g_strdup_printf("%s/version", vdi_session_static->api_url);
    g_autofree gchar *response_body_str = NULL;
    response_body_str = vdi_session_api_call("GET", url_str, NULL, NULL, NULL);
    g_free(url_str);

    // Parse
    JsonParser *parser = json_parser_new();
    ServerReplyType server_reply_type;
    JsonObject *reply_json_object = json_get_data_or_errors_object(parser, response_body_str, &server_reply_type);
    if (server_reply_type == SERVER_REPLY_TYPE_DATA) {
        const gchar *vdi_version = json_object_get_string_member_safely(reply_json_object, "version");
        atomic_string_set(&vdi_session_static->vdi_version, vdi_version);
        g_info("vdi_version: %s", vdi_version);

        if (vdi_version) {
            vdi_session_static->version_older_than_320 = (virt_viewer_compare_version(vdi_version, "3.2.0") < 0);
            g_debug("!!! Is server version_older_than_320: %i", vdi_session_static->version_older_than_320);
        }
    }
}

// some kind of singleton
VdiSession *get_vdi_session_static()
{
    if (vdi_session_static == NULL)
        vdi_session_static = vdi_session_new();

    return vdi_session_static;
}

void vdi_session_static_destroy()
{
    if (vdi_session_static == NULL)
        return;

    // logout
    vdi_session_logout();

    // free memory
    g_object_unref(vdi_session_static->soup_session);
    free_session_memory();
    atomic_string_deinit(&vdi_session_static->jwt);
    g_object_unref(vdi_session_static);
    vdi_session_static = NULL;
}

void vdi_session_vm_state_change_notify(int power_state)
{
    g_signal_emit_by_name(vdi_session_static, "vm-changed", power_state);
}

void vdi_session_ws_conn_change_notify(int ws_connected)
{
    g_signal_emit_by_name(vdi_session_static, "ws-conn-changed", ws_connected);
}

void vdi_session_ws_cmd_received_notify(const gchar *cmd)
{
    g_signal_emit_by_name(vdi_session_static, "ws-cmd-received", cmd);
}

void vdi_session_text_msg_received_notify(const gchar *sender_name, const gchar *text)
{
    g_signal_emit_by_name(vdi_session_static, "text-msg-received", sender_name, text);
}

void vdi_session_vm_prep_progress_received_notify(int request_id, int progress, const gchar *text)
{
    g_signal_emit_by_name(vdi_session_static, "vm-prep-progress-received",
                          request_id, progress, text);
}

void vdi_session_pool_entitlement_changed_notify()
{
    g_signal_emit_by_name(vdi_session_static, "pool-entitlement-changed");
}

// Вызывается в главном потоке при получении кода 401
static gboolean auth_fail_detected(gpointer user_data G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
    vdi_ws_client_stop(&vdi_session_static->vdi_ws_client);
    g_signal_emit_by_name(vdi_session_static, "auth-fail-detected");
    return FALSE;
}

//const gchar *vdi_session_get_vdi_ip()
//{
//    return vdi_session_static->
//}

int vdi_session_get_vdi_port(void)
{
    return vdi_session_static->vdi_port;
}

void vdi_session_set_vdi_port(int port)
{
    vdi_session_static->vdi_port = port;
}

gboolean vdi_session_is_ldap()
{
    return vdi_session_static->is_ldap;
}

const gchar *vdi_session_get_vdi_username()
{
    return vdi_session_static->vdi_username;
}

const gchar *vdi_session_get_vdi_password()
{
    return vdi_session_static->vdi_password;
}

gchar *vdi_session_get_token()
{
    return atomic_string_get(&vdi_session_static->jwt);
}

gboolean vdi_session_version_older_than_320()
{
    return vdi_session_static->version_older_than_320;
}

void vdi_session_cancel_pending_requests()
{
    soup_session_abort(vdi_session_static->soup_session);
}

void vdi_session_set_credentials(const gchar *username, const gchar *password,
                                 const gchar *disposable_password)
{
    free_memory_safely(&vdi_session_static->vdi_username);
    free_memory_safely(&vdi_session_static->vdi_password);
    free_memory_safely(&vdi_session_static->disposable_password);

    vdi_session_static->vdi_username = strstrip_safely(g_strdup(username));
    vdi_session_static->vdi_password = strstrip_safely(g_strdup(password));
    vdi_session_static->disposable_password = strstrip_safely(g_strdup(disposable_password));
}

void vdi_session_set_broker_addresses(GList *add_addresses)
{
    if (vdi_session_static->broker_addresses_list)
        g_list_free_full(vdi_session_static->broker_addresses_list, g_free);

    vdi_session_static->broker_addresses_list = add_addresses;
}

void vdi_session_set_multi_address_mode(MultiAddressMode multi_address_mode)
{
    vdi_session_static->multi_address_mode = multi_address_mode;
}

void vdi_session_set_ldap(gboolean is_ldap)
{
    vdi_session_static->is_ldap = is_ldap;
}

void vdi_session_set_current_pool_id(const gchar *current_pool_id)
{
    free_memory_safely(&vdi_session_static->current_pool_id);
    vdi_session_static->current_pool_id = g_strdup(current_pool_id);
}

const gchar *vdi_session_get_current_pool_id()
{
    return vdi_session_static->current_pool_id;
}

VdiPoolType vdi_session_get_current_pool_type()
{
    return vdi_session_static->pool_type;
}

const gchar *vdi_session_get_current_vm_id()
{
    return vdi_session_static->current_vm_id;
}

void vdi_session_set_current_remote_protocol(VmRemoteProtocol remote_protocol)
{
    vdi_session_static->current_remote_protocol = remote_protocol;
}

VmRemoteProtocol vdi_session_get_current_remote_protocol()
{
    return vdi_session_static->current_remote_protocol;
}

VdiWsClient *vdi_session_get_ws_client()
{
    return &vdi_session_static->vdi_ws_client;
}

const gchar *vdi_session_get_current_vm_name()
{
    return vdi_session_static->current_vm_verbose_name;
}

const gchar *vdi_session_get_current_controller_address()
{
    return vdi_session_static->current_controller_address;
}

void vdi_session_set_permissions(JsonArray *user_permissions_array)
{
    guint user_permissions_amount = json_array_get_length(user_permissions_array);
    get_vdi_session_static()->user_permissions = USER_PERMISSION_NO_PERMISSIONS;

    for (guint i = 0; i < user_permissions_amount; ++i) {
        const gchar *permission = json_array_get_string_element(user_permissions_array, i);

        // set permissions
        if (g_strcmp0(permission, "USB_REDIR") == 0)
            get_vdi_session_static()->user_permissions =
                    (get_vdi_session_static()->user_permissions | USER_PERMISSION_USB_REDIR);
        else if (g_strcmp0(permission, "FOLDERS_REDIR") == 0)
            get_vdi_session_static()->user_permissions =
                    (get_vdi_session_static()->user_permissions | USER_PERMISSION_FOLDERS_REDIR);
        else if (g_strcmp0(permission, "SHARED_CLIPBOARD_CLIENT_TO_GUEST") == 0)
            get_vdi_session_static()->user_permissions =
                    (get_vdi_session_static()->user_permissions | USER_PERMISSION_SHARED_CLIPBOARD_CLIENT_TO_GUEST);
        else if (g_strcmp0(permission, "SHARED_CLIPBOARD_GUEST_TO_CLIENT") == 0)
            get_vdi_session_static()->user_permissions =
                    (get_vdi_session_static()->user_permissions | USER_PERMISSION_SHARED_CLIPBOARD_GUEST_TO_CLIENT);
        else if (g_strcmp0(permission, "SHARED_CLIPBOARD") == 0) { // Для поддержки пред. версий сервера (<= 3.0.0)
            get_vdi_session_static()->user_permissions =
                    (get_vdi_session_static()->user_permissions | USER_PERMISSION_SHARED_CLIPBOARD_CLIENT_TO_GUEST);
            get_vdi_session_static()->user_permissions =
                    (get_vdi_session_static()->user_permissions | USER_PERMISSION_SHARED_CLIPBOARD_GUEST_TO_CLIENT);
        }
    }

    g_info("get_vdi_session_static()->user_permissions: %i", get_vdi_session_static()->user_permissions);
}

gboolean vdi_session_is_usb_redir_permitted()
{
    return (get_vdi_session_static()->user_permissions & USER_PERMISSION_USB_REDIR);
}

gboolean vdi_session_is_folders_redir_permitted()
{
    return (get_vdi_session_static()->user_permissions & USER_PERMISSION_FOLDERS_REDIR);
}
// Разрешен ли буфер в направлении клиент - ВМ
gboolean vdi_session_is_shared_clipboard_c_to_g_permitted()
{
    return (get_vdi_session_static()->user_permissions & USER_PERMISSION_SHARED_CLIPBOARD_CLIENT_TO_GUEST);
}
// Разрешен ли буфер в направлении ВМ - клиент
gboolean vdi_session_is_shared_clipboard_g_to_c_permitted()
{
    return (get_vdi_session_static()->user_permissions & USER_PERMISSION_SHARED_CLIPBOARD_GUEST_TO_CLIENT);
}

void vdi_session_refresh_login_time(void)
{
    get_vdi_session_static()->login_time = get_current_readable_time();
}

const gchar *vdi_session_get_login_time(void)
{
    return get_vdi_session_static()->login_time;
}

gchar *vdi_session_api_call(const char *method, const char *uri_string, const gchar *body_str,
        GHashTable *request_headers, int *resp_code)
{
    gchar *response_body_str = NULL;

    if (uri_string == NULL) {
        g_warning("%s :uri_string is NULL.", (const char *)__func__);
        return response_body_str;
    }

    // Check token
    g_autofree gchar *jwt_str = NULL;
    jwt_str = atomic_string_get(&vdi_session_static->jwt);
    if (jwt_str == NULL) {
        g_warning("%s :Token is NULL. Cant execute request.", (const char *)__func__);
        return response_body_str;
    }

    // create msg
    SoupMessage *msg = soup_message_new(method, uri_string);
    if (msg == NULL) // this may happen according to doc
        return response_body_str;
    // set header
    setup_header_for_vdi_session_api_call(msg, request_headers);
    // set body
    if(body_str)
        soup_message_set_request(msg, "application/json", SOUP_MEMORY_COPY, body_str, strlen_safely(body_str));

    // send request.
    util_send_message(vdi_session_static->soup_session, msg, uri_string);
    g_info("vdi_session_api_call: msg->status_code: %i", msg->status_code);
    //g_info("msg->response_body: %s", msg->response_body->data);
    // Повторяем логику как в браузере - завершаем соединение и выбрасываем пользователя к форме авторизации
    if (msg->status_code == AUTH_FAIL_RESPONSE) {
        // request to go to the auth gui (like browsers do)
        gdk_threads_add_idle((GSourceFunc)auth_fail_detected, NULL);
    }

    response_body_str = g_strdup(msg->response_body->data); // json_string_with_data. memory allocation!

    if (resp_code)
        *resp_code = msg->status_code;

    g_object_unref(msg);

    return response_body_str;
}

static gpointer vdi_session_g_list_get_item_by_index(GList *list, guint index)
{
    guint count = 0;
    for (GList *l = list; l != NULL; l = l->next) {

        if (count == index) {
            return l->data;
        }
        count++;
    }

    return NULL;
}

void vdi_session_log_in_task(GTask       *task,
                   gpointer       source_object G_GNUC_UNUSED,
                   gpointer       task_data G_GNUC_UNUSED,
                   GCancellable  *cancellable)
{
    // get token
    LoginData *login_data = calloc(1, sizeof(LoginData)); // free in callback!
    free_memory_safely(&vdi_session_static->current_logged_address);

    switch (vdi_session_static->multi_address_mode) {
        case MULTI_ADDRESS_MODE_FIRST: {
            // Connect to addresses
            guint addresses_amount = g_list_length(vdi_session_static->broker_addresses_list);
            guint count = 0;
            for (GList *l = vdi_session_static->broker_addresses_list; l != NULL; l = l->next) {

                if (g_cancellable_is_cancelled(cancellable)) {
                    g_info("%s  Cancelled", (const char *)__func__);
                    break;
                }

                count++;
                const gchar *address = (const gchar *)l->data;
                if(strlen_safely(address) > 0) {
                    g_info("Trying to login at address %s. %i from %i", address, count, addresses_amount);
                    if (vdi_session_auth_request(address, login_data))
                        break;
                }
            }

            break;
        }
        case MULTI_ADDRESS_MODE_RANDOM: {
            // Connect to random address from available ones
            GList *all_addresses_list = g_list_copy(vdi_session_static->broker_addresses_list); // shadow copy

            guint addresses_amount = g_list_length(all_addresses_list);
            //g_info("!!! addresses_amount %i", addresses_amount);
            while(addresses_amount > 0) {

                if (g_cancellable_is_cancelled(cancellable)) {
                    g_info("%s  Cancelled", (const char *)__func__);
                    break;
                }

                guint rand_index = rand() % addresses_amount;
                const gchar *address = (const gchar *)vdi_session_g_list_get_item_by_index(
                        all_addresses_list, rand_index);
                g_info("Trying to login at address %s. ", address);
                if (vdi_session_auth_request(address, login_data))
                    break;

                all_addresses_list = g_list_remove(all_addresses_list, address);
                addresses_amount = g_list_length(all_addresses_list);
                //g_info("!!! rand_index %i", rand_index);
            }

            g_list_free(all_addresses_list);
            break;
        }
        default: {

            break;
        }
    }

    // Connect to ws if token received В основном потоке вызывается на смену токена
    if (vdi_session_static->jwt.string && vdi_session_static->current_logged_address)
        g_timeout_add(500, (GSourceFunc)vdi_api_session_restart_vdi_ws_client, NULL);

    // Get VDI version
    vdi_session_request_version();

    g_task_return_pointer(task, login_data, NULL); // login_data is freed in task callback
}

void vdi_session_get_vdi_pool_data_task(GTask   *task,
                 gpointer       source_object G_GNUC_UNUSED,
                 gpointer       task_data,
                 GCancellable  *cancellable G_GNUC_UNUSED)
{
    //g_info("In %s :thread id = %lu\n", (const char *)__func__, pthread_self());
    gchar *url_str = g_strdup_printf("%s/client/pools", vdi_session_static->api_url);

    PoolsRequestTaskData *pools_request_task_data = (PoolsRequestTaskData *)task_data;

    GHashTable *request_headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(request_headers, g_strdup("Get-Favorite-Only"),
                        g_strdup(pools_request_task_data->get_favorite_only ? "true" : "false"));

    gchar *response_body_str = vdi_session_api_call("GET", url_str, NULL, request_headers, NULL);
    g_free(url_str);
    g_hash_table_destroy(request_headers);

    g_task_return_pointer(task, response_body_str, NULL); // return pointer must be freed
}

void vdi_session_get_vm_from_pool_task(GTask       *task,
                    gpointer       source_object G_GNUC_UNUSED,
                    gpointer       task_data,
                    GCancellable  *cancellable G_GNUC_UNUSED)
{
    // get vm from pool
    gchar *url_str = g_strdup_printf("%s/client/pools/%s", vdi_session_static->api_url,
            vdi_session_static->current_pool_id);

    // form request body
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    // remote_protocol
    json_builder_set_member_name(builder, "remote_protocol");

    // Для поддержки старых версий с неверным написанием протокола (<= 4.1.0)
    g_autofree gchar *vdi_version = NULL;
    vdi_version = atomic_string_get(&vdi_session_static->vdi_version);
    if (vdi_session_static->current_remote_protocol== LOUDPLAY_PROTOCOL &&
    virt_viewer_compare_version(vdi_version, "4.1.0") <= 0) {
            json_builder_add_string_value(builder, "LOADPLAY");
    } else {
        const gchar *protocol = util_remote_protocol_to_str(vdi_session_static->current_remote_protocol);
        json_builder_add_string_value(builder, protocol);
    }

    if (task_data) {
        RequestVmFromPoolData *vm_request_data = (RequestVmFromPoolData *)task_data;
        // request_id
        json_builder_set_member_name(builder, "request_id");
        json_builder_add_int_value(builder, vm_request_data->request_id);
        // time zone
        if (vm_request_data->redirect_time_zone) {
            json_builder_set_member_name(builder, "time_zone");
            g_autofree gchar *time_zone = NULL;
            time_zone = veil_time_get_time_zone();
            json_builder_add_string_value(builder, time_zone);
        }

        free(vm_request_data);
    }

    // OS
    json_builder_set_member_name(builder, "os");
#ifdef _WIN32
    json_builder_add_string_value(builder, "Windows");
#elif __linux__
    json_builder_add_string_value(builder, "Linux");
#else
    json_builder_add_string_value(builder, "Other");
#endif

    json_builder_end_object(builder);

    gchar *body_str = json_generate_from_builder(builder);
    g_object_unref(builder);

    //Установить время ожидания ответа
    int vm_await_timeout = read_int_from_ini_file("ServiceSettings", "vm_await_timeout", 65);
    g_object_set(vdi_session_static->soup_session, "timeout", CLAMP(vm_await_timeout, 1, 300), NULL);
    // Запрос
    gchar *response_body_str = vdi_session_api_call("POST", url_str, body_str, NULL, NULL);
    g_object_set(vdi_session_static->soup_session, "timeout", HTTP_RESPONSE_TIOMEOUT, NULL);

    g_free(url_str);
    g_free(body_str);

    //response_body_str == NULL. didnt receive what we wanted
    if (!response_body_str) {
        g_task_return_pointer(task, NULL, NULL);
        return;
    }

    // parse response
    JsonParser *parser = json_parser_new();

    ServerReplyType server_reply_type;
    JsonObject *reply_json_object = json_get_data_or_errors_object(parser, response_body_str, &server_reply_type);

    VeilVmData *vdi_vm_data = calloc(1, sizeof(VeilVmData));
    vdi_vm_data->server_reply_type = server_reply_type;

    if (server_reply_type == SERVER_REPLY_TYPE_DATA) {
        vdi_vm_data->vm_host = g_strdup(json_object_get_string_member_safely(reply_json_object, "host"));
        vdi_vm_data->vm_port = (int)json_object_get_int_member_safely(reply_json_object, "port");
        vdi_vm_data->vm_password = g_strdup(json_object_get_string_member_safely(reply_json_object, "password"));
        vdi_vm_data->vm_verbose_name = g_strdup(json_object_get_string_member_safely(
                reply_json_object, "vm_verbose_name"));

        // parse rds farm data
        JsonArray *farm_array = json_object_get_array_member_safely(reply_json_object, "farm_list");
        if (farm_array) {
            vdi_vm_data->farm_array = g_array_new(FALSE, FALSE, sizeof(VeilFarmData));

            guint farm_amount = json_array_get_length(farm_array);
            for (guint i = 0; i < farm_amount; ++i) {
                VeilFarmData farm_data = {};
                //  farm_alias
                JsonObject *farm_obj = json_array_get_object_element(farm_array, i);
                farm_data.farm_alias = g_strdup(json_object_get_string_member_safely( farm_obj, "farm_alias"));

                // app array
                JsonArray *app_array = json_object_get_array_member_safely(farm_obj, "app_array");
                if (app_array) {
                    farm_data.app_array = g_array_new(FALSE, FALSE, sizeof(VeilAppData));

                    guint app_amount = json_array_get_length(app_array);
                    for (guint j = 0; j < app_amount; ++j) {
                        VeilAppData app_data = {};

                        JsonObject *app_obj = json_array_get_object_element(app_array, j);
                        // app_name
                        app_data.app_name = g_strdup(json_object_get_string_member_safely(
                                app_obj, "app_name"));
                        // app_alias
                        app_data.app_alias = g_strdup(json_object_get_string_member_safely(
                                app_obj, "app_alias"));
                        // icon_base64
                        app_data.icon_base64 = g_strdup(json_object_get_string_member_safely(
                                app_obj, "icon"));

                        g_array_append_val(farm_data.app_array, app_data);
                    }
                }

                g_array_append_val(vdi_vm_data->farm_array, farm_data);
            }
        }

        // save some data in vdi_session_static
        update_string_safely(&vdi_session_static->current_vm_verbose_name, vdi_vm_data->vm_verbose_name);
        const gchar *vm_controller_address =
                json_object_get_string_member_safely(reply_json_object, "vm_controller_address");
        update_string_safely(&vdi_session_static->current_controller_address, vm_controller_address);
        const gchar *vm_id = json_object_get_string_member_safely(reply_json_object, "vm_id");
        update_string_safely(&vdi_session_static->current_vm_id, vm_id);
        // pool type
        const gchar *pool_type = json_object_get_string_member_safely(reply_json_object, "pool_type");
        if (g_strcmp0(pool_type, "AUTOMATED") == 0)
            vdi_session_static->pool_type = VDI_POOL_TYPE_AUTOMATED;
        else if (g_strcmp0(pool_type, "STATIC") == 0)
            vdi_session_static->pool_type = VDI_POOL_TYPE_STATIC;
        else if (g_strcmp0(pool_type, "RDS") == 0)
            vdi_session_static->pool_type = VDI_POOL_TYPE_RDS;
        else
            vdi_session_static->pool_type = VDI_POOL_TYPE_UNKNOWN;

        JsonArray *user_permissions_array = json_object_get_array_member_safely(reply_json_object, "permissions");
        vdi_session_set_permissions(user_permissions_array);

        g_info("!!!vm_id: %s  %s", vm_id, vdi_session_static->current_vm_id);

        g_info("vm_host %s", vdi_vm_data->vm_host);
        g_info("vm_port %i", vdi_vm_data->vm_port);
        //g_info("vm_password %s", vdi_vm_data->vm_password);
        g_info("vm_verbose_name %s", vdi_vm_data->vm_verbose_name);
        g_info("current_controller_address %s", vdi_session_static->current_controller_address);
    } else {
        g_warning("%s Error reply. Response_body_str: %s", (const char *)__func__, response_body_str);
    }

    vdi_vm_data->message = g_strdup(json_object_get_string_member_safely(reply_json_object, "message"));
    g_info("%s: server_reply_type %s", (const char *)__func__, vdi_vm_data->message);

    // no point to parse if data is invalid
    g_object_unref(parser);
    free_memory_safely(&response_body_str);
    g_task_return_pointer(task, vdi_vm_data, NULL);
}

void vdi_session_do_action_on_vm_task(GTask      *task,
                  gpointer       source_object G_GNUC_UNUSED,
                  gpointer       task_data G_GNUC_UNUSED,
                  GCancellable  *cancellable G_GNUC_UNUSED)
{
    ActionOnVmData *action_on_vm_data = g_task_get_task_data(task);
    g_info("%s: %s", (const char *)__func__, action_on_vm_data->action_on_vm_str);

    // url
    gchar *url_str = g_strdup_printf("%s/client/pools/%s/%s", vdi_session_static->api_url,
                                    vdi_session_get_current_pool_id(), action_on_vm_data->action_on_vm_str);
    // body
    gchar *bodyStr;
    if(action_on_vm_data->is_action_forced)
        bodyStr = g_strdup_printf("{\"force\":true}");
    else
        bodyStr = g_strdup_printf("{\"force\":false}");

    gchar *response_body_str = vdi_session_api_call("POST", url_str, bodyStr, NULL, NULL);

    // free url and body
    free_memory_safely(&url_str);
    free_memory_safely(&bodyStr);
    free_memory_safely(&response_body_str);
    // free ActionOnVmData
    vdi_api_session_free_action_on_vm_data(action_on_vm_data);
}

#if defined(_WIN32)
void vdi_session_windows_sso_auth_task(GTask *task,
                                       gpointer       source_object G_GNUC_UNUSED,
                                       gpointer       task_data G_GNUC_UNUSED,
                                       GCancellable  *cancellable G_GNUC_UNUSED)
{
    atomic_string_set(&vdi_session_static->jwt, NULL);
    LoginData *login_data = calloc(1, sizeof(LoginData));
    gboolean is_success = FALSE;

    SecBuffer output_token = {.BufferType = SECBUFFER_TOKEN, .cbBuffer = 0, .pvBuffer = NULL};
    SecBuffer input_token = {.BufferType = SECBUFFER_TOKEN, .cbBuffer = 0, .pvBuffer = NULL};
    SecBufferDesc input_desc = {.cBuffers = 1, .pBuffers = &input_token, .ulVersion = SECBUFFER_VERSION};
    SecBufferDesc output_desc = {.cBuffers = 1, .pBuffers = &output_token, .ulVersion = SECBUFFER_VERSION};

    g_autofree gchar *service_name = NULL;
    if(g_list_length(vdi_session_static->broker_addresses_list) >= 1) {
        // First address in the list
        service_name = g_strdup_printf("http/%s",
                (const gchar *)g_list_first(vdi_session_static->broker_addresses_list)->data);
    }
    else {
        login_data->reply_msg = g_strdup(_("Address is not specified"));
        goto end;
    }

    g_info("%s: service_name: %s", (const char *)__func__, service_name);
    OM_uint32 gss_flags = (ISC_REQ_MUTUAL_AUTH | ISC_REQ_ALLOCATE_MEMORY |
            ISC_REQ_CONFIDENTIALITY | ISC_REQ_REPLAY_DETECT);

    gchar *package = g_strdup("Kerberos");

    CredHandle cred_handle = {.dwLower = 0, .dwUpper = 0};

    TimeStamp expiry;
    PSEC_WINNT_AUTH_IDENTITY pAuthId = NULL;

    /// AcquireCredentialsHandle
    OM_uint32 maj_stat = AcquireCredentialsHandle(NULL,                       // no principal name
                                                   package,                       // package name
                                        SECPKG_CRED_OUTBOUND,
                                        NULL,                       // no logon id
                                        pAuthId,                    // no auth data
                                        NULL,                       // no get key fn
                                        NULL,                       // noget key arg
                                        &cred_handle,
                                        &expiry);
    free_memory_safely(&package);

    if (maj_stat != SEC_E_OK) {
        login_data->reply_msg = g_strdup_printf(_("Acquiring credentials error : %lu"), maj_stat);
        goto end;
    }

    /// InitializeSecurityContext in cycle
    CtxtHandle gss_context = {.dwLower = 0, .dwUpper = 0};
    PCtxtHandle context_handle = NULL;

    const int iteration_number = 5;
    OM_uint32 ret_flags;
    for (int i = 0; i < iteration_number; i++) {
        maj_stat = InitializeSecurityContext(&cred_handle,
                                             context_handle,
                                             service_name,
                                             gss_flags,
                                             0,          // reserved
                                             SECURITY_NATIVE_DREP,
                                             &input_desc,
                                             0,          // reserved
                                             &gss_context,
                                             &output_desc,
                                             &ret_flags,
                                             &expiry);

        g_info("%s: maj_stat = InitializeSecurityContext: %lu", (const char *)__func__, maj_stat);

        context_handle = &gss_context;

        g_free(input_token.pvBuffer);
        input_token.pvBuffer = NULL;
        input_token.cbBuffer = 0;

        /// Encode data to 64 string
        g_info("output_token.cbBuffer: %lu", output_token.cbBuffer);
        g_autofree gchar *g_base64_string_token = NULL;
        if (output_token.cbBuffer > 0)
            g_base64_string_token = g_base64_encode((const guchar *) output_token.pvBuffer,
                                                    (gsize) output_token.cbBuffer);

        if (output_token.pvBuffer) {
            FreeContextBuffer(output_token.pvBuffer);
            output_token.pvBuffer = NULL;
            output_token.cbBuffer = 0;
        }

        if (g_base64_string_token == NULL) {
            g_free(login_data->reply_msg);
            login_data->reply_msg = g_strdup_printf(
                    _("Failed to fetch output token. Error code: %lu"), maj_stat);
            goto end;
        }

        /// Send data to apache
        g_autofree gchar *url_str = NULL;
        url_str = g_strdup_printf("%s/sso/", vdi_session_static->api_url);

        SoupMessage *msg = soup_message_new("GET", url_str);
        // Set header
        soup_message_headers_append(msg->request_headers, "User-Agent", USER_AGENT);
        soup_message_headers_append(msg->request_headers, "Accept", "application/json, text/plain, */*");
        soup_message_headers_append(msg->request_headers, "Accept-Encoding", "gzip, deflate, br");
        soup_message_headers_append(msg->request_headers, "Connection", "keep-alive");
        soup_message_headers_append(msg->request_headers, "Sec-Fetch-Dest", "empty");
        soup_message_headers_append(msg->request_headers, "Sec-Fetch-Mode", "cors");
        soup_message_headers_append(msg->request_headers, "Sec-Fetch-Site", "same-origin");

        g_autofree gchar *neg_token_header = NULL;
        neg_token_header = g_strdup_printf("Negotiate %s", g_base64_string_token);
        //g_info("neg_token_header: %s", neg_token_header);
        soup_message_headers_append(msg->request_headers, "Authorization", neg_token_header);

        send_message(msg, url_str);
        //g_info("%s: msg->status_code: %i  %s", (const char *)__func__, msg->status_code, msg->response_body->data);

        if (msg->status_code == 200) {
            is_success = TRUE;
            vdi_session_parse_login_response(msg, login_data);
            vdi_session_request_version();
            g_object_unref(msg);
            goto end;
        }

        /// Decode data which was received from apache and generate new gssapi_data
        const char *resp = soup_message_headers_get_one(msg->response_headers, "WWW-Authenticate");
        g_info("%s: resp: %s", (const char *)__func__, resp);
        gchar **resp_gssapi_data_array = g_strsplit(resp, " ", 2);
        g_object_unref(msg);

        if (g_strv_length(resp_gssapi_data_array) > 1) {
            const gchar *gssapi_data = resp_gssapi_data_array[1];
            gsize binary_data_len;
            guchar *binary_data = g_base64_decode(gssapi_data, &binary_data_len);
            g_info("input_token  binary_data_len: %llu", binary_data_len);
            input_token.cbBuffer = (unsigned __LONG32) binary_data_len;
            input_token.pvBuffer = (void *) binary_data;

            g_strfreev(resp_gssapi_data_array);
        } else {
            g_strfreev(resp_gssapi_data_array);
            g_free(login_data->reply_msg);
            login_data->reply_msg = g_strdup(_("Server didnt send gssapi-data"));
            goto end;
        }
    }

    end:
    // free memory
    g_free(input_token.pvBuffer);
    if (!is_success && login_data->reply_msg == NULL)
        login_data->reply_msg = g_strdup(_("SSO failed"));
    g_task_return_pointer(task, login_data, NULL); // login_data is freed in task callback
}
#endif

void vdi_session_send_text_msg_task(GTask *task G_GNUC_UNUSED,
                                    gpointer source_object G_GNUC_UNUSED,
                                    gpointer task_data,
                                    GCancellable *cancellable G_GNUC_UNUSED)
{
    TextMessageData *text_message_data = (TextMessageData *)task_data;

    // url
    g_autofree gchar *url_str = NULL;
    url_str = g_strdup_printf("%s/client/send_text_message", vdi_session_static->api_url);

    // Generate body
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "msg_type");
    json_builder_add_string_value(builder, "text_msg");

    json_builder_set_member_name(builder, "message");
    json_builder_add_string_value(builder, text_message_data->message);

    json_builder_end_object(builder);

    JsonGenerator *gen = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(gen, root);
    gchar *body_str = json_generator_to_data(gen, NULL);
    g_info("%s: body_str: %s", (const char *)__func__, body_str);

    // Send
    gchar *response_body_str = vdi_session_api_call("POST", url_str, body_str, NULL, NULL);

    // Parse reply
    text_message_data->is_successfully_sent = FALSE;
    if (response_body_str) {
        JsonParser *parser = json_parser_new();
        ServerReplyType server_reply_type;
        JsonObject *reply_json_object = json_get_data_or_errors_object(parser, response_body_str, &server_reply_type);
        if (server_reply_type == SERVER_REPLY_TYPE_DATA) {
            text_message_data->is_successfully_sent = TRUE;
        } else {
            const gchar *message = json_object_get_string_member_safely(reply_json_object, "message");
            text_message_data->error_message = g_strdup(message);
        }

        g_object_unref(parser);
    }

    // Free
    g_free(body_str);
    g_free(response_body_str);
    json_node_free(root);
    g_object_unref(gen);
    g_object_unref(builder);
}
// UserData
void vdi_session_get_user_data_task(GTask *task,
                                    gpointer       source_object G_GNUC_UNUSED,
                                    gpointer       task_data G_GNUC_UNUSED,
                                    GCancellable  *cancellable G_GNUC_UNUSED)
{
    g_autofree gchar *url_str = NULL;
    url_str = g_strdup_printf("%s/client/get_user_data", vdi_session_static->api_url);
    g_autofree gchar *response_body_str = NULL;
    response_body_str = vdi_session_api_call("GET", url_str, NULL, NULL, NULL);

    // parse response
    JsonParser *parser = json_parser_new();
    ServerReplyType server_reply_type;
    JsonObject *reply_json_object = json_get_data_or_errors_object(parser, response_body_str, &server_reply_type);

    UserData *tk_user_data = calloc(1, sizeof(UserData)); // free in callback!
    tk_user_data->is_success = (server_reply_type == SERVER_REPLY_TYPE_DATA);
    if (tk_user_data->is_success) {
        JsonObject *user_obj = json_object_get_object_member_safely(reply_json_object, "user");
        tk_user_data->two_factor = json_object_get_bool_member_safely(user_obj, "two_factor");
    } else {
        const gchar *message = json_object_get_string_member_safely(reply_json_object, "message");
        tk_user_data->error_message = g_strdup(message);
    }

    g_object_unref(parser);
    g_task_return_pointer(task, tk_user_data, NULL);
}

void vdi_session_update_user_data_task(GTask *task,
                                       gpointer       source_object G_GNUC_UNUSED,
                                       gpointer       task_data G_GNUC_UNUSED,
                                       GCancellable  *cancellable G_GNUC_UNUSED)
{
    UserData *tk_user_data = g_task_get_task_data(task);

    // Build request body
    g_autofree gchar *request_body_str = NULL;
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "two_factor");
    json_builder_add_boolean_value(builder, tk_user_data->two_factor);
    json_builder_end_object(builder);
    request_body_str = json_generate_from_builder(builder);
    g_object_unref(builder);

    // Do request
    g_autofree gchar *url_str = NULL;
    url_str = g_strdup_printf("%s/client/update_user_data", vdi_session_static->api_url);
    g_autofree gchar *response_body_str = NULL;
    response_body_str = vdi_session_api_call("POST", url_str, request_body_str, NULL, NULL);

    // Parse response
    JsonParser *parser = json_parser_new();
    ServerReplyType server_reply_type;
    JsonObject *reply_json_object = json_get_data_or_errors_object(parser, response_body_str, &server_reply_type);

    tk_user_data->is_success = FALSE;
    if (server_reply_type == SERVER_REPLY_TYPE_DATA) {
        tk_user_data->is_success = json_object_get_bool_member_safely(reply_json_object, "ok");
        JsonObject *user_obj = json_object_get_object_member_safely(reply_json_object, "user");
        tk_user_data->two_factor = json_object_get_bool_member_safely(user_obj, "two_factor");
    } else {
        const gchar *message = json_object_get_string_member_safely(reply_json_object, "message");
        tk_user_data->error_message = g_strdup(message);
    }

    g_object_unref(parser);
    g_task_return_pointer(task, tk_user_data, NULL);
}

void vdi_session_generate_qr_code_task(GTask *task,
                                       gpointer       source_object G_GNUC_UNUSED,
                                       gpointer       task_data G_GNUC_UNUSED,
                                       GCancellable  *cancellable G_GNUC_UNUSED)
{
    g_autofree gchar *url_str = NULL;
    url_str = g_strdup_printf("%s/client/generate_user_qr_code", vdi_session_static->api_url);
    g_autofree gchar *response_body_str;
    response_body_str = vdi_session_api_call("POST", url_str, NULL, NULL, NULL);

    // parse response
    JsonParser *parser = json_parser_new();
    ServerReplyType server_reply_type;
    JsonObject *reply_json_object = json_get_data_or_errors_object(parser, response_body_str, &server_reply_type);

    UserData *tk_user_data = calloc(1, sizeof(UserData)); // free in callback!
    tk_user_data->is_success = (server_reply_type == SERVER_REPLY_TYPE_DATA);
    if (tk_user_data->is_success) {
        tk_user_data->qr_uri = g_strdup(json_object_get_string_member_safely(reply_json_object, "qr_uri"));
        tk_user_data->secret = g_strdup(json_object_get_string_member_safely(reply_json_object, "secret"));
    } else {
        const gchar *message = json_object_get_string_member_safely(reply_json_object, "message");
        tk_user_data->error_message = g_strdup(message);
    }

    g_object_unref(parser);
    g_task_return_pointer(task, tk_user_data, NULL);
}

void vdi_session_get_vm_data_task(GTask *task,
                                  gpointer       source_object G_GNUC_UNUSED,
                                  gpointer       task_data G_GNUC_UNUSED,
                                  GCancellable  *cancellable G_GNUC_UNUSED)
{
    // url
    g_autofree gchar *url_str = NULL;
    url_str = g_strdup_printf("%s/client/get_vm_data/%s", vdi_session_static->api_url,
                                     vdi_session_static->current_vm_id);

    // Request
    g_autofree gchar *response_body_str = NULL;
    response_body_str = vdi_session_api_call("GET", url_str, NULL, NULL, NULL);

    // Parse response (На данный момент нужны только данные для подключения по спайс)
    JsonParser *parser = json_parser_new();
    ServerReplyType server_reply_type;
    JsonObject *reply_json_object = json_get_data_or_errors_object(parser, response_body_str, &server_reply_type);

    VeilVmData *vdi_vm_data = calloc(1, sizeof(VeilVmData));
    vdi_vm_data->server_reply_type = server_reply_type;

    if (server_reply_type == SERVER_REPLY_TYPE_DATA) {
        vdi_vm_data->vm_password = g_strdup(json_object_get_string_member_safely(
                reply_json_object, "graphics_password"));

        vdi_vm_data->vm_host = g_strdup(json_object_get_string_member_safely(
                reply_json_object, "controller_address"));
        vdi_vm_data->vm_port = (int)json_object_get_int_member_safely(reply_json_object, "remote_access_port");


        JsonObject *spice_conn_object = json_object_get_object_member_safely(reply_json_object, "spice_conn");
        if (spice_conn_object) {
            vdi_vm_data->spice_conn.address =
                    g_strdup(json_object_get_string_member_safely(spice_conn_object, "address"));
            vdi_vm_data->spice_conn.port =
                    (int)json_object_get_int_member_safely(spice_conn_object, "port");
        } else {
            g_warning("%s Error reply. Response_body_str: %s", (const char *)__func__, response_body_str);
        }
    } else {
        g_warning("%s Error reply. Response_body_str: %s", (const char *)__func__, response_body_str);
    }

    vdi_vm_data->message = g_strdup(json_object_get_string_member_safely(reply_json_object, "message"));
    g_info("%s: server_reply_type %s", (const char *)__func__, vdi_vm_data->message);

    g_object_unref(parser);
    g_task_return_pointer(task, vdi_vm_data, NULL);
}

void vdi_session_set_pool_favorite_task(GTask *task G_GNUC_UNUSED,
                                   gpointer source_object G_GNUC_UNUSED,
                                   gpointer task_data,
                                   GCancellable *cancellable G_GNUC_UNUSED)
{
    FavoritePoolTaskData *favorite_pool_task_data = (FavoritePoolTaskData *)task_data;
    g_info("TEST: %s", favorite_pool_task_data->pool_id);

    // url
    g_autofree gchar *url_str = NULL;
    if (favorite_pool_task_data->is_favorite)
        url_str = g_strdup_printf("%s/client/pools/%s/add-pool-to-favorite",
                vdi_session_static->api_url, favorite_pool_task_data->pool_id);
    else
        url_str = g_strdup_printf("%s/client/pools/%s/remove-pool-from-favorite",
                vdi_session_static->api_url, favorite_pool_task_data->pool_id);

    gchar *response_body_str = vdi_session_api_call("POST", url_str, NULL, NULL, NULL);
    g_free(response_body_str);

    vdi_api_session_free_favorite_pool_task_data(favorite_pool_task_data);
}


gboolean vdi_session_logout(void)
{
    // stop websocket connection
    vdi_ws_client_stop(&vdi_session_static->vdi_ws_client);

    vdi_session_cancel_pending_requests();

    free_memory_safely(&vdi_session_static->current_logged_address);

    g_info("%s", (const char *)__func__);
    g_autofree gchar *jwt_str = NULL;
    jwt_str = atomic_string_get(&vdi_session_static->jwt);
    if (jwt_str) {
        g_autofree gchar *url_str = NULL;
        url_str = g_strdup_printf("%s/logout", vdi_session_static->api_url);

        SoupMessage *msg = soup_message_new("POST", url_str);

        if (msg == NULL) {
            g_info("%s : Cant construct logout message", (const char *)__func__);
            return FALSE;

        } else {
            // set header
            setup_header_for_vdi_session_api_call(msg, NULL);
            // send
            g_object_set(vdi_session_static->soup_session, "timeout", 1, NULL);
            send_message(msg, url_str);
            g_object_set(vdi_session_static->soup_session, "timeout", HTTP_RESPONSE_TIOMEOUT, NULL);

            guint res_code = msg->status_code;
            g_object_unref(msg);

            if (res_code == OK_RESPONSE) {
                // logout was successful so we can forget the token
                atomic_string_set(&vdi_session_static->jwt, NULL);
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

gchar *vdi_session_check_for_tk_updates(const gchar *veil_connect_url, gchar **p_last_version)
{
    // init
    gchar *string_pattern = NULL;
    gchar *bitness = NULL;
    GMatchInfo *match_info = NULL;
    GRegex *regex = NULL;
    gchar *found_match = NULL;
    gchar *download_link = NULL;
    //
    SoupMessage *msg = soup_message_new("GET", veil_connect_url);
    if (msg == NULL)
        goto clear_mark;

    guint status = util_send_message(vdi_session_static->soup_session, msg, veil_connect_url);
    if (status != OK_RESPONSE)
        goto clear_mark;

    // parse
    // find veil-connect_*.*.*-x**-installer
#if _WIN64
    bitness = g_strdup("64");
#elif _WIN32
    bitness = g_strdup("32");
#endif
    if (bitness == NULL)
        goto clear_mark;

    // str we need to find
    string_pattern = g_strdup_printf("((\\d{1,3}).(\\d{1,3}).(\\d{1,3})-x%s)", bitness);
    regex = g_regex_new(string_pattern, 0, 0, NULL);

    g_regex_match(regex, msg->response_body->data, 0, &match_info);
    g_info("msg->response_body->data %s ", msg->response_body->data);

    gboolean is_success = g_match_info_matches(match_info);
    g_info("!!!is_success: %i", is_success);

    if (!is_success) {
        goto clear_mark;
    }

    found_match = g_match_info_fetch(match_info, 0);
    g_info("!!!found_match: %s", found_match);
    // extract version
    (*p_last_version) = g_strndup(found_match, strlen_safely(found_match) - 4); // 4 letters -x**
    g_info("!!!last_version: %s", (*p_last_version));

    // compare versions
    gint version_cmp_res = virt_viewer_compare_version((*p_last_version), PACKAGE_VERSION); // PACKAGE_VERSION
    g_info("!!!version_cmp_res: %i", version_cmp_res);
    // 1 means we have a fresher version and can update
    if (version_cmp_res > 0) {
        download_link = g_strdup_printf("%s\\veil-connect_%s-installer.exe",
                veil_connect_url, found_match);
        g_info("download_link: %s", download_link);
    }

clear_mark:
    if (msg)
        g_object_unref(msg);
    free_memory_safely(&string_pattern);
    free_memory_safely(&bitness);
    if(match_info)
        g_match_info_free(match_info);
    if (regex)
        g_regex_unref(regex);
    free_memory_safely(&found_match);

    return download_link;
}

// Добавляет USB TCP и в случае успеха возвращает usb_uuid. NULL при неудаче
gchar *vdi_session_attach_usb(AttachUsbData *attach_usb_data)
{
    g_info("%s", (const char*)__func__);
    //g_usleep(1000000); // temp
    gchar *usb_uuid = NULL;

    // url
    gchar *url_str = g_strdup_printf("%s/client/pools/%s/attach-usb", vdi_session_static->api_url,
                                     vdi_session_get_current_pool_id());

    // body
    gchar *body_str = g_strdup_printf("{\"host_address\":\"%s\",\"host_port\":%i}",
            attach_usb_data->host_address, attach_usb_data->host_port);
    g_info("%s send body_str: %s", (const char *)__func__, body_str);

    // request
    gchar *response_body_str = vdi_session_api_call("POST", url_str, body_str, NULL, NULL);
    g_info("%s response_body_str: %s", (const char *)__func__, response_body_str);

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

        g_object_unref(parser);
    }

    // Для  остановки основного потока текущего USB
    if (!usb_uuid) {
        g_info("%s: TCP USB Device with host %s is NOT found in reply",
               (const char *)__func__, attach_usb_data->host_address);
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
    gchar *url_str = g_strdup_printf("%s/client/pools/%s/detach-usb", vdi_session_static->api_url,
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
    gchar *response_body_str = vdi_session_api_call("POST", url_str, body_str, NULL, &resp_code);
    g_info("%s: response_body_str: %s", (const char*)__func__, response_body_str);
    if (resp_code == OK_RESPONSE || resp_code == ACCEPT_RESPONSE)
        status = TRUE;
    // parse
    /*
    if (response_body_str) {
        g_info("%s response_body_str: %s", (const char *)__func__, response_body_str);

        JsonParser *parser = json_parser_new();

        ServerReplyType server_reply_type;
        json_get_data_or_errors_object(parser, response_body_str, &server_reply_type);
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

    execute_async_task(vdi_session_do_action_on_vm_task, NULL, action_on_vm_data, NULL);
}

void vdi_api_session_free_action_on_vm_data(ActionOnVmData *action_on_vm_data)
{
    free_memory_safely(&action_on_vm_data->action_on_vm_str);
    free(action_on_vm_data);
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

void vdi_api_session_free_text_message_data(TextMessageData *text_message_data)
{
    free_memory_safely(&text_message_data->message);
    free_memory_safely(&text_message_data->error_message);
    free(text_message_data);
}

void vdi_api_session_free_tk_user_data(UserData *tk_user_data)
{
    free_memory_safely(&tk_user_data->qr_uri);
    free_memory_safely(&tk_user_data->secret);
    free_memory_safely(&tk_user_data->error_message);
    free(tk_user_data);
}

void vdi_api_session_clear_login_data(LoginData *login_data)
{
    free_memory_safely(&login_data->reply_msg);
    free_memory_safely(&login_data->domain);
}

void vdi_api_session_free_login_data(LoginData *login_data)
{
    vdi_api_session_clear_login_data(login_data);
    free(login_data);
}

void vdi_api_session_free_favorite_pool_task_data(FavoritePoolTaskData *data)
{
    g_free(data->pool_id);
    free(data);
}