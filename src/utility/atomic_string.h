//
// Created by ubuntu on 07.04.2021.
//

#ifndef VEIL_CONNECT_ATOMIC_STRING_H
#define VEIL_CONNECT_ATOMIC_STRING_H

#include <gtk/gtk.h>

#include "async.h"

typedef struct {

    // private
    gboolean is_init;

    gchar *string;
    GMutex string_mutex;
} AtomicString;

void atomic_string_init(AtomicString *self);
void atomic_string_deinit(AtomicString *self);
void atomic_string_set(AtomicString *self, const gchar *new_string);
// return copy. must be freed
gchar *atomic_string_get(AtomicString *self);

#endif //VEIL_CONNECT_ATOMIC_STRING_H
