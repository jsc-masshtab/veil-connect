/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <stdlib.h>

#include <glib/gi18n.h>

#include "loudplay_control_server.h"
#include "remote-viewer-util.h"
#include "jsonhandler.h"

#define READ_BUFFER_SIZE 2048

#define MSG_TYPE_CONFIG 13
#define MSG_TYPE_STREAMING 14

#define EVENT_STREAMING_START 1
#define EVENT_STREAMING_FINISH 2

G_DEFINE_TYPE( LoudplayControlServer, loudplay_control_server, G_TYPE_OBJECT )

typedef struct {
    GSocketConnection *connection;
    GIOChannel *channel;
    LoudplayControlServer *server_weak_ptr;
} ConnData;

static void free_conn_data(ConnData *conn_data)
{
    if (conn_data->channel)
        g_io_channel_unref(conn_data->channel);
    g_object_unref(conn_data->connection);
    //g_info("!!! Before free(conn_data);");
    free(conn_data);
    //g_info("!!! After free(conn_data);");
}

static void loudplay_control_server_finalize( GObject *object );

static void loudplay_control_server_parse_msg(LoudplayControlServer *self, gchar *buffer)
{
    JsonParser *parser = json_parser_new();

    JsonObject *root_object = get_root_json_object(parser, buffer);
    gint64 type = json_object_get_int_member_safely(root_object, "type");
    if (type == MSG_TYPE_CONFIG) {
        // Update config
        atomic_string_set(&self->loudplay_config_json, buffer);
        // Emit signal
        g_signal_emit_by_name(self, "loudplay-config-updated");
        g_signal_emit_by_name(self, "event-happened", _("Settings updated by loudplay client"));
    } else if (type == MSG_TYPE_STREAMING) {
        gint64 event = json_object_get_int_member_safely(root_object, "event");
        if (event == EVENT_STREAMING_START) {
            g_signal_emit_by_name(self, "event-happened", _("Streaming started"));
        }
    }

    g_object_unref(parser);
}

static gboolean
loudplay_control_server_network_read(GIOChannel *channel, GIOCondition cond, ConnData *conn_data)
{
    if(cond == G_IO_HUP) {
        g_info("cond == G_IO_HUP");
        conn_data->server_weak_ptr->connections = g_list_remove(conn_data->server_weak_ptr->connections, conn_data);
        free_conn_data(conn_data);
        return FALSE;
    }

    gchar buffer[READ_BUFFER_SIZE] = {};
    GError *error = NULL;
    GIOStatus ret = g_io_channel_read_chars(channel, buffer, READ_BUFFER_SIZE, NULL, NULL);

    if (ret == G_IO_STATUS_ERROR || ret == G_IO_STATUS_EOF) {
        g_info("Stop");
        if (error)
            g_warning ("Error reading: %s\n", error->message);
        // Drop last reference on connection
        conn_data->server_weak_ptr->connections = g_list_remove(conn_data->server_weak_ptr->connections, conn_data);
        free_conn_data(conn_data);
        g_clear_error(&error);

        return FALSE;
    } else {
        buffer[READ_BUFFER_SIZE - 1] = '\0'; // Чтобы строка точно имела завершающий нуль
        g_info("%s Got: %s\n", (const char *)__func__, buffer);
        // Parse msg
        loudplay_control_server_parse_msg(conn_data->server_weak_ptr, buffer);
    }

    return TRUE;
}

static gboolean
loudplay_control_server_new_connection(GSocketService *service G_GNUC_UNUSED,
               GSocketConnection *connection,
               GObject *source_object G_GNUC_UNUSED,
               gpointer user_data)
{
    LoudplayControlServer *self = (LoudplayControlServer *)user_data;

    ConnData *conn_data = (ConnData*)calloc(1, sizeof(ConnData));
    conn_data->server_weak_ptr = self;
    conn_data->connection = g_object_ref(connection); /* Tell glib not to disconnect */
    self->connections = g_list_append(self->connections, conn_data);

    GSocketAddress *sockaddr = g_socket_connection_get_remote_address(connection, NULL);
    GInetAddress *addr = g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(sockaddr));
    guint16 port = g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(sockaddr));

    g_info("New Connection from %s:%d\n", g_inet_address_to_string(addr), port);

    // Receive msg callback
    GSocket *socket = g_socket_connection_get_socket(connection);

    gint fd = g_socket_get_fd(socket);
#ifdef G_OS_WIN32
    conn_data->channel = g_io_channel_win32_new_fd(fd);
#else
    conn_data->channel = g_io_channel_unix_new(fd);
#endif
    // Pass conn_data as user data to the watch callback
    if (conn_data->channel)
        g_io_add_watch(conn_data->channel, G_IO_IN, (GIOFunc) loudplay_control_server_network_read, conn_data);

    // Send config to client
    loudplay_control_server_send_cur_config(self, conn_data->connection);

    return TRUE;
}

static void loudplay_control_server_class_init( LoudplayControlServerClass *klass ) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = loudplay_control_server_finalize;

    // signals
    g_signal_new("loudplay-config-updated",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(LoudplayControlServerClass, loudplay_config_updated),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
    g_signal_new("event-happened",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(LoudplayControlServerClass, event_happened),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE,
                 1,
                 G_TYPE_STRING);
}

static void loudplay_control_server_init( LoudplayControlServer *self )
{
    atomic_string_init(&self->loudplay_config_json);

    self->connections = NULL;

    self->service = g_socket_service_new();
    GInetAddress *address = g_inet_address_new_from_string("0.0.0.0");
    GSocketAddress *socket_address = g_inet_socket_address_new(address, CONTROL_SERVER_PORT);
    g_socket_listener_add_address(G_SOCKET_LISTENER(self->service), socket_address, G_SOCKET_TYPE_STREAM,
                                  G_SOCKET_PROTOCOL_TCP, NULL, NULL, NULL);

    g_object_unref(socket_address);
    g_object_unref(address);

    g_signal_connect(self->service, "incoming",
            G_CALLBACK(loudplay_control_server_new_connection), self);
}

static void loudplay_control_server_finalize( GObject *object )
{
    g_info("%s", (const char *)__func__);

    LoudplayControlServer *self = LOUDPLAY_CONTROL_SERVER(object);
    loudplay_control_server_stop(self);
    atomic_string_deinit(&self->loudplay_config_json);

    GObjectClass *parent_class = G_OBJECT_CLASS( loudplay_control_server_parent_class );
    ( *parent_class->finalize )( object );
}

LoudplayControlServer *loudplay_control_server_new()
{
    LoudplayControlServer *obj = LOUDPLAY_CONTROL_SERVER( g_object_new(
            TYPE_LOUDPLAY_CONTROL_SERVER, NULL ) );
    return obj;
}

void loudplay_control_server_start(LoudplayControlServer *self, LoudPlayConfig *loudplay_config)
{
    // Заранее сохранем теущий конфиг
    loudplay_control_server_update_config(self, loudplay_config);
    // Start
    g_socket_service_start(self->service);
}

void loudplay_control_server_stop(LoudplayControlServer *self)
{
    if (self->connections) {
        g_list_free_full(self->connections, (GDestroyNotify) free_conn_data);
        self->connections = NULL;
    }

    g_socket_service_stop(self->service);
}

void loudplay_control_server_update_config(LoudplayControlServer *self,
                                           LoudPlayConfig *loudplay_config)
{
    g_autofree gchar *gobject_to_json_str = NULL;
    gsize length;
    gobject_to_json_str = json_handler_gobject_to_data((GObject *)loudplay_config, &length);

    g_autofree gchar *gobject_to_json_str_moded = NULL;
    gobject_to_json_str_moded = replace_str(gobject_to_json_str, "-", "_");
    atomic_string_set(&self->loudplay_config_json, gobject_to_json_str_moded);
}

void loudplay_control_server_send_cur_config(LoudplayControlServer *self,
                                             GSocketConnection *connection)
{
    if (!connection || !g_socket_connection_is_connected(connection))
        return;

    g_autofree gchar *config_json_str = NULL;
    config_json_str = atomic_string_get(&self->loudplay_config_json);

    if (config_json_str) {
        GOutputStream *out_stream = g_io_stream_get_output_stream(G_IO_STREAM(connection));
        GError *error = NULL;
        g_output_stream_write_async(out_stream, config_json_str,
                              strlen_safely(config_json_str), G_PRIORITY_DEFAULT,
                              NULL, NULL, NULL);
        if (error)
            g_warning ("Error g_output_stream_write: %s\n", error->message);
        g_clear_error(&error);
    }
}

void loudplay_control_server_send_cur_config_all(LoudplayControlServer *self)
{
    for (GList *l = self->connections; l != NULL; l = l->next) {
        ConnData *conn_data = (ConnData *)l->data;
        loudplay_control_server_send_cur_config(self, conn_data->connection);
    }

    g_signal_emit_by_name(self, "loudplay-config-updated");
}

gchar *loudplay_control_server_get_cur_config(LoudplayControlServer *self)
{
    return atomic_string_get(&self->loudplay_config_json);
}

void loudplay_control_server_send_stop_streaming(LoudplayControlServer *self)
{
    // Generate msg
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "type");
    json_builder_add_int_value(builder, MSG_TYPE_STREAMING);

    json_builder_set_member_name(builder, "event");
    json_builder_add_int_value(builder, EVENT_STREAMING_FINISH);

    json_builder_end_object(builder);

    JsonGenerator *gen = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(gen, root);

    g_autofree gchar *cmd = NULL;
    cmd = json_generator_to_data(gen, NULL);

    json_node_free(root);
    g_object_unref(gen);
    g_object_unref(builder);

    // Send
    for (GList *l = self->connections; l != NULL; l = l->next) {
        ConnData *conn_data = (ConnData *)l->data;
        GSocketConnection *conn = conn_data->connection;

        if (conn || g_socket_connection_is_connected(conn)) {

            GOutputStream *out_stream = g_io_stream_get_output_stream(G_IO_STREAM(conn));
            GError *error = NULL;
            g_output_stream_write_async(out_stream, cmd,
                                        strlen_safely(cmd), G_PRIORITY_DEFAULT,
                                        NULL, NULL, NULL);
            if (error)
                g_warning ("Error g_output_stream_write: %s\n", error->message);
        }
    }
}
