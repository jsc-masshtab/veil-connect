/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VIRT_VIEWER_VEIL_VDI_API_SESSION_H
#define VIRT_VIEWER_VEIL_VDI_API_SESSION_H

#include <gtk/gtk.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup-session.h>
#include <libsoup/soup-message.h>

#include "remote-viewer-util.h"
#include "vdi_ws_client.h"
#include "async.h"
#include "jsonhandler.h"
#include "atomic_string.h"

#define HTTP_RESPONSE_TIOMEOUT 20

typedef enum{
    USER_PERMISSION_NO_PERMISSIONS                   = 0,
    USER_PERMISSION_USB_REDIR                        = 1 << 0,
    USER_PERMISSION_FOLDERS_REDIR                    = 1 << 1,
    USER_PERMISSION_SHARED_CLIPBOARD_CLIENT_TO_GUEST = 1 << 2,
    USER_PERMISSION_SHARED_CLIPBOARD_GUEST_TO_CLIENT = 1 << 3

} UserPermission;

typedef enum{
    VDI_POOL_TYPE_UNKNOWN,
    VDI_POOL_TYPE_STATIC,
    VDI_POOL_TYPE_AUTOMATED,
    VDI_POOL_TYPE_RDS
} VdiPoolType;

// Data which passed to vdi_session_api_call
typedef struct{
    gchar *action_on_vm_str;
    gboolean is_action_forced;

} ActionOnVmData;

typedef struct {
    gchar *host_address;
    int host_port;
    int *p_running_flag;// pointer to a stack variable. never try to free it
} AttachUsbData;

typedef struct {
    gchar *usb_uuid;
    gboolean remove_all;
} DetachUsbData;

// Data with message to admin. Структура введена на случай появлянию до параметров
typedef struct{
    gchar *message; // сообщение
    gpointer weak_ref_to_logical_owner; // сущность, из которой было отослано сообщение

    gboolean is_successfully_sent; // Было ли сообщение успешно отправлено (определяется по ответу от сервера)
    gchar *error_message; // ответное сообщение от сервера. (Не NULL если возникла ошибка при отправке)

} TextMessageData;

// User data
typedef struct {
    gboolean two_factor; // включена ли 2fa
    gchar *qr_uri;
    gchar *secret;

    gboolean is_success; // ожидаемый ответ от сервера
    gchar *error_message; // ответное сообщение от сервера. (Не NULL если возникла ошибка при запросе)

} UserData;

// Data for vm request
typedef struct {
    int request_id;
    gboolean redirect_time_zone;
} RequestVmFromPoolData;

//VdiSession class

#define TYPE_VDI_SESSION         ( vdi_session_get_type( ) )
#define VDI_SESSION( obj )       ( G_TYPE_CHECK_INSTANCE_CAST( (obj), TYPE_VDI_SESSION, VdiSession ) )
#define IS_VDI_SESSION( obj )        ( G_TYPE_CHECK_INSTANCE_TYPE( (obj), TYPE_VDI_SESSION ) )
#define VDI_SESSION_CLASS( klass )   ( G_TYPE_CHECK_CLASS_CAST( (klass), TYPE_VDI_SESSION, VdiSessionClass ) )
#define IS_VDI_SESSION_CLASS( klass )    ( G_TYPE_CHECK_CLASS_TYPE( (klass), TYPE_VDI_SESSION ) )
#define VDI_SESSION_GET_CLASS( obj ) ( G_TYPE_INSTANCE_GET_CLASS( (obj), TYPE_VDI_SESSION, VdiSessionClass ) )

typedef struct _VdiSession      VdiSession;
typedef struct _VdiSessionClass VdiSessionClass;

struct _VdiSession
{
    GObject parent;

    SoupSession *soup_session;

    gchar *vdi_username;
    gchar *vdi_password;
    gchar *disposable_password; // 2fa
    gchar *vdi_ip;
    int vdi_port;

    gchar *api_url;
    gchar *auth_url;
    gboolean is_ldap;
    AtomicString jwt;
    AtomicString vdi_version;
    gboolean version_older_than_320;

    // data about current pool and vm
    VdiPoolType pool_type;
    gchar *current_pool_id;
    VmRemoteProtocol current_remote_protocol;
    gchar *current_vm_id;
    gchar *current_vm_verbose_name;
    gchar *current_controller_address;

    guint user_permissions; // UserPermission flags

    gchar *login_time;

    VdiWsClient vdi_ws_client;
};

struct _VdiSessionClass
{
    GObjectClass parent_class;

    /* signals */
    void (*vm_changed)(VdiSession *self, int power_state);
    void (*ws_conn_changed)(VdiSession *self, int ws_connected);
    void (*ws_cmd_received)(VdiSession *self, const gchar *cmd);
    void (*text_msg_received)(VdiSession *self, const gchar *author, const gchar *text);
    void (*auth_fail_detected)(VdiSession *self);
    void (*vm_prep_progress_received)(VdiSession *self, int request_id, int progress, const gchar *text);
    void (*pool_entitlement_changed)(VdiSession *self);
};

GType vdi_session_get_type( void ) G_GNUC_CONST;

VdiSession *vdi_session_new(void);

// Functions
VdiSession *get_vdi_session_static(void);
// deinit session
void vdi_session_static_destroy(void);

// vm params change notification
void vdi_session_vm_state_change_notify(int power_state);
void vdi_session_ws_conn_change_notify(int ws_connected);
void vdi_session_ws_cmd_received_notify(const gchar *cmd);
void vdi_session_text_msg_received_notify(const gchar *author, const gchar *text);
void vdi_session_vm_prep_progress_received_notify(int request_id, int progress, const gchar *text);
void vdi_session_pool_entitlement_changed_notify(void);

// get vid server ip
const gchar *vdi_session_get_vdi_ip(void);
// get port
int vdi_session_get_vdi_port(void);

gboolean vdi_session_is_ldap(void);
// get username
const gchar *vdi_session_get_vdi_username(void);
// get password
const gchar *vdi_session_get_vdi_password(void);
//get copy of the current token. memory must be freed
gchar *vdi_session_get_token(void);
//get copy of VDI version string. memory must be freed
gboolean vdi_session_version_older_than_320(void);

// cancell pending requests
void vdi_session_cancel_pending_requests(void);
// set vdi session credentials
void vdi_session_set_credentials(const gchar *username, const gchar *password,
                                 const gchar *disposable_password);
void vdi_session_set_conn_data(const gchar *ip, int port);
void vdi_session_set_ldap(gboolean is_ldap);
// set current pool id
void vdi_session_set_current_pool_id(const gchar *current_pool_id);
// get current vm id
const gchar *vdi_session_get_current_pool_id(void);

VdiPoolType vdi_session_get_current_pool_type(void);

const gchar *vdi_session_get_current_vm_id(void);

// set current remote protocol
void vdi_session_set_current_remote_protocol(VmRemoteProtocol remote_protocol);
// get current remote protocol
VmRemoteProtocol vdi_session_get_current_remote_protocol(void);

VdiWsClient *vdi_session_get_ws_client(void);

// get current vm name
const gchar *vdi_session_get_current_vm_name(void);

const gchar *vdi_session_get_current_controller_address(void);

// Set user permissions
void vdi_session_set_permissions(JsonArray *user_permissions_array);
gboolean vdi_session_is_usb_redir_permitted(void);
gboolean vdi_session_is_folders_redir_permitted(void);
gboolean vdi_session_is_shared_clipboard_c_to_g_permitted(void);
gboolean vdi_session_is_shared_clipboard_g_to_c_permitted(void);

// Login time
void vdi_session_refresh_login_time(void);
const gchar *vdi_session_get_login_time(void);

//void gInputStreamToBuffer(GInputStream *inputStream, gchar *responseBuffer);
// Do api call. Return response body
gchar *vdi_session_api_call(const char *method, const char *uri_string, const gchar *body_str, int *resp_code);

/// Functions for GTasks
// Fetch token
void vdi_session_log_in_task(GTask *task,
                   gpointer       source_object,
                   gpointer       task_data,
                   GCancellable  *cancellable);

// Запрашиваем список пулов
void vdi_session_get_vdi_pool_data_task(GTask *task,
                       gpointer       source_object,
                       gpointer       task_data,
                       GCancellable  *cancellable);

// Получаем виртуалку из пула
void vdi_session_get_vm_from_pool_task(GTask *task,
                      gpointer       source_object,
                      gpointer       task_data,
                      GCancellable  *cancellable);

// Do action on virtual machine
void vdi_session_do_action_on_vm_task(GTask *task,
                     gpointer       source_object,
                     gpointer       task_data,
                     GCancellable  *cancellable);

// Send text message
void vdi_session_send_text_msg_task(GTask *task,
                                      gpointer       source_object,
                                      gpointer       task_data,
                                      GCancellable  *cancellable);

// Request user data
void vdi_session_get_user_data_task(GTask *task,
                                    gpointer       source_object,
                                    gpointer       task_data,
                                    GCancellable  *cancellable);

// Update user data
void vdi_session_update_user_data_task(GTask *task,
                                       gpointer       source_object,
                                       gpointer       task_data,
                                       GCancellable  *cancellable);

// Generate QR data
void vdi_session_generate_qr_code_task(GTask *task,
                                       gpointer       source_object,
                                       gpointer       task_data,
                                       GCancellable  *cancellable);

// Get VM info
void vdi_session_get_vm_data_task(GTask *task,
                                  gpointer       source_object,
                                  gpointer       task_data,
                                  GCancellable  *cancellable);

// Log out sync
gboolean vdi_session_logout(void);

// Check for veil connect updates
// Return link to dowmload new version or NULL
gchar *vdi_session_check_for_tk_updates(const gchar *veil_connect_url, gchar **p_last_version);

// Attach USB
gchar *vdi_session_attach_usb(AttachUsbData *attach_usb_data);

// Detach USB
gboolean vdi_session_detach_usb(DetachUsbData *detach_usb_data);

// Do action on vm async
void vdi_api_session_execute_task_do_action_on_vm(const gchar *actionStr, gboolean isForced);


void vdi_api_session_free_action_on_vm_data(ActionOnVmData *action_on_vm_data);
void vdi_api_session_free_attach_usb_data(AttachUsbData *attach_usb_data);
void vdi_api_session_free_detach_usb_data(DetachUsbData *detach_usb_data);
void vdi_api_session_free_text_message_data(TextMessageData *text_message_data);
void vdi_api_session_free_tk_user_data(UserData *tk_user_data);

#endif //VIRT_VIEWER_VEIL_VDI_API_SESSION_H
