/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_USBREDIR_SPICE_H
#define VEIL_CONNECT_USBREDIR_SPICE_H

#include <glib.h>
#include <gtk/gtk.h>

#include <spice-client.h>
#include <spice-client-gtk.h>

typedef struct{

    SpiceSession *session;

    gboolean is_connected;
    gboolean is_fetching_vm_data;

    gchar *host;
    gchar *password;
    guint port;

    GtkBuilder *builder;
    GtkWidget *dialog;
    GtkWidget *main_container;
    GtkWidget *usb_device_widget;
    GtkWidget *status_label;

} SpiceUsbSession;

SpiceUsbSession *usbredir_spice_new(void);
void usbredir_spice_destroy(SpiceUsbSession *self);

void usbredir_spice_show_dialog(SpiceUsbSession *self, GtkWindow *parent);

void usbredir_spice_set_credentials(SpiceUsbSession *self,
        const gchar *host, const gchar *password, guint port);

void usbredir_spice_connect(SpiceUsbSession *self);
void usbredir_spice_disconnect(SpiceUsbSession *self);

#endif //VEIL_CONNECT_USBREDIR_SPICE_H
