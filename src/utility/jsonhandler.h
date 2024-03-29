/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef THIN_CLIENT_VEIL_JSONHANDLER_H
#define THIN_CLIENT_VEIL_JSONHANDLER_H

#include <json-glib/json-glib.h>

#include "remote-viewer-util.h"


JsonObject *get_root_json_object(JsonParser *parser, const gchar *data);
//JsonArray *get_json_array(JsonParser *parser, const gchar *data);

gint64 json_object_get_int_member_safely(JsonObject  *object, const gchar *member_name);
const gchar *json_object_get_string_member_safely(JsonObject  *object,const gchar *member_name);
JsonArray *json_object_get_array_member_safely(JsonObject *object, const gchar *member_name);
JsonObject *json_object_get_object_member_safely(JsonObject  *object, const gchar *member_name);
gboolean json_object_get_bool_member_safely(JsonObject *object, const gchar *member_name);

// Return data_object pointer or errors_object pointer or NULL (return value can be determined by server_reply_type)
// Every reply from VDI must contain errors or data
JsonObject *json_get_data_or_errors_object(JsonParser *parser, const gchar *json_str,
        ServerReplyType *server_reply_type);
// Same as json_get_data_or_errors_object but for ECP VeiL
JsonObject *json_get_data_or_errors_object_ecp(JsonParser *parser, const gchar *json_str,
                                               ServerReplyType *server_reply_type);

//gchar *string_to_json_value(const gchar *string);

gchar *json_generate_from_builder(JsonBuilder *builder);

JsonObject *json_handler_gobject_dump(GObject *gobject);
JsonNode *json_handler_gobject_serialize(GObject *gobject);
gchar *json_handler_gobject_to_data(GObject *gobject, gsize *length);

GObject *json_handler_gobject_from_file(const gchar *file_name, GType gtype);
void json_handler_gobject_to_file(const gchar *file_name, GObject *g_object);

#endif //THIN_CLIENT_VEIL_JSONHANDLER_H
