/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_LOUDPLAY_CONTROL_SERVER_H
#define VEIL_CONNECT_LOUDPLAY_CONTROL_SERVER_H

#include <gtk/gtk.h>

#include "loudplay/loudplay_config.h"
#include "remote-viewer-util.h"
#include "atomic_string.h"

#define CONTROL_SERVER_PORT 28105

#define TYPE_LOUDPLAY_CONTROL_SERVER ( loudplay_control_server_get_type() )
#define LOUDPLAY_CONTROL_SERVER( obj ) \
( G_TYPE_CHECK_INSTANCE_CAST( (obj), TYPE_LOUDPLAY_CONTROL_SERVER, LoudplayControlServer ) )

typedef struct _LoudplayControlServer      LoudplayControlServer;
typedef struct _LoudplayControlServerClass LoudplayControlServerClass;

struct _LoudplayControlServer {
    GObject parent;

    GSocketService *service;
    GList *connections;

    AtomicString loudplay_config_json;
};

struct _LoudplayControlServerClass {
    GObjectClass parent_class;

    /* signals */
    void (*streaming_started)(LoudplayControlServer *self);
    void (*loudplay_config_updated)(LoudplayControlServer *self);
    void (*event_happened)(LoudplayControlServer *self, const gchar *text);
};

GType loudplay_control_server_get_type( void ) G_GNUC_CONST;
LoudplayControlServer *loudplay_control_server_new(void);

void loudplay_control_server_start(LoudplayControlServer *self, LoudPlayConfig *loudplay_config);
void loudplay_control_server_stop(LoudplayControlServer *self);

void loudplay_control_server_update_config(LoudplayControlServer *self,
        LoudPlayConfig *loudplay_config);
void loudplay_control_server_send_cur_config(LoudplayControlServer *self,
                                             GSocketConnection *connection);
void loudplay_control_server_send_cur_config_all(LoudplayControlServer *self);
gchar *loudplay_control_server_get_cur_config(LoudplayControlServer *self); // allocate memory

void loudplay_control_server_send_stop_streaming(LoudplayControlServer *self);


#endif //VEIL_CONNECT_LOUDPLAY_CONTROL_SERVER_H
