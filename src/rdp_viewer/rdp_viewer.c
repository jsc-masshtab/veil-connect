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

#include <cairo/cairo.h>

#include <freerdp/locale/keyboard.h>
#include <freerdp/scancode.h>

#include "rdp_viewer.h"
#include "rdp_client.h"
#include "rdp_viewer_window.h"
#include "remote-viewer-util.h"
#include "config.h"

#include "settingsfile.h"
#include "async.h"
#include "usbredir_controller.h"
#include "vdi_session.h"

#define MAX_MONITOR_AMOUNT 3
#define MAC_PANEL_HEIGHT 90 // Высота панели на маке

static gboolean is_rdp_context_created = FALSE;

static gboolean update_cursor_callback(rdpContext* context)
{
    if (!is_rdp_context_created)
        return TRUE;
    ExtendedRdpContext* ex_rdp_context = (ExtendedRdpContext*)context;
    if (!ex_rdp_context || !ex_rdp_context->is_running)
        return TRUE;

    g_mutex_lock(&ex_rdp_context->cursor_mutex);

    for (guint i = 0; i < ex_rdp_context->rdp_windows_array->len; ++i) {
        RdpWindowData *rdp_window_data = g_array_index(ex_rdp_context->rdp_windows_array, RdpWindowData *, i);
        GdkWindow *parent_window = gtk_widget_get_parent_window(rdp_window_data->rdp_display);
        gdk_window_set_cursor(parent_window,  ex_rdp_context->gdk_cursor);
    }

    //g_info("%s ex_pointer->test_int: ", (const char *)__func__);
    g_mutex_unlock(&ex_rdp_context->cursor_mutex);

    return FALSE;
}

static ExtendedRdpContext* create_rdp_context()
{
    RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
    rdp_client_entry(&clientEntryPoints);
    rdpContext* context = freerdp_client_context_new(&clientEntryPoints);


    ExtendedRdpContext* ex_rdp_context = (ExtendedRdpContext*)context;
    ex_rdp_context->is_running = FALSE;
    //ex_rdp_context->update_image_callback = (UpdateImageCallback)update_image_callback;
    ex_rdp_context->update_cursor_callback = (UpdateCursorCallback)update_cursor_callback;
    ex_rdp_context->test_int = 777; // temp
    g_mutex_init(&ex_rdp_context->cursor_mutex);

    is_rdp_context_created = TRUE;

    return ex_rdp_context;
}

static void destroy_rdp_context(ExtendedRdpContext* ex_rdp_context)
{
    if (ex_rdp_context) {

        // stopping RDP routine
        rdp_client_stop_routine_thread(ex_rdp_context);

        wair_for_mutex_and_clear(&ex_rdp_context->cursor_mutex);

        g_info("%s: context free now: %i", (const char *)__func__, ex_rdp_context->test_int);
        freerdp_client_context_free((rdpContext*)ex_rdp_context);
        ex_rdp_context = NULL;
    }

    is_rdp_context_created = FALSE;
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

RemoteViewerState rdp_viewer_start(const gchar *usename, const gchar *password, gchar *domain, gchar *ip, int port)
{
    g_info("%s domain %s", (const char *)__func__, domain);

    RemoteViewerState next_app_state = APP_STATE_UNDEFINED;
    GMainLoop *loop = NULL;
    // create RDP context
    ExtendedRdpContext *ex_rdp_context = create_rdp_context();
    ex_rdp_context->p_loop = &loop;
    ex_rdp_context->next_app_state_p = &next_app_state;

    // Логин в формате name@domain не нравится freerdp, поэтому отбрасываем @domain
    gchar *corrected_usename = NULL;
    char *sub_string = strchr(usename, '@');
    if (sub_string) {
        int index = (int) (sub_string - usename);
        corrected_usename = g_strndup(usename, index);
    } else {
        corrected_usename = g_strdup(usename);
    }

    rdp_client_set_credentials(ex_rdp_context, corrected_usename, password, domain, ip, port);
    free_memory_safely(&corrected_usename);

    // Set some presettings
    usbredir_controller_reset_tcp_usb_devices_on_next_gui_opening(TRUE);
    // printers
    gboolean redirect_printers = read_int_from_ini_file("RDPSettings", "redirect_printers", FALSE);
    ex_rdp_context->context.instance->settings->RedirectPrinters = (BOOL)redirect_printers;
    // get ini config data
    gboolean is_multimon = read_int_from_ini_file("RDPSettings", "is_multimon", 0);
    // determine monitor info
    GdkDisplay *display = gdk_display_get_default();
    int total_monitor_width = 0; // Во freerdp нет мультимониторности. Единственный способ ее эмулиовать -
    // это представить, что мониторы образуют прямоугольник и запросить картинку, шириной равной суммарной ширине
    // мониторов.
    int monitor_height = 0;

    // array which will contain rdp windows
    GArray *rdp_windows_array = g_array_new(FALSE, FALSE, sizeof(RdpWindowData *));
    ex_rdp_context->rdp_windows_array = rdp_windows_array;

    // create rdp viewer windows
    rdpSettings *settings = ex_rdp_context->context.settings;
    // Create windows for every monitor
    if (is_multimon) {
        int monitor_number = MIN(gdk_display_get_n_monitors(display), MAX_MONITOR_AMOUNT);

        // set monitor data for rdp client
        settings->MonitorCount = monitor_number;
        settings->UseMultimon = TRUE;
        settings->ForceMultimon = TRUE;

        for (int i = 0; i < monitor_number; ++i) {
            // get monitor data
            GdkMonitor *monitor = gdk_display_get_monitor(display, i);
            GdkRectangle geometry = set_monitor_data_and_create_rdp_viewer_window(monitor, i, ex_rdp_context,
                                                        &loop);
            total_monitor_width += geometry.width;
            // find smallest height
            monitor_height = (i == 0) ? geometry.height : MIN(monitor_height, geometry.height);
        }
    // Create windows only for primary monitor
    } else {
        settings->MonitorCount = 1;
        settings->UseMultimon = FALSE;
        settings->ForceMultimon = FALSE;

        GdkMonitor *primary_monitor = gdk_display_get_primary_monitor(display);
        GdkRectangle geometry = set_monitor_data_and_create_rdp_viewer_window(primary_monitor, 0, ex_rdp_context,
                                                          &loop);
        total_monitor_width = geometry.width;
        monitor_height = geometry.height;
        RdpWindowData *rdp_window_data = g_array_index(rdp_windows_array, RdpWindowData *, 0);

        // Это необходимо, чтобы не было отступа при рисовании картинки или получени позиции мыши
        rdp_window_data->monitor_geometry.x = rdp_window_data->monitor_geometry.y = 0;
    }
#ifdef __APPLE__
    monitor_height = monitor_height - MAC_PANEL_HEIGHT;
#endif
    // Notify if folders redir is forbidden
    gchar *shared_folders_str = read_str_from_ini_file("RDPSettings", "rdp_shared_folders");
    if (strlen_safely(shared_folders_str) && !vdi_session_is_folders_redir_permitted()) {
        show_msg_box_dialog(NULL, "Проброс папок запрещен администратором");
    }
    free_memory_safely(&shared_folders_str);

    // Set image size which we will receive from Server
    const int max_image_width = 5120;//2560; 5120
    const int max_image_height = 2500; // 1440
    int image_width = MIN(max_image_width, total_monitor_width);
    int image_height = MIN(max_image_height, monitor_height);
    rdp_client_set_rdp_image_size(ex_rdp_context, image_width, image_height);

    // launch RDP routine in thread
    rdp_client_start_routine_thread(ex_rdp_context);

    // launch event loop
    create_loop_and_launch(&loop);

    usbredir_controller_stop_all_cur_tasks(FALSE);

    // deinit all
    destroy_rdp_context(ex_rdp_context);

    // destroy rdp windows
    guint i;
    for (i = 0; i < rdp_windows_array->len; ++i) {
        RdpWindowData *rdp_window_data = g_array_index(rdp_windows_array, RdpWindowData *, i);
        rdp_viewer_window_destroy(rdp_window_data);
    }
    g_array_free(rdp_windows_array, TRUE);

    if (next_app_state == APP_STATE_UNDEFINED)
        next_app_state = APP_STATE_EXITING;
    return next_app_state;
}
