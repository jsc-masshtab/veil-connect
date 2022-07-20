//
// Created by ubuntu on 23.06.2022.
//

#include <glib/gi18n.h>

#include "additional_addresses_widget.h"
#include "remote-viewer-util.h"


typedef struct{

    GMainLoop *loop;
    GtkBuilder *builder;
    GtkWidget *main_dialog;
    GtkWidget *main_viewport;
    GtkWidget *main_list_box;

    GtkWidget *btn_add_item;
    GtkWidget *btn_remove_all;

    GtkWidget *btn_connect_to_main_address_first;
    GtkWidget *btn_connect_to_random_address_first;

    GPtrArray *items_array;

    GList **p_list;
    MultiAddressMode *p_multiAddressMode;

} AdditionalAddressesWidget;

typedef struct{
    GtkWidget *item_entry;
    GtkWidget *remove_btn;
    GtkWidget *list_box_row;
} Item;

static void
on_btn_remove_item_clicked(GtkButton *button, AdditionalAddressesWidget *self)
{
    // find box_row by button
    Item *searched_item = NULL;
    for (guint i = 0; i < self->items_array->len; ++i) {
        Item *item = g_ptr_array_index(self->items_array, i);

        if (item->remove_btn == GTK_WIDGET(button)) {
            searched_item = item;
            break;
        }
    }

    if (searched_item) {
        gtk_widget_destroy(searched_item->list_box_row);
        g_ptr_array_remove(self->items_array, searched_item);
        free(searched_item);
    }
}

static void additional_addresses_widget_add_item(AdditionalAddressesWidget *self, const gchar *text)
{
    Item *item = (Item*)calloc(1, sizeof(Item));
    g_ptr_array_add(self->items_array, item);

    item->list_box_row = gtk_list_box_row_new();

    GtkWidget *horizontal_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);

    item->item_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(item->item_entry), text);
    gtk_box_pack_start(GTK_BOX(horizontal_box), GTK_WIDGET(item->item_entry), TRUE, TRUE, 0);

    item->remove_btn = gtk_button_new_with_label("X");
    gtk_box_pack_start(GTK_BOX(horizontal_box), GTK_WIDGET(item->remove_btn), TRUE, TRUE, 0);
    g_signal_connect(item->remove_btn, "clicked", G_CALLBACK(on_btn_remove_item_clicked), self);

    gtk_container_add(GTK_CONTAINER(item->list_box_row), horizontal_box);

    gtk_container_add(GTK_CONTAINER(self->main_list_box), item->list_box_row);
    gtk_widget_show_all(item->list_box_row);
}

static void
on_btn_add_item_clicked(GtkButton *button G_GNUC_UNUSED, AdditionalAddressesWidget *self)
{
    additional_addresses_widget_add_item(self, "0.0.0.0");
}

static void
on_btn_remove_all_clicked(GtkButton *button G_GNUC_UNUSED, AdditionalAddressesWidget *self)
{
    for (guint i = 0; i < self->items_array->len; ++i) {
        Item *item = g_ptr_array_index(self->items_array, i);
        gtk_widget_destroy(item->list_box_row);
    }

    g_ptr_array_free(self->items_array, TRUE);
    self->items_array = g_ptr_array_new();
}

static void fill_gui(AdditionalAddressesWidget *self)
{
    for (GList *l = *self->p_list; l != NULL; l = l->next) {
        const gchar *str = (const gchar *)l->data;
        additional_addresses_widget_add_item(self, str);
    }

    // connect mode
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->btn_connect_to_main_address_first),
                                 (*self->p_multiAddressMode) == MULTI_ADDRESS_MODE_MAIN_FIRST);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->btn_connect_to_random_address_first),
                                 (*self->p_multiAddressMode) == MULTI_ADDRESS_MODE_RANDOM_FIRST);
}

static void take_from_gui(AdditionalAddressesWidget *self)
{
    if (*self->p_list) {
        g_list_free_full(*self->p_list, g_free);
        *self->p_list = NULL;
    }

    for (guint i = 0; i < self->items_array->len; ++i) {
        Item *item = g_ptr_array_index(self->items_array, i);

        const gchar *text = gtk_entry_get_text(GTK_ENTRY(item->item_entry));
        (*self->p_list) = g_list_append(*self->p_list, g_strdup(text));
    }

    // connect mode
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->btn_connect_to_main_address_first)))
        *self->p_multiAddressMode = MULTI_ADDRESS_MODE_MAIN_FIRST;
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->btn_connect_to_random_address_first)))
        *self->p_multiAddressMode = MULTI_ADDRESS_MODE_RANDOM_FIRST;
}

void additional_addresses_widget_show(GList **list, MultiAddressMode *multiAddressMode,
        ConnectSettingsData *p_conn_data G_GNUC_UNUSED, GtkWindow *parent)
{
    AdditionalAddressesWidget *self = (AdditionalAddressesWidget*)calloc(1, sizeof(AdditionalAddressesWidget));

    GtkBuilder *builder = remote_viewer_util_load_ui("additional_addresses_form.glade");

    self->main_dialog = get_widget_from_builder(builder, "main_dialog");
    gtk_window_set_title(GTK_WINDOW(self->main_dialog), _("Adresses"));
    self->main_viewport = get_widget_from_builder(builder, "main_viewport");
    self->main_list_box = get_widget_from_builder(builder, "main_list_box");

    self->btn_add_item = get_widget_from_builder(builder, "btn_add_item");
    self->btn_remove_all = get_widget_from_builder(builder, "btn_remove_all");

    self->btn_connect_to_main_address_first = get_widget_from_builder(builder, "btn_connect_to_main_address_first");
    self->btn_connect_to_random_address_first = get_widget_from_builder(builder, "btn_connect_to_random_address_first");

    self->items_array = g_ptr_array_new();

    // connects
    g_signal_connect(self->btn_add_item, "clicked", G_CALLBACK(on_btn_add_item_clicked), self);
    g_signal_connect(self->btn_remove_all, "clicked", G_CALLBACK(on_btn_remove_all_clicked), self);

    gtk_window_set_transient_for(GTK_WINDOW(self->main_dialog), parent);
    gtk_window_set_default_size(GTK_WINDOW(self->main_dialog), 250, 350);
    gtk_window_set_position(GTK_WINDOW(self->main_dialog), GTK_WIN_POS_CENTER);

    // set gui data
    self->p_list = list;
    self->p_multiAddressMode = multiAddressMode;
    fill_gui(self);

    gtk_widget_show_all(self->main_dialog);

    // Start loop
    gtk_dialog_run(GTK_DIALOG(self->main_dialog));

    take_from_gui(self);

    // Clear
    g_ptr_array_free(self->items_array, TRUE);
    g_object_unref(builder);
    gtk_widget_destroy(self->main_dialog);
    free(self);
}
