/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef RDP_VIEWER_WINDOW_H
#define RDP_VIEWER_WINDOW_H

#include "rdp_client.h"
#include "rdp_display.h"

#include <gtk/gtk.h>

#include "remote-viewer-timed-revealer.h"
#include "conn_info_dialog.h"

typedef struct{
    GMainLoop **loop_p;

    // gui
    GtkBuilder *builder;
    GtkWidget *rdp_viewer_window;
    GtkWidget *overlay_toolbar;
    VirtViewerTimedRevealer *revealer;
    GtkWidget *top_menu;

    GtkWidget *menu_send_shortcut;

    RdpDisplay *rdp_display;

    ConnInfoDialog *conn_info_dialog;

    // monitor data
    int monitor_index; // порядковый номер монитора
    int monitor_number; // номер монитора в gdk glib

    ExtendedRdpContext *ex_rdp_context;

    GdkSeat *seat;
    guint grab_try_event_source_id;
    gboolean is_grab_keyboard_on_focus_in_mode;
    gboolean window_was_mapped;
#ifdef _WIN32
    HHOOK keyboard_hook;
#endif

    // signal handles
    gulong vm_changed_handle;
    gulong ws_cmd_received_handle;
    gulong auth_fail_detected_handle;
    gulong usb_redir_finished_handle;

} RdpWindowData;


RdpWindowData *rdp_viewer_window_create(ExtendedRdpContext *ex_rdp_context,
        int index,  int monitor_num, GdkRectangle geometry);
void rdp_viewer_window_destroy(RdpWindowData *rdp_window_data);
void rdp_viewer_window_stop(RdpWindowData *rdp_window_data, RemoteViewerState next_app_state,
        gboolean exit_if_cant_abort);

void rdp_viewer_window_send_key_shortcut(rdpContext* context, int key_shortcut_index);
#endif // RDP_VIEWER_WINDOW_H
