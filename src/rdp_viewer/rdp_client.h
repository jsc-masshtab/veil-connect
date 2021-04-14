/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef FREERDP_CLIENT_SAMPLE_H
#define FREERDP_CLIENT_SAMPLE_H
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <freerdp/freerdp.h>
#include <freerdp/client/rdpei.h>
#include <freerdp/client/tsmf.h>
#include <freerdp/client/rail.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/client/encomsp.h>

#include <gtk/gtk.h>

#include "rdp_cursor.h"
#include "remote-viewer.h"
#include "remote-viewer-util.h"

typedef gboolean (*UpdateImageCallback) (rdpContext* context);
typedef gboolean (*UpdateCursorCallback) (rdpContext* context);

//#define MAX_DISPLAY_NUMBER 3

typedef struct {
    rdpContext context;

    /* Channels */
    RdpeiClientContext *rdpei;
    RdpgfxClientContext *gfx;
    EncomspClientContext *encomsp;

    GArray *rdp_windows_array;

    GdkPixbuf *frame_pix_buf; //frame image data
    GMutex primary_buffer_mutex; // mutex for protecting primary_buffer

    cairo_surface_t* surface; // image surface
    double im_origin_x; // origin point of image
    double im_origin_y; // origin point of image
    int whole_image_width;
    int whole_image_height;

    GdkCursor *gdk_cursor; // cursor
    GMutex cursor_mutex; // mutex for protecting gdk_cursor

    int test_int;

    gboolean is_running; // is rdp routine running
    gboolean is_abort_demanded;
    gboolean is_reconnecting; // is reconnect going on
    gboolean is_connecting;

    // credentials
    gchar *usename;
    gchar *password;
    gchar *domain;
    gchar *ip;
    int port;

    // UpdateImageCallback update_image_callback; // callback for updating image in the main thread
    UpdateCursorCallback update_cursor_callback; // callback for updating cursor in the main thread

    // rail
    RailClientContext* rail;
    GArray *app_windows_array;

    // errors
    UINT32 last_rdp_error; // main freerdp error
    UINT32 rail_rdp_error; // remote app related error

    GMainLoop **p_loop;
    RemoteViewerState *next_app_state_p;

    GThread *rdp_client_routine_thread;

    RemoteViewer *app;

} ExtendedRdpContext;


void rdp_client_set_credentials(ExtendedRdpContext *ex_rdp_context,
                                     const gchar *usename, const gchar *password, gchar *domain, gchar *ip, int port);
void rdp_client_set_rdp_image_size(ExtendedRdpContext *ex_rdp_context,
                                         int whole_image_width, int whole_image_height);

void* rdp_client_routine(ExtendedRdpContext *ex_contect);

BOOL rdp_client_abort_connection(freerdp* instance);

void rdp_client_start_routine_thread(ExtendedRdpContext *ex_rdp_context);
void rdp_client_stop_routine_thread(ExtendedRdpContext *ex_rdp_context);

int rdp_client_entry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints);


#endif /* FREERDP_CLIENT_SAMPLE_H */
