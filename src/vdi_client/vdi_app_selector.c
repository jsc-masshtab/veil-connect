/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <string.h>

#include "vdi_session.h"
#include "remote-viewer-util.h"
#include "vdi_app_selector.h"
#include "vdi_session.h"


typedef struct{
    GtkBuilder *builder;

    GtkWidget *window;
    GtkWidget *gtk_flow_box;
    GtkWidget *main_box;

    GMainLoop *loop;

    RemoteApplicationFormat remote_application_format;
    AppSelectorResult selector_result;

} VdiAppSelector;


static gboolean window_deleted_cb(VdiAppSelector *self)
{
    shutdown_loop(self->loop);
    return TRUE;
}

static void vdi_app_selector_set_image_on_btn(GtkWidget *btn, GtkWidget *image_widget)
{
    gtk_button_set_always_show_image(GTK_BUTTON (btn), TRUE);
    gtk_button_set_image(GTK_BUTTON(btn), image_widget);
    gtk_button_set_image_position(GTK_BUTTON (btn), GTK_POS_BOTTOM);
}

static void vdi_app_selector_on_icon_btn_clicked(GtkButton *btn, VdiAppSelector *self)
{
    const gchar *app_alias = g_object_get_data(G_OBJECT(btn), "app_alias");
    if (strlen_safely(app_alias) != 0) { // пользователь выбрал приложение
        self->selector_result.result_type = APP_SELECTOR_RESULT_APP;
        self->selector_result.is_remote_app = TRUE;

        switch (self->remote_application_format) {
            case REMOTE_APPLICATION_FORMAT_ALIAS: {
                self->selector_result.remote_app_program = g_strdup(app_alias);
                break;
            }
            case REMOTE_APPLICATION_FORMAT_ALIAS_2: {
                self->selector_result.remote_app_program = g_strdup_printf("||%s", app_alias);
                break;
            }
            case REMOTE_APPLICATION_FORMAT_NAME:
            default: {
                const gchar *app_name = g_object_get_data(G_OBJECT(btn), "app_name");
                self->selector_result.remote_app_program = g_strdup(app_name);
                break;
            }
        }
    } else { // пользователь выбрал обычное подключение к рабочему столу
        self->selector_result.result_type = APP_SELECTOR_RESULT_DESKTOP;
        self->selector_result.is_remote_app = FALSE;
        self->selector_result.remote_app_program = NULL;
    }

    shutdown_loop(self->loop);
}

static void vdi_app_selector_setup_icon_btn(VdiAppSelector *self, GtkWidget *btn, VeilAppData app_data)
{
    gtk_widget_set_name(btn, "app_select_btn");
    gtk_widget_set_size_request(btn, 100, 120);
    gtk_flow_box_insert(GTK_FLOW_BOX(self->gtk_flow_box), btn, 0);

    g_object_set_data_full(G_OBJECT(btn),"app_name", g_strdup(app_data.app_name), (GDestroyNotify) g_free);
    g_object_set_data_full(G_OBJECT(btn),"app_alias", g_strdup(app_data.app_alias), (GDestroyNotify) g_free);

    g_signal_connect(btn, "clicked",
            G_CALLBACK(vdi_app_selector_on_icon_btn_clicked), self);
}

static void vdi_app_selector_add_app(VdiAppSelector *self, VeilAppData app_data)
{
    // Создается кнопка иконка для выбора приложения пользователем
    GtkWidget *app_btn = gtk_button_new_with_label(app_data.app_name);
    vdi_app_selector_setup_icon_btn(self, app_btn, app_data);

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
            GdkPixbuf *pix_buff = gdk_pixbuf_loader_get_pixbuf(loader);

            GtkWidget *image_widget = gtk_image_new_from_pixbuf(pix_buff);
            vdi_app_selector_set_image_on_btn(app_btn, image_widget);
        }

        g_free(icon_binary_data);
        g_object_unref(loader);
    } else {
        g_warning("Wrong icon_base64_len: %zu", icon_base64_len);
    }
}

AppSelectorResult vdi_app_selector_start(VeilVmData *p_vdi_vm_data, GtkWindow *parent,
        RemoteApplicationFormat remote_application_format)
{
    VdiAppSelector *self = calloc(1, sizeof(VdiAppSelector));
    self->remote_application_format = remote_application_format;
    self->selector_result.result_type = APP_SELECTOR_RESULT_NONE;
    GArray *farm_array = p_vdi_vm_data->farm_array;

    self->builder = remote_viewer_util_load_ui("vdi_app_selector_form.glade");
    self->window = GTK_WIDGET(gtk_builder_get_object(self->builder, "main_window"));
    self->main_box = GTK_WIDGET(gtk_builder_get_object(self->builder, "main_box"));

    gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(self->builder, "rds_name")),
                       p_vdi_vm_data->vm_verbose_name);
    gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(self->builder, "rds_address")),
                       p_vdi_vm_data->vm_host);

    self->gtk_flow_box = gtk_flow_box_new();
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(self->gtk_flow_box), 10);
    gtk_flow_box_set_selection_mode (GTK_FLOW_BOX(self->gtk_flow_box), GTK_SELECTION_NONE);
    gtk_flow_box_set_column_spacing (GTK_FLOW_BOX(self->gtk_flow_box), 6);
    gtk_flow_box_set_row_spacing (GTK_FLOW_BOX(self->gtk_flow_box), 6);
    gtk_box_pack_start(GTK_BOX(self->main_box), self->gtk_flow_box, FALSE, TRUE, 0);

    // Заполнить приложения для выбора пользователем
    if (farm_array) {
        for (guint i = 0; i < MIN(farm_array->len, 500); ++i) {
            VeilFarmData farm_data = g_array_index(farm_array, VeilFarmData, i);
            //g_info("farm_data.farm_alias: %s", farm_data.farm_alias);

            if (farm_data.app_array) {
                for (guint j = 0; j < MIN(farm_data.app_array->len, 1000); ++j) {
                    VeilAppData app_data = g_array_index(farm_data.app_array, VeilAppData, j);
                    vdi_app_selector_add_app(self, app_data);
                }
            }
        }
    }
    // add desktop icon
    GtkWidget *desktop_btn = gtk_button_new_with_label("Рабочий стол");
    VeilAppData app_data = {};
    vdi_app_selector_setup_icon_btn(self, desktop_btn, app_data);

    GtkWidget *image_widget = gtk_image_new_from_resource(
            VIRT_VIEWER_RESOURCE_PREFIX"/icons/content/img/windows_icon.png");
    vdi_app_selector_set_image_on_btn(desktop_btn, image_widget);

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

    AppSelectorResult selector_result = self->selector_result;
    free(self);

    return selector_result;
}