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
#include <freerdp/client/cmdline.h>

#include "virt-viewer-auth.h"

#include "rdp_keyboard.h"
#include "rdp_util.h"
#include "rdp_viewer.h"
#include "rdp_viewer_window.h"
#include "remote-viewer-util.h"
#include "config.h"

#include "vdi_event.h"
#include "settingsfile.h"
#include "async.h"
#include "usbredir_controller.h"
#include "vdi_session.h"
#include "net_speedometer.h"

#define MAX_MONITOR_AMOUNT 3
#define MAC_PANEL_HEIGHT 90 // Высота панели на маке
#define MAX_CONN_TRY_NUMBER 2 // Число попыток подключиться перед завершением сессии


G_DEFINE_TYPE( RdpViewer, rdp_viewer, G_TYPE_OBJECT )


static void rdp_viewer_finalize(GObject *object)
{
    GObjectClass *parent_class = G_OBJECT_CLASS( rdp_viewer_parent_class );
    ( *parent_class->finalize )( object );
}

static void rdp_viewer_class_init(RdpViewerClass *klass )
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = rdp_viewer_finalize;

    // signals
    g_signal_new("job-finished",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(RdpViewerClass, job_finished),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
    g_signal_new("quit-requested",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(RdpViewerClass, quit_requested),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
}

static void rdp_viewer_init(RdpViewer *self G_GNUC_UNUSED)
{
    g_info("%s", (const char *) __func__);
}

static gboolean rdp_viewer_update_cursor(rdpContext* context)
{
    ExtendedRdpContext* ex_context = (ExtendedRdpContext*)context;
    if (!ex_context || !ex_context->is_running)
        return TRUE;

    g_mutex_lock(&ex_context->cursor_mutex);

    for (guint i = 0; i < ex_context->rdp_windows_array->len; ++i) {
        RdpViewerWindow *rdp_window_data = g_array_index(ex_context->rdp_windows_array, RdpViewerWindow *, i);
        GdkWindow *parent_window = gtk_widget_get_parent_window(GTK_WIDGET(rdp_window_data->rdp_display));
        gdk_window_set_cursor(parent_window,  ex_context->gdk_cursor);
    }

    //g_info("%s ex_pointer->test_int: ", (const char *)__func__);
    g_mutex_unlock(&ex_context->cursor_mutex);

    ex_context->cursor_update_timeout_id = 0;
    return G_SOURCE_REMOVE;
}

static gboolean rdp_viewer_update_images(gpointer user_data)
{
    ExtendedRdpContext* ex_context = (ExtendedRdpContext *)user_data;

    // invalidate regions
    g_mutex_lock(&ex_context->invalid_region_mutex);

    if (ex_context->invalid_region_has_data) {
        //g_info("UDATE: %i %i %i %i  ", ex_context->invalid_region.x, ex_context->invalid_region.y,
        //       ex_context->invalid_region.width, ex_context->invalid_region.height);
        for (guint i = 0; i < ex_context->rdp_windows_array->len; ++i) {
            RdpViewerWindow *rdp_window_data = g_array_index(ex_context->rdp_windows_array, RdpViewerWindow *, i);
            RdpDisplay *rdp_display = rdp_window_data->rdp_display;
            if (rdp_display && gtk_widget_is_drawable(GTK_WIDGET(rdp_display))) {
                gtk_widget_queue_draw_area(GTK_WIDGET(rdp_display),
                                           ex_context->invalid_region.x - rdp_display->geometry.x,
                                           ex_context->invalid_region.y - rdp_display->geometry.y,
                                           ex_context->invalid_region.width,
                                           ex_context->invalid_region.height);
            }
        }

        ex_context->invalid_region_has_data = FALSE;
    }
    g_mutex_unlock(&ex_context->invalid_region_mutex);

    return G_SOURCE_CONTINUE;
}

static void
rdp_viewer_on_stop_requested(gpointer data G_GNUC_UNUSED,
                             const gchar *signal_upon_job_finish,
                             gboolean exit_if_cant_abort,
                             RdpViewer *self)
{
    rdp_viewer_stop(self, signal_upon_job_finish, exit_if_cant_abort);
}

// Returns monitor geometry
static RdpViewerWindow * set_monitor_data_and_create_rdp_viewer_window(RdpViewer *self,
        GdkRectangle *geometry, int index, int monitor_num,
        ExtendedRdpContext *ex_context)
{
    GdkDisplay *display = gdk_display_get_default();
    // get monitor geometry
    util_get_monitor_geometry(display, monitor_num, geometry);

    // set monitor data for rdp client
    rdpSettings* settings = ex_context->context.settings;

    settings->MonitorDefArray[index].x = geometry->x;
    settings->MonitorDefArray[index].y = geometry->y;
    settings->MonitorDefArray[index].width = geometry->width;
    settings->MonitorDefArray[index].height = geometry->height;
    settings->MonitorDefArray[index].is_primary = util_check_if_monitor_primary(display, monitor_num);

    // create rdp viewer window
    RdpViewerWindow *rdp_window = rdp_viewer_window_create(ex_context, index, monitor_num, *geometry);
    g_signal_connect(rdp_window, "stop-requested",
                     G_CALLBACK(rdp_viewer_on_stop_requested), self);
    g_array_append_val(ex_context->rdp_windows_array, rdp_window);

    return rdp_window;
}

static gboolean rdp_viewer_ask_for_credentials_if_required(
        ConnectSettingsData *conn_data, VeilRdpSettings *p_rdp_settings)
{

    if (!conn_data->pass_through_auth &&
    (strlen_safely(p_rdp_settings->user_name) == 0 ||
            strlen_safely(p_rdp_settings->password) == 0)) {
        g_autofree gchar *address = NULL;
        address = g_strdup_printf("%s  %s", p_rdp_settings->ip, p_rdp_settings->domain);
        return virt_viewer_auth_collect_credentials(NULL,
                                                    "RDP", address,
                                                    &p_rdp_settings->user_name, &p_rdp_settings->password);
    } else {
        return TRUE;
    }
}

static void rdp_viewer_stats_data_updated(gpointer data G_GNUC_UNUSED, VmRemoteProtocol protocol,
                                          NetworkStatsData *nw_data, ExtendedRdpContext *ex_context)
{
    GArray *rdp_windows_array = ex_context->rdp_windows_array;
    for (guint i = 0; i < rdp_windows_array->len; ++i) {
        RdpViewerWindow *rdp_window_data = g_array_index(rdp_windows_array, RdpViewerWindow *, i);
        conn_info_dialog_update(rdp_window_data->conn_info_dialog, protocol, nw_data);
    }
}

static void rdp_viewer_show_error_msg_if_required(RdpViewer *self)
{
    UINT32 last_error = self->ex_context->last_rdp_error;
    gboolean is_stop_intentional = is_disconnect_intentional(last_error);

    if (!is_stop_intentional) {
        RdpViewerWindow *rdp_window_data = g_array_index(self->ex_context->rdp_windows_array,
                                                       RdpViewerWindow *, 0);

        if (last_error != 0 || self->ex_context->rail_rdp_error != 0) {
            gchar *msg = rdp_util_get_full_error_msg(
                    self->ex_context->last_rdp_error, self->ex_context->rail_rdp_error);
            show_msg_box_dialog(GTK_WINDOW(rdp_window_data->rdp_viewer_window), msg);
            g_free(msg);
        } else if (!self->ex_context->is_connected_last_time) {
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

RdpViewer *rdp_viewer_new()
{
    g_info("%s", (const char *)__func__);
    RdpViewer *obj = RDP_VIEWER( g_object_new( TYPE_RDP_VIEWER, NULL ) );
    return obj;
}

// Функция вызывается в главном потоке при завершении рабочего потока
static gboolean rdp_viewer_work_thread_finished(RdpViewer *self)
{
    g_thread_join(self->job_thread);

    // stop usb tasks if there are any
    usbredir_controller_stop_all();
    /// Показать сообщение если завершение работы не было совершено пользователем намерено (произошла ошибка)
    rdp_viewer_show_error_msg_if_required(self);

    // destroy rdp windows
    GArray *rdp_windows_array = self->ex_context->rdp_windows_array;
    for (guint i = 0; i < rdp_windows_array->len; ++i) {
        RdpViewerWindow *rdp_window_data = g_array_index(rdp_windows_array, RdpViewerWindow *, i);
        rdp_viewer_window_destroy(rdp_window_data);
    }
    g_array_free(rdp_windows_array, TRUE);

    // Send signal
    if (self->ex_context->signal_upon_job_finish) {
        g_signal_emit_by_name(self, self->ex_context->signal_upon_job_finish);
    }

    // deinit all
    g_object_unref(self->net_speedometer);
    destroy_rdp_context(self->ex_context);
    self->ex_context = NULL;

    return FALSE;
}

//===============================Thread client routine==================================
static void* rdp_viewer_job_routine(RdpViewer *self)
{
    ExtendedRdpContext *ex_contect = self->ex_context;

    if (ex_contect->is_abort_demanded)
        return NULL;

    ex_contect->is_running = TRUE;

    int status;
    rdpContext *context = (rdpContext *)ex_contect;

    if (!context)
        goto end;

    // rdp params
    GArray *rdp_params_dyn_array = rdp_client_create_params_array(ex_contect);

    g_info("%s ex_contect->user_name %s", (const char *)__func__, ex_contect->p_rdp_settings->user_name);
    g_info("%s ex_contect->domain %s", (const char *)__func__, ex_contect->p_rdp_settings->domain);
    // + 1 к размеру для завершающего NULL
    gchar** argv = malloc((rdp_params_dyn_array->len + 1) * sizeof(gchar*));
    for (guint i = 0; i < rdp_params_dyn_array->len; ++i) {
        argv[i] = g_array_index(rdp_params_dyn_array, gchar*, i);
        if(strstr(argv[i], "/p:") == NULL) // Do not print the password
            g_info("%i RDP arg: %s", i, argv[i]);
    }
    argv[rdp_params_dyn_array->len] = NULL; // Завершающий NULL

    int argc = (int)rdp_params_dyn_array->len;

    // set rdp params
    status = freerdp_client_settings_parse_command_line(context->settings, argc, argv, TRUE);
    g_info("!!!status2: %i ", status);
    if (status)
        ex_contect->last_rdp_error = WRONG_FREERDP_ARGUMENTS;

    status = freerdp_client_settings_command_line_status_print(context->settings, status, argc, argv);

    // clear memory
    free(argv);
    rdp_client_destroy_params_array(rdp_params_dyn_array);
    if (status)
        goto end;

    // MAIN RDP LOOP
    freerdp* instance = ex_contect->context.instance;
    HANDLE handles[64];
    UINT32 conn_try = 0;

    while (TRUE) {
        if (ex_contect->is_abort_demanded)
            break;

        g_info("RDP. Connect attempt number: %i", conn_try + 1);
        ex_contect->is_connecting = TRUE;
        ex_contect->is_connected_last_time = freerdp_connect(instance);
        ex_contect->is_connecting = FALSE;
        if (!ex_contect->is_connected_last_time) {
            g_info("connection failure");
            // следующая попытка
            conn_try++;
            if (conn_try < MAX_CONN_TRY_NUMBER) {
                g_usleep(500000);
                continue; // to the next attempt
            } else { // unable to connect
                UINT32 error_code = freerdp_get_last_error(instance->context);
                g_autofree gchar *err_str = NULL;
                err_str = rdp_util_get_full_error_msg(error_code, 0);
                vdi_event_conn_error_notify(error_code, err_str);
                break;
            }
        }
        // Подключение удачно
        conn_try = 0; // Сброс счетчика
        ex_contect->last_rdp_error = 0; // Сброс инфы о последней ошибке
        vdi_event_vm_changed_notify(vdi_session_get_current_vm_id(), VDI_EVENT_TYPE_VM_CONNECTED); // Событие

        g_info("RDP successfully connected");
        while (!freerdp_shall_disconnect(instance)) {

            if (ex_contect->is_abort_demanded)
                break;

            if (freerdp_focus_required(instance)) {
                g_info(" if (freerdp_focus_required(instance))");
                rdp_keyboard_focus_in(ex_contect);
            }

            DWORD nCount = freerdp_get_event_handles(instance->context, &handles[0], 64);

            if (nCount == 0) {
                g_warning("%s: freerdp_get_event_handles failed", __FUNCTION__);
                break;
            }

            DWORD wait_status = WaitForMultipleObjects(nCount, handles, FALSE, 100);
            if (wait_status == WAIT_FAILED) {
                g_warning("%s: WaitForMultipleObjects failed with %lu" PRIu32 "", __FUNCTION__,
                        (long unsigned int)wait_status);
                break;
            }

            // выполнение следующего условия говорит о потере связи
            if (!freerdp_check_event_handles(instance->context)) {
                // выполнение следующего условия говорит о намеренном завершении сессии
                UINT32 error_info = freerdp_error_info(instance);
                if (error_info != 0)
                    ex_contect->is_abort_demanded = TRUE;
                g_info("freerdp_check_event_handles. Br. is_abort_demanded: %i  error_info: %i",
                       ex_contect->is_abort_demanded, error_info);
                break;
            }
        }

        g_info("Before freerdp_disconnect");
        ex_contect->is_disconnecting = TRUE;
        BOOL diss_res = freerdp_disconnect(instance);
        ex_contect->is_disconnecting = FALSE;
        g_info("After freerdp_disconnect res: %i", diss_res);
    }
    end:
    g_info("%s: g_mutex_unlock", (const char *)__func__);
    ex_contect->is_running = FALSE;
    rdp_client_demand_image_update(ex_contect, 0, 0, ex_contect->whole_image_width, ex_contect->whole_image_height);

    // Если соединение было успешным то сигнализируем об отключении от ВМ
    if (ex_contect->is_connected_last_time)
        vdi_event_vm_changed_notify(vdi_session_get_current_vm_id(), VDI_EVENT_TYPE_VM_DISCONNECTED);

    gdk_threads_add_idle((GSourceFunc)rdp_viewer_work_thread_finished, self);
    return NULL;
}

void rdp_viewer_start_routine_thread(RdpViewer *self)
{
    self->ex_context->is_abort_demanded = FALSE; // reset abort demand before start
    self->job_thread = g_thread_new(NULL, (GThreadFunc)rdp_viewer_job_routine, self);
}

void rdp_viewer_start(RdpViewer *self, ConnectSettingsData *conn_data, VeilMessenger *veil_messenger,
                      VeilRdpSettings *p_rdp_settings)
{
    if (p_rdp_settings == NULL) {
        g_signal_emit_by_name(self, "job-finished");
        return;
    }

    g_info("%s domain %s ip %s", (const char *)__func__, p_rdp_settings->domain, p_rdp_settings->ip);

    // Если имя и пароль по какой-либо причине отсутствуют, то предлагаем пользователю их ввести.
    gboolean ret = rdp_viewer_ask_for_credentials_if_required(conn_data, p_rdp_settings);
    if (!ret) {
        g_signal_emit_by_name(self, "job-finished");
        return;
    }

    // create RDP context
    self->ex_context = create_rdp_context(p_rdp_settings,
                                             (UpdateCursorCallback)rdp_viewer_update_cursor,
                                             (GSourceFunc)rdp_viewer_update_images);
    self->ex_context->p_conn_data = conn_data;
    self->ex_context->p_veil_messenger = veil_messenger;

    self->net_speedometer = net_speedometer_new();
    net_speedometer_update_vm_ip(self->net_speedometer, p_rdp_settings->ip);
    // set pointer for statistics accumulation
    net_speedometer_set_pointer_rdp_context(self->net_speedometer, self->ex_context->context.rdp);
    g_signal_connect(self->net_speedometer, "stats-data-updated",
                     G_CALLBACK(rdp_viewer_stats_data_updated), self->ex_context);

    // Set some presettings
    usbredir_controller_reset_tcp_usb_devices_on_next_gui_opening(TRUE);

    // determine monitor info
    GdkDisplay *display = gdk_display_get_default();
    const gchar *monitor_config_str = self->ex_context->p_rdp_settings->selectedmonitors;

    // Получить массив int номеров
    gchar **monitor_configs = monitor_config_str ? g_strsplit(monitor_config_str, ",", MAX_MONITOR_AMOUNT) : NULL;
    guint monitor_configs_len = monitor_configs != NULL ? g_strv_length(monitor_configs) : 0;
    GArray *monitor_num_array = g_array_new(FALSE, FALSE, sizeof(int));

    // array which will contain rdp windows
    self->ex_context->rdp_windows_array = g_array_new(FALSE, FALSE, sizeof(RdpViewerWindow *));

    // create rdp viewer windows
    int total_monitor_width = 0; // Во freerdp нет мультимониторности. Единственный способ ее эмулиовать -
    // это представить, что мониторы образуют прямоугольник и запросить картинку, шириной равной суммарной ширине
    // мониторов.
    int monitor_height = 0;

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

        // Сортировать по geometry.x (слева направо)
        g_array_sort(monitor_num_array, rdp_viewer_compare_monitors);

        // Запомнить geometry.x крайнего левого монитора из используемых
        int left_monitor_num = g_array_index(monitor_num_array, int, 0);
        GdkRectangle left_geometry;
        util_get_monitor_geometry(display, left_monitor_num, &left_geometry);
        int left_x = left_geometry.x;

        for (int i = 0; i < (int)monitor_num_array->len; ++i) {

            int monitor_num = g_array_index(monitor_num_array, int, i);
            // get monitor data
            GdkRectangle geometry;
            RdpViewerWindow *rdp_window_data = set_monitor_data_and_create_rdp_viewer_window(
                    self, &geometry, i, monitor_num, self->ex_context);
            total_monitor_width += geometry.width;

            // find the smallest height
            monitor_height = (i == 0) ? geometry.height : MIN(monitor_height, geometry.height);

            // Это необходимо, чтобы не было отступа при рисовании картинки или получени позиции мыши
            rdp_window_data->rdp_display->geometry.x -= left_x;
        }

    } else {
        int monitor_num = 0;
        // Берем либо первый номер из настроек (если он валидный), либо основной монитор(номер 0)
        int first_monitor_num_in_config = monitor_configs_len > 0 ? atoi(monitor_configs[0]) : 0;
        if (util_check_if_monitor_number_valid(display, first_monitor_num_in_config))
            monitor_num = first_monitor_num_in_config;

        g_array_append_val(monitor_num_array, monitor_num);

        // create rdp viewer window
        monitor_num = g_array_index(monitor_num_array, int, 0);
        GdkRectangle geometry;
        RdpViewerWindow *rdp_window_data = set_monitor_data_and_create_rdp_viewer_window(
                self, &geometry, 0, monitor_num, self->ex_context);
        total_monitor_width = geometry.width;
        monitor_height = geometry.height;
        rdp_window_data->rdp_display->geometry.x = rdp_window_data->rdp_display->geometry.y = 0;
    }
    g_strfreev(monitor_configs);

#ifdef __APPLE__
    monitor_height = monitor_height - MAC_PANEL_HEIGHT;
#endif
    // Set image size which we will receive from Server
    const int max_image_width = 5120;
    const int max_image_height = 2560;
    int image_width = MIN(max_image_width, total_monitor_width);
    int image_height = MIN(max_image_height, monitor_height);
    rdp_client_set_rdp_image_size(self->ex_context, image_width, image_height);

    // Notify if folders redir is forbidden. Проброс папок запрещен администратором
    gchar *shared_folders_str = read_str_from_ini_file("RDPSettings", "rdp_shared_folders");
    if (strlen_safely(shared_folders_str) && !vdi_session_is_folders_redir_permitted()) {
        show_msg_box_dialog(NULL, _("Folders redirection is not allowed"));
    }
    free_memory_safely(&shared_folders_str);

    // set monitor data for rdp client
    rdpSettings *settings = self->ex_context->context.settings;
    settings->MonitorCount = monitor_num_array->len;
    settings->UseMultimon = settings->ForceMultimon = p_rdp_settings->is_multimon;
    g_array_free(monitor_num_array, TRUE);

    // start RDP routine in thread
    rdp_viewer_start_routine_thread(self);
}

void rdp_viewer_stop(RdpViewer *rdp_viewer, const gchar *signal_upon_job_finish, gboolean exit_if_cant_abort)
{
    ExtendedRdpContext *ex_context = rdp_viewer->ex_context;
    if (ex_context == NULL)
        return;
    if (!ex_context->is_running)
        return;

    update_string_safely(&ex_context->signal_upon_job_finish, signal_upon_job_finish);

    // В режиме запуска удаленного приложения закрываем текущее окно послав alt f4, так как пользователь ожидает,
    // что приложение закроется
    rdpContext *context = (rdpContext *)ex_context;
    if (context->settings->RemoteApplicationMode) {
        rdp_viewer_window_send_key_shortcut(context, 14); // 14 - index in keyCombos
    }

    // Условие из-за проблемы невозможности отмены стадий коннекта и дисконнекта во freerdp
    // (функции freerdp_connect и freerdp_disconnect могут виснуть на неопределенное время)
    if (!ex_context->is_connecting && !ex_context->is_disconnecting) {
        rdp_client_abort_connection(ex_context->context.instance);
    } else if (exit_if_cant_abort) { // Завершаем приложения форсировано
        g_warning("%s: Forced exit", (const char *)__func__);
        g_signal_emit_by_name(rdp_viewer, "quit-requested");
    }
}