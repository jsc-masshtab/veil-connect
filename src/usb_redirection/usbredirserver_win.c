/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 *
 * Ported Windows version of application https://github.com/freedesktop/spice-usbredir/tree/master/usbredirserver
*/

//#define  FD_SETSIZE 256

#include <winsock2.h>
#include <winsock.h>
#include <Ws2tcpip.h>
#include <Mswsock.h>
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>

#include <glib/gi18n.h>

#include "usbredir_util.h"
#include "usbredirhost.h"
#include "remote-viewer-util.h"
#include "usbredirserver.h"
#include "vdi_session.h"

#define SERVER_VERSION "usbredirserver 0.1.0"

#if !defined(SOL_TCP) && defined(IPPROTO_TCP)
#define SOL_TCP IPPROTO_TCP
#endif
#if !defined(TCP_KEEPIDLE) && defined(TCP_KEEPALIVE) && defined(__APPLE__)
#define TCP_KEEPIDLE TCP_KEEPALIVE
#endif



static void usbredirserver_init_data(UsbRedirServerData *priv) {

    priv->verbose = usbredirparser_info;
    priv->client_fd = INVALID_SOCKET;
    priv->p_running_flag = NULL;
    priv->ctx = NULL;
    priv->host = NULL;
}

void usbredirserver_close_socket(SOCKET sock)
{
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
}

static void usbredirserver_log(void *priv, int level, const char *msg)
{
    UsbRedirServerData *usb_redir_server_data = (UsbRedirServerData*)priv;
    if (level <= usb_redir_server_data->verbose) {
        fprintf(stderr, "%s : %s\n", (const char*)__func__, msg);
    }
}

static int usbredirserver_read(void *priv, uint8_t *data, int count)
{
    //fprintf(stdout, "read result: %s %i\n", __func__, count);
    UsbRedirServerData *usb_redir_server_data = (UsbRedirServerData*)priv;
    int r = recv(usb_redir_server_data->client_fd, (char *)data, count, 0);
    //fprintf(stdout, "read result: %i\n", r);

    if (r == 0) { /* Client disconnected */
        fprintf(stdout, "%s: Disconnecting client_fd\n", (const char *)__func__);
        //closesocket(usb_redir_server_data->client_fd);
        //usb_redir_server_data->client_fd = INVALID_SOCKET;
        usbredirserver_close_socket(usb_redir_server_data->client_fd);
    }
    else if (r == SOCKET_ERROR) {

        if (errno == EAGAIN)
            return 0;

        int last_error = WSAGetLastError();
        if (last_error == WSAEWOULDBLOCK) { // not a real error. its ok
            return 0;
        }
        else {
            fprintf(stderr, "read last error: %i\n", last_error);
            return -1;
        }
    }

    return r;
}

static int usbredirserver_write(void *priv, uint8_t *data, int count)
{
    //fprintf(stdout, "read result: %s %i\n", __func__, count);
    UsbRedirServerData *usb_redir_server_data = (UsbRedirServerData*)priv;
    int r = send(usb_redir_server_data->client_fd, (const char *)data, count, 0);
    //fprintf(stdout, "write result: %i\n", r);
    if (r == SOCKET_ERROR) {

        if (errno == EAGAIN)
            return 0;
        if (errno == EPIPE) { /* Client disconnected */
            fprintf(stdout, "%s: Disconnecting client_fd\n", (const char *)__func__);
            //closesocket(usb_redir_server_data->client_fd);
            //usb_redir_server_data->client_fd = INVALID_SOCKET;
            usbredirserver_close_socket(usb_redir_server_data->client_fd);
            return 0;
        }

        int last_error = WSAGetLastError();
        if (last_error == WSAEWOULDBLOCK) { // not a real error. its ok
            return 0;
        }
        else {
            fprintf(stderr, "write last error: %i\n", last_error);
            return -1;
        }
    }
    return r;
}

static void *usbredirserver_event_thread_func(UsbRedirServerData *priv)
{
    while (*(priv->p_running_flag)) {
        libusb_handle_events(priv->ctx);
    }

    fprintf(stdout, "%s Leaving libusb event loop \n", (const char *)__func__);
    return NULL;
}

static void usbredirserver_run_main_loop(UsbRedirServerData *priv)
{
    fd_set readfds, writefds;
    int select_res, nfds;
    struct timeval timeout;

    while (*(priv->p_running_flag) && priv->client_fd != INVALID_SOCKET) {
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);

        FD_SET(priv->client_fd, &readfds);
        if (usbredirhost_has_data_to_write(priv->host)) {
            FD_SET(priv->client_fd, &writefds);
        }
        nfds = priv->client_fd + 1;

        select_res = select(nfds, &readfds, &writefds, NULL, &timeout);
        if (select_res == -1) {
            if (errno == EINTR) {
                fprintf(stdout, "%s errno == EINTR \n", (const char *)__func__);
                continue;
            }
            perror("select");
            break;
        }

        if (FD_ISSET(priv->client_fd, &readfds)) {
            if (usbredirhost_read_guest_data(priv->host)) {
                fprintf(stdout, "%s usbredirhost_read_guest_data BREAK \n", (const char *)__func__);
                break;
            }
        }
        /* usbredirhost_read_guest_data may have detected client disconnect */
        if (priv->client_fd == INVALID_SOCKET) {
            fprintf(stdout, "%s client_fd == INVALID_SOCKET BREAK \n", (const char *)__func__);
            break;
        }

        if (FD_ISSET(priv->client_fd, &writefds)) {
            if (usbredirhost_write_guest_data(priv->host)) {
                fprintf(stdout, "%s usbredirhost_write_guest_data(host) \n", (const char *)__func__);
                break;
            }
        }
    }
    //if (priv->client_fd != INVALID_SOCKET) { /* Broken out of the loop because of an error ? */
    //    closesocket(priv->client_fd);
    //    priv->client_fd = INVALID_SOCKET;
    //}
    usbredirserver_close_socket(priv->client_fd);
}

static SOCKET usbredirserver_create_server_socket(const char *ipv4_addr, int port)
{
    // Create server socket
    SOCKET server_fd = INVALID_SOCKET;
    char *ipv6_addr = NULL;
    union {
        struct sockaddr_in v4;
        struct sockaddr_in6 v6;
    } serveraddr;

    if (ipv4_addr) {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
    } else {
        server_fd = socket(AF_INET6, SOCK_STREAM, 0);
    }

    if (server_fd == INVALID_SOCKET) {
        perror("Error creating ip socket");
        return INVALID_SOCKET;
    }

    int on = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)(&on), sizeof(on))) {
        perror("Error setsockopt(SO_REUSEADDR) failed");
        closesocket(server_fd);
        return INVALID_SOCKET;
    }

    memset(&serveraddr, 0, sizeof(serveraddr));

    // set address
    if (ipv4_addr) {
        serveraddr.v4.sin_family = AF_INET;
        serveraddr.v4.sin_port   = htons(port);
        if ((inet_pton(AF_INET, ipv4_addr, &serveraddr.v4.sin_addr)) != 1) {
            perror("Error convert ipv4 address");
            closesocket(server_fd);
            return INVALID_SOCKET;
        }
    } else {
        serveraddr.v6.sin6_family = AF_INET6;
        serveraddr.v6.sin6_port   = htons(port);
        if (ipv6_addr) {
            if ((inet_pton(AF_INET6, ipv6_addr, &serveraddr.v6.sin6_addr)) != 1) {
                perror("Error convert ipv6 address");
                closesocket(server_fd);
                return INVALID_SOCKET;
            }
        } else {
            serveraddr.v6.sin6_addr   = in6addr_any;
        }
    }

    // Bind address to server socket
    if (bind(server_fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr))) {
        perror("Error bind");
        closesocket(server_fd);
        return INVALID_SOCKET;
    }

    // Listen for client
    if (listen(server_fd, 1)) {
        perror("Error listening");
        closesocket(server_fd);
        return INVALID_SOCKET;
    } else {
        fprintf(stdout, "LIstening for client\n");
    }

    return server_fd;
}

void usbredirserver_apply_keep_alive(SOCKET sock)
{
    int keepalive  = 15;//-1;
    if (keepalive > 0) {
        int optval = 1;
        socklen_t optlen = sizeof(optval);
        if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char *)(&optval), optlen) == -1) {
            if (errno != ENOTSUP) {
                perror("setsockopt SO_KEEPALIVE error.");
            }
        }
    }
}

void usbredirserver_make_socket_nonblocking(SOCKET sock)
{
    int iResult;
    u_long iMode = 1;
    iResult = ioctlsocket(sock, FIONBIO, &iMode);
    if (iResult != NO_ERROR)
        printf("ioctlsocket failed with error: %i\n", iResult);
}

libusb_device_handle *usbredirserver_find_usb_device_and_open(UsbRedirServerData *priv,
                                               int usbbus, int usbaddr, int usbvendor, int usbproduct)
{
    libusb_device_handle *handle = NULL;

    if (usbvendor != -1) {
        handle = libusb_open_device_with_vid_pid(priv->ctx, usbvendor, usbproduct);
        if (!handle) {
            fprintf(stderr,
                    "Could not open an usb-device with vid:pid %04x:%04x\n",
                    usbvendor, usbproduct);
        } else if (priv->verbose >= usbredirparser_info) {
            libusb_device *dev = libusb_get_device(handle);
            fprintf(stderr, "Open a usb-device with vid:pid %04x:%04x on "
                            "bus %03x device %03x\n",
                    usbvendor, usbproduct,
                    libusb_get_bus_number(dev),
                    libusb_get_device_address(dev));
        }
    } else {
        libusb_device **list = NULL;
        ssize_t i, n;

        n = libusb_get_device_list(priv->ctx, &list);
        for (i = 0; i < n; i++) {
            fprintf(stderr,
                    "TEST: usbbus usbaddr: %04x:%04x\n",
                    libusb_get_bus_number(list[i]), libusb_get_device_address(list[i]));

            if (libusb_get_bus_number(list[i]) == usbbus &&
                libusb_get_device_address(list[i]) == usbaddr)
                break;
        }

        if (i < n) {
            if (libusb_open(list[i], &handle) != 0) {
                fprintf(stderr,
                        "Could not open usb-device at busnum-devnum %d-%d\n",
                        usbbus, usbaddr);
            } else {
                fprintf(stdout, "Device opened %d-%d\n", usbbus, usbaddr);
            }

        } else {
            fprintf(stderr,
                    "Could not find an usb-device at busnum-devnum %d-%d\n",
                    usbbus, usbaddr);
        }
        libusb_free_device_list(list, 1);
    }

    return handle;
}
/*
static void usbredirserver_wait_until_handling_thread_stops(UsbRedirServerData *priv, HANDLE hThread)
{
    *(priv->p_running_flag) = 0;
    HANDLE hThreadArray[] = {hThread};
    WaitForMultipleObjects(1, hThreadArray, TRUE, INFINITE);
    CloseHandle(hThread);
}*/

//static int usbredirserver_init_win_sockets()
//{
//    // The WSAStartup function initiates use of the Winsock DLL by a process.
//    // Если все таки будет частью ТК то чекнуть нужно ли это вообще.
//    WORD wVersionRequested;
//    WSADATA wsaData;
//    int err;
//
//    /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
//    wVersionRequested = MAKEWORD(2, 2);
//
//    err = WSAStartup(wVersionRequested, &wsaData); // must free with WSACleanup
//    if (err != 0) {
//        /* Tell the user that we could not find a usable */
//        /* Winsock DLL.                                  */
//        printf("WSAStartup failed with error: %d\n", err);
//        return -1;
//    }
//    return 0;
//}

void usbredirserver_launch_task(GTask           *task,
                          gpointer         source_object G_GNUC_UNUSED,
                          gpointer         task_data,
                          GCancellable    *cancellable G_GNUC_UNUSED)
{
    gchar *cur_usb_uuid = NULL; // uuid willl be assined on veil in case of success

    // external data
    UsbRedirRunningTask *usb_task_data = (UsbRedirRunningTask *)task_data;

    // data where the task result will be placed
    UsbRedirTaskResaultData *task_res_data = calloc(1, sizeof(UsbRedirTaskResaultData)); // free in the main thread
    task_res_data->code = USB_REDIR_FINISH_FAIL;
    task_res_data->usbbus = usb_task_data->start_data.usbbus;
    task_res_data->usbaddr = usb_task_data->start_data.usbaddr;

    // app data
    UsbRedirServerData priv;
    usbredirserver_init_data(&priv);
    priv.p_running_flag = &usb_task_data->running_flag; // Передаем ссылку на task.running_flag для того чтобы
    // иметь возможность остановить задачу из главного потока
    *(priv.p_running_flag) = 1;

    if (usbredir_util_init_libusb_and_set_options(&priv.ctx, priv.verbose) == -1) {
        task_res_data->message = g_strdup(_("Failed to init libusb")); // must be freed
        g_task_return_pointer(task, task_res_data, NULL);
        free_memory_safely(&usb_task_data->start_data.ipv4_addr);
        return;
    }

    // usbredirserver_create_server_socket
    SOCKET server_fd = usbredirserver_create_server_socket(usb_task_data->start_data.ipv4_addr,
                                                           usb_task_data->start_data.port);
    if (server_fd == INVALID_SOCKET) {
        task_res_data->message = g_strdup(_("Error while creating server socket"));
        goto releasing_resources;
    }
    usbredirserver_make_socket_nonblocking(server_fd);

    // send command 'launch usb tcp client' to vdi(then vdi to veil)
    AttachUsbData *attach_usb_data = calloc(1, sizeof(AttachUsbData));
    attach_usb_data->host_address = g_strdup(usb_task_data->start_data.ipv4_addr);
    attach_usb_data->host_port = usb_task_data->start_data.port;
    attach_usb_data->p_running_flag = &usb_task_data->running_flag;
    GThread *attach_usb_func_thread = g_thread_new(NULL, (GThreadFunc)vdi_session_attach_usb, attach_usb_data);

    // wait for client connection
    g_info("%s: Wait for client", (const char *)__func__);
    while( priv.client_fd == WSAEWOULDBLOCK || priv.client_fd == INVALID_SOCKET ) {
        priv.client_fd = accept(server_fd, NULL, 0);
        if ( !(*(priv.p_running_flag)) )
            break;
        g_usleep(10);
    }

    // result of vdi_session_attach_usb
    cur_usb_uuid = g_thread_join(attach_usb_func_thread);
    if (!cur_usb_uuid) {
        task_res_data->message = g_strdup(_("Failed to add TCP USB device to remote machine"));
        goto releasing_resources;
    }

    if (priv.client_fd == INVALID_SOCKET) {
        task_res_data->message = g_strdup(_("Error while accepting client connection"));
        goto releasing_resources;
    } else {
        fprintf(stdout, "Client accepted\n");
    }

    // socket configuration
    usbredirserver_apply_keep_alive(priv.client_fd);
    usbredirserver_make_socket_nonblocking(priv.client_fd);

    // Try to find the specified usb device and open it
    libusb_device_handle *handle = usbredirserver_find_usb_device_and_open(&priv, usb_task_data->start_data.usbbus,
                                                            usb_task_data->start_data.usbaddr,
                                                            usb_task_data->start_data.usbvendor,
                                                            usb_task_data->start_data.usbproduct);

    if (!handle) { // Failed to open USB device.
        fprintf(stdout, "libusb_device_handle !handle. Close client_fd\n");
        task_res_data->message = g_strdup(_("Failed to open USB device."));
        goto releasing_resources;
    }

    // open usbredir host
    fprintf(stdout, "Before priv.host = usbredirhost_open\n");
    priv.host = usbredirhost_open(priv.ctx, handle, usbredirserver_log,
                                  usbredirserver_read, usbredirserver_write,
                                  &priv, SERVER_VERSION, priv.verbose, 0);
    fprintf(stdout, "After priv.host = usbredirhost_open\n");
    if (!priv.host) {
        task_res_data->message = g_strdup(_("usbredirhost_open failed"));
        goto releasing_resources;
    }

    // Explanation about events http://libusb.sourceforge.net/api-1.0/group__libusb__asyncio.html
    GThread *usb_events_thread = g_thread_new(NULL, (GThreadFunc)usbredirserver_event_thread_func, &priv);

    usbredirserver_run_main_loop(&priv);

    *(priv.p_running_flag) = 0;

    // release resources
    if (priv.host) {
        fprintf(stdout, "Before usbredirhost_close(host); \n");
        usbredirhost_close(priv.host);
        fprintf(stdout, "After usbredirhost_close(host); \n");
    }

    // wait until event thread finishes and close handle
    g_thread_join(usb_events_thread);

    // successsfull finish
    task_res_data->message = g_strdup(_("USB redirection finished"));
    task_res_data->code = USB_REDIR_FINISH_SUCCESS;

releasing_resources:
    free_memory_safely(&usb_task_data->start_data.ipv4_addr);

    // remove usb tcp device from qemu vm
    if (cur_usb_uuid) {
        DetachUsbData *detach_usb_data = calloc(1, sizeof(DetachUsbData));
        detach_usb_data->usb_uuid = cur_usb_uuid; // move ownership. I mean
        vdi_session_detach_usb(detach_usb_data);
    }

    // close sockets
    usbredirserver_close_socket(priv.client_fd);
    usbredirserver_close_socket(server_fd);

    // libusb_exit
    libusb_exit(priv.ctx);

    g_task_return_pointer(task, task_res_data, NULL);
}
