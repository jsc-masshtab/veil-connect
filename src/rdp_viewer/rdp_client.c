/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>


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

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "rdp_channels.h"
#include "rdp_client.h"
#include "rdp_cursor.h"
#include "rdp_util.h"
#include "rdp_viewer_window.h"
#include "rdp_keyboard.h"
#include "rdp_rail.h"

#include "remote-viewer-util.h"
#include "settingsfile.h"

#include "async.h"
#include "vdi_session.h"
#include "vdi_event.h"

#define PROGRAMM_NAME "rdp_gtk_client"
#define TAG CLIENT_TAG(PROGRAMM_NAME)
#define MAX_CONN_TRY_NUMBER 2 // Число попыток подключиться перед завершением сессии


static int rdp_client_entry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints);


static void rdp_client_demand_image_update(ExtendedRdpContext* ex_context, int x, int y, int width, int height)
{
    // Формируем область которую нужно перерисовать в главном потоке
    g_mutex_lock(&ex_context->invalid_region_mutex);

    if (!ex_context->invalid_region_has_data) {
        ex_context->invalid_region.x = x;
        ex_context->invalid_region.y = y;
        ex_context->invalid_region.width = width;
        ex_context->invalid_region.height = height;

        ex_context->invalid_region_has_data = TRUE;
    } else {
        // Сдвиг влево
        int left_x_delta = ex_context->invalid_region.x - x;
        if (left_x_delta > 0) {
            ex_context->invalid_region.x -= left_x_delta;
            ex_context->invalid_region.width += left_x_delta;
        }
        // Сдвиг наверх
        int up_y_delta = ex_context->invalid_region.y - y;
        if (up_y_delta > 0) {
            ex_context->invalid_region.y -= up_y_delta;
            ex_context->invalid_region.height += up_y_delta;
        }
        // Сдвиг вправо
        int right_x_delta = (x + width) - (ex_context->invalid_region.x + ex_context->invalid_region.width);
        if ( right_x_delta > 0 )
            ex_context->invalid_region.width += right_x_delta;
        // Сдвиг вниз
        int bottom_y_delta = (y + height) - (ex_context->invalid_region.y + ex_context->invalid_region.height);
        if ( bottom_y_delta > 0 )
            ex_context->invalid_region.height += bottom_y_delta;
    }

    g_mutex_unlock(&ex_context->invalid_region_mutex);
}

static void add_rdp_param(GArray *rdp_params_dyn_array, gchar *rdp_param)
{
    if (rdp_param == NULL)
        return;

    g_array_append_val(rdp_params_dyn_array, rdp_param);
}

static GArray *rdp_client_create_params_array(ExtendedRdpContext* ex)
{
    g_info("%s W: %i x H:%i", (const char*)__func__, ex->whole_image_width, ex->whole_image_height);

    GArray *rdp_params_dyn_array = g_array_new(FALSE, FALSE, sizeof(gchar *));

    add_rdp_param(rdp_params_dyn_array, g_strdup(PROGRAMM_NAME));
    gchar *full_adress = ex->p_rdp_settings->port != 0 ?
            g_strdup_printf("/v:%s:%i", ex->p_rdp_settings->ip, ex->p_rdp_settings->port) :
            g_strdup_printf("/v:%s", ex->p_rdp_settings->ip);
    add_rdp_param(rdp_params_dyn_array, full_adress);

    // Проверить находится ли имя в формате name@domain и извлечь данные
    extract_name_and_domain(ex->p_rdp_settings->user_name, &ex->p_rdp_settings->user_name, &ex->p_rdp_settings->domain);

    add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/u:%s", ex->p_rdp_settings->user_name));

    if (strlen_safely(ex->p_rdp_settings->domain))
        add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/d:%s", ex->p_rdp_settings->domain));
    add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/p:%s", ex->p_rdp_settings->password));
    add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/w:%i", ex->whole_image_width));
    add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/h:%i", ex->whole_image_height));
    add_rdp_param(rdp_params_dyn_array, g_strdup("/cert-ignore"));
    add_rdp_param(rdp_params_dyn_array, g_strdup("/sound:rate:44100,channel:2"));
    add_rdp_param(rdp_params_dyn_array, g_strdup("/relax-order-checks"));
#ifdef __linux__
#elif _WIN32
    //add_rdp_param(rdp_params_dyn_array, g_strdup("+glyph-cache"));
#endif
    if (ex->p_rdp_settings->redirectsmartcards)
        add_rdp_param(rdp_params_dyn_array, g_strdup("/smartcard"));
    if (ex->p_rdp_settings->redirectprinters)
        add_rdp_param(rdp_params_dyn_array, g_strdup("/printer"));
    if (ex->p_rdp_settings->redirect_microphone)
        add_rdp_param(rdp_params_dyn_array, g_strdup("/microphone"));

    if (ex->p_rdp_settings->alternate_shell)
        add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/shell:%s", ex->p_rdp_settings->alternate_shell));

    if (ex->p_rdp_settings->allow_desktop_composition)
        add_rdp_param(rdp_params_dyn_array, g_strdup("+aero"));

    // /gfx-h264:AVC444
    gboolean is_rdp_vid_comp_used = ex->p_rdp_settings->is_rdp_vid_comp_used;
    if (is_rdp_vid_comp_used) {
        // Если не указан то по умолчанию RFX
        const gchar *codec = ex->p_rdp_settings->rdp_vid_comp_codec;
        if (codec == NULL || g_strcmp0(codec, "RFX") == 0)
            add_rdp_param(rdp_params_dyn_array, g_strdup("/gfx:RFX"));
        else if (g_strcmp0(codec, "AVC420") == 0 || g_strcmp0(codec, "AVC444") == 0)
            add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/gfx-h264:%s", codec));
    }

    // drives (folders)
    const gchar *shared_folders_str = ex->p_rdp_settings->shared_folders_str;

    if (shared_folders_str && vdi_session_is_folders_redir_permitted()) {
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

    // remote app. Сначала смотрим есть ли в переданных настройках, иначе - есть ли в ini
    if(ex->p_rdp_settings->is_remote_app) {
        add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/app:%s", ex->p_rdp_settings->remote_app_program));
        if (ex->p_rdp_settings->remote_app_options)
            add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/app-cmd:%s",
                                                                ex->p_rdp_settings->remote_app_options));
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

    // network type
    if (ex->p_rdp_settings->is_rdp_network_assigned && ex->p_rdp_settings->rdp_network_type)
        add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/network:%s", ex->p_rdp_settings->rdp_network_type));

    // security protocol
    if (ex->p_rdp_settings->is_sec_protocol_assigned && ex->p_rdp_settings->sec_protocol_type)
        add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/sec:%s", ex->p_rdp_settings->sec_protocol_type));

    // additional graphics settings
    if (ex->p_rdp_settings->disable_rdp_decorations)
        add_rdp_param(rdp_params_dyn_array, g_strdup("-decorations"));
    gchar *fonts = ex->p_rdp_settings->disable_rdp_fonts ? g_strdup("-fonts") : g_strdup("+fonts");
    add_rdp_param(rdp_params_dyn_array, fonts);
    if (ex->p_rdp_settings->disable_rdp_themes)
        add_rdp_param(rdp_params_dyn_array, g_strdup("-themes"));

    // USB for RemoteFX
    if (vdi_session_is_usb_redir_permitted() && strlen_safely(ex->p_rdp_settings->usb_devices))
        add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/usb:addr:%s", ex->p_rdp_settings->usb_devices));

    // RD Gateway
    if (ex->p_rdp_settings->use_gateway)
        add_rdp_param(rdp_params_dyn_array, g_strdup_printf("/g:%s", ex->p_rdp_settings->gateway_address));

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

ExtendedRdpContext* create_rdp_context(VeilRdpSettings *p_rdp_settings, UpdateCursorCallback update_cursor_callback,
                                              GSourceFunc update_images_func)
{
    RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
    rdp_client_entry(&clientEntryPoints);
    rdpContext* context = freerdp_client_context_new(&clientEntryPoints);

    ExtendedRdpContext* ex_rdp_context = (ExtendedRdpContext*)context;
    ex_rdp_context->p_rdp_settings = p_rdp_settings;
    ex_rdp_context->is_running = FALSE;
    ex_rdp_context->update_cursor_callback = update_cursor_callback;
    ex_rdp_context->test_int = 777; // temp
    g_mutex_init(&ex_rdp_context->cursor_mutex);

    // setup display updating queue. В очередь из потока поступают сообщения о необходимости
    // обновить изображение
    //ex_rdp_context->display_update_queue = g_async_queue_new();
    g_mutex_init(&ex_rdp_context->invalid_region_mutex);
    // get desired fps from ini file
    UINT32 rdp_fps = CLAMP(ex_rdp_context->p_rdp_settings->rdp_fps, 1, 60);
    guint redraw_timeout = 1000 / rdp_fps;
    ex_rdp_context->display_update_timeout_id = g_timeout_add(redraw_timeout, update_images_func, ex_rdp_context);

    return ex_rdp_context;
}

void destroy_rdp_context(ExtendedRdpContext* ex_rdp_context)
{
    if (ex_rdp_context) {
        // stopping RDP routine
        rdp_client_stop_routine_thread(ex_rdp_context);

        // stop image updating
        g_source_remove_safely(&ex_rdp_context->display_update_timeout_id);
        //g_async_queue_unref(ex_rdp_context->display_update_queue);
        //ex_rdp_context->display_update_queue = NULL;

        // stop cursor updating
        g_source_remove_safely(&ex_rdp_context->cursor_update_timeout_id);

        g_info("%s: wai for mutex (invalid_region_mutex)", (const char *)__func__);
        wait_for_mutex_and_clear(&ex_rdp_context->invalid_region_mutex);
        g_info("%s: wai for mutex (cursor_mutex)", (const char *)__func__);
        wait_for_mutex_and_clear(&ex_rdp_context->cursor_mutex);

        g_info("%s: context free no ", (const char *)__func__);
        freerdp_client_context_free((rdpContext*)ex_rdp_context);
        g_info("%s: context freed", (const char *)__func__);
    }
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
    if (ex_contect->is_abort_demanded)
        return NULL;

    ex_contect->is_running = TRUE;

    int status;
    rdpContext *context = (rdpContext *)ex_contect;

    if (!context)
        goto end;

    // rdp params
    GArray *rdp_params_dyn_array = rdp_client_create_params_array(ex_contect);

    g_info("%s ex_contect->user_name %s", (const char *)__func__, ex_contect->p_rdp_settings->user_name);
    g_info("%s ex_contect->domain %s", (const char *)__func__, ex_contect->p_rdp_settings->domain);
    // + 1 к размеру для завершающего NULL
    gchar** argv = malloc((rdp_params_dyn_array->len + 1) * sizeof(gchar*));
    for (guint i = 0; i < rdp_params_dyn_array->len; ++i) {
        argv[i] = g_array_index(rdp_params_dyn_array, gchar*, i);
        if(strstr(argv[i], "/p:") == NULL) // Do not print the password
            g_info("%i RDP arg: %s", i, argv[i]);
    }
    argv[rdp_params_dyn_array->len] = NULL; // Завершающий NULL

    int argc = (int)rdp_params_dyn_array->len;

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
        goto end;

    // MAIN RDP LOOP
    freerdp* instance = (freerdp*)ex_contect->context.instance;
    HANDLE handles[64];
    UINT32 conn_try = 0;

    while (TRUE) {
        if (ex_contect->is_abort_demanded)
            break;

        g_info("RDP. Connect attempt number: %i", conn_try + 1);
        ex_contect->is_connecting = TRUE;
        ex_contect->is_connected_last_time = freerdp_connect(instance);
        ex_contect->is_connecting = FALSE;
        if (!ex_contect->is_connected_last_time) {
            g_info("connection failure");
            // следующая попытка
            conn_try++;
            if (conn_try < MAX_CONN_TRY_NUMBER) {
                g_usleep(500000);
                continue; // to the next attempt
            } else { // unable to connect
                UINT32 error_code = freerdp_get_last_error(instance->context);
                g_autofree gchar *err_str = NULL;
                err_str = rdp_util_get_full_error_msg(error_code, 0);
                vdi_event_conn_error_notify(error_code, err_str);
                break;
            }
        }
        // Подключение удачно
        conn_try = 0; // Сброс счетчика
        ex_contect->last_rdp_error = 0; // Сброс инфы о последней ошибке
        vdi_event_vm_changed_notify(vdi_session_get_current_vm_id()); // Событие подключения к ВМ

        g_info("RDP successfully connected");
        while (!freerdp_shall_disconnect(instance)) {

            if (ex_contect->is_abort_demanded)
                break;

            if (freerdp_focus_required(instance)) {
                g_info(" if (freerdp_focus_required(instance))");
                rdp_keyboard_focus_in(ex_contect);
            }

            DWORD nCount = freerdp_get_event_handles(instance->context, &handles[0], 64);

            if (nCount == 0) {
                WLog_ERR(TAG, "%s: freerdp_get_event_handles failed", __FUNCTION__);
                break;
            }

            DWORD wait_status = WaitForMultipleObjects(nCount, handles, FALSE, 100);
            if (wait_status == WAIT_FAILED) {
                WLog_ERR(TAG, "%s: WaitForMultipleObjects failed with %" PRIu32 "", __FUNCTION__, wait_status);
                break;
            }

            // выполнение следующего условия говорит о потере связи
            if (!freerdp_check_event_handles(instance->context)) {
                // выполнение следующего условия говорит о намеренном завершении сессии
                UINT32 error_info = freerdp_error_info(instance);
                if (error_info != 0)
                    ex_contect->is_abort_demanded = TRUE;
                g_info("freerdp_check_event_handles. Br. is_abort_demanded: %i  error_info: %i",
                       ex_contect->is_abort_demanded, error_info);
                break;
            }
        }

        g_info("Before freerdp_disconnect");
        BOOL diss_res = freerdp_disconnect(instance);
        g_info("After freerdp_disconnect res: %i", diss_res);
    }
end:
    g_info("%s: g_mutex_unlock", (const char *)__func__);
    ex_contect->is_running = FALSE;
    rdp_client_demand_image_update(ex_contect, 0, 0, ex_contect->whole_image_width, ex_contect->whole_image_height);

    // Если соединение было успешным то сигнализируем об отключении от ВМ
    if (ex_contect->is_connected_last_time)
        vdi_event_vm_changed_notify(NULL);
    shutdown_loop(ex_contect->main_loop);
    return NULL;
}

BOOL rdp_client_abort_connection(freerdp* instance)
{
    ExtendedRdpContext *ex_context = (ExtendedRdpContext*)instance->context;
    ex_context->is_abort_demanded = TRUE;
    return freerdp_abort_connect(instance);
}

void rdp_client_start_routine_thread(ExtendedRdpContext *ex_rdp_context)
{
    ex_rdp_context->is_abort_demanded = FALSE; // reset abort demand before start
    ex_rdp_context->rdp_client_routine_thread = g_thread_new(NULL, (GThreadFunc)rdp_client_routine, ex_rdp_context);
}

void rdp_client_stop_routine_thread(ExtendedRdpContext *ex_rdp_context)
{
    if (!ex_rdp_context->rdp_client_routine_thread)
        return;

    // В режиме запуска удаленного приложения закрываем текущее окно послав alt f4, так как пользователь ожидает,
    // что приложение закроется
    rdpContext *context = (rdpContext *) ex_rdp_context;
    if (context->settings->RemoteApplicationMode) {
        rdp_viewer_window_send_key_shortcut(context, 14); // 14 - index in keyCombos
    }

    g_info("%s: rdp abort now", (const char *)__func__);
    rdp_client_abort_connection(ex_rdp_context->context.instance);
    // wait until rdp thread finishes (it should happen after abort)
    g_info("%s: waiting for rdp thread completion", (const char *)__func__);
    g_thread_join(ex_rdp_context->rdp_client_routine_thread);
    g_info("%s: rdp thread stopped", (const char *)__func__);
    ex_rdp_context->rdp_client_routine_thread = NULL;
    // reset errors
    freerdp_set_last_error(context, FREERDP_ERROR_SUCCESS);
}

/* This function is called whenever a new frame starts.
*/
static BOOL rdp_begin_paint(rdpContext* context)
{
    //g_info("%s\n", (const char *)__func__);
    rdpGdi* gdi = context->gdi;
    gdi->primary->hdc->hwnd->invalid->null = TRUE;

    return TRUE;
}

/* This function is called when the library completed composing a new frame.
 */
static BOOL rdp_end_paint(rdpContext* context)
{
    //g_info("%s\n", (const char *)__func__);
    rdpGdi* gdi = context->gdi;
    ExtendedRdpContext* ex_context = (ExtendedRdpContext*)context;

    if (gdi->primary->hdc->hwnd->invalid->null) {
        return TRUE;
    }

    if (gdi->primary->hdc->hwnd->ninvalid < 1) {
        return TRUE;
    }

    rdp_client_demand_image_update(ex_context,
            gdi->primary->hdc->hwnd->invalid->x,
            gdi->primary->hdc->hwnd->invalid->y,
            gdi->primary->hdc->hwnd->invalid->w,
            gdi->primary->hdc->hwnd->invalid->h);

    gdi->primary->hdc->hwnd->invalid->null = TRUE;
    gdi->primary->hdc->hwnd->ninvalid = 0;

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

    ExtendedRdpContext* ex = (ExtendedRdpContext*)instance->context;

    UINT32 freerdp_pix_format = PIXEL_FORMAT_RGB16; // default
    cairo_format_t cairo_format = CAIRO_FORMAT_RGB16_565; // default
#ifdef __APPLE__ // support only BGRA32
    gchar *rdp_pixel_format_str = g_strdup("BGRA32");
#else
    // get image pixel format from ini file
    const gchar *rdp_pixel_format_str = ex->p_rdp_settings->rdp_pixel_format_str;
#endif
    if (g_strcmp0(rdp_pixel_format_str, "BGRA32") == 0) {
        freerdp_pix_format = PIXEL_FORMAT_BGRA32;
        cairo_format = CAIRO_FORMAT_ARGB32;
    }

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
    rdpGdi* gdi = ex->context.gdi;

    g_mutex_lock(&ex->primary_buffer_mutex);

    //g_info("%s W: %i H: %i", (const char *)__func__, gdi->width, gdi->height);
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

    wait_for_mutex_and_clear(&ex_rdp_context->primary_buffer_mutex);
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
