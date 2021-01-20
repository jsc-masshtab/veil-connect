#ifndef RDP_VIEWER_WINDOW_H
#define RDP_VIEWER_WINDOW_H

#include "rdp_client.h"

#include <gtk/gtk.h>

#include "remote-viewer-timed-revealer.h"

typedef struct{
    GMainLoop **loop_p;

    // gui
    GtkBuilder *builder;
    GtkWidget *rdp_viewer_window;
    GtkWidget *overlay_toolbar;
    VirtViewerTimedRevealer *revealer;
    GtkWidget *top_menu;

    gboolean is_rdp_display_being_redrawed;
    GtkWidget *rdp_display;

    // image data
    int im_origin_x;
    int im_origin_y;
    int im_width;
    int im_height;

    // monitor data
    int monitor_index;
    GdkRectangle monitor_geometry;

    guint g_timeout_id;

    ExtendedRdpContext *ex_rdp_context;

    GdkSeat *seat;
    guint grab_try_event_source_id;
    gboolean is_grab_keyboard_on_focus_in_mode;
#ifdef _WIN32
    HHOOK keyboard_hook;
#endif

    // signal handles
    gulong vm_changed_handle;
    gulong ws_cmd_received_handle;

} RdpWindowData;


RdpWindowData *rdp_viewer_window_create(ExtendedRdpContext *ex_rdp_context);
void rdp_viewer_window_destroy(RdpWindowData *rdp_window_data);
void rdp_viewer_window_set_monitor_data(RdpWindowData *rdp_window_data, GdkRectangle geometry, int monitor_index);
void rdp_viewer_window_stop(RdpWindowData *rdp_window_data, RemoteViewerState next_app_state);
#endif // RDP_VIEWER_WINDOW_H
