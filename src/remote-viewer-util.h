/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2012 Red Hat, Inc.
 * Copyright (C) 2009-2012 Daniel P. Berrange
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Daniel P. Berrange <berrange@redhat.com>
 */

#ifndef VIRT_VIEWER_UTIL_H
#define VIRT_VIEWER_UTIL_H

#include <gtk/gtk.h>

extern gboolean doDebug;

enum {
    VIRT_VIEWER_ERROR_FAILED,
    VIRT_VIEWER_ERROR_CANCELLED
};

typedef enum
{
    AUTH_DIALOG,
    VDI_DIALOG
} RemoteViewerState;

typedef enum{
    H264_CODEC_AVC420,
    H264_CODEC_AVC444
} H264_CODEC_TYPE;


// Инфо для соеднинения сигнала и каллбэка
typedef struct
{
    gboolean response;
    GMainLoop *loop;
    GtkResponseType dialog_window_response;
} ConnectionInfo;


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

// Conver string from local to utf8
void convert_string_from_utf8_to_locale(gchar **utf8_str);

gchar *get_windows_app_data_location(void);

void show_about_dialog(GtkWindow *parent_window, gpointer data_for_builder_connect_signal);

const gchar *get_cur_ini_param_group(void);

const gchar *h264_codec_to_string(H264_CODEC_TYPE codec);
H264_CODEC_TYPE string_to_h264_codec(const gchar *str);
H264_CODEC_TYPE get_default_h264_codec(void);

//void gtk_combo_box_text_set_active_text(GtkComboBoxText *combo_box, const gchar *text);

gchar *string_to_json_value(const gchar *string);

#endif

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
