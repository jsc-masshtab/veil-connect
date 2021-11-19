/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib/garray.h>
#include <glib/gi18n.h>

#include <cairo/cairo.h>

#include <freerdp/locale/keyboard.h>
#include <freerdp/scancode.h>

#include "virt-viewer-auth.h"

#include "rdp_util.h"
#include "rdp_viewer.h"
#include "rdp_client.h"
#include "rdp_viewer_window.h"
#include "remote-viewer-util.h"
#include "config.h"

#include "settingsfile.h"
#include "async.h"
#include "usbredir_controller.h"
#include "vdi_session.h"
#include "net_speedometer.h"

#define MAX_MONITOR_AMOUNT 3
#define MAC_PANEL_HEIGHT 90 // Высота панели на маке

typedef struct {
    ExtendedRdpContext *ex_rdp_context;
    NetSpeedometer *net_speedometer;

}RemoteViewerData;

static gboolean rdp_viewer_update_cursor(rdpContext* context)
{
    ExtendedRdpContext* ex_rdp_context = (ExtendedRdpContext*)context;
    if (!ex_rdp_context || !ex_rdp_context->is_running)
        return TRUE;

    g_mutex_lock(&ex_rdp_context->cursor_mutex);

    for (guint i = 0; i < ex_rdp_context->rdp_windows_array->len; ++i) {
        RdpWindowData *rdp_window_data = g_array_index(ex_rdp_context->rdp_windows_array, RdpWindowData *, i);
        GdkWindow *parent_window = gtk_widget_get_parent_window(GTK_WIDGET(rdp_window_data->rdp_display));
        gdk_window_set_cursor(parent_window,  ex_rdp_context->gdk_cursor);
    }

    //g_info("%s ex_pointer->test_int: ", (const char *)__func__);
    g_mutex_unlock(&ex_rdp_context->cursor_mutex);

    ex_rdp_context->cursor_update_timeout_id = 0;
    return G_SOURCE_REMOVE;
}

static gboolean rdp_viewer_update_images(gpointer user_data)
{
    ExtendedRdpContext* ex_rdp_context = (ExtendedRdpContext *)user_data;

    // invalidate regions
    g_mutex_lock(&ex_rdp_context->invalid_region_mutex);

    if (ex_rdp_context->invalid_region_has_data) {
        //g_info("UDATE: %i %i %i %i  ", ex_rdp_context->invalid_region.x, ex_rdp_context->invalid_region.y,
        //       ex_rdp_context->invalid_region.width, ex_rdp_context->invalid_region.height);
        for (guint i = 0; i < ex_rdp_context->rdp_windows_array->len; ++i) {
            RdpWindowData *rdp_window_data = g_array_index(ex_rdp_context->rdp_windows_array, RdpWindowData *, i);
            RdpDisplay *rdp_display = rdp_window_data->rdp_display;
            if (rdp_display && gtk_widget_is_drawable(GTK_WIDGET(rdp_display))) {
                gtk_widget_queue_draw_area(GTK_WIDGET(rdp_display),
                                           ex_rdp_context->invalid_region.x - rdp_display->geometry.x,
                                           ex_rdp_context->invalid_region.y - rdp_display->geometry.y,
                                           ex_rdp_context->invalid_region.width,
                                           ex_rdp_context->invalid_region.height);
            }
        }

        ex_rdp_context->invalid_region_has_data = FALSE;
    }
    g_mutex_unlock(&ex_rdp_context->invalid_region_mutex);

    return G_SOURCE_CONTINUE;
}

// set_monitor_data_and_create_rdp_viewer_window. Returns monitor geometry
static GdkRectangle set_monitor_data_and_create_rdp_viewer_window(GdkMonitor *monitor, int index,
        ExtendedRdpContext *ex_rdp_context, GMainLoop **loop_p)
{
    gboolean is_mon_primary = gdk_monitor_is_primary(monitor);

    // get monitor geometry
    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);

    // set monitor data for rdp client
    rdpSettings* settings = ex_rdp_context->context.settings;

    settings->MonitorDefArray[index].x = geometry.x;
    settings->MonitorDefArray[index].y = geometry.y;
    settings->MonitorDefArray[index].width = geometry.width;
    settings->MonitorDefArray[index].height = geometry.height;
    settings->MonitorDefArray[index].is_primary = is_mon_primary;

    // create rdp viewer window
    RdpWindowData *rdp_window_data = rdp_viewer_window_create(ex_rdp_context, index, geometry);
    g_array_append_val(ex_rdp_context->rdp_windows_array, rdp_window_data);

    // set references
    rdp_window_data->loop_p = loop_p;

    return geometry;
}

static gboolean rdp_viewer_ask_for_credentials_if_required(VeilRdpSettings *p_rdp_settings)
{
    if (p_rdp_settings->user_name == NULL || p_rdp_settings->password == NULL)
        return virt_viewer_auth_collect_credentials(NULL,
                                               "RDP", p_rdp_settings->ip,
                                               &p_rdp_settings->user_name, &p_rdp_settings->password);
    else
        return TRUE;
}

static void rdp_viewer_stats_data_updated(gpointer data G_GNUC_UNUSED, VdiVmRemoteProtocol protocol,
                                               NetworkStatsData *nw_data, ExtendedRdpContext *ex_rdp_context)
{
    GArray *rdp_windows_array = ex_rdp_context->rdp_windows_array;
    for (guint i = 0; i < rdp_windows_array->len; ++i) {
        RdpWindowData *rdp_window_data = g_array_index(rdp_windows_array, RdpWindowData *, i);
        conn_info_dialog_update(rdp_window_data->conn_info_dialog, protocol, nw_data);
    }
}

static void rdp_viewer_show_error_msg_if_required(RemoteViewerData *self)
{
    UINT32 last_error = self->ex_rdp_context->last_rdp_error;
    gboolean is_stop_intentional = is_disconnect_intentional(last_error);

    if (!is_stop_intentional) {
        RdpWindowData *rdp_window_data = g_array_index(self->ex_rdp_context->rdp_windows_array,
                                                       RdpWindowData *, 0);

        if (last_error != 0 || self->ex_rdp_context->rail_rdp_error != 0) {
            gchar *msg = rdp_util_get_full_error_msg(
                    self->ex_rdp_context->last_rdp_error, self->ex_rdp_context->rail_rdp_error);
            show_msg_box_dialog(GTK_WINDOW(rdp_window_data->rdp_viewer_window), msg);
            g_free(msg);
        } else if (!self->ex_rdp_context->is_connected_last_time) {
            // Не удалось установить соединение. Проверьте разрешен ли удаленный доступ для текущего пользователя
            show_msg_box_dialog(GTK_WINDOW(rdp_window_data->rdp_viewer_window),
                    _("Unable to connect. Check if remote access allowed for this user."));
        }
    }
}

RemoteViewerState rdp_viewer_start(RemoteViewer *app, VeilRdpSettings *p_rdp_settings)
{
    RemoteViewerData self;
    RemoteViewerState next_app_state = APP_STATE_UNDEFINED;
    if (p_rdp_settings == NULL)
        return next_app_state;

    g_info("%s domain %s ip %s", (const char *)__func__, p_rdp_settings->domain, p_rdp_settings->ip);

    // Если имя и пароль по какой-либо причине отсутствуют, то предлагаем пользователю их ввести.
    gboolean ret = rdp_viewer_ask_for_credentials_if_required(p_rdp_settings);
    if (!ret)
        return APP_STATE_AUTH_DIALOG;

    GMainLoop *loop = NULL;
    // create RDP context
    self.ex_rdp_context = create_rdp_context(p_rdp_settings,
            (UpdateCursorCallback)rdp_viewer_update_cursor,
            (GSourceFunc)rdp_viewer_update_images);
    self.ex_rdp_context->p_loop = &loop;
    self.ex_rdp_context->next_app_state_p = &next_app_state;
    self.ex_rdp_context->app = app;

    self.net_speedometer = net_speedometer_new();
    net_speedometer_update_vm_ip(self.net_speedometer, p_rdp_settings->ip);
    // set pointer for statistics accumulation
    net_speedometer_set_pointer_rdp_context(self.net_speedometer, self.ex_rdp_context->context.rdp);
    g_signal_connect(self.net_speedometer, "stats-data-updated",
                     G_CALLBACK(rdp_viewer_stats_data_updated), self.ex_rdp_context);

    // Set some presettings
    usbredir_controller_reset_tcp_usb_devices_on_next_gui_opening(TRUE);

    // determine monitor info
    GdkDisplay *display = gdk_display_get_default();
    int total_monitor_width = 0; // Во freerdp нет мультимониторности. Единственный способ ее эмулиовать -
    // это представить, что мониторы образуют прямоугольник и запросить картинку, шириной равной суммарной ширине
    // мониторов.
    int monitor_height = 0;

    // array which will contain rdp windows
    GArray *rdp_windows_array = g_array_new(FALSE, FALSE, sizeof(RdpWindowData *));
    self.ex_rdp_context->rdp_windows_array = rdp_windows_array;

    // create rdp viewer windows
    rdpSettings *settings = self.ex_rdp_context->context.settings;
    // Create windows for every monitor
    if (p_rdp_settings->is_multimon) {
        int monitor_number = MIN(gdk_display_get_n_monitors(display), MAX_MONITOR_AMOUNT);

        // set monitor data for rdp client
        settings->MonitorCount = monitor_number;
        settings->UseMultimon = TRUE;
        settings->ForceMultimon = TRUE;

        for (int i = 0; i < monitor_number; ++i) {
            // get monitor data
            GdkMonitor *monitor = gdk_display_get_monitor(display, i);
            GdkRectangle geometry = set_monitor_data_and_create_rdp_viewer_window(monitor, i, self.ex_rdp_context,
                                                        &loop);
            total_monitor_width += geometry.width;
            // find the smallest height
            monitor_height = (i == 0) ? geometry.height : MIN(monitor_height, geometry.height);
        }
    // Create windows only for primary monitor
    } else {
        settings->MonitorCount = 1;
        settings->UseMultimon = FALSE;
        settings->ForceMultimon = FALSE;

        GdkMonitor *primary_monitor = gdk_display_get_primary_monitor(display);
        GdkRectangle geometry = set_monitor_data_and_create_rdp_viewer_window(primary_monitor, 0, self.ex_rdp_context,
                                                          &loop);
        total_monitor_width = geometry.width;
        monitor_height = geometry.height;
        RdpWindowData *rdp_window_data = g_array_index(rdp_windows_array, RdpWindowData *, 0);

        // Это необходимо, чтобы не было отступа при рисовании картинки или получени позиции мыши
        rdp_window_data->rdp_display->geometry.x = rdp_window_data->rdp_display->geometry.y = 0;
    }
#ifdef __APPLE__
    monitor_height = monitor_height - MAC_PANEL_HEIGHT;
#endif
    // Set image size which we will receive from Server
    const int max_image_width = 5120;//2560; 5120
    const int max_image_height = 2500; // 1440
    int image_width = MIN(max_image_width, total_monitor_width);
    int image_height = MIN(max_image_height, monitor_height);
    rdp_client_set_rdp_image_size(self.ex_rdp_context, image_width, image_height);

    // Notify if folders redir is forbidden. Проброс папок запрещен администратором
    gchar *shared_folders_str = read_str_from_ini_file("RDPSettings", "rdp_shared_folders");
    if (strlen_safely(shared_folders_str) && !vdi_session_is_folders_redir_permitted()) {
        show_msg_box_dialog(NULL, _("Folders redirection is not allowed"));
    }
    free_memory_safely(&shared_folders_str);

    // start RDP routine in thread
    rdp_client_start_routine_thread(self.ex_rdp_context);

    // launch event loop
    create_loop_and_launch(&loop);

    // stop usb tasks if there are any
    usbredir_controller_stop_all_cur_tasks(FALSE);

    /// Показать сообщение если завершение работы не было совершено пользователем намерено (произошла ошибка)
    rdp_viewer_show_error_msg_if_required(&self);

    // deinit all
    g_object_unref(self.net_speedometer);
    destroy_rdp_context(self.ex_rdp_context);
    // destroy rdp windows
    for (guint i = 0; i < rdp_windows_array->len; ++i) {
        RdpWindowData *rdp_window_data = g_array_index(rdp_windows_array, RdpWindowData *, i);
        rdp_viewer_window_destroy(rdp_window_data);
    }
    g_array_free(rdp_windows_array, TRUE);

    if (next_app_state == APP_STATE_UNDEFINED) {
        if (app->conn_data.opt_manual_mode)
            next_app_state = APP_STATE_AUTH_DIALOG;
        else
            next_app_state = APP_STATE_VDI_DIALOG;
    }
    return next_app_state;
}
