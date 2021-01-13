//
// Created by solomin on 02.09.2020.
//

#ifndef VEIL_CONNECT_USBREDIRSERVER_H
#define VEIL_CONNECT_USBREDIRSERVER_H

#include <gio/gio.h>

#include "usbredirserver_data.h"

void usbredirserver_launch_task(GTask           *task,
                           gpointer         source_object,
                           gpointer         task_data,
                           GCancellable    *cancellable);

//DWORD WINAPI usbredirserver_launch_task(UsbServerStartData *server_start_data);

#endif //VEIL_CONNECT_USBREDIRSERVER_H
