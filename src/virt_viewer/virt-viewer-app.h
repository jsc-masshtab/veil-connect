/*
 * Virt Viewer: A virtual machine console viewer
 *
 * Copyright (C) 2007-2012 Red Hat, Inc.
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
 *
 * Updated by: http://mashtab.org/
 */

#ifndef VIRT_VIEWER_APP_H
#define VIRT_VIEWER_APP_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include "virt-viewer-window.h"
#include "remote-viewer-util.h"
#include "vdi_session.h"
#include "net_speedometer.h"
#include "settings_data.h"

G_BEGIN_DECLS

#define VIRT_VIEWER_TYPE_APP virt_viewer_app_get_type()
#define VIRT_VIEWER_APP(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIRT_VIEWER_TYPE_APP, VirtViewerApp))
#define VIRT_VIEWER_APP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), VIRT_VIEWER_TYPE_APP, VirtViewerAppClass))
#define VIRT_VIEWER_IS_APP(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), VIRT_VIEWER_TYPE_APP))
#define VIRT_VIEWER_IS_APP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), VIRT_VIEWER_TYPE_APP))
#define VIRT_VIEWER_APP_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), VIRT_VIEWER_TYPE_APP, VirtViewerAppClass))

typedef struct _VirtViewerAppPrivate VirtViewerAppPrivate;
typedef struct _VirtViewerApp VirtViewerApp;

struct _VirtViewerApp {
    GObject parent;
    VirtViewerAppPrivate *priv;

    ConnectSettingsData *p_conn_data;
    GtkApplication *application_p; // pointer to current application instance
    NetSpeedometer *net_speedometer;
};

typedef struct {
    GObjectClass parent_class;

    /*< private >*/
    gboolean (*initial_connect) (VirtViewerApp *self, GError **error);
    gboolean (*activate) (VirtViewerApp *self, GError **error);
    void (*deactivated) (VirtViewerApp *self, gboolean connect_error);
    gboolean (*open_connection)(VirtViewerApp *self, int *fd);
    void (*add_option_entries)(VirtViewerApp *self, GOptionContext *context, GOptionGroup *group);

    /* signals */
    void (*job_finished)(VirtViewerApp *self);
    void (*quit_requested)(VirtViewerApp *self);
} VirtViewerAppClass;

GType virt_viewer_app_get_type (void);

VirtViewerApp *virt_viewer_app_new(void);
void virt_viewer_app_set_app_pointer(VirtViewerApp *self, GtkApplication *application);
void virt_viewer_app_set_spice_session_data(VirtViewerApp *self, ConnectSettingsData *p_conn_data);

void virt_viewer_app_setup(VirtViewerApp *self, ConnectSettingsData *conn_data);
gboolean virt_viewer_app_show_main_window(VirtViewerApp *self);
void virt_viewer_app_maybe_quit(VirtViewerApp *self, VirtViewerWindow *window);
VirtViewerWindow* virt_viewer_app_get_main_window(VirtViewerApp *self);
void virt_viewer_app_trace(VirtViewerApp *self, const char *fmt, ...);
void virt_viewer_app_simple_message_dialog(VirtViewerApp *self, const char *fmt, ...);
gboolean virt_viewer_app_is_active(VirtViewerApp *app);
void virt_viewer_app_free_connect_info(VirtViewerApp *self);
gboolean virt_viewer_app_create_session(VirtViewerApp *self, const gchar *type, GError **error);
gboolean virt_viewer_app_activate(VirtViewerApp *self, GError **error);
gboolean virt_viewer_app_initial_connect(VirtViewerApp *self, GError **error);
gboolean virt_viewer_app_get_direct(VirtViewerApp *self);
void virt_viewer_app_set_direct(VirtViewerApp *self, gboolean direct);
void virt_viewer_app_set_hotkeys(VirtViewerApp *self, const gchar *hotkeys);
void virt_viewer_app_set_attach(VirtViewerApp *self, gboolean attach);
gboolean virt_viewer_app_get_attach(VirtViewerApp *self);
gboolean virt_viewer_app_has_session(VirtViewerApp *self);
void virt_viewer_app_set_connect_info(VirtViewerApp *self,
                                      const gchar *host,
                                      const gchar *ghost,
                                      const gchar *gport,
                                      const gchar *gtlsport,
                                      const gchar *transport,
                                      const gchar *unixsock,
                                      const gchar *user,
                                      gint port,
                                      const gchar *guri);
gboolean virt_viewer_app_window_set_visible(VirtViewerApp *self, VirtViewerWindow *window, gboolean visible);
void virt_viewer_app_show_status(VirtViewerApp *self, const gchar *fmt, ...);
void virt_viewer_app_show_display(VirtViewerApp *self);
GList* virt_viewer_app_get_windows(VirtViewerApp *self);
gboolean virt_viewer_app_get_enable_accel(VirtViewerApp *self);
VirtViewerSession* virt_viewer_app_get_session(VirtViewerApp *self);
gboolean virt_viewer_app_get_fullscreen(VirtViewerApp *app);
void virt_viewer_app_clear_hotkeys(VirtViewerApp *app);
GList* virt_viewer_app_get_initial_displays(VirtViewerApp* self);
gint virt_viewer_app_get_initial_monitor_for_display(VirtViewerApp* self, gint display);
void virt_viewer_app_set_enable_accel(VirtViewerApp *app, gboolean enable);
void virt_viewer_app_show_preferences(VirtViewerApp *app, GtkWidget *parent);
void virt_viewer_app_set_menus_sensitive(VirtViewerApp *self, gboolean sensitive);


gboolean virt_viewer_app_get_session_cancelled(VirtViewerApp *self);
gboolean virt_viewer_app_is_quitting(VirtViewerApp *self);

void virt_viewer_app_hide_all_windows_forced(VirtViewerApp *app);

void virt_viewer_app_set_window_name(VirtViewerApp *app, const gchar *vm_verbose_name, const gchar *user_name);

void virt_viewer_app_stop(VirtViewerApp *self, const gchar *signal_upon_job_finish);

//
void virt_viewer_app_deactivate(VirtViewerApp *self, gboolean connect_error);

void virt_viewer_app_enable_auto_clipboard(VirtViewerApp *self, gboolean enabled);

gboolean virt_viewer_connect_attempt(VirtViewerApp *self);
void virt_viewer_app_instant_start(VirtViewerApp *self, ConnectSettingsData *p_conn_data);


G_END_DECLS

#endif /* VIRT_VIEWER_APP_H */
/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
