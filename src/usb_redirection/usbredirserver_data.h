/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_USBREDIRSERVER_DATA_H
#define VEIL_CONNECT_USBREDIRSERVER_DATA_H

#include <glib.h>
#include <libusb.h>

#define USB_REDIR_FINISH_SUCCESS 0
#define USB_REDIR_FINISH_FAIL -1

typedef struct{

    int verbose;
#ifdef __linux__
    int client_fd;
#elif __APPLE__ || __MACH__
    int client_fd;
#elif _WIN32
    SOCKET client_fd;
#endif
    int *p_running_flag; // pointer to a stack variable. never try to free it
    libusb_context *ctx;
    struct usbredirhost *host;

} UsbRedirServerData;

typedef struct{

    gchar *ipv4_addr;
    int port, usbbus, usbaddr, usbvendor, usbproduct;

} UsbServerStartData;

typedef struct{
    UsbServerStartData start_data;
    int running_flag;

} UsbRedirRunningTask;

typedef struct{
    int code;
    gchar *message;

    int usbbus;
    int usbaddr;

} UsbRedirTaskResaultData;

/*
static void free_usbredir_task_resault_data(UsbRedirTaskResaultData *usbredir_task_resault_data)
{
    if (!usbredir_task_resault_data)
        return;

    free_memory_safely(&usbredir_task_resault_data->message);
    free(usbredir_task_resault_data);
    usbredir_task_resault_data = NULL;
}*/

/*
static UsbRedirTaskResaultData *clone_usbredir_task_resault_data(UsbRedirTaskResaultData *usbredir_task_resault_data)
{
    if (!usbredir_task_resault_data)
        return NULL;

    UsbRedirTaskResaultData *cloned_data = calloc(1, sizeof(UsbRedirTaskResaultData));
    memcpy(cloned_data, usbredir_task_resault_data, sizeof(UsbRedirTaskResaultData));
    cloned_data->message = g_strdup(usbredir_task_resault_data->message);

    return cloned_data;
}*/


#endif //VEIL_CONNECT_USBREDIRSERVER_DATA_H
