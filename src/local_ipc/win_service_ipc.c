#include <gtk/gtk.h>

#include "remote-viewer-util.h"
#include "win_service_ipc.h"
#include "jsonhandler.h"

#define BUF_SIZE 256
// #pragma comment(lib, "user32.lib") ?

void shared_memory_ipc_init(SharedMemoryIpc *self)
{
	TCHAR szName[] = TEXT("Global\\VeilConnectMappingObject");

    self->pBuf = NULL;
    self->hMapFile = OpenFileMapping(
            FILE_MAP_ALL_ACCESS,   // read/write access
            FALSE,                 // do not inherit the name
            szName);               // name of mapping object

    if (self->hMapFile == NULL) {
        _tprintf(TEXT("Could not open file mapping object (%lu).\n"), GetLastError());
        return;
    }

    self->pBuf = MapViewOfFile(self->hMapFile, // handle to map object
                               FILE_MAP_ALL_ACCESS,  // read/write permission
                               0,
                               0,
                               BUF_SIZE);

    if (self->pBuf == NULL) {
        _tprintf(TEXT("Could not map view of file (%lu).\n"), GetLastError());
        CloseHandle(self->hMapFile);
        self->hMapFile = NULL;
    }
}

void shared_memory_ipc_deinit(SharedMemoryIpc *self)
{
    if (self->pBuf) {
        UnmapViewOfFile(self->pBuf);
        self->pBuf = NULL;
    }
    if (self->hMapFile) {
        CloseHandle(self->hMapFile);
        self->hMapFile = NULL;
    }
}

void shared_memory_ipc_get_logon_data(GTask       *task,
                                      gpointer     source_object G_GNUC_UNUSED,
                                      gpointer     task_data G_GNUC_UNUSED,
                                      GCancellable *cancellable G_GNUC_UNUSED)
{
    SharedMemoryIpcLogonData *shared_memory_ipc_logon_data =
            calloc(1, sizeof(SharedMemoryIpcLogonData)); // free in callback!

    SharedMemoryIpc ipc;
    shared_memory_ipc_init(&ipc);
    if (ipc.pBuf) {
        // Request data
        swprintf((wchar_t*)ipc.pBuf, BUF_SIZE, L"REQ_LOGON_DATA");

        // Wait for data from service
        gulong timeout = 2000; // msec
        gulong interval_time = 100; // msec
        gulong max_num_of_iteration = timeout / interval_time;
        for (gulong i = 0; i < max_num_of_iteration; ++i) {

            g_usleep(interval_time * 1000);
            // Fetch and parse response
            char *msg = util_wstr_to_multibyte_str((const wchar_t *) ipc.pBuf);
            gboolean is_res_received = FALSE;

            if (msg) {
                JsonParser *parser = json_parser_new();
                JsonObject *root_object = get_root_json_object(parser, msg);
                if (root_object == NULL)
                    goto clear;

                const gchar *type = json_object_get_string_member_safely(root_object, "type");
                if (type && strcmp(type, "DATA") != 0)
                    goto clear;

                const gchar *login = json_object_get_string_member_safely(root_object, "login");
                if (strlen_safely(login) == 0)
                    goto clear;

                shared_memory_ipc_logon_data->login = g_strdup(login);
                const gchar *domain = json_object_get_string_member_safely(root_object, "domain");
                shared_memory_ipc_logon_data->domain = g_strdup(domain);
                const gchar *password = json_object_get_string_member_safely(root_object, "password");
                shared_memory_ipc_logon_data->password = g_strdup(password);
                is_res_received = TRUE;

                clear:
                g_object_unref(parser);
                free(msg);
                if (is_res_received)
                    break;
            }
        }

        // clear data in memory
        swprintf((wchar_t*)ipc.pBuf, BUF_SIZE, L"");
    }
    shared_memory_ipc_deinit(&ipc);

    g_task_return_pointer(task, shared_memory_ipc_logon_data, NULL);
}

void shared_memory_ipc_logon_data_free(SharedMemoryIpcLogonData *self)
{
    free_memory_safely(&self->login);
    free_memory_safely(&self->domain);
    free_memory_safely(&self->password);
    free(self);
}