#include <math.h>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib/garray.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <cairo/cairo.h>

#include <freerdp/locale/keyboard.h>
#include <freerdp/scancode.h>

#include "rdp_display.h"

static double scale_f = 1; // todo: make local

static gboolean fuzzy_compare(double number_1, double number_2)
{
    const double epsilon = 0.00001;
    return fabs(number_1 - number_2) < epsilon;
}

static const gchar *error_to_str(UINT32 rdp_error)
{
    //rdp_error = (rdp_error >= 0x10000 && rdp_error < 0x00020000) ? (rdp_error & 0xFFFF) : rdp_error;

    if (rdp_error >= 0x10000 && rdp_error < 0x00020000) {
        rdp_error = (rdp_error & 0xFFFF);

        switch (rdp_error) {
        case ERRINFO_RPC_INITIATED_DISCONNECT:
            return "ERRINFO_RPC_INITIATED_DISCONNECT";
        case ERRINFO_RPC_INITIATED_LOGOFF:
            return "ERRINFO_RPC_INITIATED_LOGOFF";
        case ERRINFO_IDLE_TIMEOUT:
            return "ERRINFO_IDLE_TIMEOUT";
        case ERRINFO_LOGON_TIMEOUT:
            return "ERRINFO_LOGON_TIMEOUT";
        case ERRINFO_DISCONNECTED_BY_OTHER_CONNECTION:
            return "ERRINFO_DISCONNECTED_BY_OTHER_CONNECTION";
        case ERRINFO_OUT_OF_MEMORY:
            return "ERRINFO_OUT_OF_MEMORY";
        case ERRINFO_SERVER_DENIED_CONNECTION:
            return "ERRINFO_SERVER_DENIED_CONNECTION";
        case ERRINFO_SERVER_INSUFFICIENT_PRIVILEGES:
            return "ERRINFO_SERVER_INSUFFICIENT_PRIVILEGES";
        case ERRINFO_SERVER_FRESH_CREDENTIALS_REQUIRED:
            return "ERRINFO_SERVER_FRESH_CREDENTIALS_REQUIRED";
        case ERRINFO_RPC_INITIATED_DISCONNECT_BY_USER:
            return "ERRINFO_RPC_INITIATED_DISCONNECT_BY_USER";
        case ERRINFO_LOGOFF_BY_USER:
            return "ERRINFO_LOGOFF_BY_USER";
//        case ERRINFO_CLOSE_STACK_ON_DRIVER_NOT_READY:
//            return "ERRINFO_CLOSE_STACK_ON_DRIVER_NOT_READY";
//        case ERRINFO_SERVER_DWM_CRASH:
//            return "ERRINFO_SERVER_DWM_CRASH";
//        case ERRINFO_CLOSE_STACK_ON_DRIVER_FAILURE:
//            return "ERRINFO_CLOSE_STACK_ON_DRIVER_FAILURE";
//        case ERRINFO_CLOSE_STACK_ON_DRIVER_IFACE_FAILURE:
//            return "ERRINFO_CLOSE_STACK_ON_DRIVER_IFACE_FAILURE";
//        case ERRINFO_SERVER_WINLOGON_CRASH:
//            return "ERRINFO_SERVER_WINLOGON_CRASH";
//        case ERRINFO_SERVER_CSRSS_CRASH:
//            return "ERRINFO_SERVER_CSRSS_CRASH";
        default:
            return "";
        }
    }
    else {
        rdp_error = (rdp_error & 0xFFFF);

        switch (rdp_error) {
        case ERRCONNECT_PRE_CONNECT_FAILED:
            return "ERRCONNECT_PRE_CONNECT_FAILED";
        case ERRCONNECT_CONNECT_UNDEFINED:
            return "ERRCONNECT_CONNECT_UNDEFINED";
        case ERRCONNECT_POST_CONNECT_FAILED:
            return "ERRCONNECT_POST_CONNECT_FAILED";
        case ERRCONNECT_DNS_ERROR:
            return "ERRCONNECT_DNS_ERROR";
        case ERRCONNECT_DNS_NAME_NOT_FOUND:
            return "ERRCONNECT_DNS_NAME_NOT_FOUND";
        case ERRCONNECT_CONNECT_FAILED:
            return "ERRCONNECT_CONNECT_FAILED";
        case ERRCONNECT_MCS_CONNECT_INITIAL_ERROR:
            return "ERRCONNECT_MCS_CONNECT_INITIAL_ERROR";
        case ERRCONNECT_TLS_CONNECT_FAILED:
            return "ERRCONNECT_TLS_CONNECT_FAILED";
        case ERRCONNECT_AUTHENTICATION_FAILED:
            return "ERRCONNECT_AUTHENTICATION_FAILED";
        case ERRCONNECT_INSUFFICIENT_PRIVILEGES:
            return "ERRCONNECT_INSUFFICIENT_PRIVILEGES";
        case ERRCONNECT_CONNECT_CANCELLED:
            return "ERRCONNECT_CONNECT_CANCELLED";
        case ERRCONNECT_SECURITY_NEGO_CONNECT_FAILED:
            return "ERRCONNECT_SECURITY_NEGO_CONNECT_FAILED";
        case ERRCONNECT_CONNECT_TRANSPORT_FAILED:
            return "ERRCONNECT_CONNECT_TRANSPORT_FAILED";
        case ERRCONNECT_PASSWORD_EXPIRED:
            return "ERRCONNECT_PASSWORD_EXPIRED";
        default:
            return "";
        }
    }
}

static void rdp_display_translate_mouse_pos(UINT16 *rdp_x_p, UINT16 *rdp_y_p,
                                            gdouble gtk_x, gdouble gtk_y, RdpViewerData *rdp_viewer_data) {
    ExtendedRdpContext *ex_rdp_context = rdp_viewer_data->ex_rdp_context;

    *rdp_x_p = (UINT16) ((gtk_x - ex_rdp_context->im_origin_x + rdp_viewer_data->monitor_geometry.x) * scale_f);
    *rdp_y_p = (UINT16) ((gtk_y - ex_rdp_context->im_origin_y + rdp_viewer_data->monitor_geometry.y) * scale_f);
}

static void rdp_viewer_handle_key_event(GdkEventKey *event, ExtendedRdpContext* tf, gboolean down)
{
    if (!tf || !tf->is_running)
        return;
    rdpInput *input = tf->context.input;

#ifdef __linux__
    DWORD rdp_scancode = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(event->hardware_keycode);
#elif _WIN32
    DWORD rdp_scancode = 0;
    switch (event->keyval) {
        // keys with special treatment
        case GDK_KEY_Control_L:
            rdp_scancode = RDP_SCANCODE_LCONTROL;
            break;
        case GDK_KEY_Control_R:
            rdp_scancode = RDP_SCANCODE_RCONTROL;
            break;
        case GDK_KEY_Shift_L:
            rdp_scancode = RDP_SCANCODE_LSHIFT;
            break;
        case GDK_KEY_Shift_R:
            rdp_scancode = RDP_SCANCODE_RSHIFT;
            break;
        case GDK_KEY_Alt_L:
            rdp_scancode = RDP_SCANCODE_LMENU;
            break;
        case GDK_KEY_Alt_R:
            rdp_scancode = RDP_SCANCODE_RMENU;
            break;
        case GDK_KEY_Left:
            rdp_scancode = RDP_SCANCODE_LEFT;
            break;
        case GDK_KEY_Up:
            rdp_scancode = RDP_SCANCODE_UP;
            break;
        case GDK_KEY_Right:
            rdp_scancode = RDP_SCANCODE_RIGHT;
            break;
        case GDK_KEY_Down:
            rdp_scancode = RDP_SCANCODE_DOWN;
            break;
        default:
            // other keys
            rdp_scancode = GetVirtualScanCodeFromVirtualKeyCode(event->hardware_keycode, 4);
            break;
    }
#endif
    BOOL is_success = freerdp_input_send_keyboard_event_ex(input, down, rdp_scancode);
    (void) is_success;
//    printf("%s: hardkey %i %i keyval: %i down: %i\n", (const char *) __func__,
//           event->hardware_keycode, rdp_scancode, event->keyval, down);
}

static gboolean rdp_display_key_pressed(GtkWidget *widget G_GNUC_UNUSED, GdkEventKey *event, gpointer user_data)
{
    ExtendedRdpContext* tf = (ExtendedRdpContext*)user_data;
    rdp_viewer_handle_key_event(event, tf, TRUE);

    return TRUE;
}

static gboolean rdp_display_key_released(GtkWidget *widget G_GNUC_UNUSED, GdkEventKey *event, gpointer user_data)
{
    ExtendedRdpContext* tf = (ExtendedRdpContext*)user_data;
    rdp_viewer_handle_key_event(event, tf, FALSE);

    return TRUE;
}

static gboolean rdp_display_mouse_moved(GtkWidget *widget G_GNUC_UNUSED, GdkEventMotion *event, gpointer user_data)
{
    RdpViewerData *rdp_viewer_data = (RdpViewerData *)user_data;
    ExtendedRdpContext* ex_contect = rdp_viewer_data->ex_rdp_context;
    if (!ex_contect || !ex_contect->is_running)
        return TRUE;

    rdpContext* rdp_contect = (rdpContext*)ex_contect;
    rdpInput *input = rdp_contect->input;

    UINT16 x, y;
    rdp_display_translate_mouse_pos(&x, &y, event->x, event->y, rdp_viewer_data);
    BOOL is_success = freerdp_input_send_mouse_event(input, PTR_FLAGS_MOVE, x, y);
    (void)is_success;
    //printf("%s: event->x %f, event->y %f  %i\n", (const char *)__func__, event->x, event->y, is_success);

    return TRUE;
}

static void rdp_viewer_handle_mouse_btn_event(GtkWidget *widget G_GNUC_UNUSED, GdkEventButton *event, gpointer user_data,
                                                  UINT16 additional_flags)
{
    RdpViewerData *rdp_viewer_data = (RdpViewerData *)user_data;
    ExtendedRdpContext* ex_rdp_contect = rdp_viewer_data->ex_rdp_context;
    if (!ex_rdp_contect || !ex_rdp_contect->is_running)
        return;

    rdpContext* context = (rdpContext*)ex_rdp_contect;
    rdpInput *input = context->input;

    UINT16 button = 0;
    switch (event->button)
    {
    case GDK_BUTTON_PRIMARY:{
        button = PTR_FLAGS_BUTTON1;
        break;
    }
    case GDK_BUTTON_SECONDARY:{
        button = PTR_FLAGS_BUTTON2;
        break;
    }
    case GDK_BUTTON_MIDDLE:{
        button = PTR_FLAGS_BUTTON3;
        break;
    }
    }

    if (button) {
        //event->state;
        UINT16 x, y;
        rdp_display_translate_mouse_pos(&x, &y, event->x, event->y, rdp_viewer_data);
        freerdp_input_send_mouse_event(input, additional_flags | button, x, y);
//        printf("%s: event->x %f, event->y %f  %i %i\n", (const char *)__func__,
//               event->x, event->y, event->button, event->state);
    }
}
// PTR_FLAGS_DOWN
static gboolean rdp_display_mouse_btn_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    if (event->type == GDK_BUTTON_PRESS)
        rdp_viewer_handle_mouse_btn_event(widget, event, user_data, PTR_FLAGS_DOWN);

    return TRUE;
}

static gboolean rdp_display_mouse_btn_released(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    if (event->type == GDK_BUTTON_RELEASE)
        rdp_viewer_handle_mouse_btn_event(widget, event, user_data, 0);
    return TRUE;
}

static gboolean rdp_display_wheel_scrolled(GtkWidget *widget G_GNUC_UNUSED, GdkEventScroll *event, gpointer user_data)
{
    RdpViewerData *rdp_viewer_data = (RdpViewerData *)user_data;
    ExtendedRdpContext* ex_rdp_context = rdp_viewer_data->ex_rdp_context;
    if (!ex_rdp_context || !ex_rdp_context->is_running)
        return TRUE;

    rdpContext* context = (rdpContext*)ex_rdp_context;
    rdpInput *input = context->input;
    //printf("%s event->delta_y %f event->delta_x %f\n", (const char *)__func__, event->delta_y, event->delta_x);

    UINT16 x, y;
    rdp_display_translate_mouse_pos(&x, &y, event->x, event->y, rdp_viewer_data);
    if ( event->delta_y > 0.5)
        freerdp_input_send_mouse_event(input, PTR_FLAGS_WHEEL | PTR_FLAGS_WHEEL_NEGATIVE | 0x0078, x, y);
    else if (event->delta_y < -0.5)
        freerdp_input_send_mouse_event(input, PTR_FLAGS_WHEEL | 0x0078, x, y);

    return TRUE;
}

static gboolean rdp_display_event_on_draw(GtkWidget* widget, cairo_t* context, gpointer user_data)
{
    //printf("%s START\n", (const char *)__func__);

    RdpViewerData *rdp_viewer_data = (RdpViewerData *)user_data;
    rdp_viewer_data->is_rdp_display_being_redrawed = TRUE;

    ExtendedRdpContext *ex_rdp_contect = rdp_viewer_data->ex_rdp_context;
    //GtkWidget *rdp_viewer_window = rdp_viewer_data->rdp_viewer_window;

    if (ex_rdp_contect && ex_rdp_contect->is_running) {

        g_mutex_lock(&ex_rdp_contect->primary_buffer_mutex);

        if (ex_rdp_contect->surface) {

            //cairo_set_source_surface(context, ex_rdp_contect->surface,
            //ex_rdp_contect->im_origin_x, ex_rdp_contect->im_origin_y);
            cairo_set_source_surface(context, ex_rdp_contect->surface, -rdp_viewer_data->monitor_geometry.x,
                                     -rdp_viewer_data->monitor_geometry.y);
            if (!fuzzy_compare(scale_f, 1))
                cairo_surface_set_device_scale(ex_rdp_contect->surface, scale_f, scale_f);

            cairo_set_operator(context, CAIRO_OPERATOR_OVER);     // Ignore alpha channel from FreeRDP
            cairo_set_antialias(context, CAIRO_ANTIALIAS_FAST);

            double x = 0;
            double y = 0;
            double width = (ex_rdp_contect->optimal_image_width - rdp_viewer_data->monitor_geometry.x);
            double height = (ex_rdp_contect->optimal_image_height - rdp_viewer_data->monitor_geometry.y);

            if (width > 0 && height > 0) {
                cairo_rectangle(context, x, y, width, height);
                cairo_clip(context);
                cairo_paint(context);
            }
        }

        g_mutex_unlock(&ex_rdp_contect->primary_buffer_mutex);

    } else {
        /* Draw text */
        UINT32 *last_rdp_error_p = (g_object_get_data(G_OBJECT(widget), "last_rdp_error"));
        UINT32 last_rdp_error = *last_rdp_error_p;
        gchar *msg = g_strdup_printf(("Нет соединения. Код: %i %s"), last_rdp_error,
                                     error_to_str(last_rdp_error));

        cairo_select_font_face(context, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(context, 15);
        cairo_set_source_rgb(context, 0.1, 0.2, 0.9);

        cairo_move_to(context, 50, 50);
        cairo_show_text(context, msg);

        g_free(msg);
    }

    rdp_viewer_data->is_rdp_display_being_redrawed = FALSE;
    return TRUE;
}

static gboolean rdp_display_event_on_configure(GtkWidget *widget G_GNUC_UNUSED,
                                               GdkEvent *event G_GNUC_UNUSED, gpointer user_data)
{
    RdpViewerData *rdp_viewer_data = (RdpViewerData *)user_data;
    ExtendedRdpContext *ex_contect = rdp_viewer_data->ex_rdp_context;

    if (ex_contect && ex_contect->is_running) {
        g_mutex_lock(&ex_contect->primary_buffer_mutex);
        //rdp_client_adjust_im_origin_point(ex_contect);
        g_mutex_unlock(&ex_contect->primary_buffer_mutex);
    }

    return TRUE;
}

GtkWidget *rdp_display_create(RdpViewerData *rdp_viewer_data, ExtendedRdpContext *ex_rdp_context,
                              UINT32 *last_rdp_error_p)
{
    GtkWidget *rdp_display = gtk_drawing_area_new();

    gtk_widget_add_events(rdp_display, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                          GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

    g_object_set_data(G_OBJECT(rdp_display), "last_rdp_error", last_rdp_error_p);

    GtkWidget *rdp_viewer_window = rdp_viewer_data->rdp_viewer_window;
    g_signal_connect(rdp_viewer_window, "key-press-event", G_CALLBACK(rdp_display_key_pressed), ex_rdp_context);
    g_signal_connect(rdp_viewer_window, "key-release-event", G_CALLBACK(rdp_display_key_released), ex_rdp_context);

    g_signal_connect(rdp_display, "motion-notify-event",G_CALLBACK (rdp_display_mouse_moved), rdp_viewer_data);
    g_signal_connect(rdp_display, "button-press-event",G_CALLBACK (rdp_display_mouse_btn_pressed), rdp_viewer_data);
    g_signal_connect(rdp_display, "button-release-event",G_CALLBACK (rdp_display_mouse_btn_released), rdp_viewer_data);
    g_signal_connect(rdp_display, "scroll-event",G_CALLBACK (rdp_display_wheel_scrolled), rdp_viewer_data);
    g_signal_connect(rdp_display, "configure-event", G_CALLBACK(rdp_display_event_on_configure), rdp_viewer_data);
    g_signal_connect(rdp_display, "draw", G_CALLBACK(rdp_display_event_on_draw), rdp_viewer_data);

    return rdp_display;
}