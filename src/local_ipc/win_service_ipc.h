#pragma once

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <tchar.h>


typedef struct {
	HANDLE hMapFile;
    LPVOID pBuf;

} SharedMemoryIpc;

typedef struct {
    gchar *login;
    gchar *domain;
    gchar *password;

} SharedMemoryIpcLogonData;


void shared_memory_ipc_init(SharedMemoryIpc *self);
void shared_memory_ipc_deinit(SharedMemoryIpc* self);
void shared_memory_ipc_get_logon_data(GTask       *task,
                                      gpointer     source_object,
                                      gpointer     task_data ,
                                      GCancellable *cancellable);
void shared_memory_ipc_logon_data_free(SharedMemoryIpcLogonData *self);