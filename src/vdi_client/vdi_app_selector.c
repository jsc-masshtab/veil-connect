/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include "vdi_session.h"
#include "remote-viewer-util.h"
#include "vdi_app_selector.h"


typedef struct{
    GtkBuilder *builder;

    GtkWidget *window;
    GtkWidget *gtk_flow_box;
    GtkWidget *main_box;

    GMainLoop *loop;

} VdiAppSelector;

typedef struct{
    GtkWidget *app_btn;
    GdkPixbuf *pix_buff;

} VdiGuiApp;

static gboolean window_deleted_cb(VdiAppSelector *self)
{
    shutdown_loop(self->loop);
    return TRUE;
}

void vdi_app_selector_add_app(VdiAppSelector *self, VdiAppData app_data)
{
    VdiGuiApp gui_app_data = {};

    // Создается кнопка иконка для выбора приложения пользователем
    gui_app_data.app_btn = gtk_button_new_with_label(app_data.app_name);

    // decode base64 string to bytes array
    size_t icon_base64_len = strlen_safely(app_data.icon_base64);
    if(icon_base64_len != 0 && icon_base64_len < 1048576) { // sanity check

        gsize icon_binary_data_len;
        guchar *icon_binary_data = g_base64_decode(app_data.icon_base64, &icon_binary_data_len);

        // set image
        GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
        GError *error = NULL;
        gboolean res = gdk_pixbuf_loader_write(loader, icon_binary_data, icon_binary_data_len, &error);
        if (error)
            printf("gdk_pixbuf_loader_write error:\n%s\n", error->message);
        g_clear_error(&error);

        gdk_pixbuf_loader_close(loader, NULL);

        if (res) {
            gui_app_data.pix_buff = gdk_pixbuf_loader_get_pixbuf(loader);

            GtkWidget *image_widget = gtk_image_new_from_pixbuf(gui_app_data.pix_buff);
            gtk_button_set_always_show_image(GTK_BUTTON (gui_app_data.app_btn), TRUE);
            gtk_button_set_image(GTK_BUTTON(gui_app_data.app_btn), image_widget);
            gtk_button_set_image_position(GTK_BUTTON (gui_app_data.app_btn), GTK_POS_BOTTOM);
        }

        g_free(icon_binary_data);
        g_object_unref(loader);
    } else {
        g_warning("Wrong icon_base64_len: %lu", icon_base64_len);
    }

    gtk_widget_set_size_request(gui_app_data.app_btn, 100, 120);
    gtk_flow_box_insert(GTK_FLOW_BOX(self->gtk_flow_box), gui_app_data.app_btn, 0);
}

void vdi_app_selector_start(GArray *farm_array, GtkWindow *parent)
{
    if(farm_array == NULL)
        return;

    VdiAppSelector *self = calloc(1, sizeof(VdiAppSelector));

    self->builder = remote_viewer_util_load_ui("vdi_app_selector_form.ui");
    self->window = GTK_WIDGET(gtk_builder_get_object(self->builder, "main_window"));
    self->main_box = GTK_WIDGET(gtk_builder_get_object(self->builder, "main_box"));

    self->gtk_flow_box = gtk_flow_box_new();
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(self->gtk_flow_box), 10);
    gtk_flow_box_set_selection_mode (GTK_FLOW_BOX(self->gtk_flow_box), GTK_SELECTION_NONE);
    gtk_flow_box_set_column_spacing (GTK_FLOW_BOX(self->gtk_flow_box), 6);
    gtk_box_pack_start(GTK_BOX(self->main_box), self->gtk_flow_box, FALSE, TRUE, 0);

    // Заполнить приложения для выбора пользователем
    for (guint i = 0; i < MIN(farm_array->len, 500); ++i) {
        VdiFarmData farm_data = g_array_index(farm_array, VdiFarmData, i);
        g_info("farm_data.farm_alias: %s", farm_data.farm_alias);

        if (farm_data.app_array) {
            for (guint j = 0; j < MIN(farm_data.app_array->len, 2000); ++j) {
                VdiAppData app_data = g_array_index(farm_data.app_array, VdiAppData, j);
                vdi_app_selector_add_app(self, app_data);
            }
        }
    }

    // Signals
    g_signal_connect_swapped(self->window, "delete-event",
            G_CALLBACK(window_deleted_cb), self);

    // show window
    gtk_window_set_transient_for(GTK_WINDOW(self->window), parent);
    gtk_window_set_position(GTK_WINDOW(self->window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(self->window), 650, 400);
    gtk_widget_show_all(self->window);

    create_loop_and_launch(&self->loop);

    // free
    g_object_unref(self->builder);
    gtk_widget_destroy(self->window);
    free(self);
}