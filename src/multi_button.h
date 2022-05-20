/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

// Button opening a popup widget that contains a list of buttons (like menu)

#ifndef VEIL_CONNECT_MULTI_BUTTON_H
#define VEIL_CONNECT_MULTI_BUTTON_H

#define TYPE_MULTI_BUTTON (multi_button_get_type())
#define MULTI_BUTTON(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_MULTI_BUTTON, MultiButton))

typedef struct _MultiButton        MultiButton;
typedef struct _MultiButtonClass   MultiButtonClass;
typedef void (*SubButtonClickHandler)(GtkButton *button, gpointer user_data);

struct _MultiButton {
    GtkButton parent_instance;

    GtkWidget *main_menu;
};

struct _MultiButtonClass
{
    GtkButtonClass parent_class;

    /* class members */
};

GType multi_button_get_type(void);

MultiButton *multi_button_new(const gchar *text);

void multi_button_add_buttons(MultiButton *self, GHashTable *buttons_data, gpointer user_data);

#endif //VEIL_CONNECT_MULTI_BUTTON_H
