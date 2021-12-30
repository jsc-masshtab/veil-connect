/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */
// Based on remmina source code https://github.com/FreeRDP/Remmina

#include <stdlib.h>

#include "rdp_viewer_window.h"
#include "rdp_clipboard.h"
#include "veil_logger.h"

#define CLIPBOARD_TRANSFER_WAIT_TIME 1 // sec

typedef enum {
    RDP_CLIPBOARD_GET_DATA,
    RDP_CLIPBOARD_SET_DATA,
    RDP_CLIPBOARD_SET_CONTENT
} RdpClipboardEventType;

typedef struct {
    ExtendedRdpContext *ex_context;
    RdpClipboard *clipboard;
    RdpClipboardEventType rdp_clipboard_event_type;

    GtkTargetList *target_list;

    gpointer data;
    UINT32 format;

// union can be use to decrease size
} RdpClipboardEventData;

// We cant use GTK functions in thread so invoke callback which will be executed in the main thread
typedef gboolean (*RdpClipboardEventProcess) (RdpClipboardEventData *rdp_clipboard_event_data);

// magic functions from remmina
static UINT8* lf2crlf(UINT8* data, int* size)
{
    UINT8 c;
    UINT8* outbuf;
    UINT8* out;
    UINT8* in_end;
    UINT8* in;
    int out_size;

    out_size = (*size) * 2 + 1;
    outbuf = (UINT8*)malloc(out_size);
    out = outbuf;
    in = data;
    in_end = data + (*size);

    while (in < in_end) {
        c = *in++;
        if (c == '\n') {
            *out++ = '\r';
            *out++ = '\n';
        }else  {
            *out++ = c;
        }
    }

    *out++ = 0;
    *size = out - outbuf;

    return outbuf;
}

static void crlf2lf(UINT8* data, size_t* size)
{
    UINT8 c;
    UINT8* out;
    UINT8* in;
    UINT8* in_end;

    out = data;
    in = data;
    in_end = data + (*size);

    while (in < in_end) {
        c = *in++;
        if (c != '\r')
            *out++ = c;
    }

    *size = out - data;
}

static GtkClipboard* get_gtk_clipboard(ExtendedRdpContext *ex_context)
{
    if (ex_context->rdp_windows_array->len == 0)
        return NULL;

    RdpWindowData *rdp_window_data = g_array_index(ex_context->rdp_windows_array, RdpWindowData *, 0);
    return gtk_widget_get_clipboard(rdp_window_data->rdp_viewer_window, GDK_SELECTION_CLIPBOARD);
}

static void rdp_cliprdr_request_data(GtkClipboard *gtkClipboard G_GNUC_UNUSED,
        GtkSelectionData *selection_data G_GNUC_UNUSED, guint info, RdpClipboard* clipboard)
{
    g_info("%s", (const char *)__func__);

    /* Called by GTK when someone press "Paste" on the client side.
     * We ask to the server the data we need */

    if (clipboard->waiting_for_transfered_data) {
        g_info("[RDP] Cannot paste now, I’m transferring clipboard data from server. "
                                           "Try again later");
        return;
    }

    clipboard->format = info;

    /* Request Clipboard content from the server, the request is async */
    g_mutex_lock(&clipboard->transfer_clip_mutex);

    CLIPRDR_FORMAT_DATA_REQUEST *pFormatDataRequest =
            (CLIPRDR_FORMAT_DATA_REQUEST*)calloc(1, sizeof(CLIPRDR_FORMAT_DATA_REQUEST));
    pFormatDataRequest->requestedFormatId = clipboard->format;

    clipboard->context->ClientFormatDataRequest(clipboard->context ,pFormatDataRequest);
    free(pFormatDataRequest);

    ///* Busy wait clibpoard data for CLIPBOARD_TRANSFER_WAIT_TIME seconds */
    // g_cond_wait_until return FALSE if end_time has passed
    // GLib doc forces us to use g_cond_wait_until with while
    clipboard->is_transfered = FALSE;
    clipboard->waiting_for_transfered_data = TRUE;
    //g_info("%s %li", (const char *)__func__, pthread_self());
    gint64 end_time = g_get_monotonic_time() + CLIPBOARD_TRANSFER_WAIT_TIME * G_TIME_SPAN_SECOND;
    while (!clipboard->is_transfered) {
        if (g_cond_wait_until(&clipboard->transfer_clip_cond, &clipboard->transfer_clip_mutex,
                              end_time) == FALSE) {
            g_info("end_time has passed");
            break;
        }
    }
    clipboard->waiting_for_transfered_data = FALSE;

    // g_info("Broke from while");
    g_mutex_unlock(&clipboard->transfer_clip_mutex);
}

void rdp_cliprdr_empty_clipboard(GtkClipboard *gtkClipboard G_GNUC_UNUSED, RdpClipboard *clipboard G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
}

static void rdp_cliprdr_get_clipboard_data(RdpClipboardEventData *rdp_clipboard_event_data)
{
    g_info("%s %i", (const char *)__func__, rdp_clipboard_event_data->format);
    UINT8* inbuf = NULL;
    UINT8* outbuf = NULL;
    GdkPixbuf *image = NULL;
    int size = 0;

    RdpClipboard *clipboard = rdp_clipboard_event_data->clipboard;
    ExtendedRdpContext *ex_context = clipboard->ex_context;
    clipboard->format = rdp_clipboard_event_data->format;

    GtkClipboard *gtkClipboard = get_gtk_clipboard(ex_context);
    if (gtkClipboard) {
        switch (clipboard->format) {
            case CF_TEXT:
            case CF_UNICODETEXT:
            case CB_FORMAT_HTML:
            {
                inbuf = (UINT8*)gtk_clipboard_wait_for_text(gtkClipboard);
                if (inbuf)
                    logger_save_clipboard_data((const gchar *)inbuf, strlen((char*)inbuf),
                        CLIPBOARD_LOGGER_FROM_CLIENT_TO_VM);
                break;
            }

            case CB_FORMAT_PNG:
            case CB_FORMAT_JPEG:
            case CF_DIB:
            case CF_DIBV5:
            {
                image = gtk_clipboard_wait_for_image(gtkClipboard);
                break;
            }
        }
    }

    /* No data received, send nothing */
    if (inbuf != NULL || image != NULL) {
        switch (clipboard->format) {
            case CF_TEXT:
            case CB_FORMAT_HTML:
            {
                size = strlen((char*)inbuf);
                outbuf = lf2crlf(inbuf, &size);
                break;
            }
            case CF_UNICODETEXT:
            {
                size = strlen((char*)inbuf);
                inbuf = lf2crlf(inbuf, &size);
                size = (ConvertToUnicode(CP_UTF8, 0, (CHAR*)inbuf, -1, (WCHAR**)&outbuf, 0) ) * sizeof(WCHAR);
                g_free(inbuf);
                break;
            }
            case CB_FORMAT_PNG:
            {
                gchar* data;
                gsize buffersize;
                gdk_pixbuf_save_to_buffer(image, &data, &buffersize, "png", NULL, NULL);
                outbuf = (UINT8*)malloc(buffersize);
                memcpy(outbuf, data, buffersize);
                size = buffersize;
                g_object_unref(image);
                break;
            }
            case CB_FORMAT_JPEG:
            {
                gchar* data;
                gsize buffersize;
                gdk_pixbuf_save_to_buffer(image, &data, &buffersize, "jpeg", NULL, NULL);
                outbuf = (UINT8*)malloc(buffersize);
                memcpy(outbuf, data, buffersize);
                size = buffersize;
                g_object_unref(image);
                break;
            }
            case CF_DIB:
            case CF_DIBV5:
            {
                gchar* data;
                gsize buffersize;
                gdk_pixbuf_save_to_buffer(image, &data, &buffersize, "bmp", NULL, NULL);
                size = buffersize - 14;
                outbuf = (UINT8*)malloc(size);
                memcpy(outbuf, data + 14, size);
                g_object_unref(image);
                break;
            }
        }
    }

    //
    CLIPRDR_FORMAT_DATA_RESPONSE response = { 0 };
    response.msgType = CB_FORMAT_DATA_RESPONSE;
    response.msgFlags = outbuf ? CB_RESPONSE_OK : CB_RESPONSE_FAIL;
    response.dataLen = size;
    response.requestedFormatData = outbuf;

    clipboard->context->ClientFormatDataResponse(clipboard->context, &response);
}

static void rdp_cliprdr_set_clipboard_data(RdpClipboardEventData *rdp_clipboard_event_data)
{
    g_info("%s", (const char *)__func__);

    gint n_targets;
    ExtendedRdpContext *ex_context = rdp_clipboard_event_data->ex_context;
    GtkTargetEntry *targets = gtk_target_table_new_from_list(rdp_clipboard_event_data->target_list, &n_targets);

    GtkClipboard* gtkClipboard = get_gtk_clipboard(ex_context);

    if (gtkClipboard && targets) {
        gtk_clipboard_set_with_data(gtkClipboard, targets, n_targets,
                                     (GtkClipboardGetFunc)rdp_cliprdr_request_data,
                                     (GtkClipboardClearFunc)rdp_cliprdr_empty_clipboard,
                                     rdp_clipboard_event_data->clipboard);
        gtk_target_table_free(targets, n_targets);
    }

    gtk_target_list_unref(rdp_clipboard_event_data->target_list);
}

static void rdp_cliprdr_set_clipboard_content(RdpClipboardEventData *rdp_clipboard_event_data)
{
    g_info("%s", (const char *)__func__);

    ExtendedRdpContext *ex_context = rdp_clipboard_event_data->ex_context;

    GtkClipboard* gtkClipboard = get_gtk_clipboard(ex_context);

    if (!rdp_clipboard_event_data->data) {
        g_info("%s rdp_clipboard_event_data->data == NULL", (const char *)__func__);
        return;
    }

    RdpClipboard *clipboard = rdp_clipboard_event_data->clipboard;
    if (clipboard->format == CB_FORMAT_PNG || clipboard->format == CF_DIB ||
            clipboard->format == CF_DIBV5 || clipboard->format == CB_FORMAT_JPEG) {
        gtk_clipboard_set_image(gtkClipboard, rdp_clipboard_event_data->data);
        g_object_unref(rdp_clipboard_event_data->data);
    } else {
        const gchar *text = (gchar *)rdp_clipboard_event_data->data;
        //g_info("text: %s", text);
        if (text) {
            gtk_clipboard_set_text(gtkClipboard, text, -1);
            logger_save_clipboard_data(text, strlen_safely(text), CLIPBOARD_LOGGER_FROM_VM_TO_CLIENT);
            free(rdp_clipboard_event_data->data);
        }
    }
}

// Executed in the main thread (GTK requirement)
static gboolean rdp_cliprdr_event_process(RdpClipboardEventData *rdp_clipboard_event_data)
{
    g_info("%s", (const char *)__func__);
    if (!rdp_clipboard_event_data)
        return FALSE;

    switch (rdp_clipboard_event_data->rdp_clipboard_event_type) {

        case RDP_CLIPBOARD_GET_DATA:
            rdp_cliprdr_get_clipboard_data(rdp_clipboard_event_data);
            break;

        case RDP_CLIPBOARD_SET_DATA:
            rdp_cliprdr_set_clipboard_data(rdp_clipboard_event_data);
            break;

        case RDP_CLIPBOARD_SET_CONTENT:
            rdp_cliprdr_set_clipboard_content(rdp_clipboard_event_data);
            break;
    }

    // free
    free(rdp_clipboard_event_data);

    return FALSE;
}

static UINT rdp_cliprdr_send_client_capabilities(RdpClipboard* clipboard)
{
    g_info("%s", (const char *)__func__);
    CLIPRDR_CAPABILITIES capabilities;
    CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;
    capabilities.cCapabilitiesSets = 1;
    capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*)&(generalCapabilitySet);
    generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
    generalCapabilitySet.capabilitySetLength = 12;
    generalCapabilitySet.version = CB_CAPS_VERSION_2;
    generalCapabilitySet.generalFlags = CB_USE_LONG_FORMAT_NAMES;

    //if (clipboard->streams_supported && clipboard->file_formats_registered)
    //    generalCapabilitySet.generalFlags |= CB_STREAM_FILECLIP_ENABLED | CB_FILECLIP_NO_FILE_PATHS;

    return clipboard->context->ClientCapabilities(clipboard->context, &capabilities);
}

static UINT rdp_cliprdr_send_used_client_format_list(RdpClipboard* clipboard)
{
    g_info("%s", (const char *)__func__);
    CLIPRDR_FORMAT* formats = NULL;
    CLIPRDR_FORMAT_LIST formatList = { 0 };

    UINT32 numFormats = 4; // 5

    if (!(formats = (CLIPRDR_FORMAT *)calloc(numFormats, sizeof(CLIPRDR_FORMAT)))) {
        g_info("failed to allocate %" PRIu32 " CLIPRDR_FORMAT structs", numFormats);
        return CHANNEL_RC_NO_MEMORY;
    }

    // Used formats
    formats[0].formatId = CF_UNICODETEXT;
    formats[0].formatName = "UTF8_STRING";
    formats[1].formatId = CB_FORMAT_HTML;
    formats[1].formatName = "HTML Format";
    formats[2].formatId = CB_FORMAT_PNG;
    formats[2].formatName = "image/png";
    formats[3].formatId = CB_FORMAT_JPEG;
    formats[3].formatName = "image/jpeg";
    //formats[4].formatId = CF_RAW; // no on Windows
    //formats[4].formatName = "_FREERDP_RAW";

    formatList.msgFlags = CB_RESPONSE_OK;
    formatList.numFormats = numFormats;
    formatList.formats = formats;
    formatList.msgType = CB_FORMAT_LIST;
    UINT ret = clipboard->context->ClientFormatList(clipboard->context, &formatList);
    free(formats);

    return ret;
}

static UINT32 rdp_cliprdr_get_format_from_gdkatom(GdkAtom atom)
{
    UINT32 rc;
    gchar* name = gdk_atom_name(atom);
    rc = 0;
    if (g_strcmp0("UTF8_STRING", name) == 0 || g_strcmp0("text/plain;charset=utf-8", name) == 0) {
        rc = CF_UNICODETEXT;
    }
    if (g_strcmp0("TEXT", name) == 0 || g_strcmp0("text/plain", name) == 0) {
        rc =  CF_TEXT;
    }
    if (g_strcmp0("text/html", name) == 0) {
        rc =  CB_FORMAT_HTML;
    }
    if (g_strcmp0("image/png", name) == 0) {
        rc =  CB_FORMAT_PNG;
    }
    if (g_strcmp0("image/jpeg", name) == 0) {
        rc =  CB_FORMAT_JPEG;
    }
    if (g_strcmp0("image/bmp", name) == 0) {
        rc =  CF_DIB;
    }
    g_free(name);
    return rc;
}

static CLIPRDR_FORMAT_LIST rdp_cliprdr_get_client_format_list(RdpClipboard* clipboard)
{
    g_info("%s", (const char *)__func__);

    ExtendedRdpContext *ex_context = clipboard->ex_context;
    GdkAtom* targets = NULL;
    gboolean result = FALSE;
    gint n_targets = 0;

    CLIPRDR_FORMAT_LIST cliprdr_format_list;
    cliprdr_format_list.formats = NULL;
    cliprdr_format_list.numFormats = 0;

    GtkClipboard *gtkClipboard = get_gtk_clipboard(ex_context);
    if (gtkClipboard) {
        result = gtk_clipboard_wait_for_targets(gtkClipboard, &targets, &n_targets);
    }

    CLIPRDR_FORMAT* all_formats = NULL;
    if (result && n_targets > 0) {
        // Судя по всему есть множество форматов которые нам не подходят, поэтому нужен
        // промежуточный массив
        all_formats = (CLIPRDR_FORMAT *)calloc(1, n_targets * sizeof(CLIPRDR_FORMAT));
        gint srvcount = 0;
        for (int i = 0; i < n_targets; i++) {
            gint formatId = rdp_cliprdr_get_format_from_gdkatom(targets[i]);

            if ( formatId != 0 ) {
                all_formats[srvcount].formatId = formatId;
                all_formats[srvcount].formatName = NULL;
                srvcount++;
            }
        }

        if (srvcount > 0) {
            cliprdr_format_list.formats = (CLIPRDR_FORMAT*)calloc(1, sizeof(CLIPRDR_FORMAT) * srvcount); // will
            // be freed after usage
            memcpy(cliprdr_format_list.formats, all_formats, sizeof(CLIPRDR_FORMAT) * srvcount);
            cliprdr_format_list.numFormats = srvcount;
        }
    }

    if (all_formats)
        free(all_formats);

    if (result)
        g_free(targets);

    cliprdr_format_list.msgType = CB_FORMAT_LIST;
    cliprdr_format_list.msgFlags = CB_RESPONSE_OK;

    return cliprdr_format_list;
}

static UINT rdp_cliprdr_monitor_ready(CliprdrClientContext* context,
        const CLIPRDR_MONITOR_READY* monitorReady G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
    RdpClipboard* clipboard = (RdpClipboard*)context->custom;

    INT ret;

    if ((ret = rdp_cliprdr_send_client_capabilities(clipboard)) != CHANNEL_RC_OK)
        return ret;

    if ((ret = rdp_cliprdr_send_used_client_format_list(clipboard)) != CHANNEL_RC_OK)
        return ret;

    return CHANNEL_RC_OK;
}

static UINT rdp_cliprdr_server_capabilities(CliprdrClientContext* context G_GNUC_UNUSED,
        const CLIPRDR_CAPABILITIES* capabilities G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
    return CHANNEL_RC_OK;
}

//
static UINT rdp_cliprdr_server_format_list(CliprdrClientContext* context, const CLIPRDR_FORMAT_LIST* formatList)
{
    g_info("%s", (const char *)__func__);
    /* Called when a user do a "Copy" on the server: we collect all formats
     * the server send us and then setup the local clipboard with the appropiate
     * functions to request server data */

    RdpClipboard *clipboard = (RdpClipboard*)context->custom;

    if (vdi_session_is_shared_clipboard_g_to_c_permitted()) { // Проверяем разрешение от админа
        GtkTargetList *list = gtk_target_list_new(NULL, 0); // will be freed in callback

        for (UINT32 i = 0; i < formatList->numFormats; i++) {
            CLIPRDR_FORMAT *format = &formatList->formats[i];
            if (format->formatId == CF_UNICODETEXT) {
                GdkAtom atom = gdk_atom_intern("UTF8_STRING", TRUE);
                gtk_target_list_add(list, atom, 0, CF_UNICODETEXT);
            } else if (format->formatId == CF_TEXT) {
                GdkAtom atom = gdk_atom_intern("TEXT", TRUE);
                gtk_target_list_add(list, atom, 0, CF_TEXT);
            } else if (format->formatId == CF_DIB) {
                GdkAtom atom = gdk_atom_intern("image/bmp", TRUE);
                gtk_target_list_add(list, atom, 0, CF_DIB);
            } else if (format->formatId == CF_DIBV5) {
                GdkAtom atom = gdk_atom_intern("image/bmp", TRUE);
                gtk_target_list_add(list, atom, 0, CF_DIBV5);
            } else if (format->formatId == CB_FORMAT_JPEG) {
                GdkAtom atom = gdk_atom_intern("image/jpeg", TRUE);
                gtk_target_list_add(list, atom, 0, CB_FORMAT_JPEG);
            } else if (format->formatId == CB_FORMAT_PNG) {
                GdkAtom atom = gdk_atom_intern("image/png", TRUE);
                gtk_target_list_add(list, atom, 0, CB_FORMAT_PNG);
            } else if (format->formatId == CB_FORMAT_HTML) {
                GdkAtom atom = gdk_atom_intern("text/html", TRUE);
                gtk_target_list_add(list, atom, 0, CB_FORMAT_HTML);
            }
        }

        // Schedule to execute in the main thread
        RdpClipboardEventData *rdp_clipboard_event_data = calloc(1, sizeof(RdpClipboardEventData)); // will be freed
        // in callback
        rdp_clipboard_event_data->rdp_clipboard_event_type = RDP_CLIPBOARD_SET_DATA;
        rdp_clipboard_event_data->target_list = list;

        rdp_clipboard_event_data->ex_context = clipboard->ex_context;
        rdp_clipboard_event_data->clipboard = clipboard;

        g_idle_add((GSourceFunc) rdp_cliprdr_event_process, rdp_clipboard_event_data);
    }

    /* Send FormatListResponse to server */
    CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse;
    formatListResponse.msgType = CB_FORMAT_LIST_RESPONSE;
    formatListResponse.msgFlags = CB_RESPONSE_OK; // Can be CB_RESPONSE_FAIL in case of error
    formatListResponse.dataLen = 0;

    return context->ClientFormatListResponse(clipboard->context, &formatListResponse);
}

static UINT rdp_cliprdr_server_format_list_response(CliprdrClientContext* context G_GNUC_UNUSED,
        const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
    return CHANNEL_RC_OK;
}

static UINT rdp_cliprdr_server_format_data_request(CliprdrClientContext* context,
        const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
    g_info("%s requestedFormatId: %i", (const char *)__func__, formatDataRequest->requestedFormatId);

    RdpClipboardEventData *rdp_clipboard_event_data = calloc(1, sizeof(RdpClipboardEventData));
    rdp_clipboard_event_data->rdp_clipboard_event_type = RDP_CLIPBOARD_GET_DATA;
    rdp_clipboard_event_data->format = formatDataRequest->requestedFormatId;
    rdp_clipboard_event_data->clipboard = (RdpClipboard*)context->custom;

    g_idle_add((GSourceFunc)rdp_cliprdr_event_process, rdp_clipboard_event_data);

    return CHANNEL_RC_OK;
}

static UINT rdp_cliprdr_server_format_data_response(CliprdrClientContext* context,
        const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
    g_info("%s", (const char *)__func__);

    const UINT8* data;
    size_t size;
    GdkPixbufLoader *pixbuf;
    gpointer output = NULL;
    RdpClipboard *clipboard = (RdpClipboard*)context->custom;

    data = formatDataResponse->requestedFormatData;
    size = formatDataResponse->dataLen;

    // formatDataResponse->requestedFormatData is allocated
    //  by freerdp and freed after returning from this callback function.
    //  So we must make a copy if we need to preserve it

    if (size > 0) {
        switch (clipboard->format) {
            case CF_UNICODETEXT:
            {
#ifdef __linux__
                size = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*)data, size / 2, (CHAR**)&output, 0, NULL, NULL);
                crlf2lf(output, &size);
#elif _WIN32
                gchar *utf8_str = g_utf16_to_utf8((const gunichar2 *)(data), size, NULL, NULL, NULL);
                output = (gpointer)utf8_str;
#endif
                break;
            }

            case CF_TEXT:
            case CB_FORMAT_HTML:
            {
                output = (gpointer)calloc(1, size + 1);
                if (output) {
                    memcpy(output, data, size);
                    crlf2lf(output, &size);
                }
                break;
            }

            case CF_DIBV5:
            case CF_DIB:
            {
                wStream* s;
                UINT32 offset;
                BITMAPINFOHEADER* pbi;
                BITMAPV5HEADER* pbi5;

                pbi = (BITMAPINFOHEADER*)data;

                // offset calculation inspired by
                // http://downloads.poolelan.com/MSDN/MSDNLibrary6/Disk1/Samples/VC/OS/WindowsXP/GetImage/BitmapUtil.cpp
                offset = 14 + pbi->biSize;
                if (pbi->biClrUsed != 0)
                    offset += sizeof(RGBQUAD) * pbi->biClrUsed;
                else if (pbi->biBitCount <= 8)
                    offset += sizeof(RGBQUAD) * (1 << pbi->biBitCount);
                if (pbi->biSize == sizeof(BITMAPINFOHEADER)) {
                    if (pbi->biCompression == 3)         // BI_BITFIELDS is 3
                        offset += 12;
                } else if (pbi->biSize >= sizeof(BITMAPV5HEADER)) {
                    pbi5 = (BITMAPV5HEADER*)pbi;
                    if (pbi5->bV5ProfileData <= offset)
                        offset += pbi5->bV5ProfileSize;
                }
                s = Stream_New(NULL, 14 + size);
                Stream_Write_UINT8(s, 'B');
                Stream_Write_UINT8(s, 'M');
                Stream_Write_UINT32(s, 14 + size);
                Stream_Write_UINT32(s, 0);
                Stream_Write_UINT32(s, offset);
                Stream_Write(s, data, size);

                data = Stream_Buffer(s);
                size = Stream_Length(s);

                pixbuf = gdk_pixbuf_loader_new();
                GError *perr = NULL;
                if ( !gdk_pixbuf_loader_write(pixbuf, data, size, &perr) ) {
                    g_info("[RDP] rdp_cliprdr: gdk_pixbuf_loader_write() returned error %s\n", perr->message);
                    g_clear_error(&perr);
                }else  {
                    if ( !gdk_pixbuf_loader_close(pixbuf, &perr) ) {
                        g_info("[RDP] rdp_cliprdr: gdk_pixbuf_loader_close() returned error %s\n", perr->message);
                        g_clear_error(&perr);
                    }
                    Stream_Free(s, TRUE);
                    output = g_object_ref(gdk_pixbuf_loader_get_pixbuf(pixbuf));
                }
                g_object_unref(pixbuf);
                break;
            }

            case CB_FORMAT_PNG:
            case CB_FORMAT_JPEG:
            {
                pixbuf = gdk_pixbuf_loader_new();
                gdk_pixbuf_loader_write(pixbuf, data, size, NULL);
                output = g_object_ref(gdk_pixbuf_loader_get_pixbuf(pixbuf));
                gdk_pixbuf_loader_close(pixbuf, NULL);
                g_object_unref(pixbuf);
                break;
            }
        }
    }

    //g_info("%s Before g_mutex_lock(&clipboard->transfer_clip_mutex); %lu", __func__, pthread_self());
    g_mutex_lock(&clipboard->transfer_clip_mutex);
    clipboard->is_transfered = TRUE;
    g_cond_signal(&clipboard->transfer_clip_cond);
    g_info("g_cond_signal(&clipboard->transfer_clip_cond);");

    // Schedule to execute in the main thread
    RdpClipboardEventData *rdp_clipboard_event_data = calloc(1, sizeof(RdpClipboardEventData)); // will be freed
    // in callback
    rdp_clipboard_event_data->rdp_clipboard_event_type = RDP_CLIPBOARD_SET_CONTENT;

    rdp_clipboard_event_data->ex_context = clipboard->ex_context;
    rdp_clipboard_event_data->clipboard = clipboard;
    rdp_clipboard_event_data->data = output;
    rdp_clipboard_event_data->format = clipboard->format;

    g_idle_add((GSourceFunc)rdp_cliprdr_event_process, rdp_clipboard_event_data);

    g_mutex_unlock(&clipboard->transfer_clip_mutex);

    return CHANNEL_RC_OK;
}

static gboolean rdp_event_on_clipboard(GtkClipboard *gtkClipboard G_GNUC_UNUSED, GdkEvent *event G_GNUC_UNUSED,
        RdpClipboard* clipboard)
{
    /* Signal handler for GTK clipboard owner-change */
    g_info("%s",(const char*)__func__);

    // Смотрим находится ли какое-либо из окон в фокусе. Если находится, то игнорируем событие, так как
    // манинимуляции с буфером были скорее всего на удаленной машине
    gboolean is_toplevel_focus = FALSE;
    GArray *rdp_windows_array = clipboard->ex_context->rdp_windows_array;
    for (guint i = 0; i < rdp_windows_array->len; ++i) {
        RdpWindowData *rdp_window_data = g_array_index(rdp_windows_array, RdpWindowData *, i);
        is_toplevel_focus = gtk_window_has_toplevel_focus(GTK_WINDOW(rdp_window_data->rdp_viewer_window));
    }
    g_info("%s is_focus: %i", (const char *)__func__, is_toplevel_focus);

    /* Usually "owner-change" is fired when a user pres "COPY" on the client
    */

    if (!is_toplevel_focus) {
        CLIPRDR_FORMAT_LIST formatList = rdp_cliprdr_get_client_format_list(clipboard);
        clipboard->context->ClientFormatList(clipboard->context, &formatList);
        if (formatList.formats)
            free(formatList.formats);
    }

    return TRUE;
}

//GdkDisplay *display = gdk_display_get_default();
//GtkClipboard *gtk_clipboard = gtk_clipboard_get_for_display(display, GDK_SELECTION_CLIPBOARD);
void rdp_cliprdr_init(ExtendedRdpContext *ex_context, CliprdrClientContext *cliprdr)
{
    RdpClipboard* clipboard = (RdpClipboard*)calloc(1, sizeof(rdpPointer));
    clipboard->context = cliprdr;
    cliprdr->custom = (void*)clipboard;
    clipboard->ex_context = ex_context;

    g_mutex_init(&clipboard->transfer_clip_mutex);
    g_cond_init(&clipboard->transfer_clip_cond);

    cliprdr->MonitorReady = rdp_cliprdr_monitor_ready;
    cliprdr->ServerCapabilities = rdp_cliprdr_server_capabilities;
    cliprdr->ServerFormatList = rdp_cliprdr_server_format_list;
    cliprdr->ServerFormatListResponse = rdp_cliprdr_server_format_list_response;
    cliprdr->ServerFormatDataRequest = rdp_cliprdr_server_format_data_request;
    cliprdr->ServerFormatDataResponse = rdp_cliprdr_server_format_data_response;

    if (vdi_session_is_shared_clipboard_c_to_g_permitted()) {
        // clipboard monitor. Copy event on client
        GtkClipboard *gtkClipboard = get_gtk_clipboard(ex_context);
        g_info("%s", (const char *) __func__);
        if (gtkClipboard)
            clipboard->clipboard_handler = g_signal_connect(gtkClipboard, "owner-change",
                                                            G_CALLBACK(rdp_event_on_clipboard), clipboard);
    }
}

void rdp_cliprdr_uninit(ExtendedRdpContext *ex_context, CliprdrClientContext* cliprdr)
{
    if (cliprdr->custom) {
        RdpClipboard* clipboard = (RdpClipboard*)cliprdr->custom;

        g_mutex_clear(&clipboard->transfer_clip_mutex);
        g_cond_clear(&clipboard->transfer_clip_cond);

        /* unregister the clipboard monitor */
        if (clipboard->clipboard_handler) {
            GtkClipboard *gtkClipboard = get_gtk_clipboard(ex_context);
            g_signal_handler_disconnect(G_OBJECT(gtkClipboard), clipboard->clipboard_handler);
            clipboard->clipboard_handler = 0;
        }

        clipboard->context = NULL;
        free(cliprdr->custom);
    }
    cliprdr->custom = NULL;
}
