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

#include "settings_data.h"

void launch_native_rdp_client(GtkWindow *parent, const VeilRdpSettings *p_rdp_settings);

#endif // WINDOWS_RDP_LAUNCHER_H