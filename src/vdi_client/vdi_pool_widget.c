/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include "vdi_pool_widget.h"


#define MAX_PROTOCOLS_NUMBER 32 // for sanity check

VdiPoolWidget build_pool_widget(const gchar *pool_id, const gchar *pool_name,
                                const gchar *os_type, const gchar *status, JsonArray *conn_types_json_array,
                                GtkWidget *gtk_flow_box)
{
    VdiPoolWidget vdi_pool_widget = {};

    if (gtk_flow_box == NULL)
        return vdi_pool_widget;

    //GtkFrame
    vdi_pool_widget.main_widget = gtk_frame_new(NULL); // status

    vdi_pool_widget.gtk_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_name(vdi_pool_widget.main_widget, "vdi_pool_widget_box");
    
    // overlay
    vdi_pool_widget.gtk_overlay = gtk_overlay_new();
    gtk_container_add((GtkContainer *)vdi_pool_widget.gtk_overlay, vdi_pool_widget.gtk_box);

    // spinner
    vdi_pool_widget.vm_spinner = gtk_spinner_new();
    gtk_overlay_add_overlay((GtkOverlay *)vdi_pool_widget.gtk_overlay, vdi_pool_widget.vm_spinner);
    gtk_overlay_set_overlay_pass_through((GtkOverlay *)vdi_pool_widget.gtk_overlay, vdi_pool_widget.vm_spinner, TRUE);

    gtk_container_add((GtkContainer *)vdi_pool_widget.main_widget, vdi_pool_widget.gtk_overlay);

    // os image
    gchar *os_icon_path = NULL;
    if (g_strcmp0(os_type, "Windows") == 0) {
        os_icon_path = g_strdup(VIRT_VIEWER_RESOURCE_PREFIX"/icons/content/img/windows_icon.png");

    } else if (g_strcmp0(os_type, "Linux") == 0) {
        os_icon_path = g_strdup(VIRT_VIEWER_RESOURCE_PREFIX"/icons/content/img/linux_icon.png");

    } else {
        os_icon_path = g_strdup(VIRT_VIEWER_RESOURCE_PREFIX"/icons/content/img/veil-32x32.png");
    }

    vdi_pool_widget.image_widget = gtk_image_new_from_resource(os_icon_path);
    gtk_widget_set_name(vdi_pool_widget.image_widget, "vdi_pool_widget_image");
    free_memory_safely(&os_icon_path);

    // combobox_remote_protocol
    vdi_pool_widget.combobox_remote_protocol = gtk_combo_box_text_new();
    gtk_box_pack_start((GtkBox *)vdi_pool_widget.gtk_box, vdi_pool_widget.combobox_remote_protocol, TRUE, TRUE, 0);

    //fill combobox_remote_protocol
    guint protocols_number = MIN(json_array_get_length(conn_types_json_array), MAX_PROTOCOLS_NUMBER);
    for (int i = 0; i < (int) protocols_number; ++i) {
        const gchar *protocol_name = json_array_get_string_element(conn_types_json_array, (guint) i);
        if (g_strcmp0(protocol_name, vdi_session_remote_protocol_to_str(VDI_SPICE_PROTOCOL)) == 0
            || g_strcmp0(protocol_name, vdi_session_remote_protocol_to_str(VDI_SPICE_DIRECT_PROTOCOL)) == 0
            || g_strcmp0(protocol_name, vdi_session_remote_protocol_to_str(VDI_RDP_PROTOCOL)) == 0
#ifdef _WIN32
            || g_strcmp0(protocol_name, vdi_session_remote_protocol_to_str(VDI_RDP_WINDOWS_NATIVE_PROTOCOL)) == 0
#endif
                )
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(vdi_pool_widget.combobox_remote_protocol), protocol_name);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(vdi_pool_widget.combobox_remote_protocol), 0);

    // vm start button
    vdi_pool_widget.vm_start_button = gtk_button_new_with_label(pool_name);
    gtk_widget_set_name(vdi_pool_widget.vm_start_button, "vdi_pool_widget_button");

    gtk_button_set_always_show_image(GTK_BUTTON (vdi_pool_widget.vm_start_button), TRUE);
    gtk_button_set_image(GTK_BUTTON (vdi_pool_widget.vm_start_button), vdi_pool_widget.image_widget);
    gtk_button_set_image_position(GTK_BUTTON (vdi_pool_widget.vm_start_button), GTK_POS_BOTTOM);

    gtk_box_pack_start((GtkBox *)vdi_pool_widget.gtk_box, vdi_pool_widget.vm_start_button, TRUE, TRUE, 0);

    g_object_set_data_full(G_OBJECT(vdi_pool_widget.vm_start_button),
            "pool_id",
            g_strdup(pool_id),
            (GDestroyNotify) g_free);

    // main_widget setup
    gtk_widget_set_size_request(vdi_pool_widget.main_widget, 100, 120);
    gtk_flow_box_insert((GtkFlowBox *)gtk_flow_box, vdi_pool_widget.main_widget, 0);

    // if pool status is not ACTIVE them we disable the widget
    if (g_strcmp0(status, "ACTIVE") != 0) {
        gtk_widget_set_sensitive(vdi_pool_widget.main_widget, FALSE);
        gtk_frame_set_label(GTK_FRAME(vdi_pool_widget.main_widget), status);
    }

    gtk_widget_show_all(vdi_pool_widget.main_widget);

    return vdi_pool_widget;
}

VdiVmRemoteProtocol vdi_pool_widget_get_current_protocol(VdiPoolWidget *vdi_pool_widget)
{
    gchar *current_protocol_str = gtk_combo_box_text_get_active_text(
            (GtkComboBoxText*)vdi_pool_widget->combobox_remote_protocol);
    g_info("%s current_protocol_str %s", (const char *)__func__, current_protocol_str);

    VdiVmRemoteProtocol protocol = vdi_session_str_to_remote_protocol(current_protocol_str);
    free_memory_safely(&current_protocol_str);
    return protocol;
}

void enable_spinner_visible(VdiPoolWidget *vdi_pool_widget, gboolean enable)
{
    if(vdi_pool_widget->vm_spinner == NULL)
        return;

    if(enable)
        gtk_spinner_start((GtkSpinner *)vdi_pool_widget->vm_spinner);
    else
        gtk_spinner_stop((GtkSpinner *)vdi_pool_widget->vm_spinner);
}

void destroy_vdi_pool_widget(VdiPoolWidget *vdi_pool_widget)
{
    if (vdi_pool_widget->btn_click_sig_hadle)
        g_signal_handler_disconnect(vdi_pool_widget->vm_start_button, vdi_pool_widget->btn_click_sig_hadle);

    gtk_widget_destroy(vdi_pool_widget->vm_spinner);
    gtk_widget_destroy(vdi_pool_widget->image_widget);
    gtk_widget_destroy(vdi_pool_widget->vm_start_button);
    gtk_widget_destroy(vdi_pool_widget->combobox_remote_protocol);
    gtk_widget_destroy(vdi_pool_widget->gtk_box);
    gtk_widget_destroy(vdi_pool_widget->gtk_overlay);
    gtk_widget_destroy(vdi_pool_widget->main_widget);
}
