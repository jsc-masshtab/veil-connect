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

#include <freerdp/locale/keyboard.h>
#include <freerdp/scancode.h>

#include "vdi_session.h"

#include "rdp_util.h"
#include "rdp_rail.h"
#include "rdp_display.h"

G_DEFINE_TYPE( RdpDisplay, rdp_display, GTK_TYPE_DRAWING_AREA )


static void rdp_display_translate_mouse_pos(RdpDisplay *self, UINT16 *rdp_x_p, UINT16 *rdp_y_p,
                                            gdouble gtk_x, gdouble gtk_y) {
    ExtendedRdpContext *ex_rdp_context = self->ex_context;
    *rdp_x_p = (UINT16) (gtk_x - ex_rdp_context->im_origin_x + self->geometry.x); // * scale_f
    *rdp_y_p = (UINT16) (gtk_y - ex_rdp_context->im_origin_y + self->geometry.y); // * scale_f
}

static gboolean rdp_display_mouse_moved(GtkWidget *widget G_GNUC_UNUSED, GdkEventMotion *event,
        gpointer user_data)
{
    RdpDisplay *rdp_display = (RdpDisplay *)user_data;
    ExtendedRdpContext* ex_rdp_context = rdp_display->ex_context;
    if (!ex_rdp_context || !ex_rdp_context->is_running)
        return TRUE;

    rdpContext* rdp_context = (rdpContext*)ex_rdp_context;
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

    ExtendedRdpContext *ex_rdp_context = rdp_display->ex_context;
    if (!ex_rdp_context || !ex_rdp_context->is_running)
        return;

    rdpContext* context = (rdpContext*)ex_rdp_context;
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
    ExtendedRdpContext* ex_rdp_context = rdp_display->ex_context;
    if (!ex_rdp_context || !ex_rdp_context->is_running)
        return TRUE;

    rdpContext* context = (rdpContext*)ex_rdp_context;
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

    ExtendedRdpContext *ex_rdp_context = rdp_display->ex_context;

    if (ex_rdp_context) {

        if (ex_rdp_context->is_running) {
            g_mutex_lock(&ex_rdp_context->primary_buffer_mutex);

            if (ex_rdp_context->surface) {
                //gint64 start = g_get_monotonic_time();
                //cairo_push_group(context);
                cairo_set_source_surface(context, ex_rdp_context->surface, -rdp_display->geometry.x,
                                         -rdp_display->geometry.y);

                cairo_set_operator(context, CAIRO_OPERATOR_OVER);     // Ignore alpha channel from FreeRDP
                cairo_set_antialias(context, CAIRO_ANTIALIAS_FAST);

                //cairo_pop_group(context);
                cairo_paint(context);
                //g_info("Paint time %lli", g_get_monotonic_time() - start);

            } else { // Поверхность создается сразу после подключения. Если ее нет, значит мы ожидаем подключение
                rdp_display_draw_text_message(context, "Ожидаем подключение", 50);
            }

            g_mutex_unlock(&ex_rdp_context->primary_buffer_mutex);
        } else {
            /* Draw text */
            gchar *msg = rdp_client_get_full_error_msg(ex_rdp_context);
            rdp_display_draw_text_message(context, msg, 50);
            g_free(msg);

        }
    }

    return TRUE;
}

static void rdp_display_class_init(RdpDisplayClass *klass G_GNUC_UNUSED)
{

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

    return rdp_display;
}
