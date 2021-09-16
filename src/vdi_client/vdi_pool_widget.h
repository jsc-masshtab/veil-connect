/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

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

    GtkWidget *image_widget;

    GtkWidget *combobox_remote_protocol;
    GtkWidget *vm_start_button;
    //GtkWidget *vm_progress_bar;

    gulong btn_click_sig_hadle;

} VdiPoolWidget;

// build vm widget and insert it in gtk_flow_box
VdiPoolWidget build_pool_widget(const gchar *pool_id, const gchar *pool_name,
        const gchar *os_type, const gchar *status, JsonArray *conn_types_json_array, GtkWidget *gtk_flow_box);

// get current remote protocol
VdiVmRemoteProtocol vdi_pool_widget_get_current_protocol(VdiPoolWidget *vdi_pool_widget);

// start / stop spinner
void vdi_pool_widget_enable_spinner(VdiPoolWidget *vdi_pool_widget, gboolean enable);

// Vm preparation progress
//void vdi_pool_widget_set_progress(VdiPoolWidget *vdi_pool_widget, gdouble fraction);

// destroy widget
void destroy_vdi_pool_widget(VdiPoolWidget *vdi_pool_widget);

#endif //VIRT_VIEWER_VEIL_VDI_VM_WIDGET_H
