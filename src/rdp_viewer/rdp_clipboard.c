//
// Created by ubuntu on 24.09.2020.
//

#include <stdlib.h>

#include "rdp_viewer_window.h"
#include "rdp_clipboard.h"

typedef enum {
    RDP_CLIPBOARD_FORMATLIST,
    RDP_CLIPBOARD_GET_DATA,
    RDP_CLIPBOARD_SET_DATA,
    RDP_CLIPBOARD_SET_CONTENT
} RdpClipboardEventType;

typedef struct {
    ExtendedRdpContext *ex_context;
    RdpClipboard *clipboard;
    RdpClipboardEventType rdp_clipboard_event_type;
    //gpointer custom_data; // Зависит от  типа rdp_clipboard_event_type
    GtkTargetList *target_list;

} RdpClipboardEventData;

// We cant use GTK functions in thread so invoke callback which will beexecuted in the main thread
typedef gboolean (*RdpClipboardEventProcess) (RdpClipboardEventData *rdp_clipboard_event_data);

void rdp_cliprdr_request_data(GtkClipboard *gtkClipboard, GtkSelectionData *selection_data, guint info,
                              RdpClipboard* clipboard)
{
    g_info("%s", (const char *)__func__);

    /* Called by GTK when someone press "Paste" on the client side.
     * We ask to the server the data we need */

    // clipboard->context;
    //if ( clipboard->srv_clip_data_wait != SCDW_NONE ) {
    //    remmina_plugin_service->log_printf("[RDP] Cannot paste now, I’m transferring clipboard data from server. "
    //                                       "Try again later\n");
    //    return;
    //}

    clipboard->format = info;

    /* Request Clipboard content from the server, the request is async */
    CLIPRDR_FORMAT_DATA_REQUEST *pFormatDataRequest =
            (CLIPRDR_FORMAT_DATA_REQUEST*)calloc(1, sizeof(CLIPRDR_FORMAT_DATA_REQUEST));
    pFormatDataRequest->requestedFormatId = clipboard->format;

    clipboard->context->ClientFormatDataRequest(clipboard->context ,pFormatDataRequest);
    free(pFormatDataRequest);



    ///* Busy wait clibpoard data for CLIPBOARD_TRANSFER_WAIT_TIME seconds */
    //gettimeofday(&tv, NULL);
    //struct timespec to;
    //struct timeval tv;
    //int rc;
    //to.tv_sec = tv.tv_sec + CLIPBOARD_TRANSFER_WAIT_TIME;
    //to.tv_nsec = tv.tv_usec * 1000;
    //rc = pthread_cond_timedwait(&clipboard->transfer_clip_cond, &clipboard->transfer_clip_mutex, &to);
    //
    //if ( rc == 0 ) {
    //    /* Data has arrived without timeout */
    //    if (clipboard->srv_data != NULL) {
    //        if (info == CB_FORMAT_PNG || info == CF_DIB || info == CF_DIBV5 || info == CB_FORMAT_JPEG) {
    //            gtk_selection_data_set_pixbuf(selection_data, clipboard->srv_data);
    //            g_object_unref(clipboard->srv_data);
    //        }else  {
    //            gtk_selection_data_set_text(selection_data, clipboard->srv_data, -1);
    //            free(clipboard->srv_data);
    //        }
    //    }
    //    clipboard->srv_clip_data_wait = SCDW_NONE;
    //} else {
    //    clipboard->srv_clip_data_wait = SCDW_ASYNCWAIT;
    //    if ( rc == ETIMEDOUT ) {
    //        remmina_plugin_service->log_printf("[RDP] Clipboard data has not been transferred from the server in %d seconds. Try to paste later.\n",
    //                                           CLIPBOARD_TRANSFER_WAIT_TIME);
    //    }else  {
    //        remmina_plugin_service->log_printf("[RDP] internal error: pthread_cond_timedwait() returned %d\n", rc);
    //        clipboard->srv_clip_data_wait = SCDW_NONE;
    //    }
    //}
    //pthread_mutex_unlock(&clipboard->transfer_clip_mutex);
}

void rdp_cliprdr_empty_clipboard(GtkClipboard *gtkClipboard G_GNUC_UNUSED, RdpClipboard *clipboard G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
}

void rdp_cliprdr_set_clipboard_data(RdpClipboardEventData *rdp_clipboard_event_data)
{
    g_info("%s", (const char *)__func__);

    gint n_targets;
    ExtendedRdpContext *ex_context = rdp_clipboard_event_data->ex_context;
    GtkTargetEntry *targets = gtk_target_table_new_from_list(rdp_clipboard_event_data->target_list, &n_targets);

    // get the first window
    RdpWindowData *rdp_window_data = g_array_index(ex_context->rdp_windows_array, RdpWindowData *, 0);
    GtkClipboard* gtkClipboard = gtk_widget_get_clipboard(rdp_window_data->rdp_viewer_window, GDK_SELECTION_CLIPBOARD);

    if (gtkClipboard && targets) {
        gtk_clipboard_set_with_data(gtkClipboard, targets, n_targets,
                                     (GtkClipboardGetFunc)rdp_cliprdr_request_data,
                                     (GtkClipboardClearFunc)rdp_cliprdr_empty_clipboard, ex_context);
        gtk_target_table_free(targets, n_targets);
    }

    gtk_target_list_unref(rdp_clipboard_event_data->target_list);
}

//void rdp_cliprdr_set_clipboard_content(RdpClipboardEventData *rdp_clipboard_event_data)
//{
//    g_info("%s", (const char *)__func__);
//
//    GtkClipboard* gtkClipboard;
//    ExtendedRdpContext *ex_context = rdp_clipboard_event_data->ex_context;
//
//
//    RdpWindowData *rdp_window_data = g_array_index(ex_context->rdp_windows_array, RdpWindowData *, 0);
//    gtkClipboard = gtk_widget_get_clipboard(rdp_window_data->rdp_viewer_window, GDK_SELECTION_CLIPBOARD);
//
//    if (ui->clipboard.format == CB_FORMAT_PNG || ui->clipboard.format == CF_DIB ||
//    ui->clipboard.format == CF_DIBV5 || ui->clipboard.format == CB_FORMAT_JPEG) {
//        gtk_clipboard_set_image( gtkClipboard, ui->clipboard.data );
//        g_object_unref(ui->clipboard.data);
//    }else  {
//        gtk_clipboard_set_text( gtkClipboard, ui->clipboard.data, -1 );
//        free(ui->clipboard.data);
//    }
//}

// Executed in the main thread (GTK requirement)
static gboolean rdp_cliprdr_event_process(RdpClipboardEventData *rdp_clipboard_event_data)
{
    g_info("%s", (const char *)__func__);
    if (!rdp_clipboard_event_data)
        return FALSE;

    switch (rdp_clipboard_event_data->rdp_clipboard_event_type) {

        case RDP_CLIPBOARD_FORMATLIST:
            //rdp_cliprdr_mt_get_format_list(gp, ui);
            break;

        case RDP_CLIPBOARD_GET_DATA:
            //rdp_cliprdr_get_clipboard_data(gp, ui);
            break;

        case RDP_CLIPBOARD_SET_DATA:
            rdp_cliprdr_set_clipboard_data(rdp_clipboard_event_data);
            break;

        case RDP_CLIPBOARD_SET_CONTENT:
            //rdp_cliprdr_set_clipboard_content(gp, ui);
            break;
    }

    // free
    //if (rdp_clipboard_event_data->custom_data)
    //    free(rdp_clipboard_event_data->custom_data);
    free(rdp_clipboard_event_data);

    return FALSE;
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

static UINT rdp_cliprdr_send_client_format_list(RdpClipboard* clipboard)
{
    g_info("%s", (const char *)__func__);
    CLIPRDR_FORMAT* formats = NULL;
    CLIPRDR_FORMAT_LIST formatList = { 0 };

    UINT ret;
    UINT32 numFormats = 3;//clipboard->numClientFormats;

    if (numFormats) {
        if (!(formats = (CLIPRDR_FORMAT*)calloc(numFormats, sizeof(CLIPRDR_FORMAT)))) {
            g_info("failed to allocate %" PRIu32 " CLIPRDR_FORMAT structs", numFormats);
            return CHANNEL_RC_NO_MEMORY;
        }
    }

    //for (UINT32 i = 0; i < numFormats; i++) {
    //    formats[i].formatId = clipboard->clientFormats[i].formatId;
    //    formats[i].formatName = clipboard->clientFormats[i].formatName;
    //}
    formats[0].formatId = CF_RAW;
    formats[0].formatName = "_FREERDP_RAW";
    formats[1].formatId = CF_UNICODETEXT;
    formats[1].formatName = "UTF8_STRING";
    formats[2].formatId = CB_FORMAT_HTML;
    formats[2].formatName = "HTML Format";

    formatList.msgFlags = CB_RESPONSE_OK;
    formatList.numFormats = numFormats;
    formatList.formats = formats;
    formatList.msgType = CB_FORMAT_LIST;
    ret = clipboard->context->ClientFormatList(clipboard->context, &formatList);
    free(formats);

    //xfContext* xfc = clipboard->xfc;
    //if (clipboard->owner && clipboard->owner != xfc->drawable)
    //{
    //    /* Request the owner for TARGETS, and wait for SelectionNotify event */
    //    XConvertSelection(xfc->display, clipboard->clipboard_atom, clipboard->targets[1],
    //                      clipboard->property_atom, xfc->drawable, CurrentTime);
    //}

    return ret;
}

static UINT rdp_cliprdr_monitor_ready(CliprdrClientContext* context, const CLIPRDR_MONITOR_READY* monitorReady)
{
    g_info("%s", (const char *)__func__);
    RdpClipboard* clipboard = (RdpClipboard*)context->custom;

    INT ret;

    if ((ret = rdp_cliprdr_send_client_capabilities(clipboard)) != CHANNEL_RC_OK)
        return ret;

    if ((ret = rdp_cliprdr_send_client_format_list(clipboard)) != CHANNEL_RC_OK)
        return ret;

    return CHANNEL_RC_OK;
}

static UINT rdp_cliprdr_server_capabilities(CliprdrClientContext* context, const CLIPRDR_CAPABILITIES* capabilities)
{
    g_info("%s", (const char *)__func__);
    return CHANNEL_RC_OK;
}

// Эта функция вызывается когда копируешь что-то на ВМ
static UINT rdp_cliprdr_server_format_list(CliprdrClientContext* context, const CLIPRDR_FORMAT_LIST* formatList)
{
    g_info("%s", (const char *)__func__);
    /* Called when a user do a "Copy" on the server: we collect all formats
     * the server send us and then setup the local clipboard with the appropiate
     * functions to request server data */

    RdpClipboard *clipboard = (RdpClipboard*)context->custom;

    GtkTargetList *list = gtk_target_list_new(NULL, 0); // will be freed in callback

    for (int i = 0; i < formatList->numFormats; i++) {
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

    g_idle_add((GSourceFunc)rdp_cliprdr_event_process, rdp_clipboard_event_data);

    /* Send FormatListResponse to server */
    CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse;
    formatListResponse.msgType = CB_FORMAT_LIST_RESPONSE;
    formatListResponse.msgFlags = CB_RESPONSE_OK; // Can be CB_RESPONSE_FAIL in case of error
    formatListResponse.dataLen = 0;

    return context->ClientFormatListResponse(clipboard->context, &formatListResponse);
}

static UINT rdp_cliprdr_server_format_list_response(CliprdrClientContext* context,
        const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
    g_info("%s", (const char *)__func__);
    return CHANNEL_RC_OK;
}

static UINT rdp_cliprdr_server_format_data_request(CliprdrClientContext* context,
        const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
    g_info("%s", (const char *)__func__);

    //RemminaPluginRdpUiObject* ui;
    //RemminaProtocolWidget* gp;
    //rfClipboard* clipboard;
    //
    //clipboard = (rfClipboard*)context->custom;
    //gp = clipboard->rfi->protocol_widget;
    //
    //ui = g_new0(RemminaPluginRdpUiObject, 1);
    //ui->type = REMMINA_RDP_UI_CLIPBOARD;
    //ui->clipboard.clipboard = clipboard;
    //ui->clipboard.type = REMMINA_RDP_UI_CLIPBOARD_GET_DATA;
    //ui->clipboard.format = formatDataRequest->requestedFormatId;
    //remmina_rdp_event_queue_ui_sync_retint(gp, ui);

    return CHANNEL_RC_OK;
}

// Вызывается когда у себя делаешь Вставить первый раз (Должна)
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
                size = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*)data, size / 2, (CHAR**)&output, 0, NULL, NULL);
                crlf2lf(output, &size);
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

    //pthread_mutex_lock(&clipboard->transfer_clip_mutex);
    //pthread_cond_signal(&clipboard->transfer_clip_cond);
    //if ( clipboard->srv_clip_data_wait == SCDW_BUSY_WAIT ) {
    //    clipboard->srv_data = output;
    //}else  {
    //    // Clipboard data arrived from server when we are not busywaiting.
    //    // Just put it on the local clipboard
    //
    //    ui = g_new0(RemminaPluginRdpUiObject, 1);
    //    ui->type = REMMINA_RDP_UI_CLIPBOARD;
    //    ui->clipboard.clipboard = clipboard;
    //    ui->clipboard.type = REMMINA_RDP_UI_CLIPBOARD_SET_CONTENT;
    //    ui->clipboard.data = output;
    //    ui->clipboard.format = clipboard->format;
    //    remmina_rdp_event_queue_ui_sync_retint(gp, ui);
    //
    //    clipboard->srv_clip_data_wait = SCDW_NONE;
    //
    //}
    //pthread_mutex_unlock(&clipboard->transfer_clip_mutex);

    return CHANNEL_RC_OK;
}

//GdkDisplay *display = gdk_display_get_default();
//GtkClipboard *gtk_clipboard = gtk_clipboard_get_for_display(display, GDK_SELECTION_CLIPBOARD);
void rdp_cliprdr_init(ExtendedRdpContext *ex_context, CliprdrClientContext *cliprdr)
{
    RdpClipboard* clipboard = (RdpClipboard*)calloc(1, sizeof(rdpPointer));
    clipboard->context = cliprdr;
    cliprdr->custom = (void*)clipboard;
    clipboard->ex_context = ex_context;

    cliprdr->MonitorReady = rdp_cliprdr_monitor_ready;
    cliprdr->ServerCapabilities = rdp_cliprdr_server_capabilities;
    cliprdr->ServerFormatList = rdp_cliprdr_server_format_list;
    cliprdr->ServerFormatListResponse = rdp_cliprdr_server_format_list_response;
    cliprdr->ServerFormatDataRequest = rdp_cliprdr_server_format_data_request;
    cliprdr->ServerFormatDataResponse = rdp_cliprdr_server_format_data_response;
}

void rdp_cliprdr_uninit(ExtendedRdpContext *ex_context, CliprdrClientContext* cliprdr)
{
    if (cliprdr->custom) {
        RdpClipboard* clipboard = (RdpClipboard*)cliprdr->custom;
        clipboard->context = NULL;
        free(cliprdr->custom);
    }
    cliprdr->custom = NULL;
}
