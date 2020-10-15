#include <stdio.h>
#include <pthread.h>

#include <libsoup/soup-session.h>
#include <libsoup/soup-message.h>
#include <libsoup/soup-websocket.h>
#include <gio/gio.h>

#include "remote-viewer-util.h"

#include "config.h"
#include "async.h"
#include "vdi_ws_client.h"
#include "settingsfile.h"

#define TIMEOUT 1000000 // 1 sec
#define WS_READ_TIMEOUT 200000

// static functions declarations
static void protocol_switching_callback(SoupMessage *ws_msg, gpointer user_data);
static void *async_create_ws_connect(VdiWsClient *vdi_ws_client);

// implementations

static void protocol_switching_callback(SoupMessage *ws_msg, gpointer user_data)
{

    g_info("In %s :thread id = %lu", (const char *)__func__, pthread_self());
    VdiWsClient *vdi_ws_client = user_data;

    g_info("WS: %s %i", (const char *)__func__, vdi_ws_client->test_int);
    vdi_ws_client->stream = soup_session_steal_connection(vdi_ws_client->ws_soup_session, ws_msg);
}

static void *async_create_ws_connect(VdiWsClient *vdi_ws_client)
{
    // hand shake preparation
    SoupMessage *ws_msg = soup_message_new("GET", vdi_ws_client->vdi_url);
    if (ws_msg == NULL) {
        g_info("%s Cant parse message", (const char *)__func__);
        return NULL;
    }
    soup_websocket_client_prepare_handshake(ws_msg, NULL, NULL);
    soup_message_headers_replace(ws_msg->request_headers, "Connection", "keep-alive, Upgrade");

    soup_message_add_status_code_handler(ws_msg, "got-informational",
                          SOUP_STATUS_SWITCHING_PROTOCOLS,
                          G_CALLBACK (protocol_switching_callback), vdi_ws_client);

    while (vdi_ws_client->is_running) {
        vdi_ws_client->stream = NULL;

        // make handshake
        guint status = soup_session_send_message(vdi_ws_client->ws_soup_session, ws_msg);

        if (vdi_ws_client->stream == NULL) {
            g_idle_add((GSourceFunc)vdi_ws_client->ws_data_received_callback, (gpointer)FALSE); // notify GUI
            // try to reconnect (go to another loop)
            cancellable_sleep(TIMEOUT, &vdi_ws_client->is_running);
            continue;
        } else {
            g_info("WS: %s: SUCCESSFUL HANDSHAKE %i", (const char *)__func__, status);
            g_idle_add((GSourceFunc)vdi_ws_client->ws_data_received_callback, (gpointer)TRUE);
        }

        GInputStream *inputStream = g_io_stream_get_input_stream(vdi_ws_client->stream);

        enum { buf_length = 2 };
        gchar buffer[buf_length];
        gsize bytes_read = 0;

        const guint8 max_read_try = 3; // maximum amount of read tries
        guint8 read_try_count = 0; // read try counter

        // receiving messages
        while (vdi_ws_client->is_running && !g_io_stream_is_closed(vdi_ws_client->stream)) {
            //GError *error = NULL;
            /*gboolean res = */g_input_stream_read_all(inputStream,
                                                   &buffer,
                                                   buf_length, &bytes_read,
                                                   vdi_ws_client->cancel_job, NULL);
            //g_info("WS: %s res: %i bytes_read: %lu\n", (const char *)__func__, res, bytes_read);

            if (bytes_read == 0) {
                read_try_count ++;

                if (read_try_count == max_read_try) {
                    g_idle_add((GSourceFunc)vdi_ws_client->ws_data_received_callback, (gpointer)FALSE); // notify GUI
                    break; // try to reconnect (another loop)
                }
            } else { // successfull read
                read_try_count = 0;
                g_idle_add((GSourceFunc)vdi_ws_client->ws_data_received_callback, (gpointer)TRUE); // notify GUI
            }
            cancellable_sleep(WS_READ_TIMEOUT, &vdi_ws_client->is_running); // sec
        }

        // close stream
        if (vdi_ws_client->stream)
            g_io_stream_close(vdi_ws_client->stream, NULL, NULL);
    }

    g_object_unref(ws_msg);

    return NULL;
}

void start_vdi_ws_polling(VdiWsClient *vdi_ws_client, const gchar *vdi_ip, int vdi_port,
                          WsDataReceivedCallback ws_data_received_callback)
{
    g_info("%s", (const char *)__func__);
    g_info("In %s :thread id = %lu", (const char *)__func__, pthread_self());
    gboolean ssl_strict = FALSE;
    vdi_ws_client->ws_soup_session = soup_session_new_with_options("idle-timeout", 0, "timeout", 0,
                                                                   "ssl-strict", ssl_strict, NULL);

    vdi_ws_client->ws_data_received_callback = ws_data_received_callback;
    vdi_ws_client->test_int = 666;// temp trash test
    const gchar *http_protocol = determine_http_protocol_by_port(vdi_port);

    gboolean is_nginx_vdi_prefix_disabled = read_int_from_ini_file("General", "is_nginx_vdi_prefix_disabled", 0);
    if (is_nginx_vdi_prefix_disabled)
        vdi_ws_client->vdi_url = g_strdup_printf("%s://%s:%i/ws/client/vdi_server_check",
                http_protocol, vdi_ip, vdi_port);
    else
        vdi_ws_client->vdi_url = g_strdup_printf("%s://%s:%i/%s/ws/client/vdi_server_check",
            http_protocol, vdi_ip, vdi_port, NGINX_VDI_API_PREFIX);

    vdi_ws_client->is_running = TRUE;
    vdi_ws_client->cancel_job = g_cancellable_new();

    vdi_ws_client->thread = g_thread_new(NULL, (GThreadFunc)async_create_ws_connect, vdi_ws_client);
}

void stop_vdi_ws_polling(VdiWsClient *vdi_ws_client)
{
    // cancell
    vdi_ws_client->is_running = FALSE;
    g_cancellable_cancel(vdi_ws_client->cancel_job);
    soup_session_abort(vdi_ws_client->ws_soup_session);

    // for syncronization. wait untill thread ws job finished.
    g_thread_join(vdi_ws_client->thread);

    // free memory
    g_object_unref(vdi_ws_client->ws_soup_session);
    g_free(vdi_ws_client->vdi_url);
    g_object_unref(vdi_ws_client->cancel_job);
    g_info("%s", (const char *)__func__);
}
