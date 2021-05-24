/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <stdio.h>

#include "jsonhandler.h"

JsonObject *get_root_json_object(JsonParser *parser, const gchar *data)
{

    gboolean result = json_parser_load_from_data (parser, data, -1, NULL);
    if(!result)
        return NULL;

    JsonNode *root = json_parser_get_root (parser);
    if(!JSON_NODE_HOLDS_OBJECT (root))
        return NULL;

    JsonObject *object = json_node_get_object (root);
    return object;
}

gint64 json_object_get_int_member_safely(JsonObject *object, const gchar *member_name)
{
    if (object && json_object_has_member(object, member_name))
        return json_object_get_int_member(object, member_name);

    g_info("json member '%s' does not exist", member_name);
    return 0;
}

const gchar *json_object_get_string_member_safely(JsonObject *object, const gchar *member_name)
{
    if (object && json_object_has_member(object, member_name)) {

        JsonNode *json_node = json_object_get_member(object, member_name);
        if (JSON_NODE_HOLDS_VALUE(json_node))
            return json_object_get_string_member(object, member_name);
    }

    g_info("json member '%s' does not exist", member_name);
    return "";
}

JsonArray *json_object_get_array_member_safely(JsonObject *object, const gchar *member_name)
{
    if (object && json_object_has_member(object, member_name))
        return json_object_get_array_member(object, member_name);

    g_info("json member '%s' does not exist", member_name);
    return NULL;
}

JsonObject *json_object_get_object_member_safely(JsonObject *object, const gchar *member_name)
{
    if (object && json_object_has_member(object, member_name)) {

        JsonNode *json_node = json_object_get_member(object, member_name);
        if (JSON_NODE_HOLDS_OBJECT(json_node))
            return json_object_get_object_member(object, member_name);
    }

    g_info("json object member '%s' does not exist", member_name);
    return NULL;
}

JsonObject *jsonhandler_get_data_or_errors_object(JsonParser *parser, const gchar *json_str,
        ServerReplyType *server_reply_type)
{
    if (!json_str)
        goto total_fail;

    JsonObject *root_object = get_root_json_object(parser, json_str);
    if (!root_object)
        goto total_fail;

    // errors
    if (json_object_has_member(root_object, "errors")) {

        JsonArray *errors_json_array = json_object_get_array_member_safely(root_object, "errors");

        if (errors_json_array && json_array_get_length(errors_json_array) > 0) {
            JsonObject *error_json_object_0 = json_array_get_object_element(errors_json_array, (guint) 0);

            if (error_json_object_0) {
                *server_reply_type = SERVER_REPLY_TYPE_ERROR;
                return error_json_object_0;
            }
            else {
                g_info("Errors json first element in is NOT json object!");
                goto total_fail;
            }
        }
        else {
            g_info("Errors json array is empty!");
            goto total_fail;
        }
    }
    // data
    else if (json_object_has_member(root_object, "data")) {
        *server_reply_type = SERVER_REPLY_TYPE_DATA;
        JsonObject *data_member_object = json_object_get_object_member_safely(root_object, "data");
        return data_member_object;
    }

total_fail:
    *server_reply_type = SERVER_REPLY_TYPE_UNKNOWN;
    return NULL;
}

gchar *string_to_json_value(const gchar *string)
{
    gchar *json_str_rep;
    if (string)
        json_str_rep = g_strdup_printf("\"%s\"", string);
    else
        json_str_rep = g_strdup("null");
    return json_str_rep;
}

/*
 * Generate json string. Must be freed
 */
gchar *json_generate_from_builder(JsonBuilder *builder)
{
    JsonGenerator *gen = json_generator_new();
    JsonNode * root = json_builder_get_root(builder);
    json_generator_set_root(gen, root);

    gchar *json_string = json_generator_to_data(gen, NULL);

    json_node_free(root);
    g_object_unref(gen);

    return json_string;
}