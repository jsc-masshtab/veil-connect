/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include<stdio.h>
#include<stdlib.h>

#include "remote-viewer-util.h"
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
    if (object && json_object_has_member(object, member_name)) {
        JsonNode *node = json_object_get_member(object, member_name);
        if (node && json_node_get_node_type(node) == JSON_NODE_VALUE)
            return json_node_get_int(node);
    }

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

gboolean json_object_get_bool_member_safely(JsonObject *object, const gchar *member_name)
{
    if (object && json_object_has_member(object, member_name))
        return json_object_get_boolean_member(object, member_name);

    g_info("json member '%s' does not exist", member_name);
    return FALSE;
}

JsonObject *json_get_data_or_errors_object(JsonParser *parser, const gchar *json_str,
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

JsonObject *json_get_data_or_errors_object_ecp(JsonParser *parser, const gchar *json_str,
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
    } else {
        *server_reply_type = SERVER_REPLY_TYPE_DATA;
        return root_object;
    }

    total_fail:
    *server_reply_type = SERVER_REPLY_TYPE_UNKNOWN;
    return NULL;
}
/*
gchar *string_to_json_value(const gchar *string)
{
    gchar *json_str_rep;
    if (string)
        json_str_rep = g_strdup_printf("\"%s\"", string);
    else
        json_str_rep = g_strdup("null");
    return json_str_rep;
}
*/
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


static JsonNode *json_serialize_pspec(const GValue *real_value, GParamSpec *pspec G_GNUC_UNUSED)
{
    JsonNode *retval = NULL;
    JsonNodeType node_type;

    switch (G_TYPE_FUNDAMENTAL (G_VALUE_TYPE (real_value)))
    {
        /* JSON native types */
        case G_TYPE_INT64:
            retval = json_node_init_int (json_node_alloc (), g_value_get_int64 (real_value));
            break;

        case G_TYPE_BOOLEAN:
            retval = json_node_init_boolean (json_node_alloc (), g_value_get_boolean (real_value));
            break;

        case G_TYPE_DOUBLE:
            retval = json_node_init_double (json_node_alloc (), g_value_get_double (real_value));
            break;

        case G_TYPE_STRING:
            retval = json_node_init_string (json_node_alloc (), g_value_get_string (real_value));
            break;

            /* auto-promoted types */
        case G_TYPE_INT:
            retval = json_node_init_int (json_node_alloc (), g_value_get_int (real_value));
            break;

        case G_TYPE_UINT:
            retval = json_node_init_int (json_node_alloc (), g_value_get_uint (real_value));
            break;

        case G_TYPE_LONG:
            retval = json_node_init_int (json_node_alloc (), g_value_get_long (real_value));
            break;

        case G_TYPE_ULONG:
            retval = json_node_init_int (json_node_alloc (), g_value_get_ulong (real_value));
            break;

        case G_TYPE_UINT64:
            retval = json_node_init_int (json_node_alloc (), (gint64)g_value_get_uint64 (real_value));
            break;

        case G_TYPE_FLOAT:
            retval = json_node_init_double (json_node_alloc (), g_value_get_float (real_value));
            break;

        case G_TYPE_CHAR:
            retval = json_node_alloc ();
            json_node_init_int (retval, g_value_get_schar (real_value));
            break;

        case G_TYPE_UCHAR:
            retval = json_node_init_int (json_node_alloc (), g_value_get_uchar (real_value));
            break;

        case G_TYPE_ENUM:
            retval = json_node_init_int (json_node_alloc (), g_value_get_enum (real_value));
            break;

        case G_TYPE_FLAGS:
            retval = json_node_init_int (json_node_alloc (), g_value_get_flags (real_value));
            break;

            /* complex types */
        case G_TYPE_BOXED:
            if (G_VALUE_HOLDS (real_value, G_TYPE_STRV))
            {
                gchar **strv = g_value_get_boxed (real_value);
                gint i, strv_len;
                JsonArray *array;

                strv_len = (gint)g_strv_length (strv);
                array = json_array_sized_new (strv_len);

                for (i = 0; i < strv_len; i++)
                {
                    JsonNode *str = json_node_new (JSON_NODE_VALUE);

                    json_node_set_string (str, strv[i]);
                    json_array_add_element (array, str);
                }

                retval = json_node_init_array (json_node_alloc (), array);
                json_array_unref (array);
            }
            else if (json_boxed_can_serialize (G_VALUE_TYPE (real_value), &node_type))
            {
                gpointer boxed = g_value_get_boxed (real_value);

                retval = json_boxed_serialize (G_VALUE_TYPE (real_value), boxed);
            }
            else
                g_warning ("Boxed type '%s' is not handled by JSON-GLib",
                           g_type_name (G_VALUE_TYPE (real_value)));
            break;

        case G_TYPE_OBJECT:
        {
            GObject *object = g_value_get_object (real_value);

            retval = json_node_alloc ();

            if (object != NULL)
            {
                json_node_init (retval, JSON_NODE_OBJECT);
                json_node_take_object (retval, json_handler_gobject_dump (object));
            }
            else
                json_node_init_null (retval);
        }
            break;

        case G_TYPE_NONE:
            retval = json_node_new (JSON_NODE_NULL);
            break;

        default:
            g_warning ("Unsupported type `%s'", g_type_name (G_VALUE_TYPE (real_value)));
            break;
    }

    return retval;
}

// Преобразовать GObject в JsonObject. json_gobject_dump нельзя использовать так как он не сериализует
// дефолтные значения.
JsonObject *json_handler_gobject_dump (GObject *gobject)
{
    JsonObject *object;
    GParamSpec **pspecs;
    guint n_pspecs, i;

    object = json_object_new ();

    pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (gobject), &n_pspecs);

    for (i = 0; i < n_pspecs; i++)
    {
        GParamSpec *pspec = pspecs[i];
        GValue value = { 0, };
        JsonNode *node = NULL;

        /* read only what we can */
        if (!(pspec->flags & G_PARAM_READABLE))
            continue;

        g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
        g_object_get_property (gobject, pspec->name, &value);

        node = json_serialize_pspec (&value, pspec);

        if (node)
            json_object_set_member (object, pspec->name, node);

        g_value_unset (&value);
    }

    g_free (pspecs);

    return object;
}

JsonNode *json_handler_gobject_serialize(GObject *gobject)
{
    JsonNode *retval;

    g_return_val_if_fail (G_IS_OBJECT (gobject), NULL);

    retval = json_node_new (JSON_NODE_OBJECT);
    json_node_take_object (retval, json_handler_gobject_dump (gobject));

    return retval;
}

gchar *json_handler_gobject_to_data(GObject *gobject, gsize *length)
{
    JsonGenerator *gen;
    JsonNode *root;
    gchar *data;

    g_return_val_if_fail (G_OBJECT (gobject), NULL);

    root = json_handler_gobject_serialize (gobject);

    gen = g_object_new (JSON_TYPE_GENERATOR,
                        "root", root,
                        "pretty", TRUE,
                        "indent", 2,
                        NULL);

    data = json_generator_to_data (gen, length);
    g_object_unref (gen);

    json_node_unref (root);

    return data;
}

GObject *json_handler_gobject_from_file(const gchar *file_name, GType gtype)
{
    GObject *object = NULL;

    FILE *file = fopen(file_name, "r");
    if (!file) {
        g_warning("%s: Error: file == 0", (const char *) __func__);
        return object;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    if (size == 0) {
        g_warning("%s: Error: size == 0", (const char *) __func__);
        fclose(file);
        return object;
    }

    // Read file
    fseek(file, 0, SEEK_SET);
    gsize length = size + 1;
    gchar *fcontent = (gchar *)calloc(1, sizeof(gchar) * length);
    size_t bytes_read = fread(fcontent, 1, size, file);
    fclose(file);
    fcontent[size] = 0;
    if (bytes_read == 0) {
        g_warning("%s: Error: bytes_read == 0", (const char *) __func__);
        free(fcontent);
        return object;
    }

    // Create gobject
    g_autofree gchar *utf8 = NULL;
    utf8 = g_locale_to_utf8(fcontent, -1, NULL, NULL, NULL);
    free(fcontent);

    GError *error = NULL;
    object = json_gobject_from_data(gtype, utf8, -1, &error);
    if (error)
        g_warning ("%s: Unable to create instance: %s", (const char *)__func__, error->message);
    g_clear_error(&error);

    return object;
}

void json_handler_gobject_to_file(const gchar *file_name, GObject *g_object)
{
    // Gobject to json string
    g_autofree gchar *gobject_to_json_str = NULL;
    gsize length;
    gobject_to_json_str = json_handler_gobject_to_data(g_object, &length); // free
    if (!gobject_to_json_str)
        return;

    // Save string to file
    g_autofree gchar *gobject_to_json_str_moded = NULL;
    gobject_to_json_str_moded = replace_str(gobject_to_json_str, "-", "_"); // free

    FILE *file_w = fopen(file_name, "w");
    if (file_w) {
        fwrite(gobject_to_json_str_moded, sizeof(gchar), strlen(gobject_to_json_str_moded), file_w);
        fclose(file_w);
    }
}