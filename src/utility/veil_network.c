//
// Created by solomin on 19.01.2022.
//

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <iphlpapi.h>
#elif __linux__
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#endif

#include "veil_network.h"


// sed -n 2p /proc/net/route |  awk '{print $1}'
gchar *network_get_primary_mac_address()
{
    gchar *mac_addr = NULL;

#ifdef _WIN32
    IP_ADAPTER_INFO  *pAdapterInfo;
    ULONG            ulOutBufLen;
    DWORD            dwRetVal;

    pAdapterInfo = (IP_ADAPTER_INFO *) malloc( sizeof(IP_ADAPTER_INFO) );
    ulOutBufLen = sizeof(IP_ADAPTER_INFO);

    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) != ERROR_SUCCESS) {
        free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO *) malloc( ulOutBufLen );
    }

    if ((dwRetVal = GetAdaptersInfo( pAdapterInfo, &ulOutBufLen)) != ERROR_SUCCESS) {
        g_info("GetAdaptersInfo call failed with %i", (int)dwRetVal);
    } else {
        mac_addr = g_strdup_printf("%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
                                   pAdapterInfo->Address[0], pAdapterInfo->Address[1],
                                   pAdapterInfo->Address[2], pAdapterInfo->Address[3],
                                   pAdapterInfo->Address[4], pAdapterInfo->Address[5]);
    }

    free(pAdapterInfo);
    return mac_addr;

#elif __linux__
    // Get default interface
    g_autofree gchar *standard_output = NULL;
    g_autofree gchar *standard_error = NULL;
    gint exit_status = 0;

    g_autofree gchar *command_line = NULL;
    command_line = g_strdup("sed -n 2p /proc/net/route");

    GError *error = NULL;
    gboolean cmd_success = g_spawn_command_line_sync(command_line,
                                                     &standard_output,
                                                     &standard_error,
                                                     &exit_status,
                                                     &error);

    if (!cmd_success) {
        g_warning("%s: Failed to spawn process. %s", (const char *)__func__, error ? error->message : "");
        g_clear_error(&error);
        return NULL;
    }

    g_autofree gchar *iface = NULL;
    GStrv tokens_list = g_strsplit(standard_output, "\t", 10);
    if (g_strv_length(tokens_list) >= 1) {
        iface = g_strdup(tokens_list[0]);
    }
    g_strfreev(tokens_list);

    if (iface == NULL) {
        g_warning("%s: iface == NULL.", (const char *)__func__);
        return NULL;
    }

    // Get mac address
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        g_warning("%s: fd == -1.", (const char *)__func__);
        return NULL;
    }

    struct ifreq ifr;
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name , iface , IFNAMSIZ - 1);

    ioctl(fd, SIOCGIFHWADDR, &ifr);

    close(fd);

    unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;

    //Form mac address representation
    mac_addr = g_strdup_printf("%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif

    return mac_addr;
}
