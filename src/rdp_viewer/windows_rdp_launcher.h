/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef WINDOWS_RDP_LAUNCHER_H
#define WINDOWS_RDP_LAUNCHER_H

#include <gtk/gtk.h>

void launch_windows_rdp_client(const gchar *usename, const gchar *password, const gchar *ip, int port,
   const gchar *domain);

#endif // WINDOWS_RDP_LAUNCHER_H