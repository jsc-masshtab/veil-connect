/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef ASYNC_H
#define ASYNC_H

#include <gio/gio.h>

// threads
void execute_async_task(GTaskThreadFunc  task_func,
                        GAsyncReadyCallback  callback,
                        gpointer task_data,
                        gpointer callback_data);

void execute_async_task_cancellable(GTaskThreadFunc  task_func,
                        GAsyncReadyCallback  callback,
                        gpointer task_data,
                        gpointer callback_data,
                        GCancellable *cancellable);

// sleep which can be cancelled so user will not notice any freeze
void cancellable_sleep(gulong microseconds, volatile gboolean *running_flag);

void wait_for_mutex_and_clear(GMutex *mutex);

#endif // ASYNC_H
