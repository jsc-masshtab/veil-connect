/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GTK GUI
 * Solomin a.solomin@mashtab.otg
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/utils/signal.h>

#include <freerdp/client/cmdline.h>

#include <freerdp/locale/keyboard.h>
#include <freerdp/error.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <freerdp/log.h>

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "rdp_channels.h"
#include "rdp_client.h"
#include "rdp_cursor.h"
#include "rdp_data.h"
#include "rdp_viewer_window.h"

#include "remote-viewer-util.h"
#include "settingsfile.h"

#include "async.h"

#define PROGRAMM_NAME "rdp_gtk_client"
#define TAG CLIENT_TAG(PROGRAMM_NAME)


static DWORD WINAPI rdp_client_thread_proc(ExtendedRdpContext *tf);
static int rdp_client_entry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints);

static void add_rdp_param(GArray *rdp_params_dyn_array, gchar *rdp_param)
{
    g_array_append_val(rdp_params_dyn_array, rdp_param);
}

static GArray * rdp_client_create_params_array(ExtendedRdpContext* tf)
{
    g_info("%s W: %i x H:%i", (const char*)__func__, tf->whole_image_width, tf->whole_image_height);

    GArray *rdp_params_dyn_array = g_array_new(FALSE, FALSE, sizeof(gchar *));

    add_rdp_param(rdp_params_dyn_array, g_strdup(PROGRAMM_NAME));
    gchar *full_adress = tf->port != 0 ? g_strdup_printf("/v:%s:%i", tf->ip, tf->port) :
            g_strdup_printf("/v:%s", tf->ip);
    add_rdp_param(rdp_params_dyn_array, full_adress);
    add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/d:%s", tf->domain));
    add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/u:%s", tf->usename));
    add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/p:%s", tf->password));
    add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/w:%i", tf->whole_image_width));
    add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/h:%i", tf->whole_image_height));
    add_rdp_param(rdp_params_dyn_array, g_strdup("-clipboard"));
    add_rdp_param(rdp_params_dyn_array, g_strdup("/cert-ignore"));
    add_rdp_param(rdp_params_dyn_array, g_strdup("/sound:rate:44100,channel:2"));
    add_rdp_param(rdp_params_dyn_array, g_strdup("/smartcard"));
    add_rdp_param(rdp_params_dyn_array, g_strdup("+fonts"));

#ifdef __linux__
    add_rdp_param(rdp_params_dyn_array,g_strdup("/usb:auto"));
#elif _WIN32
    add_rdp_param(rdp_params_dyn_array, g_strdup("/relax-order-checks"));
    add_rdp_param(rdp_params_dyn_array, g_strdup("+glyph-cache"));
#endif
    // /gfx-h264:AVC444
    gboolean is_rdp_h264_used = read_int_from_ini_file("RDPSettings", "is_rdp_h264_used", FALSE);
    if (is_rdp_h264_used) {
        gchar *rdp_h264_codec = read_str_from_ini_file("RDPSettings", "rdp_h264_codec");
        gchar *gfx_h264_param_str = g_strdup_printf("/gfx-h264:%s", rdp_h264_codec);
        //g_info("gfx_h264_param_str:  %s\n", gfx_h264_param_str);
        add_rdp_param(rdp_params_dyn_array, gfx_h264_param_str);
        free_memory_safely(&rdp_h264_codec);
    }
    // drives (folders)
    gchar *shared_folders_str = read_str_from_ini_file("RDPSettings", "rdp_shared_folders");

    if (shared_folders_str) {
        gchar **shared_folders_array = g_strsplit(shared_folders_str, ";", 10);

        gchar **shared_folder;
        for (shared_folder = shared_folders_array; *shared_folder; shared_folder++) {

            // Проверяем, что строка непустая и папка доступна, и добавляем драйв в опции rdp
            if( (**shared_folder != '\0') && (g_access(*shared_folder, R_OK) == 0) ) {
                // create folder name (how it will be visible on the remote machine)
                // Get rid of / and : because Freerdp dosnt like them in drive names (on Windows)...
                gchar *folder_name = replace_str(*shared_folder, "/", "_");
                gchar *folder_name_2 = replace_str(folder_name, ":", "_");

                gchar *shared_folder_param_str = g_strdup_printf("/drive:%s,%s", folder_name_2, *shared_folder);
                g_info("shared_folder_param_str %s", shared_folder_param_str);
                add_rdp_param(rdp_params_dyn_array, shared_folder_param_str); // being removed later

                g_free(folder_name);
                g_free(folder_name_2);
            }
        }

        g_strfreev(shared_folders_array);
    }

    // rdp_args     custom from ini file
    gchar *rdp_args_str = read_str_from_ini_file("RDPSettings", "rdp_args");

    if (rdp_args_str) {
        gchar **rdp_args_array = g_strsplit(rdp_args_str, ",", 100);

        gchar **rdp_arg;
        for (rdp_arg = rdp_args_array; *rdp_arg; rdp_arg++) {
            add_rdp_param(rdp_params_dyn_array, g_strdup(*rdp_arg));
        }

        g_strfreev(rdp_args_array);
    }

    // null terminating arg
    add_rdp_param(rdp_params_dyn_array, NULL);

    return rdp_params_dyn_array;
}

static void rdp_client_destroy_params_array(GArray *rdp_params_dyn_array)
{
    for (guint i = 0; i < rdp_params_dyn_array->len; ++i) {
        gchar *rdp_param = g_array_index(rdp_params_dyn_array, gchar*, i);
        free_memory_safely(&rdp_param);
    }
    g_array_free(rdp_params_dyn_array, TRUE);
}

rdpContext* rdp_client_create_context()
{
    RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
    rdpContext* context;
    rdp_client_entry(&clientEntryPoints);
    context = freerdp_client_context_new(&clientEntryPoints);
    return context;
}

void rdp_client_set_credentials(ExtendedRdpContext *ex_rdp_context,
                                const gchar *usename, const gchar *password, gchar *domain,
                                gchar *ip, int port)
{
    ex_rdp_context->usename = g_strdup(usename);
    ex_rdp_context->password = g_strdup(password);
    ex_rdp_context->domain = g_strdup(domain);
    ex_rdp_context->ip = g_strdup(ip);
    ex_rdp_context->port = port;
}

void rdp_client_set_rdp_image_size(ExtendedRdpContext *ex_rdp_context,
                                         int whole_image_width, int whole_image_height)
{
    ex_rdp_context->whole_image_width = whole_image_width;
    ex_rdp_context->whole_image_height = whole_image_height;
}

//===============================Thread client routine==================================
void* rdp_client_routine(ExtendedRdpContext *ex_contect)
{
    int status;
    rdpContext *context = (rdpContext *)ex_contect;

    if (!context)
        goto stop;

    // rdp params
    GArray *rdp_params_dyn_array = rdp_client_create_params_array(ex_contect);

    g_info("%s ex_contect->usename %s", (const char *)__func__, ex_contect->usename);
    g_info("%s ex_contect->domain %s", (const char *)__func__, ex_contect->domain);
    // /v:192.168.20.104 /u:solomin /p:5555 -clipboard /sound:rate:44100,channel:2 /cert-ignore

    gchar** argv = malloc(rdp_params_dyn_array->len * sizeof(gchar*));
    for (guint i = 0; i < rdp_params_dyn_array->len; ++i)
        argv[i] = g_array_index(rdp_params_dyn_array, gchar*, i);

    int argc = rdp_params_dyn_array->len - 1;
    //g_info("sizeof(argv): %lu, argc: %i\n", sizeof(argv), argc);

    // set rdp params
    status = freerdp_client_settings_parse_command_line(context->settings, argc, argv, FALSE);
    if (status)
        *ex_contect->last_rdp_error_p = WRONG_FREERDP_ARGUMENTS;

    status = freerdp_client_settings_command_line_status_print(context->settings, status, argc, argv);

    // clear memory
    free(argv);
    rdp_client_destroy_params_array(rdp_params_dyn_array);

    if (status)
        goto stop;

    if (freerdp_client_start(context) != 0)
        goto stop;

    ex_contect->is_running = TRUE;

    // main loop
    rdp_client_thread_proc(ex_contect);

    // stopping
    freerdp_client_stop(context);

stop:
    g_info("%s: g_mutex_unlock", (const char *)__func__);
    ex_contect->is_running = FALSE;
    return NULL;
}

//void rdp_client_adjust_im_origin_point(ExtendedRdpContext* ex_rdp_context)
//{
//    if (ex_rdp_context->surface && ex_rdp_context->rdp_display) {

//        int delta_x = gtk_widget_get_allocated_width(ex_rdp_context->rdp_display) -
//                cairo_image_surface_get_width(ex_rdp_context->surface);
//        int delta_y = gtk_widget_get_allocated_height(ex_rdp_context->rdp_display) -
//                cairo_image_surface_get_height(ex_rdp_context->surface);

//        ex_rdp_context->im_origin_x = delta_x <= 0 ? 0.0 : (int)(delta_x * 0.5);
//        ex_rdp_context->im_origin_y = delta_y <= 0 ? 0.0 : (int)(delta_y * 0.5);
//    }
//}

//static BOOL update_send_synchronize(rdpContext* context)
//{
//    //g_info("%s\n", (const char *)__func__);
//}

/* This function is called whenever a new frame starts.
 * It can be used to reset invalidated areas. */
static BOOL rdp_begin_paint(rdpContext* context)
{
    //g_info("%s\n", (const char *)__func__);
    rdpGdi* gdi = context->gdi;
    gdi->primary->hdc->hwnd->invalid->null = TRUE;

    // Lock mutex to protect buffer
    //ExtendedRdpContext* tf = (ExtendedRdpContext*)context;

    //g_mutex_lock(&tf->primary_buffer_mutex);
    return TRUE;
}

/* This function is called when the library completed composing a new
 * frame. Read out the changed areas and blit them to your output device.
 * The image buffer will have the format specified by gdi_init
 */
static BOOL rdp_end_paint(rdpContext* context)
{
    //g_info("%s\n", (const char *)__func__);

    rdpGdi* gdi = context->gdi;
    //ExtendedRdpContext* tf = (ExtendedRdpContext*)context;

    if (gdi->primary->hdc->hwnd->invalid->null) {
        //g_mutex_unlock(&tf->primary_buffer_mutex);
        return TRUE;
    }

    if (gdi->primary->hdc->hwnd->ninvalid < 1) {
        //g_mutex_unlock(&tf->primary_buffer_mutex);
        return TRUE;
    }

//   g_info("STATS: %i %i %i %i \n", gdi->primary->hdc->hwnd->invalid->x,
//           gdi->primary->hdc->hwnd->invalid->y,
//           gdi->primary->hdc->hwnd->invalid->w,
//           gdi->primary->hdc->hwnd->invalid->h);


    gdi->primary->hdc->hwnd->invalid->null = TRUE;
    gdi->primary->hdc->hwnd->ninvalid = 0;

    //g_mutex_unlock(&tf->primary_buffer_mutex);
    return TRUE;
}

/* This function is called to output a System BEEP */
static BOOL rdp_play_sound(rdpContext* context G_GNUC_UNUSED, const PLAY_SOUND_UPDATE* play_sound G_GNUC_UNUSED)
{
    /* TODO: Implement */
    return TRUE;
}

/* This function is called to update the keyboard indocator LED */
static BOOL rdp_keyboard_set_indicators(rdpContext* context G_GNUC_UNUSED, UINT16 led_flags G_GNUC_UNUSED)
{
    /* TODO: Set local keyboard indicator LED status */
    return TRUE;
}

/* This function is called to set the IME state */
static BOOL rdp_keyboard_set_ime_status(rdpContext* context, UINT16 imeId, UINT32 imeState,
                                       UINT32 imeConvMode)
{
    if (!context)
        return FALSE;

    WLog_WARN(TAG,
              "KeyboardSetImeStatus(unitId=%04" PRIx16 ", imeState=%08" PRIx32
              ", imeConvMode=%08" PRIx32 ") ignored",
              imeId, imeState, imeConvMode);
    return TRUE;
}
// XInitThreads  GDK_SYNCHRONIZE
/* Called before a connection is established.
 * Set all configuration options to support and load channels here. */
static BOOL rdp_pre_connect(freerdp* instance)
{
    g_info("%s", (const char *)__func__);
    rdpSettings* settings;
    settings = instance->settings;
    /* Optional OS identifier sent to server */
    settings->OsMajorType = OSMAJORTYPE_UNSPECIFIED;
    settings->OsMinorType = OSMINORTYPE_UNSPECIFIED;


    settings->BitmapCacheEnabled = false;
    settings->OffscreenSupportLevel = false;
//    settings->OffscreenCacheSize;    /* 2817 */
//    settings->OffscreenCacheEntries; /* 2818 */
    settings->CompressionEnabled = false;

//    settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
//    settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
//    settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
//    settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
//    settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = TRUE;

//    settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = TRUE;
//    settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = TRUE;
//    settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = TRUE;
//    settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
//    settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = TRUE;
//    settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
//    settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
////    settings->OrderSupport[NEG_MEMBLT_INDEX] = TRUE;
//    settings->OrderSupport[NEG_MEM3BLT_INDEX] = TRUE;
//    settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = TRUE;

//    settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
//    settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
//    settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
//    settings->OrderSupport[NEG_POLYGON_SC_INDEX] = TRUE;
//    settings->OrderSupport[NEG_POLYGON_CB_INDEX] = TRUE;
//    settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = TRUE;
//    settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = TRUE;

    /* settings->OrderSupport is initialized at this point.
     * Only override it if you plan to implement custom order
     * callbacks or deactiveate certain features. */
    /* Register the channel listeners.
     * They are required to set up / tear down channels if they are loaded. */
    PubSub_SubscribeChannelConnected(instance->context->pubSub, rdp_OnChannelConnectedEventHandler);
    PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
                                        rdp_OnChannelDisconnectedEventHandler);

    /* Load all required plugins / channels / libraries specified by current
     * settings. */
    if (!freerdp_client_load_addins(instance->context->channels, instance->settings))
        return FALSE;

    // its required for key event sending
    freerdp_keyboard_init(instance->context->settings->KeyboardLayout);

    /* TODO: Any code your client requires */
    return TRUE;
}

/* Called after a RDP connection was successfully established.
 * Settings might have changed during negociation of client / server feature
 * support.
 *
 * Set up local framebuffers and paing callbacks.
 * If required, register pointer callbacks to change the local mouse cursor
 * when hovering over the RDP window
 */
static BOOL rdp_post_connect(freerdp* instance)
{
    // only 2 working pairs found
    // PIXEL_FORMAT_RGB16       CAIRO_FORMAT_RGB16_565
    // PIXEL_FORMAT_BGRA32      CAIRO_FORMAT_ARGB32
    g_info("%s", (const char *)__func__);

    // get image pixel format from ini file
    gchar *rdp_pixel_format_str = read_str_from_ini_file("RDPSettings", "rdp_pixel_format");
    UINT32 freerdp_pix_format = PIXEL_FORMAT_RGB16; // default
    cairo_format_t cairo_format = CAIRO_FORMAT_RGB16_565; // default

    if (g_strcmp0(rdp_pixel_format_str, "BGRA32") == 0) {
        freerdp_pix_format = PIXEL_FORMAT_BGRA32;
        cairo_format = CAIRO_FORMAT_ARGB32;
    }

    free_memory_safely(&rdp_pixel_format_str);

    // init
    if (!gdi_init(instance, freerdp_pix_format))
        return FALSE;

    if (!rdp_register_pointer(instance->context->graphics))
        return FALSE;

    instance->update->BeginPaint = rdp_begin_paint;
    instance->update->EndPaint = rdp_end_paint;
    instance->update->PlaySound = rdp_play_sound;
    instance->update->SetKeyboardIndicators = rdp_keyboard_set_indicators;
    instance->update->SetKeyboardImeStatus = rdp_keyboard_set_ime_status;
    //instance->update->Synchronize = update_send_synchronize;

    // create image surface. TODO: must be recreated on resize
    ExtendedRdpContext* tf = (ExtendedRdpContext*)instance->context;
    rdpGdi* gdi = tf->context.gdi;

    g_mutex_lock(&tf->primary_buffer_mutex);

    g_info("%s W: %i H: %i", (const char *)__func__, gdi->width, gdi->height);
    int stride = cairo_format_stride_for_width(cairo_format, gdi->width);
    tf->surface = cairo_image_surface_create_for_data((unsigned char*)gdi->primary_buffer,
                                                      cairo_format, gdi->width, gdi->height, stride);

    // calculate point in which the image is displayed
    //rdp_client_adjust_im_origin_point(tf);

    g_mutex_unlock(&tf->primary_buffer_mutex);

    return TRUE;
}

/* This function is called whether a session ends by failure or success.
 * Clean up everything allocated by pre_connect and post_connect.
 */
static void rdp_post_disconnect(freerdp* instance)
{
    g_info("%s", (const char *)__func__);

    if (!instance)
        return;

    if (!instance->context)
        return;

    ExtendedRdpContext* ex_rdp_context = (ExtendedRdpContext*)instance->context;
    PubSub_UnsubscribeChannelConnected(instance->context->pubSub,
                                       rdp_OnChannelConnectedEventHandler);
    PubSub_UnsubscribeChannelDisconnected(instance->context->pubSub,
                                          rdp_OnChannelDisconnectedEventHandler);

    g_mutex_lock(&ex_rdp_context->primary_buffer_mutex);
    cairo_surface_destroy(ex_rdp_context->surface);
    ex_rdp_context->surface = NULL;
    g_mutex_unlock(&ex_rdp_context->primary_buffer_mutex);

    UINT32 last_error = freerdp_get_last_error(instance->context);
    *(ex_rdp_context->last_rdp_error_p) = last_error;
    g_info("%s last_error_code: %u", (const char *)__func__, last_error);

    gdi_free(instance);

    // Close rdp windows if LOGOFF_BY_USER received
    if ((last_error & 0xFFFF) == ERRINFO_LOGOFF_BY_USER) {
        g_info("HERE WE GO AGAIN");
        // to close rdp window
        if (ex_rdp_context->rdp_windows_array->len) {
            RdpWindowData *rdp_window_data = g_array_index(ex_rdp_context->rdp_windows_array, RdpWindowData *, 0);
            rdp_viewer_window_cancel(rdp_window_data);
        }
    }
}

/* RDP main loop.
 * Connects RDP, loops while running and handles event and dispatch, cleans up
 * after the connection ends. */
static DWORD WINAPI rdp_client_thread_proc(ExtendedRdpContext* tf)
{
    freerdp* instance = (freerdp*)tf->context.instance;
    DWORD nCount;
    DWORD status;
    HANDLE handles[64];

    if (!freerdp_connect(instance))
    {
        WLog_ERR(TAG, "connection failure");
        return 0;
    }

    while (!freerdp_shall_disconnect(instance))
    {
        //g_info("In RDP while\n");
        nCount = freerdp_get_event_handles(instance->context, &handles[0], 64);

        if (nCount == 0)
        {
            WLog_ERR(TAG, "%s: freerdp_get_event_handles failed", __FUNCTION__);
            break;
        }

        status = WaitForMultipleObjects(nCount, handles, FALSE, 100);

        if (status == WAIT_FAILED)
        {
            WLog_ERR(TAG, "%s: WaitForMultipleObjects failed with %" PRIu32 "", __FUNCTION__,
                     status);
            break;
        }

        if (!freerdp_check_event_handles(instance->context))
        {
            if (freerdp_get_last_error(instance->context) == FREERDP_ERROR_SUCCESS)
                WLog_ERR(TAG, "Failed to check FreeRDP event handles");

            break;
        }
    }

    BOOL res = freerdp_disconnect(instance);
    g_info("diss conn res: %i", res);
    return 0;
}


/* Optional global initializer.
 * Here we just register a signal handler to print out stack traces
 * if available. */
static BOOL rdp_client_global_init(void)
{
    if (freerdp_handle_signals() != 0)
        return FALSE;

    return TRUE;
}

/* Optional global tear down */
static void rdp_client_global_uninit(void)
{
}

static int rdp_logon_error_info(freerdp* instance, UINT32 data, UINT32 type)
{
    //ExtendedRdpContext* tf;
    const char* str_data = freerdp_get_logon_error_info_data(data);
    const char* str_type = freerdp_get_logon_error_info_type(type);

    if (!instance || !instance->context)
        return -1;

    //tf = (ExtendedRdpContext*)instance->context;
    WLog_INFO(TAG, "Logon Error Info %s [%s]", str_data, str_type);

    return 1;
}

static BOOL rdp_client_new(freerdp* instance, rdpContext* context)
{
    ExtendedRdpContext* tf = (ExtendedRdpContext*)context;
    g_info("%s: tf->test_int: %i", (const char *)__func__, tf->test_int);

    if (!instance || !context)
        return FALSE;

    instance->PreConnect = rdp_pre_connect;
    instance->PostConnect = rdp_post_connect;
    instance->PostDisconnect = rdp_post_disconnect;
    instance->Authenticate = client_cli_authenticate;
    instance->GatewayAuthenticate = client_cli_gw_authenticate;
    //instance->VerifyCertificateEx = client_cli_verify_certificate_ex;
    //instance->VerifyChangedCertificateEx = client_cli_verify_changed_certificate_ex;
    instance->VerifyCertificate = client_cli_verify_certificate;
    instance->VerifyChangedCertificate = client_cli_verify_changed_certificate;


    instance->LogonErrorInfo = rdp_logon_error_info;

    g_mutex_init(&tf->primary_buffer_mutex);

    return TRUE;
}

static void rdp_client_free(freerdp* instance G_GNUC_UNUSED, rdpContext* context)
{
    g_info("%s", (const char *)__func__);

    if (!context)
        return;

    // some clean.
    ExtendedRdpContext* ex_rdp_context = (ExtendedRdpContext*)context;

    free_memory_safely(&ex_rdp_context->usename);
    free_memory_safely(&ex_rdp_context->password);
    free_memory_safely(&ex_rdp_context->domain);
    free_memory_safely(&ex_rdp_context->ip);

    wair_for_mutex_and_clear(&ex_rdp_context->primary_buffer_mutex);
}

static int rdp_client_start(rdpContext* context)
{
    /* TODO: Start client related stuff */
    ExtendedRdpContext* tf = (ExtendedRdpContext*)context;
    g_info("%s: %i", (const char *)__func__, tf->test_int);

    return 0;
}

static int rdp_client_stop(rdpContext* context)
{
    /* TODO: Stop client related stuff */
    ExtendedRdpContext* tf = (ExtendedRdpContext*)context;
    g_info("%s: %i", (const char *)__func__, tf->test_int);

    return 0;
}

static int rdp_client_entry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
    ZeroMemory(pEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
    pEntryPoints->Version = RDP_CLIENT_INTERFACE_VERSION;
    pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
    pEntryPoints->GlobalInit = rdp_client_global_init;
    pEntryPoints->GlobalUninit = rdp_client_global_uninit;
    pEntryPoints->ContextSize = sizeof(ExtendedRdpContext);
    pEntryPoints->ClientNew = rdp_client_new;
    pEntryPoints->ClientFree = rdp_client_free;
    pEntryPoints->ClientStart = rdp_client_start;
    pEntryPoints->ClientStop = rdp_client_stop;
    return 0;
}
