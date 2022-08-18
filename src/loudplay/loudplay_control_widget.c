/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <glib/gi18n.h>

#include "loudplay_control_widget.h"
#include "remote-viewer-util.h"
#include "config.h"
#include "vm_control_menu.h"

G_DEFINE_TYPE( LoudplayControlWidget, loudplay_control_widget, G_TYPE_OBJECT )


static void loudplay_control_widget_clear(LoudplayControlWidget *self)
{
    if (self->builder) {
        g_object_unref(self->builder);
        self->builder = NULL;
    }
    if (self->main_window) {
        gtk_widget_destroy(self->main_window);
        self->main_window = NULL;
    }
    if (self->menu_vm_control) {
        gtk_widget_destroy(self->menu_vm_control);
        self->menu_vm_control = NULL;
    }

    self->loudplay_config_gui = NULL;
    self->status_label = NULL;
    self->settings_invoke_window = NULL;
}

static void loudplay_control_widget_finalize(GObject *object)
{
    g_info("%s", (const char *) __func__);

    LoudplayControlWidget *self = LOUDPLAY_CONTROL_WIDGET(object);
    loudplay_control_widget_clear(self);

    GObjectClass *parent_class = G_OBJECT_CLASS( loudplay_control_widget_parent_class );
    ( *parent_class->finalize )( object );
}

static void
loudplay_control_widget_btn_invoke_settings_clicked(GtkButton *button G_GNUC_UNUSED, gpointer data)
{
    LoudplayControlWidget *self = (LoudplayControlWidget *)data;
    gtk_widget_show_all(self->main_window);
    gtk_window_present(GTK_WINDOW(self->main_window));
}

static void
loudplay_control_widget_btn_apply_settings_clicked(GtkButton *button G_GNUC_UNUSED, gpointer data)
{
    LoudplayControlWidget *self = (LoudplayControlWidget *)data;

    gobject_gui_fill_gobject_from_gui(G_OBJECT(self->p_conn_data->loudplay_config), self->loudplay_config_gui);

    g_autofree gchar *msg = NULL;
    msg = g_strdup_printf(_("Settings update requested by %s"), APPLICATION_NAME);
    loudplay_control_widget_set_status(self, msg);
    g_signal_emit_by_name(self, "settings-change-requested");
}

static void
loudplay_control_widget_btn_stop_clicked(GtkButton *button G_GNUC_UNUSED, gpointer data)
{
    LoudplayControlWidget *self = (LoudplayControlWidget *)data;
    g_autofree gchar *msg = NULL;
    msg = g_strdup_printf(_("Stop requested by %s"), APPLICATION_NAME);
    loudplay_control_widget_set_status(self, msg);
    g_signal_emit_by_name(self, "process-stop-requested");
}

static void
loudplay_control_widget_btn_show_vm_control_clicked(GtkButton *button G_GNUC_UNUSED, gpointer data)
{
    LoudplayControlWidget *self = (LoudplayControlWidget *)data;

    gtk_widget_show_all(self->menu_vm_control);
    gtk_menu_popup_at_widget(GTK_MENU(self->menu_vm_control),
                             self->btn_show_vm_control, GDK_GRAVITY_SOUTH, GDK_GRAVITY_NORTH, NULL);
}

static gboolean
window_deleted_cb(LoudplayControlWidget *self)
{
    g_info("%s", (const char *) __func__);
    loudplay_control_widget_clear(self);
    return TRUE;
}

static void loudplay_control_widget_class_init(LoudplayControlWidgetClass *klass )
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = loudplay_control_widget_finalize;

    // signals
    g_signal_new("settings-change-requested",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(LoudplayControlWidgetClass, settings_change_requested),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
    g_signal_new("process-stop-requested",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(LoudplayControlWidgetClass, process_stop_requested),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
}

static void loudplay_control_widget_init(LoudplayControlWidget *self)
{
    g_info("%s", (const char *) __func__);
    self->builder = NULL;
    self->settings_invoke_window = NULL;
    self->main_window = NULL;
}

static void loudplay_control_widget_create_gui_if_required(LoudplayControlWidget *self)
{
    if (self->builder)
        return;

    self->builder = remote_viewer_util_load_ui("loudplay_control_form.glade");
    self->main_window = GTK_WIDGET(gtk_builder_get_object(self->builder, "main_window"));
    g_autofree gchar *title = NULL;
    title = g_strdup_printf("Loudplay  -  %s", APPLICATION_NAME);
    gtk_window_set_title(GTK_WINDOW(self->main_window), title);
    self->status_label = GTK_WIDGET(gtk_builder_get_object(self->builder, "status_label"));

    self->btn_apply_settings = GTK_WIDGET(gtk_builder_get_object(self->builder, "btn_apply_settings"));
    self->btn_stop = GTK_WIDGET(gtk_builder_get_object(self->builder, "btn_stop"));

    self->loudplay_viewport = GTK_WIDGET(gtk_builder_get_object(self->builder, "loudplay_viewport"));

    self->settings_invoke_window = GTK_WIDGET(gtk_builder_get_object(self->builder, "settings_invoke_window"));
    self->btn_invoke_settings = GTK_WIDGET(gtk_builder_get_object(self->builder, "btn_invoke_settings"));

    self->btn_show_vm_control = GTK_WIDGET(gtk_builder_get_object(self->builder, "btn_show_vm_control"));

    self->menu_vm_control = vm_control_menu_create();

    g_signal_connect(self->btn_invoke_settings, "clicked",
                     G_CALLBACK(loudplay_control_widget_btn_invoke_settings_clicked), self);
    g_signal_connect(self->btn_apply_settings, "clicked",
                     G_CALLBACK(loudplay_control_widget_btn_apply_settings_clicked), self);
    g_signal_connect(self->btn_stop, "clicked",
                     G_CALLBACK(loudplay_control_widget_btn_stop_clicked), self);
    g_signal_connect(self->btn_show_vm_control, "clicked",
                     G_CALLBACK(loudplay_control_widget_btn_show_vm_control_clicked), self);
    g_signal_connect_swapped(self->main_window, "delete-event",
                             G_CALLBACK(window_deleted_cb), self);
}

LoudplayControlWidget *loudplay_control_widget_new(ConnectSettingsData *conn_data)
{
    g_info("%s", (const char *)__func__);
    LoudplayControlWidget *self =
            LOUDPLAY_CONTROL_WIDGET( g_object_new( TYPE_LOUDPLAY_CONTROL_WIDGET, NULL ) );
    self->p_conn_data = conn_data;

    return self;
}

void loudplay_control_widget_show_on_top(LoudplayControlWidget *self)
{
    loudplay_control_widget_set_status(self, _("..."));
    loudplay_control_widget_create_gui_if_required(self);
    loudplay_control_widget_update_gui(self);

    // loudplay settings
    // Показать окно поверх всех
    gtk_widget_set_size_request(self->main_window, 400, 450);
    gtk_widget_show_all(self->main_window);
    gtk_window_present(GTK_WINDOW(self->main_window));

    // loudplay settings invoker
    gtk_window_set_transient_for(GTK_WINDOW(self->settings_invoke_window),
                                 GTK_WINDOW(self->main_window));
    const gint width = 15;
    const gint height = 40;
    gtk_widget_set_size_request(self->settings_invoke_window, width, height);
    gtk_window_set_keep_above(GTK_WINDOW(self->settings_invoke_window), TRUE);

    // Show invoker on the same screen as the main settings window
    GdkDisplay *display = gtk_widget_get_display(self->main_window);
    GdkWindow *gdk_window = gtk_widget_get_window(self->main_window);

    GdkRectangle geometry;
#if GDK_VERSION_CUR_STABLE < G_ENCODE_VERSION(3, 22)
    const int monitor_num = gdk_screen_get_monitor_at_window(screen, gdk_window);
    gdk_screen_get_monitor_geometry(screen, monitor_num, &geometry);
#else
    GdkMonitor *monitor = gdk_display_get_monitor_at_window(display, gdk_window);
    gdk_monitor_get_geometry(monitor, &geometry);
#endif

    gtk_window_move(GTK_WINDOW(self->settings_invoke_window),
            geometry.x + geometry.width - width,
            geometry.y + geometry.height * 0.5 - height * 0.5);
    gtk_widget_show_all(self->settings_invoke_window);
}

void loudplay_control_widget_hide(LoudplayControlWidget *self)
{
    if (self->settings_invoke_window)
        gtk_widget_hide(self->settings_invoke_window);
    if (self->main_window)
        gtk_widget_hide(self->main_window);
}

void loudplay_control_widget_update_gui(LoudplayControlWidget *self)
{
    if (self->loudplay_config_gui)
        gtk_widget_destroy(self->loudplay_config_gui);
    self->loudplay_config_gui = gobject_gui_generate_gui(G_OBJECT(self->p_conn_data->loudplay_config));
    gtk_container_add(GTK_CONTAINER(self->loudplay_viewport), self->loudplay_config_gui);
    gtk_widget_show_all(self->loudplay_config_gui);
}

void loudplay_control_widget_set_status(LoudplayControlWidget *self, const gchar *text)
{
    if (self->status_label)
        gtk_label_set_text(GTK_LABEL(self->status_label), text);
}