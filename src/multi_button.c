/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <glib/gi18n.h>
#include <locale.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <config.h>
#include "remote-viewer-util.h"
#include "multi_button.h"


G_DEFINE_TYPE( MultiButton, multi_button, GTK_TYPE_BUTTON )


static void multi_button_main_btn_clicked(GtkButton *button, MultiButton *self)
{

    g_info("%s", (const char *)__func__);
    gtk_widget_show_all(self->main_menu);
    gtk_menu_popup_at_widget(GTK_MENU(self->main_menu), GTK_WIDGET(button),
                             GDK_GRAVITY_SOUTH, GDK_GRAVITY_SOUTH, NULL);
}

static void multi_button_cancel_btn_clicked(GtkButton *button G_GNUC_UNUSED, MultiButton *self)
{
    gtk_widget_hide(self->main_menu);
}

static void finalize(GObject *object)
{
    g_info("%s", (const char *)__func__);
    MultiButton *self = MULTI_BUTTON(object);

    gtk_widget_destroy(self->main_menu);

    GObjectClass *parent_class = G_OBJECT_CLASS( multi_button_parent_class );
    ( *parent_class->finalize )( object );
}

static void
multi_button_class_init(MultiButtonClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = finalize;
}

static void
multi_button_init(MultiButton *self)
{
    self->main_menu = gtk_menu_new();

    g_signal_connect(self, "clicked", G_CALLBACK(multi_button_main_btn_clicked), self);
}

MultiButton *multi_button_new(const gchar *text)
{
    MultiButton *multi_button = MULTI_BUTTON( g_object_new( TYPE_MULTI_BUTTON, NULL ) );
    gtk_button_set_label(GTK_BUTTON(multi_button), text);
    return multi_button;
}

void multi_button_add_buttons(MultiButton *self, GHashTable *buttons_data, gpointer user_data)
{
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init(&iter, buttons_data);
    while (g_hash_table_iter_next(&iter, &key, &value)) {

        GtkWidget *item = gtk_menu_item_new_with_label((const char *)key);

        SubButtonClickHandler handler = (SubButtonClickHandler)value;
        g_signal_connect(item, "activate", G_CALLBACK(handler), user_data);

        gtk_container_add(GTK_CONTAINER(self->main_menu), item);
    }

    // special button - cancel
    GtkWidget *item_cancel = gtk_menu_item_new_with_label(_("Cancel"));
    g_signal_connect(item_cancel, "activate",
            G_CALLBACK(multi_button_cancel_btn_clicked), self);
    gtk_container_add(GTK_CONTAINER(self->main_menu), item_cancel);
}