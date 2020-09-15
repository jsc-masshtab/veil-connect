//
// Created by ubuntu on 10.09.2020.
// Взята из spice-gtk
//

#ifndef VEIL_CONNECT_USBREDIR_UTIL_H
#define VEIL_CONNECT_USBREDIR_UTIL_H

#include <libusb.h>
#include <gtk/gtk.h>

void usbredir_util_get_device_strings(int bus, int address,
                                       int vendor_id, int product_id,
                                       gchar **manufacturer, gchar **product);

// Check if software USBdk installed
gboolean usbredir_util_check_if_usbdk_installed(GtkWindow *parent);

int usbredir_util_init_libusb_and_set_options(libusb_context **ctx, int verbose);

#endif //VEIL_CONNECT_USBREDIR_UTIL_H
