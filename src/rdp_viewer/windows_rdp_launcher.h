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

#include "rdp_settings.h"

void launch_windows_rdp_client(const VeilRdpSettings *p_rdp_settings);

#endif // WINDOWS_RDP_LAUNCHER_H