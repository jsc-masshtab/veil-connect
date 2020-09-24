#include <math.h>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib/garray.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <cairo/cairo.h>

#include <freerdp/locale/keyboard.h>
#include <freerdp/scancode.h>

#include "rdp_display.h"
#include "rdp_data.h"
#include "rdp_rail.h"

static double scale_f = 1; // todo: make local

static gboolean fuzzy_compare(double number_1, double number_2)
{
    const double epsilon = 0.00001;
    return fabs(number_1 - number_2) < epsilon;
}

static const gchar *error_to_str(UINT32 rdp_error)
{
    if (rdp_error == WRONG_FREERDP_ARGUMENTS)
        return "Невалидные агрументы Freerdp";

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
        /* For non-domain workstation where we can't contact a kerberos server */
        case ERRCONNECT_PASSWORD_CERTAINLY_EXPIRED:
            return "ERRCONNECT_PASSWORD_CERTAINLY_EXPIRED";
        case ERRCONNECT_CLIENT_REVOKED:
            return "ERRCONNECT_CLIENT_REVOKED";
        case ERRCONNECT_KDC_UNREACHABLE:
            return "ERRCONNECT_KDC_UNREACHABLE";
        case ERRCONNECT_ACCOUNT_DISABLED:
            return "ERRCONNECT_ACCOUNT_DISABLED";
        case ERRCONNECT_PASSWORD_MUST_CHANGE:
            return "ERRCONNECT_PASSWORD_MUST_CHANGE";
        case ERRCONNECT_LOGON_FAILURE:
            return "ERRCONNECT_LOGON_FAILURE";
        case ERRCONNECT_WRONG_PASSWORD:
            return "ERRCONNECT_WRONG_PASSWORD";
        case ERRCONNECT_ACCESS_DENIED:
            return "ERRCONNECT_ACCESS_DENIED";
        case ERRCONNECT_ACCOUNT_RESTRICTION:
            return "ERRCONNECT_ACCOUNT_RESTRICTION";
        case ERRCONNECT_ACCOUNT_LOCKED_OUT:
            return "ERRCONNECT_ACCOUNT_LOCKED_OUT";
        case ERRCONNECT_ACCOUNT_EXPIRED:
            return "ERRCONNECT_ACCOUNT_EXPIRED";
        case ERRCONNECT_LOGON_TYPE_NOT_GRANTED:
            return "ERRCONNECT_LOGON_TYPE_NOT_GRANTED";
        case ERRCONNECT_NO_OR_MISSING_CREDENTIALS:
            return "ERRCONNECT_NO_OR_MISSING_CREDENTIALS";
        default:
            return "";
        }
    }
}

static void rdp_display_translate_mouse_pos(UINT16 *rdp_x_p, UINT16 *rdp_y_p,
                                            gdouble gtk_x, gdouble gtk_y, RdpWindowData *rdp_window_data) {
    ExtendedRdpContext *ex_rdp_context = rdp_window_data->ex_rdp_context;

    *rdp_x_p = (UINT16) ((gtk_x - ex_rdp_context->im_origin_x + rdp_window_data->monitor_geometry.x) * scale_f);
    *rdp_y_p = (UINT16) ((gtk_y - ex_rdp_context->im_origin_y + rdp_window_data->monitor_geometry.y) * scale_f);
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
        case GDK_KEY_Insert:
            rdp_scancode = RDP_SCANCODE_INSERT;
            break;
        case GDK_KEY_Home:
            rdp_scancode = RDP_SCANCODE_HOME;
            break;
        case GDK_KEY_End:
            rdp_scancode = RDP_SCANCODE_END;
            break;
        case GDK_KEY_Delete:
            rdp_scancode = RDP_SCANCODE_DELETE;
            break;
        case GDK_KEY_Prior:
            rdp_scancode = RDP_SCANCODE_PRIOR;
            break;
        case GDK_KEY_Next:
            rdp_scancode = RDP_SCANCODE_NEXT;
            break;
        case GDK_KEY_KP_Divide:
            rdp_scancode = RDP_SCANCODE_DIVIDE;
            break;
        default:
            // other keys
            rdp_scancode = GetVirtualScanCodeFromVirtualKeyCode(event->hardware_keycode, 4);
            break;
    }
#endif
    BOOL is_success = freerdp_input_send_keyboard_event_ex(input, down, rdp_scancode);
    (void) is_success;

    //g_info("%s: hardkey: 0x%X scancode: 0x%X keyval: 0x%X down: %i\n", (const char *) __func__,
    //       event->hardware_keycode, rdp_scancode, event->keyval, down);
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
    RdpWindowData *rdp_window_data = (RdpWindowData *)user_data;
    ExtendedRdpContext* ex_contect = rdp_window_data->ex_rdp_context;
    if (!ex_contect || !ex_contect->is_running)
        return TRUE;

    rdpContext* rdp_contect = (rdpContext*)ex_contect;
    rdpInput *input = rdp_contect->input;

    UINT16 x, y;
    rdp_display_translate_mouse_pos(&x, &y, event->x, event->y, rdp_window_data);
    BOOL is_success = freerdp_input_send_mouse_event(input, PTR_FLAGS_MOVE, x, y);
    (void)is_success;
    //g_info("%s: event->x %f, event->y %f  %i\n", (const char *)__func__, event->x, event->y, is_success);

    return TRUE;
}

static void rdp_viewer_handle_mouse_btn_event(GtkWidget *widget G_GNUC_UNUSED, GdkEventButton *event,
        gpointer user_data, UINT16 additional_flags)
{
    RdpWindowData *rdp_window_data = (RdpWindowData *)user_data;
    ExtendedRdpContext* ex_rdp_contect = rdp_window_data->ex_rdp_context;
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
        rdp_display_translate_mouse_pos(&x, &y, event->x, event->y, rdp_window_data);
        freerdp_input_send_mouse_event(input, additional_flags | button, x, y);
//        g_info("%s: event->x %f, event->y %f  %i %i\n", (const char *)__func__,
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
    RdpWindowData *rdp_window_data = (RdpWindowData *)user_data;
    ExtendedRdpContext* ex_rdp_context = rdp_window_data->ex_rdp_context;
    if (!ex_rdp_context || !ex_rdp_context->is_running)
        return TRUE;

    rdpContext* context = (rdpContext*)ex_rdp_context;
    rdpInput *input = context->input;
    //g_info("%s event->delta_y %f event->delta_x %f\n", (const char *)__func__, event->delta_y, event->delta_x);

    UINT16 x, y;
    rdp_display_translate_mouse_pos(&x, &y, event->x, event->y, rdp_window_data);
    if ( event->delta_y > 0.5)
        freerdp_input_send_mouse_event(input, PTR_FLAGS_WHEEL | PTR_FLAGS_WHEEL_NEGATIVE | 0x0078, x, y);
    else if (event->delta_y < -0.5)
        freerdp_input_send_mouse_event(input, PTR_FLAGS_WHEEL | 0x0078, x, y);

    return TRUE;
}

static void rdp_display_draw_text_message(cairo_t* context, const gchar *msg, double y)
{
    cairo_select_font_face(context, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(context, 15);
    cairo_set_source_rgb(context, 0.1, 0.2, 0.9);

    cairo_move_to(context, 50, y);
    cairo_show_text(context, msg);
}

static gboolean rdp_display_event_on_draw(GtkWidget* widget, cairo_t* context, gpointer user_data)
{
    //g_info("%s START\n", (const char *)__func__);

    RdpWindowData *rdp_window_data = (RdpWindowData *)user_data;
    rdp_window_data->is_rdp_display_being_redrawed = TRUE;

    ExtendedRdpContext *ex_rdp_contect = rdp_window_data->ex_rdp_context;

    if (ex_rdp_contect) {

        if (ex_rdp_contect->is_running) {
            g_mutex_lock(&ex_rdp_contect->primary_buffer_mutex);

            if (ex_rdp_contect->surface) {

                cairo_set_source_surface(context, ex_rdp_contect->surface, -rdp_window_data->monitor_geometry.x,
                                         -rdp_window_data->monitor_geometry.y);
                if (!fuzzy_compare(scale_f, 1))
                    cairo_surface_set_device_scale(ex_rdp_contect->surface, scale_f, scale_f);

                cairo_set_operator(context, CAIRO_OPERATOR_OVER);     // Ignore alpha channel from FreeRDP
                cairo_set_antialias(context, CAIRO_ANTIALIAS_FAST);

                cairo_paint(context);

            } else { // Поверхность создается сразу после подключения. Если ее нет, значит мы ожидаем подключение
                rdp_display_draw_text_message(context, "Ожидаем подключение", 50);
            }

            g_mutex_unlock(&ex_rdp_contect->primary_buffer_mutex);
        } else {
            /* Draw text */
            gchar *msg = g_strdup_printf(("Нет соединения. Код: 0x%X %s"),
                                         ex_rdp_contect->last_rdp_error, error_to_str(ex_rdp_contect->last_rdp_error));
            rdp_display_draw_text_message(context, msg, 50);
            g_free(msg);

            if (ex_rdp_contect->rail_rdp_error != RAIL_EXEC_S_OK) {
                msg = g_strdup_printf("Remote application error. 0x%X  %s", ex_rdp_contect->rail_rdp_error,
                        rail_error_to_string(ex_rdp_contect->rail_rdp_error));
                rdp_display_draw_text_message(context, msg, 100);
                g_free(msg);
            }
        }
    }

    rdp_window_data->is_rdp_display_being_redrawed = FALSE;
    return TRUE;
}

static gboolean rdp_display_event_on_configure(GtkWidget *widget G_GNUC_UNUSED,
                                               GdkEvent *event G_GNUC_UNUSED, gpointer user_data)
{
    RdpWindowData *rdp_window_data = (RdpWindowData *)user_data;
    ExtendedRdpContext *ex_contect = rdp_window_data->ex_rdp_context;

    if (ex_contect && ex_contect->is_running) {
        g_mutex_lock(&ex_contect->primary_buffer_mutex);
        //rdp_client_adjust_im_origin_point(ex_contect);
        g_mutex_unlock(&ex_contect->primary_buffer_mutex);
    }

    return TRUE;
}

GtkWidget *rdp_display_create(RdpWindowData *rdp_window_data, ExtendedRdpContext *ex_rdp_context)
{
    GtkWidget *rdp_display = gtk_drawing_area_new();

    gtk_widget_add_events(rdp_display, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                          GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

    GtkWidget *rdp_viewer_window = rdp_window_data->rdp_viewer_window;
    g_signal_connect(rdp_viewer_window, "key-press-event", G_CALLBACK(rdp_display_key_pressed), ex_rdp_context);
    g_signal_connect(rdp_viewer_window, "key-release-event", G_CALLBACK(rdp_display_key_released), ex_rdp_context);

    g_signal_connect(rdp_display, "motion-notify-event",G_CALLBACK (rdp_display_mouse_moved), rdp_window_data);
    g_signal_connect(rdp_display, "button-press-event",G_CALLBACK (rdp_display_mouse_btn_pressed), rdp_window_data);
    g_signal_connect(rdp_display, "button-release-event",G_CALLBACK (rdp_display_mouse_btn_released), rdp_window_data);
    g_signal_connect(rdp_display, "scroll-event",G_CALLBACK (rdp_display_wheel_scrolled), rdp_window_data);
    g_signal_connect(rdp_display, "configure-event", G_CALLBACK(rdp_display_event_on_configure), rdp_window_data);
    g_signal_connect(rdp_display, "draw", G_CALLBACK(rdp_display_event_on_draw), rdp_window_data);

    return rdp_display;
}
