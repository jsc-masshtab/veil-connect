//
// Created by ubuntu on 11.01.2022.
//

#include "veil_time.h"

#include "remote-viewer-util.h"


gchar *veil_time_get_time_zone()
{
#ifdef _WIN32
    GDateTime *dt_info = g_date_time_new_now_local();
    GTimeZone *tz = g_date_time_get_timezone(dt_info);
    const gchar *tz_id = g_time_zone_get_identifier(tz);

    gchar *time_zone = g_strdup(tz_id);

    g_date_time_unref(dt_info);
    return time_zone;

#elif __linux__
    g_autofree gchar *standard_output = NULL;
    g_autofree gchar *standard_error = NULL;
    gint exit_status = 0;

    g_autofree gchar *command_line = NULL;

    // timedatectl status | awk '/Time zone:/ {print $3}'
    command_line = g_strdup("timedatectl status");

    gchar *time_zone = NULL;

    GError *error = NULL;
    gboolean cmd_success = g_spawn_command_line_sync(command_line,
                                                 &standard_output,
                                                 &standard_error,
                                                 &exit_status,
                                                 &error);
    if (!cmd_success) {
        g_warning("%s: Failed to spawn process. %s", (const char *)__func__, error ? error->message : "");
        free_memory_safely(&standard_output);
        g_clear_error(&error);
        return time_zone;
    }

    // parse
    GRegex *regex = g_regex_new("(Time zone: .*)$", G_REGEX_MULTILINE, 0, NULL);
    GMatchInfo *match_info = NULL;
    g_regex_match(regex, standard_output, 0, &match_info);
    gboolean is_success = g_match_info_matches(match_info);

    if (is_success) {
        g_autofree gchar *data_str = NULL;
        data_str = g_match_info_fetch(match_info, 0);
        if (data_str) {
            GStrv tokens_list = g_strsplit(data_str, " ", 10);
            if (g_strv_length(tokens_list) >= 3) {
                time_zone = g_strdup(tokens_list[2]);
            }
            g_strfreev(tokens_list);
        }
    }

    if(match_info)
        g_match_info_free(match_info);
    g_regex_unref(regex);

    g_clear_error(&error);
    return time_zone;
#endif
}