/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VIRT_VIEWER_UTIL_H
#define VIRT_VIEWER_UTIL_H

#include <gtk/gtk.h>
#include <libsoup/soup-session.h>

#include <spice-client-gtk.h>

#include "jsonhandler.h"

extern gboolean doDebug;

// remote protocol type
typedef enum{
    SPICE_PROTOCOL,
    SPICE_DIRECT_PROTOCOL,
    RDP_PROTOCOL,
    RDP_NATIVE_PROTOCOL,
    X2GO_PROTOCOL,
    ANOTHER_REMOTE_PROTOCOL
} VmRemoteProtocol;

typedef enum {
    LINUX_DISTRO_UNKNOWN,
    LINUX_DISTRO_DEBIAN_LIKE,
    LINUX_DISTRO_CENTOS_LIKE
} LINUX_DISTRO;

enum {
    VIRT_VIEWER_ERROR_FAILED,
    VIRT_VIEWER_ERROR_CANCELLED
};

typedef enum
{
    APP_STATE_UNDEFINED,
    APP_STATE_AUTH_DIALOG, // Начальое окно авторизации
    APP_STATE_CONNECT_TO_VM, // Выбор и подключение к ВМ.
    APP_STATE_EXITING // Завершение приложение
} RemoteViewerState;

typedef enum{
    VM_POWER_STATE_OFF = 1,
    VM_POWER_STATE_PAUSED = 2,
    VM_POWER_STATE_ON = 3
} VM_POWER_STATE;

// Инфо для соеднинения сигнала и каллбэка
typedef struct
{
    gboolean response;
    GMainLoop *loop;
    RemoteViewerState next_app_state;
} ConnectionInfo;


typedef struct {
    gulong bytes_inputs;
    gulong bytes_webdav;
    gulong bytes_cursor;
    gulong bytes_display;
    gulong bytes_record;
    gulong bytes_playback;
    gulong bytes_main;

} SpiceReadBytes;

// vm operational system
typedef enum{
    VDI_VM_WIN,
    VDI_VM_LINUX,
    VDI_VM_ANOTHER_OS
} VeilVmOs;

typedef struct{

    gchar *farm_alias;
    GArray *app_array;

} VeilFarmData;

typedef struct{

    gchar *app_name;
    gchar *app_alias;
    gchar *icon_base64;

} VeilAppData;

// Инфа о виртуальной машине, полученная от сервера
typedef struct{

    VeilVmOs os_type;

    gchar *vm_host;
    int vm_port;

    gchar *vm_username;
    gchar *vm_password;
    gchar *vm_verbose_name;

    gchar *message;

    gint test_data;
    ServerReplyType server_reply_type;

    // For RDS only
    GArray *farm_array;

} VeilVmData;


#define VIRT_VIEWER_ERROR virt_viewer_error_quark ()
#define VIRT_VIEWER_RESOURCE_PREFIX  "/org/virt-manager/virt-viewer"

#define MARK_VAR_UNUSED(x) (void)(x)

void create_loop_and_launch(GMainLoop **loop);
void shutdown_loop(GMainLoop *loop);

GQuark virt_viewer_error_quark(void);

void virt_viewer_util_init(const char *appname);

GtkBuilder *remote_viewer_util_load_ui(const char *name);
int virt_viewer_util_extract_host(const char *uristr,
                                  char **scheme,
                                  char **host,
                                  char **transport,
                                  char **user,
                                  int *port);

gulong virt_viewer_signal_connect_object(gpointer instance,
                                         const gchar *detailed_signal,
                                         GCallback c_handler,
                                         gpointer gobject,
                                         GConnectFlags connect_flags);

gchar* spice_hotkey_to_gtk_accelerator(const gchar *key);

gint virt_viewer_compare_version(const gchar *s1, const gchar *s2);
gint virt_viewer_compare_buildid(const gchar *s1, const gchar *s2);

/* monitor alignment */
void virt_viewer_align_monitors_linear(GHashTable *displays);
void virt_viewer_shift_monitors_to_origin(GHashTable *displays);

/* monitor mapping */
GHashTable* virt_viewer_parse_monitor_mappings(gchar **mappings,
                                               const gsize nmappings,
                                               const gint nmonitors);

void free_memory_safely(gchar **string_ptr);

size_t strlen_safely(const gchar *str);

void update_string_safely(gchar **string_ptr, const gchar *new_string);

gchar *strstrip_safely(gchar *str);

void g_source_remove_safely(guint *timeout_id);

const gchar* determine_http_protocol_by_port(int port);

// enable spice debug cursor
// use env variable to activate spice debug cursor
void set_client_spice_cursor_visible(gboolean is_visible);

// Get widget from widget builder
GtkWidget *get_widget_from_builder(GtkBuilder *builder, const gchar *name);

// Get path to log directory. Must be freed
gchar *get_log_dir_path(void);

// Replace substring
gchar *replace_str(const gchar *src, const gchar *find, const gchar *replace);

gchar *util_remove_all_spaces(const gchar *src);

// Conver string from local to utf8
void convert_string_from_utf8_to_locale(gchar **utf8_str);

gchar *get_windows_app_data_location(void);
gchar *get_windows_app_temp_location(void);

// ini секция для данных подключения  VDI
const gchar *get_cur_ini_group_vdi(void);
// ini секция для подключения к ВМ напрямую
const gchar *get_cur_ini_group_direct(void);
// ini секция для данных подключения к контроллеру
const gchar *get_cur_ini_group_controller(void);

//void gtk_combo_box_text_set_active_text(GtkComboBoxText *combo_box, const gchar *text);

const gchar *util_get_os_name(void);

const gchar *bool_to_str(gboolean flag);

const gchar *vm_power_state_to_str(int power_state);
void set_vm_power_state_on_label(GtkLabel *label, int power_state);

void show_msg_box_dialog(GtkWindow *parent, const gchar *message);

void remove_char(char *str, char character);

// must be freed
gchar *get_current_readable_time(void);

void copy_file_content(FILE *sourceFile, FILE *destFile);

// Extract name and domain from full_user_name (name@domain)
void extract_name_and_domain(const gchar *full_user_name, gchar **user_name, gchar **domain);

void util_show_monitor_config_window(GtkWindow *parent, GdkDisplay *display);
int util_get_monitor_number(GdkDisplay *display);
void util_get_monitor_geometry(GdkDisplay *display, int num, GdkRectangle *geometry);
gboolean util_check_if_monitor_primary(GdkDisplay *display, int num);
gboolean util_check_if_monitor_number_valid(GdkDisplay *display, int num);

const gchar *util_spice_channel_event_to_str(SpiceChannelEvent event);

guint util_send_message(SoupSession *soup_session, SoupMessage *msg, const char *uri_string);

void util_free_veil_vm_data(VeilVmData *vm_data);

void util_set_message_to_info_label(GtkLabel *label, const gchar *message);

VmRemoteProtocol util_str_to_remote_protocol(const gchar *protocol_str);
const gchar *util_remote_protocol_to_str(VmRemoteProtocol protocol);
#endif

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
