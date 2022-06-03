/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <config.h>

#include <locale.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "loudplay_config.h"
#include "remote-viewer-util.h"


G_DEFINE_TYPE( LoudPlayConfig, loudplay_config, G_TYPE_OBJECT )

gchar *loudplay_config_get_file_name(void)
{
    g_autofree gchar *file_name_dir = NULL;
    file_name_dir = g_build_filename(g_get_user_config_dir(), APP_FILES_DIRECTORY_NAME,
                                     "loudplay_data", NULL);
    g_mkdir_with_parents(file_name_dir, 0755);

    gchar *file_name = g_build_filename(file_name_dir, "client.cfg", NULL);
    convert_string_from_utf8_to_locale(&file_name);
    return file_name;
}

static void
set_property (GObject *object, guint property_id G_GNUC_UNUSED,
              const GValue *value G_GNUC_UNUSED, GParamSpec *pspec) {

    LoudPlayConfig *self = LOUDPLAY_CONFIG(object);

    if (g_strcmp0(pspec->name, "log-level") == 0)
        update_string_safely(&self->log_level, g_value_get_string(value));
    else if (g_strcmp0(pspec->name, "log-path") == 0)
        update_string_safely(&self->log_path, g_value_get_string(value));
    else if (g_strcmp0(pspec->name, "server-url") == 0)
        update_string_safely(&self->server_url, g_value_get_string(value));

    else if (g_strcmp0(pspec->name, "proto") == 0)
        update_string_safely(&self->proto, g_value_get_string(value));
    else if (g_strcmp0(pspec->name, "auto-fps") == 0)
        self->auto_fps = g_value_get_int(value);
    else if (g_strcmp0(pspec->name, "auto-bitrate") == 0)
        self->auto_bitrate = g_value_get_int(value);
    else if (g_strcmp0(pspec->name, "bitrate-adaptation") == 0)
        self->bitrate_adaptation = g_value_get_boolean(value);
    else if (g_strcmp0(pspec->name, "bbr-port") == 0)
        self->bbr_port = g_value_get_int(value);
    else if (g_strcmp0(pspec->name, "hw-decoder") == 0)
        self->hw_decoder = g_value_get_boolean(value);

    else if (g_strcmp0(pspec->name, "control-enabled") == 0)
        self->control_enabled = g_value_get_boolean(value);
    else if (g_strcmp0(pspec->name, "control-send-mouse-motion") == 0)
        self->control_send_mouse_motion = g_value_get_boolean(value);
    else if (g_strcmp0(pspec->name, "control-port") == 0)
        self->control_port = g_value_get_int(value);
    else if (g_strcmp0(pspec->name, "control-proto") == 0)
        update_string_safely(&self->control_proto, g_value_get_string(value));

    else if (g_strcmp0(pspec->name, "rtp-video-port") == 0)
        self->rtp_video_port = g_value_get_int(value);
    else if (g_strcmp0(pspec->name, "rtp-audio-port") == 0)
        self->rtp_audio_port = g_value_get_int(value);
    else if (g_strcmp0(pspec->name, "type") == 0)
        self->type = g_value_get_int(value);

    else if (g_strcmp0(pspec->name, "x1") == 0)
        self->x1 = g_value_get_int(value);
    else if (g_strcmp0(pspec->name, "x2") == 0)
        self->x2 = g_value_get_int(value);
    else if (g_strcmp0(pspec->name, "capture") == 0)
        update_string_safely(&self->capture, g_value_get_string(value));
    else if (g_strcmp0(pspec->name, "fec-redundancy") == 0)
        self->fec_redundancy = g_value_get_int(value);
    else if (g_strcmp0(pspec->name, "rtp-retransmission") == 0)
        self->rtp_retransmission = g_value_get_boolean(value);

    else
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
get_property (GObject *object, guint property_id G_GNUC_UNUSED,
              GValue *value, GParamSpec *pspec)
{
    LoudPlayConfig *self = LOUDPLAY_CONFIG(object);

    if (g_strcmp0(pspec->name, "log-level") == 0)
        g_value_set_static_string(value, self->log_level);
    else if (g_strcmp0(pspec->name, "log-path") == 0)
        g_value_set_static_string(value, self->log_path);
    else if (g_strcmp0(pspec->name, "server-url") == 0)
        g_value_set_static_string(value, self->server_url);

    else if (g_strcmp0(pspec->name, "proto") == 0)
        g_value_set_static_string(value, self->proto);
    else if (g_strcmp0(pspec->name, "auto-fps") == 0)
        g_value_set_int(value, self->auto_fps);
    else if (g_strcmp0(pspec->name, "auto-bitrate") == 0)
        g_value_set_int(value, self->auto_bitrate);
    else if (g_strcmp0(pspec->name, "bitrate-adaptation") == 0)
        g_value_set_boolean(value, self->bitrate_adaptation);
    else if (g_strcmp0(pspec->name, "bbr-port") == 0)
        g_value_set_int(value, self->bbr_port);
    else if (g_strcmp0(pspec->name, "hw-decoder") == 0)
        g_value_set_boolean(value, self->hw_decoder);

    else if (g_strcmp0(pspec->name, "control-enabled") == 0)
        g_value_set_boolean(value, self->control_enabled);
    else if (g_strcmp0(pspec->name, "control-send-mouse-motion") == 0)
        g_value_set_boolean(value, self->control_send_mouse_motion);
    else if (g_strcmp0(pspec->name, "control-port") == 0)
        g_value_set_int(value, self->control_port);
    else if (g_strcmp0(pspec->name, "control-proto") == 0)
        g_value_set_static_string(value, self->control_proto);

    else if (g_strcmp0(pspec->name, "rtp-video-port") == 0)
        g_value_set_int(value, self->rtp_video_port);
    else if (g_strcmp0(pspec->name, "rtp-audio-port") == 0)
        g_value_set_int(value, self->rtp_audio_port);
    else if (g_strcmp0(pspec->name, "type") == 0)
        g_value_set_int(value, self->type);

    else if (g_strcmp0(pspec->name, "x1") == 0)
        g_value_set_int(value, self->x1);
    else if (g_strcmp0(pspec->name, "x2") == 0)
        g_value_set_int(value, self->x2);
    else if (g_strcmp0(pspec->name, "capture") == 0)
        g_value_set_static_string(value, self->capture);
    else if (g_strcmp0(pspec->name, "fec-redundancy") == 0)
        g_value_set_int(value, self->fec_redundancy);
    else if (g_strcmp0(pspec->name, "rtp-retransmission") == 0)
        g_value_set_boolean(value, self->rtp_retransmission);

    else
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void finalize(GObject *object)
{
    LoudPlayConfig *self = LOUDPLAY_CONFIG(object);

    free_memory_safely(&self->log_level);
    free_memory_safely(&self->log_path);
    free_memory_safely(&self->server_url);
    free_memory_safely(&self->proto);
    free_memory_safely(&self->control_proto);
    free_memory_safely(&self->capture);

    free_memory_safely(&self->loudplay_client_path);

    GObjectClass *parent_class = G_OBJECT_CLASS( loudplay_config_parent_class );
    ( *parent_class->finalize )( object );
}

static void
loudplay_config_class_init (LoudPlayConfigClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->finalize = finalize;

    // Properties
    util_install_string_property(object_class, 1, "log-level", "info");
    util_install_string_property(object_class, 1, "log-path", "logs/client.log");
    util_install_string_property(object_class, 1, "server-url", "");
    
    util_install_string_property(object_class, 1, "proto", "udp");
    util_install_int_property(object_class, 1, "auto-fps", 1, 150, 30);
    util_install_int_property(object_class, 1, "auto-bitrate", INT_MIN, INT_MAX, 10000);
    util_install_bool_property(object_class, 1, "bitrate-adaptation", FALSE);
    util_install_int_property(object_class, 1, "bbr-port", INT_MIN, INT_MAX, 8556);
    util_install_bool_property(object_class, 1, "hw-decoder", FALSE);
    
    util_install_bool_property(object_class, 1, "control-enabled", TRUE);
    util_install_bool_property(object_class, 1, "control-send-mouse-motion", TRUE);
    util_install_int_property(object_class, 1, "control-port", 1, INT_MAX, 8555);
    util_install_string_property(object_class, 1, "control-proto", "udp");
    
    util_install_int_property(object_class, 1, "rtp-video-port", 1, INT_MAX, 6970);
    util_install_int_property(object_class, 1, "rtp-audio-port", 1, INT_MAX, 6972);
    util_install_int_property(object_class, 1, "type", INT_MIN, INT_MAX, 13);
    
    util_install_int_property(object_class, 1, "x1", INT_MIN, INT_MAX, 600);
    util_install_int_property(object_class, 1, "x2", INT_MIN, INT_MAX, 300);
    util_install_string_property(object_class, 1, "capture", "dda");
    util_install_int_property(object_class, 1, "fec-redundancy", INT_MIN, INT_MAX, 50);
    util_install_bool_property(object_class, 1, "rtp-retransmission", TRUE);
}

static void
loudplay_config_init (LoudPlayConfig *self)
{
    (void)self;
}
