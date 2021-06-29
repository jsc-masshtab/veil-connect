//
// Created by ubuntu on 07.04.2021.
//

#include "atomic_string.h"
#include "remote-viewer-util.h"

void atomic_string_init(AtomicString *self)
{
    self->string = NULL;
    g_mutex_init(&self->string_mutex);
    self->is_init = TRUE;
}

void atomic_string_deinit(AtomicString *self)
{
    if (!self->is_init)
        return;

    g_mutex_lock(&self->string_mutex);
    free_memory_safely(&self->string);
    g_mutex_unlock(&self->string_mutex);

    g_mutex_clear(&self->string_mutex);

    self->is_init = FALSE;
}

void atomic_string_set(AtomicString *self, const gchar *new_string)
{
    if (!self->is_init)
        return;

    g_mutex_lock(&self->string_mutex);
    update_string_safely(&self->string, new_string);
    g_mutex_unlock(&self->string_mutex);
}

gchar *atomic_string_get(AtomicString *self)
{
    if (!self->is_init)
        return NULL;

    gchar *copy_string;
    g_mutex_lock(&self->string_mutex);
    if (self->string)
        copy_string = g_strdup(self->string);
    else
        copy_string = NULL;
    g_mutex_unlock(&self->string_mutex);

    return copy_string;
}
