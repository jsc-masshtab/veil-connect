/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_LOUDPLAY_CONFIG_H
#define VEIL_CONNECT_LOUDPLAY_CONFIG_H


#define TYPE_LOUDPLAY_CONFIG (loudplay_config_get_type ())
#define LOUDPLAY_CONFIG(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_LOUDPLAY_CONFIG, LoudPlayConfig))

typedef struct _LoudPlayConfig        LoudPlayConfig;
typedef struct _LoudPlayConfigClass   LoudPlayConfigClass;

struct _LoudPlayConfig
{
    GObject parent_instance;

    // properties
    gchar *log_level;
    gchar *log_path;
    gchar *server_url;

    gchar *proto;
    gint auto_fps;
    gint auto_bitrate;
    gboolean bitrate_adaptation;
    gint bbr_port;
    gboolean hw_decoder;

    gboolean control_enabled;
    gboolean control_send_mouse_motion;
    gint control_port;
    gchar *control_proto;

    gint rtp_video_port;
    gint rtp_audio_port;
    gint type;

    gint x1;
    gint x2;
    gchar *capture;
    gint fec_redundancy;
    gboolean rtp_retransmission;

    /* instance members */
    gchar *loudplay_client_path;
    gboolean is_client_path_relative;

};

struct _LoudPlayConfigClass
{
    GObjectClass parent_class;

    /* class members */
};

/* used by TYPE_LOUDPLAY_CONFIG */


GType loudplay_config_get_type (void);

gchar *loudplay_config_get_file_name(void);

#endif //VEIL_CONNECT_LOUDPLAY_CONFIG_H
