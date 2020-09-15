//
// Created by solomin on 02.09.2020.
//

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


#endif //VEIL_CONNECT_USBREDIRSERVER_DATA_H
