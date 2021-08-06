/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_QR_CODE_RENDERER_H
#define VEIL_CONNECT_QR_CODE_RENDERER_H

#include <gtk/gtk.h>

#include "qr_code_gen.h"


#define TYPE_QR_WIDGET  (qr_widget_get_type ())
#define QR_WIDGET(obj)  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_QR_WIDGET, QrWidget))


typedef struct
{
    GtkDrawingArea parent_instance;

    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
    gboolean is_qr_generated;

} QrWidget;

typedef struct
{
    GtkDrawingAreaClass parent_class;
} QrWidgetClass;


GType qr_widget_get_type(void) G_GNUC_CONST;

QrWidget *qr_widget_new(void);
void qr_widget_generate_qr(QrWidget *self, const char *text);
void qr_widget_clear(QrWidget *self);

#endif //VEIL_CONNECT_QR_CODE_RENDERER_H
