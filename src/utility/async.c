/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include "async.h"

void execute_async_task(GTaskThreadFunc task_func, GAsyncReadyCallback callback,
                        gpointer task_data, gpointer callback_data)
{
    GTask *task = g_task_new(NULL, NULL, callback, callback_data);
    if (task_data)
        g_task_set_task_data(task, task_data, NULL);
    g_task_run_in_thread(task, task_func);
    g_object_unref(task);
}

void execute_async_task_cancellable(GTaskThreadFunc  task_func,
                                    GAsyncReadyCallback  callback,
                                    gpointer task_data,
                                    gpointer callback_data,
                                    GCancellable *cancellable)
{
    GTask *task = g_task_new(NULL, cancellable, callback, callback_data);
    if (task_data)
        g_task_set_task_data(task, task_data, NULL);
    g_task_set_check_cancellable(task, FALSE);
    g_task_run_in_thread(task, task_func);
    g_object_unref(task);
}

// sleep which can be cancelled so user will not notice any freeze
void cancellable_sleep(gulong microseconds, volatile gboolean *running_flag)
{
    const gulong interval = 30000; // 30 ms

    gulong fractional_part = microseconds % interval;
    gulong integral_part = microseconds / interval;

    g_usleep(fractional_part);

    for (gulong i = 0; i < integral_part; ++i) {
        if (!(*running_flag))
            return;
        g_usleep(interval);
    }
}

void wait_for_mutex_and_clear(GMutex *mutex)
{
    g_mutex_lock(mutex);
    g_mutex_unlock(mutex);
    g_mutex_clear(mutex);
}
