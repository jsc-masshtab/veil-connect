//
// Created by ubuntu on 11.01.2022.
//

#ifndef VEIL_CONNECT_TIME_H
#define VEIL_CONNECT_TIME_H

#include <gtk/gtk.h>

// Return current timezone or NULL. Must be freed if not NULL
gchar *veil_time_get_time_zone(void);

#endif //VEIL_CONNECT_TIME_H
