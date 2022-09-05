//
// Created by ubuntu on 23.06.2022.
//

#ifndef VEIL_CONNECT_ADDITIONAL_ADDRESSES_WIDGET_H
#define VEIL_CONNECT_ADDITIONAL_ADDRESSES_WIDGET_H

#include <glib.h>
#include <gtk/gtk.h>

#include "settings_data.h"

gchar *additional_addresses_widget_show(const gchar *comma_separated_string, MultiAddressMode *multiAddressMode,
        GtkWindow *parent);


#endif //VEIL_CONNECT_ADDITIONAL_ADDRESSES_WIDGET_H
