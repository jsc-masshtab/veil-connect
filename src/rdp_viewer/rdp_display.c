/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <math.h>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib/garray.h>
#include <glib/gi18n.h>

#include <freerdp/locale/keyboard.h>
#include <freerdp/scancode.h>

#include "vdi_session.h"

#include "rdp_util.h"
#include "rdp_rail.h"
#include "rdp_display.h"

G_DEFINE_TYPE( RdpDisplay, rdp_display, GTK_TYPE_DRAWING_AREA )


static void rdp_display_translate_mouse_pos(RdpDisplay *self, UINT16 *rdp_x_p, UINT16 *rdp_y_p,
                                            gdouble gtk_x, gdouble gtk_y) {
    ExtendedRdpContext *ex_context = self->ex_context;
    *rdp_x_p = (UINT16) (gtk_x - ex_context->im_origin_x + self->geometry.x);
    *rdp_y_p = (UINT16) (gtk_y - ex_context->im_origin_y + self->geometry.y);
}

static gboolean rdp_display_mouse_moved(GtkWidget *widget G_GNUC_UNUSED, GdkEventMotion *event,
        gpointer user_data)
{
    RdpDisplay *rdp_display = (RdpDisplay *)user_data;
    ExtendedRdpContext* ex_context = rdp_display->ex_context;
    if (!ex_context || !ex_context->is_running)
        return TRUE;

    rdpContext* rdp_context = (rdpContext*)ex_context;
    rdpInput *input = rdp_context->input;

    UINT16 x, y;
    rdp_display_translate_mouse_pos(rdp_display, &x, &y, event->x, event->y);
    BOOL is_success = freerdp_input_send_mouse_event(input, PTR_FLAGS_MOVE, x, y);
    (void)is_success;
    //g_info("%s: event->x %f, event->y %f  %i\n", (const char *)__func__, event->x, event->y, is_success);

    return TRUE;
}

static void rdp_viewer_handle_mouse_btn_event(GtkWidget *widget G_GNUC_UNUSED, GdkEventButton *event,
        gpointer user_data, UINT16 additional_flags)
{
    RdpDisplay *rdp_display = (RdpDisplay*)user_data;

    ExtendedRdpContext *ex_context = rdp_display->ex_context;
    if (!ex_context || !ex_context->is_running)
        return;

    rdpContext* context = (rdpContext*)ex_context;
    rdpInput *input = context->input;

    UINT16 button = 0;
    switch (event->button) {
        case GDK_BUTTON_PRIMARY: {
            button = PTR_FLAGS_BUTTON1;
            break;
        }
        case GDK_BUTTON_SECONDARY: {
            button = PTR_FLAGS_BUTTON2;
            break;
        }
        case GDK_BUTTON_MIDDLE: {
            button = PTR_FLAGS_BUTTON3;
            break;
        }
        default:
            break;
    }

    if (button) {
        //event->state;
        UINT16 x, y;
        rdp_display_translate_mouse_pos(rdp_display, &x, &y, event->x, event->y);
        freerdp_input_send_mouse_event(input, additional_flags | button, x, y);
//        g_info("%s: event->x %f, event->y %f  %i %i\n", (const char *)__func__,
//               event->x, event->y, event->button, event->state);
    }
}
// PTR_FLAGS_DOWN
static gboolean rdp_display_mouse_btn_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    if (event->type == GDK_BUTTON_PRESS) {
        vdi_ws_client_send_user_gui(vdi_session_get_ws_client()); // notify server
        rdp_viewer_handle_mouse_btn_event(widget, event, user_data, PTR_FLAGS_DOWN);
    }
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
    RdpDisplay *rdp_display = (RdpDisplay *)user_data;
    ExtendedRdpContext* ex_context = rdp_display->ex_context;
    if (!ex_context || !ex_context->is_running)
        return TRUE;

    rdpContext* context = (rdpContext*)ex_context;
    rdpInput *input = context->input;
    //g_info("%s event->delta_y %f event->delta_x %f\n", (const char *)__func__, event->delta_y, event->delta_x);

    const int rotationUnits = 0x0078;
    UINT16 x, y;
    rdp_display_translate_mouse_pos(rdp_display, &x, &y, event->x, event->y);
    if ( event->delta_y > 0.5)
        freerdp_input_send_mouse_event(input, PTR_FLAGS_WHEEL | PTR_FLAGS_WHEEL_NEGATIVE | rotationUnits, x, y);
    else if (event->delta_y < -0.5)
        freerdp_input_send_mouse_event(input, PTR_FLAGS_WHEEL | rotationUnits, x, y);

    return TRUE;
}

static void rdp_display_draw_text_message(cairo_t* context, const gchar *msg, double y)
{
    cairo_select_font_face(context, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(context, 15);
    cairo_set_source_rgb(context, 0.1, 0.2, 0.9);

    cairo_move_to(context, 10, y);
    cairo_show_text(context, msg);
}

static gboolean rdp_display_event_on_draw(GtkWidget* widget G_GNUC_UNUSED, cairo_t* context, gpointer user_data)
{
    //g_info("%s START\n", (const char *)__func__);
    RdpDisplay *rdp_display = (RdpDisplay *)user_data;

    ExtendedRdpContext *ex_context = rdp_display->ex_context;

    if (ex_context) {

        if (ex_context->is_running) {
            g_mutex_lock(&ex_context->primary_buffer_mutex);

            if (ex_context->surface) {
                //gint64 start = g_get_monotonic_time();
                //cairo_push_group(context);
                cairo_set_source_surface(context, ex_context->surface, -rdp_display->geometry.x,
                                         -rdp_display->geometry.y);

                cairo_set_operator(context, CAIRO_OPERATOR_OVER);     // Ignore alpha channel from FreeRDP
                cairo_set_antialias(context, CAIRO_ANTIALIAS_FAST);

                //cairo_pop_group(context);
                cairo_paint(context);
                //g_info("Paint time %lli", g_get_monotonic_time() - start);

            } else { // Поверхность создается сразу после подключения. Если ее нет, значит мы ожидаем подключение
                rdp_display_draw_text_message(context, _("Waiting for connection"), 50);
            }

            g_mutex_unlock(&ex_context->primary_buffer_mutex);
        } else {
            /* Draw text */
            gchar *msg = rdp_util_get_full_error_msg(ex_context->last_rdp_error, ex_context->rail_rdp_error);
            rdp_display_draw_text_message(context, msg, 50);
            g_free(msg);
        }
    }

    return TRUE;
}

static gboolean rdp_display_size_changed(RdpDisplay *self)
{
    g_info("%s", (const char *)__func__);

    ExtendedRdpContext *ex_context = self->ex_context;

    GtkAllocation allocation;
    int baseline;
    gtk_widget_get_allocated_size(GTK_WIDGET(self), &allocation, &baseline);
    UINT32 targetWidth = allocation.width;
    UINT32 targetHeight = allocation.height;

    // Send resize request
    disp_update_layout(ex_context, targetWidth, targetHeight);

    self->resize_event_source_id = 0;
    return G_SOURCE_REMOVE;
}

static void rdp_display_on_size_allocated(GtkWidget *self, GtkAllocation *allocation G_GNUC_UNUSED,
        gpointer user_data G_GNUC_UNUSED)
{
    RdpDisplay *rdp_display = (RdpDisplay *)self;

    // Отложенный сигнал, чтобы не спамить сервер запросами на изменение расширения
    g_source_remove_safely(&rdp_display->resize_event_source_id);
    rdp_display->resize_event_source_id =
            g_timeout_add(300, (GSourceFunc) rdp_display_size_changed, rdp_display);
}

static void rdp_display_finalize(GObject *object)
{
    g_info("%s", (const char *)__func__);
    RdpDisplay *self = RDP_DISPLAY(object);
    g_source_remove_safely(&self->resize_event_source_id);
}

static void rdp_display_class_init(RdpDisplayClass *klass G_GNUC_UNUSED)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS( klass );
    gobject_class->finalize = rdp_display_finalize;
}

static void rdp_display_init(RdpDisplay *self G_GNUC_UNUSED)
{

}

RdpDisplay *rdp_display_new(ExtendedRdpContext *ex_context, GdkRectangle geometry)
{
    RdpDisplay *rdp_display = RDP_DISPLAY(g_object_new(TYPE_RDP_DISPLAY, NULL));
    rdp_display->ex_context = ex_context;
    rdp_display->geometry = geometry;

    gtk_widget_add_events(GTK_WIDGET(rdp_display), GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
    GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

    g_signal_connect(rdp_display, "motion-notify-event",G_CALLBACK (rdp_display_mouse_moved), rdp_display);
    g_signal_connect(rdp_display, "button-press-event",G_CALLBACK (rdp_display_mouse_btn_pressed), rdp_display);
    g_signal_connect(rdp_display, "button-release-event",G_CALLBACK (rdp_display_mouse_btn_released), rdp_display);
    g_signal_connect(rdp_display, "scroll-event",G_CALLBACK (rdp_display_wheel_scrolled), rdp_display);
    g_signal_connect(rdp_display, "draw", G_CALLBACK(rdp_display_event_on_draw), rdp_display);
    g_signal_connect(rdp_display, "size-allocate", G_CALLBACK(rdp_display_on_size_allocated), rdp_display);


    return rdp_display;
}
