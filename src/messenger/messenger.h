/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_VEIL_MESSENGER_H
#define VEIL_CONNECT_VEIL_MESSENGER_H

#include <glib.h>
#include <gtk/gtk.h>


#define TYPE_VEIL_MESSENGER ( veil_messenger_get_type( ) )
#define VEIL_MESSENGER( obj ) ( G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_VEIL_MESSENGER, VeilMessenger) )

typedef struct{
    GObject parent;

    GtkBuilder *builder;

    GtkWidget *main_window;
    GtkWidget *message_entry_view;
    GtkWidget *btn_send_message;
    GtkWidget *message_main_view;
    GtkWidget *message_main_scrolled_window;

    GtkWidget *label_status;
    GtkWidget *spinner_status;

    gulong text_msg_received_handle;

    gboolean scroll_down_required;

} VeilMessenger;

typedef struct
{
    GObjectClass parent_class;

    /* signals */

} VeilMessengerClass;

GType veil_messenger_get_type( void ) G_GNUC_CONST;

VeilMessenger *veil_messenger_new(void);

void veil_messenger_show_on_top(VeilMessenger *self);
void veil_messenger_hide(VeilMessenger *self);

#endif //VEIL_CONNECT_VEIL_MESSENGER_H
