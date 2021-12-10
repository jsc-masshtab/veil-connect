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

// Returns monitor geometry
static RdpWindowData * set_monitor_data_and_create_rdp_viewer_window(
        GdkRectangle *geometry, int index, int monitor_num,
        ExtendedRdpContext *ex_rdp_context, GMainLoop **loop_p)
{
    GdkDisplay *display = gdk_display_get_default();
    // get monitor geometry
    util_get_monitor_geometry(display, monitor_num, geometry);

    // set monitor data for rdp client
    rdpSettings* settings = ex_rdp_context->context.settings;

    settings->MonitorDefArray[index].x = geometry->x;
    settings->MonitorDefArray[index].y = geometry->y;
    settings->MonitorDefArray[index].width = geometry->width;
    settings->MonitorDefArray[index].height = geometry->height;
    settings->MonitorDefArray[index].is_primary = util_check_if_monitor_primary(display, monitor_num);

    // create rdp viewer window
    RdpWindowData *rdp_window_data = rdp_viewer_window_create(ex_rdp_context, index, monitor_num, *geometry);
    g_array_append_val(ex_rdp_context->rdp_windows_array, rdp_window_data);

    // set references
    rdp_window_data->loop_p = loop_p;

    return rdp_window_data;
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

//  The function should return a negative integer if the first value comes before the second, 0 if they are equal,
//  or a positive integer if the first value comes after the second.
// Функция сравнения для сортировки мониторов от левого к правому
gint rdp_viewer_compare_monitors(gconstpointer a, gconstpointer b)
{
    GdkDisplay *display = gdk_display_get_default();

    GdkRectangle geometry_a;
    util_get_monitor_geometry(display, *(int *)a, &geometry_a);

    GdkRectangle geometry_b;
    util_get_monitor_geometry(display, *(int *)b, &geometry_b);

    return (geometry_a.x - geometry_b.x);
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
    const gchar *monitor_config_str = self.ex_rdp_context->p_rdp_settings->selectedmonitors;

    // Получить массив int номеров
    gchar **monitor_configs = monitor_config_str ? g_strsplit(monitor_config_str, ",", MAX_MONITOR_AMOUNT) : NULL;
    guint monitor_configs_len = monitor_configs != NULL ? g_strv_length(monitor_configs) : 0;
    GArray *monitor_num_array = g_array_new(FALSE, FALSE, sizeof(int));

    if (p_rdp_settings->is_multimon) {
        // Берем из настроек
        if (monitor_configs_len > 0) {
            for (guint i = 0; i < monitor_configs_len; i++) {
                int monitor_num = atoi(monitor_configs[i]);
                // Проверить что monitor_num валидный и добавить в массив
                gboolean is_mon_valid = util_check_if_monitor_number_valid(display, monitor_num);
                if (is_mon_valid)
                    g_array_append_val(monitor_num_array, monitor_num);
            }
        }
        // Если нет (корректных) данных в настройках то используем все мониторы (до MAX_MONITOR_AMOUNT)
        if (monitor_num_array->len == 0) {
            int monitor_amount = MIN(util_get_monitor_number(display), MAX_MONITOR_AMOUNT);
            for (int i = 0; i < monitor_amount; ++i) {
                g_array_append_val(monitor_num_array, i);
            }
        }

    } else {
        int monitor_num = 0;
        // Берем либо первый номер из настроек (если он валидный), либо основной монитор(номер 0)
        int first_monitor_num_in_config = monitor_configs_len > 0 ? atoi(monitor_configs[0]) : 0;
        if (util_check_if_monitor_number_valid(display, first_monitor_num_in_config))
            monitor_num = first_monitor_num_in_config;

        g_array_append_val(monitor_num_array, monitor_num);
    }
    g_strfreev(monitor_configs);

    // Сортировать по geometry.x (слева направо)
    g_array_sort(monitor_num_array, rdp_viewer_compare_monitors);

    // set monitor data for rdp client
    rdpSettings *settings = self.ex_rdp_context->context.settings;
    settings->MonitorCount = monitor_num_array->len;
    settings->UseMultimon = settings->ForceMultimon = p_rdp_settings->is_multimon;

    // Запомнить geometry.x крайнего левого монитора из используемых
    int left_monitor_num = g_array_index(monitor_num_array, int, 0);
    GdkRectangle left_geometry;
    util_get_monitor_geometry(display, left_monitor_num, &left_geometry);
    int left_x = left_geometry.x;

    // array which will contain rdp windows
    GArray *rdp_windows_array = g_array_new(FALSE, FALSE, sizeof(RdpWindowData *));
    self.ex_rdp_context->rdp_windows_array = rdp_windows_array;

    // create rdp viewer windows
    int total_monitor_width = 0; // Во freerdp нет мультимониторности. Единственный способ ее эмулиовать -
    // это представить, что мониторы образуют прямоугольник и запросить картинку, шириной равной суммарной ширине
    // мониторов.
    int monitor_height = 0;
    for (int i = 0; i < (int)monitor_num_array->len; ++i) {

        int monitor_num = g_array_index(monitor_num_array, int, i);
        // get monitor data
        GdkRectangle geometry;
        RdpWindowData *rdp_window_data = set_monitor_data_and_create_rdp_viewer_window(
                &geometry, i, monitor_num, self.ex_rdp_context, &loop);
        total_monitor_width += geometry.width;

        // find the smallest height
        monitor_height = (i == 0) ? geometry.height : MIN(monitor_height, geometry.height);

        // Это необходимо, чтобы не было отступа при рисовании картинки или получени позиции мыши
        rdp_window_data->rdp_display->geometry.x -= left_x;
    }

    g_array_free(monitor_num_array, TRUE);

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
    usbredir_controller_stop_all();
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
