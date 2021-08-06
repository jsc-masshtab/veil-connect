/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "qr_code_renderer.h"

G_DEFINE_TYPE( QrWidget, qr_widget, GTK_TYPE_DRAWING_AREA )


static gboolean qr_widget_event_on_draw(GtkWidget* widget G_GNUC_UNUSED, cairo_t* context, gpointer user_data)
{
    QrWidget *qr_widget = (QrWidget *)user_data;

    // get widget size
    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);

    if (!qr_widget->is_qr_generated) {
        // default background if qr is not generated
        cairo_set_source_rgb(context, 238.0 / 255.0, 238.0 / 255.0, 228.0 / 255.0);
        cairo_rectangle(context, 0, 0, alloc.width, alloc.height);
        cairo_fill(context);

        cairo_set_font_size(context, 19);
        cairo_set_source_rgb(context, 0, 0, 0);
        cairo_move_to(context, alloc.width / 2 - 15, alloc.height / 2);
        cairo_show_text(context, "QR");

        return TRUE;
    }

    // draw QR
    int qr_size = qrcodegen_getSize(qr_widget->qrcode);
    if (qr_size <= 0)
        return TRUE;

    int min_widget_size = MIN(alloc.width, alloc.height);
    int step = min_widget_size / qr_size;
    int border_x = (alloc.width - min_widget_size) / 2;
    int border_y = (alloc.height - min_widget_size) / 2;

    for (int x = 0; x < qr_size; x++) {
        for (int y = 0; y < qr_size; y++) {
            bool is_black = qrcodegen_getModule(qr_widget->qrcode, x, y);
            // set color
            if (is_black)
                cairo_set_source_rgb(context, 0, 0, 0);
            else
                cairo_set_source_rgb(context, 1, 1, 1);
            // draw rectangle
            cairo_rectangle(context, border_x + x * step, border_y + y * step, step, step);
            cairo_fill(context);
        }
    }

    return TRUE;
}

static void qr_widget_class_init(QrWidgetClass *klass G_GNUC_UNUSED)
{

}

static void qr_widget_init(QrWidget *self)
{
    self->is_qr_generated = FALSE;
}

QrWidget *qr_widget_new()
{
    QrWidget *qr_widget = QR_WIDGET(g_object_new(TYPE_QR_WIDGET, NULL));
    g_signal_connect(qr_widget, "draw", G_CALLBACK(qr_widget_event_on_draw), qr_widget);

    return qr_widget;
}

void qr_widget_generate_qr(QrWidget *self, const char *text)
{
    enum qrcodegen_Ecc errCorLvl = qrcodegen_Ecc_LOW;  // Error correction level

    // Generate qr data
    memset(self->qrcode, 0, sizeof(self->qrcode));
    uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];
    self->is_qr_generated = qrcodegen_encodeText(text, tempBuffer, self->qrcode, errCorLvl,
                                                 qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
                                                 qrcodegen_Mask_AUTO, true);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void qr_widget_clear(QrWidget *self)
{
    memset(self->qrcode, 0, sizeof(self->qrcode));
    self->is_qr_generated = FALSE;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}