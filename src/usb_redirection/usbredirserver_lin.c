
/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 *
 * Refactored version version of application https://github.com/freedesktop/spice-usbredir/tree/master/usbredirserver
*/

//#define  FD_SETSIZE 256

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <glib.h>
#include <gtk/gtk.h>

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


void usbredirserver_init_data(UsbRedirServerData *priv) {

    priv->verbose = usbredirparser_info;
    priv->client_fd = -1;
    priv->p_running_flag = NULL;
    priv->ctx = NULL;
    priv->host = NULL;
}

static void usbredirserver_log(void *priv, int level, const char *msg)
{
    UsbRedirServerData *usb_redir_server_data = (UsbRedirServerData*)priv;
    if (level <= usb_redir_server_data->verbose) {
        fprintf(stderr, "usbredirserver: %s\n", msg);
    }
}

static void usbredirserver_close_socket(int *sock)
{
    if (*sock != -1) {
        close(*sock);
        *sock = -1;
    }
}

static int usbredirserver_read(void *priv, uint8_t *data, int count)
{
    //fprintf(stdout, "%s\n", (const char *)__func__);
    UsbRedirServerData *usb_redir_server_data = (UsbRedirServerData*)priv;
    int r = read(usb_redir_server_data->client_fd, data, count);
    if (r < 0) {
        if (errno == EAGAIN)
            return 0;
        return -1;
    }
    if (r == 0) { /* Client disconnected */
        fprintf(stdout, "%s: Disconnecting client_fd\n", (const char *)__func__);
        usbredirserver_close_socket(&usb_redir_server_data->client_fd);
    }
    return r;
}

static int usbredirserver_write(void *priv, uint8_t *data, int count)
{
    //fprintf(stdout, "%s\n", (const char *)__func__);
    UsbRedirServerData *usb_redir_server_data = (UsbRedirServerData*)priv;
    int r = write(usb_redir_server_data->client_fd, data, count);
    if (r < 0) {
        if (errno == EAGAIN)
            return 0;
        if (errno == EPIPE) { /* Client disconnected */
            fprintf(stdout, "%s: Disconnecting client_fd\n", (const char *)__func__);
            usbredirserver_close_socket(&usb_redir_server_data->client_fd);
            return 0;
        }
        return -1;
    }
    return r;
}

static void usbredirserver_run_main_loop(UsbRedirServerData *priv)
{
    const struct libusb_pollfd **pollfds = NULL;
    fd_set readfds, writefds;
    int i, n, nfds;
    struct timeval timeout, *timeout_p;

    while (*(priv->p_running_flag) && priv->client_fd != -1) {
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);

        FD_SET(priv->client_fd, &readfds);
        if (usbredirhost_has_data_to_write(priv->host)) {
            FD_SET(priv->client_fd, &writefds);
        }
        nfds = priv->client_fd + 1;

        free(pollfds);
        pollfds = libusb_get_pollfds(priv->ctx);
        for (i = 0; pollfds && pollfds[i]; i++) {
            if (pollfds[i]->events & POLLIN) {
                FD_SET(pollfds[i]->fd, &readfds);
            }
            if (pollfds[i]->events & POLLOUT) {
                FD_SET(pollfds[i]->fd, &writefds);
            }
            if (pollfds[i]->fd >= nfds)
                nfds = pollfds[i]->fd + 1;
        }

        if (libusb_get_next_timeout(priv->ctx, &timeout) == 1) { // refactor later
            timeout_p = &timeout;
        } else {
            timeout.tv_sec = 3;
            timeout_p = &timeout;
        }
        n = select(nfds, &readfds, &writefds, NULL, timeout_p);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("select");
            break;
        }
        memset(&timeout, 0, sizeof(timeout));
        if (n == 0) {
            libusb_handle_events_timeout(priv->ctx, &timeout);
            continue;
        }

        if (FD_ISSET(priv->client_fd, &readfds)) {
            if (usbredirhost_read_guest_data(priv->host)) {
                break;
            }
        }
        /* usbredirhost_read_guest_data may have detected client disconnect */
        if (priv->client_fd == -1)
            break;

        if (FD_ISSET(priv->client_fd, &writefds)) {
            if (usbredirhost_write_guest_data(priv->host)) {
                break;
            }
        }

        for (i = 0; pollfds && pollfds[i]; i++) {
            if (FD_ISSET(pollfds[i]->fd, &readfds) ||
                FD_ISSET(pollfds[i]->fd, &writefds)) {
                libusb_handle_events_timeout(priv->ctx, &timeout);
                break;
            }
        }
    }

    usbredirserver_close_socket(&priv->client_fd);
    free(pollfds);
}

static int usbredirserver_create_server_socket(const char *ipv4_addr, int port)
{
    // Create server socket
    int server_fd = -1;
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

    if (server_fd == -1) {
        perror("Error creating ip socket");
        return -1;
    }

    int on = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)(&on), sizeof(on))) {
        perror("Error setsockopt(SO_REUSEADDR) failed");
        close(server_fd);
        return -1;
    }

    memset(&serveraddr, 0, sizeof(serveraddr));

    // set address
    if (ipv4_addr) {
        serveraddr.v4.sin_family = AF_INET;
        serveraddr.v4.sin_port   = htons(port);
        if ((inet_pton(AF_INET, ipv4_addr, &serveraddr.v4.sin_addr)) != 1) {
            perror("Error convert ipv4 address");
            close(server_fd);
            return -1;
        }
    } else {
        serveraddr.v6.sin6_family = AF_INET6;
        serveraddr.v6.sin6_port   = htons(port);
        if (ipv6_addr) {
            if ((inet_pton(AF_INET6, ipv6_addr, &serveraddr.v6.sin6_addr)) != 1) {
                perror("Error convert ipv6 address");
                close(server_fd);
                return -1;
            }
        } else {
            serveraddr.v6.sin6_addr   = in6addr_any;
        }
    }

    // Bind address to server socket
    if (bind(server_fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr))) {
        perror("Error bind");
        close(server_fd);
        return -1;//INVALID_SOCKET;
    }

    // Listen for client
    if (listen(server_fd, 1)) {
        perror("Error listening");
        close(server_fd);
        return -1;//INVALID_SOCKET;
    } else {
        fprintf(stdout, "LIstening for client\n");
    }

    return server_fd;
}

void usbredirserver_apply_keep_alive(int sock)
{
    int keepalive  = 15;
    if (keepalive > 0) {
        int optval = 1;
        socklen_t optlen = sizeof(optval);
        if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) == -1) {
            if (errno != ENOTSUP) {
                perror("setsockopt SO_KEEPALIVE error.");
                return;
            }
        }
        optval = keepalive;	/* set default TCP_KEEPIDLE time from cmdline */
        if (setsockopt(sock, SOL_TCP, TCP_KEEPIDLE, &optval, optlen) == -1) {
            if (errno != ENOTSUP) {
                perror("setsockopt TCP_KEEPIDLE error.");
                return;
            }
        }
        optval = 10;	/* set default TCP_KEEPINTVL time as 10s */
        if (setsockopt(sock, SOL_TCP, TCP_KEEPINTVL, &optval, optlen) == -1) {
            if (errno != ENOTSUP) {
                perror("setsockopt TCP_KEEPINTVL error.");
                return;
            }
        }
        optval = 3;	/* set default TCP_KEEPCNT as 3 */
        if (setsockopt(sock, SOL_TCP, TCP_KEEPCNT, &optval, optlen) == -1) {
            if (errno != ENOTSUP) {
                perror("setsockopt TCP_KEEPCNT error.");
                return;
            }
        }
    }
}

void usbredirserver_make_socket_nonblocking(int sock)
{
    int flags = fcntl(sock, F_GETFL);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return;
    }
    flags = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    if (flags == -1) {
        perror("fcntl F_SETFL O_NONBLOCK");
        return;
    }
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

    // usbredirserver_init_libusb_and_set_options
    if (usbredir_util_init_libusb_and_set_options(&priv.ctx, priv.verbose) == -1) {
        task_res_data->message = g_strdup("Could not init libusb"); // must be freed
        g_task_return_pointer(task, task_res_data, NULL);
        free_memory_safely(&usb_task_data->start_data.ipv4_addr);
        return;
    }

    // usbredirserver_create_server_socket
    int server_fd = usbredirserver_create_server_socket(usb_task_data->start_data.ipv4_addr,
                                                           usb_task_data->start_data.port);
    if (server_fd == -1) {
        task_res_data->message = g_strdup("Error while creating server socket");
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
    while( priv.client_fd == -1 ) {
        priv.client_fd = accept(server_fd, NULL, 0);
        if ( !(*(priv.p_running_flag)) )
            break;
        g_usleep(10);
    }

    // result of vdi_session_attach_usb
    cur_usb_uuid = g_thread_join(attach_usb_func_thread);
    if (!cur_usb_uuid) { // Cant add tcp usb device"
        task_res_data->message = g_strdup("Не удалось добавить TCP USB устройство к виртуальной машине");
        goto releasing_resources;
    }

    if (priv.client_fd == -1) {
        task_res_data->message = g_strdup("Error while accepting client connection"); // must be freed
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

    if (!handle) { // Failed to open USB device. Possibly permissions problem.
        fprintf(stdout, "libusb_device_handle !handle. Close client_fd\n");
        task_res_data->message = g_strdup_printf("Не удалось открыть USB устройство. Нет прав?\n"
                                                 "Попробуйте выполнить chmod 666 /dev/bus/usb/%03i/%03i",
                                                 task_res_data->usbbus, task_res_data->usbaddr);
        goto releasing_resources;
    }

    // open usbredir host
    fprintf(stdout, "Before priv.host = usbredirhost_open\n");
    priv.host = usbredirhost_open(priv.ctx, handle, usbredirserver_log,
                                  usbredirserver_read, usbredirserver_write,
                                  &priv, SERVER_VERSION, priv.verbose, 0);
    fprintf(stdout, "After priv.host = usbredirhost_open\n");
    if (!priv.host) {
        task_res_data->message = g_strdup("usbredirhost_open failed"); // must be freed
        goto releasing_resources;
    }

    usbredirserver_run_main_loop(&priv);

    *(priv.p_running_flag) = 0;

    // release resources
    if (priv.host) {
        fprintf(stdout, "Before usbredirhost_close(host); \n");
        usbredirhost_close(priv.host);
        fprintf(stdout, "After usbredirhost_close(host); \n");
    }

    // successsfull finish
    task_res_data->message = g_strdup("Перенаправление USB завершилось");
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
    usbredirserver_close_socket(&priv.client_fd);
    usbredirserver_close_socket(&server_fd);

    // libusb_exit
    libusb_exit(priv.ctx);

    g_task_return_pointer(task, task_res_data, NULL);
}