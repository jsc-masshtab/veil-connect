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
#include "remote-viewer-util.h"
#include "settings_data.h"
#include "messenger.h"

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
    gboolean is_connecting;
    gboolean is_disconnecting;
    gboolean is_connected_last_time; // флаг было ли успешное соединение на последней попытке
    gchar *signal_upon_job_finish;

    // RDP settings
    VeilRdpSettings *p_rdp_settings; // указатель на данные. Не владеет этими данными

    UpdateCursorCallback update_cursor_callback; // callback for updating cursor in the main thread
    guint cursor_update_timeout_id;

    // rail
    RailClientContext* rail;

    // errors
    UINT32 last_rdp_error; // main freerdp error
    UINT32 rail_rdp_error; // remote app related error

    guint display_update_timeout_id;
    GMutex invalid_region_mutex;
    gboolean invalid_region_has_data;
    GdkRectangle invalid_region;

    ConnectSettingsData *p_conn_data;
    VeilMessenger *p_veil_messenger;

} ExtendedRdpContext;


void rdp_client_demand_image_update(ExtendedRdpContext* ex_context, int x, int y, int width, int height);

ExtendedRdpContext* create_rdp_context(VeilRdpSettings *p_rdp_settings,
        UpdateCursorCallback update_cursor_callback, GSourceFunc update_images_func);
void destroy_rdp_context(ExtendedRdpContext* ex_rdp_context);

void rdp_client_set_rdp_image_size(ExtendedRdpContext *ex_rdp_context,
                                         int whole_image_width, int whole_image_height);

BOOL rdp_client_abort_connection(freerdp* instance);

GArray *rdp_client_create_params_array(ExtendedRdpContext* ex);
void rdp_client_destroy_params_array(GArray *rdp_params_dyn_array);

#endif /* FREERDP_CLIENT_SAMPLE_H */
