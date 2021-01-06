//
// Created by Solomin on 18.06.19.
//

#ifndef VIRT_VIEWER_VEIL_VDI_VM_WIDGET_H
#define VIRT_VIEWER_VEIL_VDI_VM_WIDGET_H

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gtk/gtktypes.h>
#include <json-glib/json-glib.h>

#include "remote-viewer-util.h"

#include "vdi_session.h"

typedef struct{

    GtkWidget *main_widget;

    GtkWidget *gtk_box;
    GtkWidget *gtk_overlay;

    GtkWidget *vm_spinner;

    GtkWidget *vm_name_label;
    GtkWidget *image_widget;
    GtkWidget *vm_start_button;
    GtkWidget *combobox_remote_protocol;

    gulong btn_click_sig_hadle;

} VdiPoolWidget;

// build vm widget and insert it in gtk_flow_box
VdiPoolWidget build_pool_widget(const gchar *pool_id, const gchar *pool_name,
        const gchar *os_type, const gchar *status, JsonArray *conn_types_json_array, GtkWidget *gtk_flow_box);

// get current remore protocol
VdiVmRemoteProtocol vdi_pool_widget_get_current_protocol(VdiPoolWidget *vdi_pool_widget);

// start / stop spinner
void enable_spinner_visible(VdiPoolWidget *vdi_pool_widget, gboolean enable);

// destroy widget
void destroy_vdi_pool_widget(VdiPoolWidget *vdi_pool_widget);

#endif //VIRT_VIEWER_VEIL_VDI_VM_WIDGET_H
