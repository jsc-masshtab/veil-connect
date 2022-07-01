/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_GOBJECT_GUI_H
#define VEIL_CONNECT_GOBJECT_GUI_H

#include <gtk/gtk.h>
#include <gio/gio.h>


typedef GtkWidget GObjectGui;

GObjectGui *gobject_gui_generate_gui(GObject *gobject);
void gobject_gui_fill_gobject_from_gui(GObject *gobject, GObjectGui *gui);

#endif //VEIL_CONNECT_GOBJECT_GUI_H
