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
#include "rdp_keyboard.h"

#include "remote-viewer-util.h"
#include "settingsfile.h"

#include "async.h"

#define PROGRAMM_NAME "rdp_gtk_client"
#define TAG CLIENT_TAG(PROGRAMM_NAME)


static DWORD WINAPI rdp_client_thread_proc(ExtendedRdpContext *ex);
static int rdp_client_entry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints);

//// Проверка поддерживается ли параметр. По другому никак
//static gboolean check_if_param_supported(rdpSettings* settings, const gchar *param_name)
//{
//    const int arg_number = 3;
//    gchar** argv = malloc(arg_number * sizeof(gchar*));
//    argv[0] = g_strdup(PROGRAMM_NAME);
//    argv[1] = g_strdup(param_name);
//    argv[2] = NULL;
//
//    int status = freerdp_client_settings_parse_command_line(settings, arg_number - 1, argv, FALSE);
//    g_info("!!!status1: %i ", status);
//    g_free(argv[0]);
//    g_free(argv[1]);
//    free(argv);
//
//    return (status == 0) ? TRUE : FALSE;
//}

static void add_rdp_param(GArray *rdp_params_dyn_array, gchar *rdp_param)
{
    g_array_append_val(rdp_params_dyn_array, rdp_param);
}

static void rdp_client_read_str_rdp_param_from_ini_and_add(GArray *rdp_params_dyn_array,
        const gchar *ini_key, const gchar *rdp_param_name, const gchar *default_rdp_param_value)
{
    gchar *ini_param = read_str_from_ini_file("RDPSettings", ini_key);
    if (ini_param) {
        g_strstrip(ini_param);
        add_rdp_param(rdp_params_dyn_array, g_strdup_printf("%s:%s", rdp_param_name, ini_param));
        g_free(ini_param);
    } else if (default_rdp_param_value) {
        add_rdp_param(rdp_params_dyn_array, g_strdup_printf("%s:%s", rdp_param_name, default_rdp_param_value));
    }
}

static GArray * rdp_client_create_params_array(ExtendedRdpContext* ex)
{
    g_info("%s W: %i x H:%i", (const char*)__func__, ex->whole_image_width, ex->whole_image_height);
    rdpContext *context = (rdpContext *)ex;

    GArray *rdp_params_dyn_array = g_array_new(FALSE, FALSE, sizeof(gchar *));

    add_rdp_param(rdp_params_dyn_array, g_strdup(PROGRAMM_NAME));
    gchar *full_adress = ex->port != 0 ? g_strdup_printf("/v:%s:%i", ex->ip, ex->port) :
            g_strdup_printf("/v:%s", ex->ip);
    add_rdp_param(rdp_params_dyn_array, full_adress);
    add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/d:%s", ex->domain));
    add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/u:%s", ex->usename));
    add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/p:%s", ex->password));
    add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/w:%i", ex->whole_image_width));
    add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/h:%i", ex->whole_image_height));
    add_rdp_param(rdp_params_dyn_array, g_strdup("/cert-ignore"));
    add_rdp_param(rdp_params_dyn_array, g_strdup("/sound:rate:44100,channel:2"));
    add_rdp_param(rdp_params_dyn_array, g_strdup("/smartcard"));
    add_rdp_param(rdp_params_dyn_array, g_strdup("+fonts"));
    add_rdp_param(rdp_params_dyn_array, g_strdup("/relax-order-checks"));
    add_rdp_param(rdp_params_dyn_array, g_strdup("+auto-reconnect"));
    add_rdp_param(rdp_params_dyn_array, g_strdup("/auto-reconnect-max-retries:3"));
#ifdef __linux__
#elif _WIN32
    add_rdp_param(rdp_params_dyn_array, g_strdup("+glyph-cache"));
#endif

    // /gfx-h264:AVC444
    gboolean is_rdp_h264_used = read_int_from_ini_file("RDPSettings", "is_rdp_h264_used", TRUE);
    if (is_rdp_h264_used)
        rdp_client_read_str_rdp_param_from_ini_and_add(rdp_params_dyn_array, "rdp_h264_codec", "/gfx-h264",
                                                       h264_codec_to_string(get_default_h264_codec()));
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

    // remote app
    gboolean is_remote_app = read_int_from_ini_file("RDPSettings", "is_remote_app", 0);
    if (is_remote_app) {
        rdp_client_read_str_rdp_param_from_ini_and_add(rdp_params_dyn_array, "remote_app_name", "/app", NULL);
        rdp_client_read_str_rdp_param_from_ini_and_add(rdp_params_dyn_array, "remote_app_options", "/app-cmd", NULL);
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
    for (guint i = 0; i < rdp_params_dyn_array->len; ++i) {
        argv[i] = g_array_index(rdp_params_dyn_array, gchar*, i);
        g_info("%i RDP arg: %s", i, argv[i]);
    }

    int argc = rdp_params_dyn_array->len - 1;

    // set rdp params
    status = freerdp_client_settings_parse_command_line(context->settings, argc, argv, TRUE);
    g_info("!!!status2: %i ", status);
    if (status)
        ex_contect->last_rdp_error = WRONG_FREERDP_ARGUMENTS;

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

BOOL rdp_client_abort_connection(freerdp* instance)
{
    ExtendedRdpContext *ex_context = (ExtendedRdpContext*)instance->context;
    ex_context->is_stop_intentional = TRUE;
    return freerdp_abort_connect(instance);
}

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
    //ExtendedRdpContext* ex = (ExtendedRdpContext*)context;

    //g_mutex_lock(&ex->primary_buffer_mutex);
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
    //ExtendedRdpContext* ex = (ExtendedRdpContext*)context;

    if (gdi->primary->hdc->hwnd->invalid->null) {
        //g_mutex_unlock(&ex->primary_buffer_mutex);
        return TRUE;
    }

    if (gdi->primary->hdc->hwnd->ninvalid < 1) {
        //g_mutex_unlock(&ex->primary_buffer_mutex);
        return TRUE;
    }

//   g_info("STATS: %i %i %i %i \n", gdi->primary->hdc->hwnd->invalid->x,
//           gdi->primary->hdc->hwnd->invalid->y,
//           gdi->primary->hdc->hwnd->invalid->w,
//           gdi->primary->hdc->hwnd->invalid->h);


    gdi->primary->hdc->hwnd->invalid->null = TRUE;
    gdi->primary->hdc->hwnd->ninvalid = 0;

    //g_mutex_unlock(&ex->primary_buffer_mutex);
    return TRUE;
}

/* This function is called to output a System BEEP */
static BOOL rdp_play_sound(rdpContext* context G_GNUC_UNUSED, const PLAY_SOUND_UPDATE* play_sound G_GNUC_UNUSED)
{
    // beep here
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

//    settings->BitmapCacheEnabled = false;
//    settings->OffscreenSupportLevel = false;
//    settings->OffscreenCacheSize;    /* 2817 */
//    settings->OffscreenCacheEntries; /* 2818 */
//    settings->CompressionEnabled = false;

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
    /*
    if (!instance->settings->SoftwareGdi)
    {
        brush_cache_register_callbacks(instance->update);
        glyph_cache_register_callbacks(instance->update);
        bitmap_cache_register_callbacks(instance->update);
        offscreen_cache_register_callbacks(instance->update);
        palette_cache_register_callbacks(instance->update);
    }*/

    instance->update->BeginPaint = rdp_begin_paint;
    instance->update->EndPaint = rdp_end_paint;
    instance->update->PlaySound = rdp_play_sound;
    instance->update->SetKeyboardIndicators = rdp_keyboard_set_indicators;
    instance->update->SetKeyboardImeStatus = rdp_keyboard_set_ime_status;
    //instance->update->Synchronize = update_send_synchronize;

    // create image surface. Must be recreated on resize (but its not required to this date 22.10.2020)
    ExtendedRdpContext* ex = (ExtendedRdpContext*)instance->context;
    rdpGdi* gdi = ex->context.gdi;

    g_mutex_lock(&ex->primary_buffer_mutex);

    g_info("%s W: %i H: %i", (const char *)__func__, gdi->width, gdi->height);
    int stride = cairo_format_stride_for_width(cairo_format, gdi->width);
    ex->surface = cairo_image_surface_create_for_data((unsigned char*)gdi->primary_buffer,
                                                      cairo_format, gdi->width, gdi->height, stride);

    g_mutex_unlock(&ex->primary_buffer_mutex);

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
    ex_rdp_context->last_rdp_error = last_error;
    g_info("%s last_error_code: %u", (const char *)__func__, last_error);

    gdi_free(instance);

    // Close rdp windows if LOGOFF_BY_USER received or there are no errors
    if (((last_error & 0xFFFF) == ERRINFO_LOGOFF_BY_USER) ||
    (last_error == 0 && ex_rdp_context->rail_rdp_error == 0)) {
        shutdown_loop(*(ex_rdp_context->p_loop));
    }
}

static BOOL handle_window_events(freerdp* instance)
{
    rdpSettings* settings;

    if (!instance || !instance->settings)
        return FALSE;

    settings = instance->settings;

    if (!settings->AsyncInput) {
         ExtendedRdpContext *ex_context = (ExtendedRdpContext*)instance->context;

        if (ex_context->is_stop_intentional) {
            WLog_INFO(TAG, "Stopped");
            return FALSE;
        }
    }

    return TRUE;
}

/* RDP main loop.
 * Connects RDP, loops while running and handles event and dispatch, cleans up
 * after the connection ends. */
static DWORD WINAPI rdp_client_thread_proc(ExtendedRdpContext* ex)
{
    freerdp* instance = (freerdp*)ex->context.instance;
    DWORD nCount;
    DWORD status;
    HANDLE handles[64];
    DWORD exit_code = 0;

    if (!freerdp_connect(instance))
    {
        WLog_ERR(TAG, "connection failure");
        return 0;
    }

    while (!freerdp_shall_disconnect(instance))
    {
        if (freerdp_focus_required(instance))
        {
            g_info(" if (freerdp_focus_required(instance))");
            rdp_keyboard_focus_in(ex);
        }

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
            UINT32 last_error = freerdp_get_last_error(instance->context);
            if (last_error == FREERDP_ERROR_SUCCESS)
                break;

            // try to reconnect
            if (client_auto_reconnect_ex(instance, handle_window_events))
                continue;

            if (freerdp_get_last_error(instance->context) == FREERDP_ERROR_SUCCESS)
                WLog_ERR(TAG, "Failed to check FreeRDP event handles");

            break;
        }
    }

    BOOL res = freerdp_disconnect(instance);
    g_info("diss conn res: %i", res);
    return exit_code;
}


static BOOL rdp_client_global_init(void)
{
    return TRUE;
}

/* Optional global tear down */
static void rdp_client_global_uninit(void)
{
}

static int rdp_logon_error_info(freerdp* instance, UINT32 data, UINT32 type)
{
    //ExtendedRdpContext* ex;
    const char* str_data = freerdp_get_logon_error_info_data(data);
    const char* str_type = freerdp_get_logon_error_info_type(type);

    if (!instance || !instance->context)
        return -1;

    WLog_INFO(TAG, "Logon Error Info %s [%s]", str_data, str_type);

    return 1;
}

static BOOL rdp_client_new(freerdp* instance, rdpContext* context)
{
    ExtendedRdpContext* ex = (ExtendedRdpContext*)context;
    g_info("%s: ex->test_int: %i", (const char *)__func__, ex->test_int);

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

    g_mutex_init(&ex->primary_buffer_mutex);

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
    ExtendedRdpContext* ex = (ExtendedRdpContext*)context;
    g_info("%s: %i", (const char *)__func__, ex->test_int);

    return 0;
}

static int rdp_client_stop(rdpContext* context)
{
    ExtendedRdpContext* ex = (ExtendedRdpContext*)context;
    g_info("%s: %i", (const char *)__func__, ex->test_int);

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
