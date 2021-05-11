/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VIRT_VIEWER_VEIL_VDI_API_SESSION_H
#define VIRT_VIEWER_VEIL_VDI_API_SESSION_H

#include "vdi_redis_client.h"

#include <gtk/gtk.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup-session.h>
#include <libsoup/soup-message.h>


#include "vdi_ws_client.h"
#include "async.h"
#include "jsonhandler.h"

#define HTTP_RESPONSE_TIOMEOUT 20

typedef enum{
    USER_PERMISSION_NO_PERMISSIONS = 0,
    USER_PERMISSION_USB_REDIR = 1, // 0001
    USER_PERMISSION_FOLDERS_REDIR = 2, // 0010
    USER_PERMISSION_SHARED_CLIPBOARD = 4 // 0100
} UserPermission;
// remote protocol type
typedef enum{
    VDI_SPICE_PROTOCOL,
    VDI_SPICE_DIRECT_PROTOCOL,
    VDI_RDP_PROTOCOL,
    VDI_RDP_WINDOWS_NATIVE_PROTOCOL,
    VDI_ANOTHER_REMOTE_PROTOCOL
} VdiVmRemoteProtocol;

// vm operational system
typedef enum{
    VDI_VM_WIN,
    VDI_VM_LINUX,
    VDI_VM_ANOTHER_OS
} VdiVmOs;

typedef enum{
    VDI_POOL_TYPE_UNKNOWN,
    VDI_POOL_TYPE_STATIC,
    VDI_POOL_TYPE_AUTOMATED,
    VDI_POOL_TYPE_RDS
} VdiPoolType;

typedef struct{

    gchar *farm_alias;
    GArray *app_array;

} VdiFarmData;

typedef struct{

    gchar *app_name;
    gchar *app_alias;
    gchar *icon_base64;

} VdiAppData;

// Инфа о виртуальной машине полученная от vdi
typedef struct{

    VdiVmOs os_type;

    gchar *vm_host;
    int vm_port;
    gchar *vm_password;
    gchar *vm_verbose_name;

    gchar *message;

    gint test_data;
    ServerReplyType server_reply_type;

    // For RDS only
    GArray *farm_array;

} VdiVmData;

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
    gchar *vdi_ip;
    int vdi_port;

    gchar *api_url;
    gchar *auth_url;
    gchar *jwt;
    gboolean is_ldap;

    // data about current pool and vm
    VdiPoolType pool_type;
    gchar *current_pool_id;
    VdiVmRemoteProtocol current_remote_protocol;
    gchar *current_vm_id;
    gchar *current_vm_verbose_name;
    gchar *current_controller_address;

    guint user_permissions; // UserPermission flags

    RedisClient redis_client;
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

// get vid server ip
const gchar *vdi_session_get_vdi_ip(void);
// get port
int vdi_session_get_vdi_port(void);
// get username
const gchar *vdi_session_get_vdi_username(void);
// get password
const gchar *vdi_session_get_vdi_password(void);
//get current token
const gchar *vdi_session_get_token(void);

// cancell pending requests
void vdi_session_cancell_pending_requests(void);
// set vdi session credentials
void vdi_session_set_credentials(const gchar *username, const gchar *password, const gchar *ip,
                         int port, gboolean is_ldap);
// set current vm id
void vdi_session_set_current_pool_id(const gchar *current_pool_id);
// get current vm id
const gchar *vdi_session_get_current_pool_id(void);

VdiPoolType vdi_session_get_current_pool_type(void);

const gchar *vdi_session_get_current_vm_id(void);

// set current remote protocol
void vdi_session_set_current_remote_protocol(VdiVmRemoteProtocol remote_protocol);
// get current remote protocol
VdiVmRemoteProtocol vdi_session_get_current_remote_protocol(void);

VdiVmRemoteProtocol vdi_session_str_to_remote_protocol(const gchar *protocol_str);
const gchar *vdi_session_remote_protocol_to_str(VdiVmRemoteProtocol protocol);

VdiWsClient *vdi_session_get_ws_client(void);

// get current vm name
const gchar *vdi_session_get_current_vm_name(void);

const gchar *vdi_session_get_current_controller_address(void);

// Set user permissions
void vdi_session_set_permissions(JsonArray *user_permissions_array);
gboolean vdi_session_is_usb_redir_permitted(void);
gboolean vdi_session_is_folders_redir_permitted(void);
gboolean vdi_session_is_shared_clipboard_permitted(void);

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

// Log out sync
gboolean vdi_session_logout(void);

// Check for veil connect updates
// Return link to dowmload new version or NULL
gchar *vdi_session_check_for_tk_updates(const gchar *veil_connect_url, gchar **p_last_version);

// Download installer
// Return path to installer
gchar *vdi_session_download_tk_installer(const gchar *download_link);

// Attach USB
gchar *vdi_session_attach_usb(AttachUsbData *attach_usb_data);

// Detach USB
gboolean vdi_session_detach_usb(DetachUsbData *detach_usb_data);

// Do action on vm async
void vdi_api_session_execute_task_do_action_on_vm(const gchar *actionStr, gboolean isForced);


void vdi_api_session_free_action_on_vm_data(ActionOnVmData *action_on_vm_data);
void vdi_api_session_free_vdi_vm_data(VdiVmData *vdi_vm_data);
void vdi_api_session_free_attach_usb_data(AttachUsbData *attach_usb_data);
void vdi_api_session_free_detach_usb_data(DetachUsbData *detach_usb_data);
void vdi_api_session_free_text_message_data(TextMessageData *text_message_data);

#endif //VIRT_VIEWER_VEIL_VDI_API_SESSION_H
