//
// Created by ubuntu on 23.06.2022.
//

#ifndef VEIL_CONNECT_ADDITIONAL_ADDRESSES_WIDGET_H
#define VEIL_CONNECT_ADDITIONAL_ADDRESSES_WIDGET_H

#include <glib.h>
#include <gtk/gtk.h>

#include "settings_data.h"

void additional_addresses_widget_show(GList **list, MultiAddressMode *multiAddressMode,
        ConnectSettingsData *p_conn_data, GtkWindow *parent);


#endif //VEIL_CONNECT_ADDITIONAL_ADDRESSES_WIDGET_H
