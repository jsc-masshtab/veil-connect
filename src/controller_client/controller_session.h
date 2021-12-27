/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_CONTROLLER_SESSION_H
#define VEIL_CONNECT_CONTROLLER_SESSION_H

#include <glib.h>
#include <gtk/gtk.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup-session.h>
#include <libsoup/soup-message.h>
#include <libsoup/soup-uri.h>

#include "atomic_string.h"


#define TYPE_CONTROLLER_SESSION  ( controller_session_get_type( ) )
#define CONTROLLER_SESSION( obj ) ( G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_CONTROLLER_SESSION, ControllerSession) )


typedef struct{
    gchar *name_filter;
    gchar *ordering;

    guint limit;
    guint offset;

} VmListRequestData;

typedef struct{
    GObject parent;

    SoupSession *soup_session;

    AtomicString username;
    AtomicString password;
    AtomicString address;
    AtomicString api_url;
    int port;

    gboolean is_ldap;

    AtomicString jwt;

    AtomicString last_error_phrase;
    AtomicString login_time;

    VmRemoteProtocol current_remote_protocol;

} ControllerSession;

typedef struct
{
    GObjectClass parent_class;

    /* signals */

} ControllerSessionClass;

GType controller_session_get_type( void ) G_GNUC_CONST;

ControllerSession *get_controller_session_static(void);
void controller_session_static_destroy(void);
ControllerSession *controller_session_new(void);

void controller_session_set_credentials(const gchar *username, const gchar *password);
void controller_session_set_conn_data(const gchar *ip, int port);
void controller_session_set_ldap(gboolean is_ldap);

void controller_session_cancel_pending_requests(void);

VmRemoteProtocol controller_session_get_current_remote_protocol(void);
void controller_session_set_current_remote_protocol(VmRemoteProtocol protocol);

gchar *controller_session_get_login_time(void);

// Extract error msg. Must be freed
gchar *controller_session_extract_error_msg(SoupMessage *msg, JsonObject *reply_json_object);
// Extract ipv4.
const gchar *controller_session_extract_ipv4(JsonObject *reply_json_object);

// Log out sync
gboolean controller_session_logout(void);

/// Functions for GTasks
// Fetch token
void controller_session_log_in_task(GTask *task,
                             gpointer       source_object,
                             gpointer       task_data,
                             GCancellable  *cancellable);

void controller_session_get_vm_list_task(GTask *task,
                                        gpointer       source_object,
                                        gpointer       task_data,
                                        GCancellable  *cancellable);

void controller_session_get_vm_data_task(GTask *task,
                                         gpointer       source_object,
                                         gpointer       task_data,
                                         GCancellable  *cancellable);


void controller_session_free_vm_list_request_data(VmListRequestData *vm_list_request_data);

#endif //VEIL_CONNECT_CONTROLLER_SESSION_H
