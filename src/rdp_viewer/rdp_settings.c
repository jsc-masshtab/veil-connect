//
// Created by solomin on 07.05.2021.
//

#include<stdio.h>
#include<stdlib.h>

#include "rdp_settings.h"
#include "remote-viewer-util.h"
#include "settingsfile.h"

void rdp_settings_set_connect_data(VeilRdpSettings *rdp_settings, const gchar *user_name, const gchar *password,
                                   const gchar *domain, const gchar *ip, int port)
{
    update_string_safely(&rdp_settings->user_name, user_name);
    update_string_safely(&rdp_settings->password, password);
    update_string_safely(&rdp_settings->domain, domain);
    update_string_safely(&rdp_settings->ip, ip);

    rdp_settings->port = port;
}

///*
// * Прочитать настройки из файла настройки. В зависимости от того, что выбрал пользователь, читаем из ini или из
// * стандартного файла RDP
// */
//void rdp_settings_read_from_file(VeilRdpSettings *rdp_settings)
//{
//    gboolean read_from_std_rdp_file = read_int_from_ini_file("RDPSettings", "use_rdp_file", 0);
//
//    if (read_from_std_rdp_file) {
//        gchar *rdp_settings_file_name = read_str_from_ini_file("RDPSettings", "rdp_settings_file");
//        rdp_settings_read_standard_file(rdp_settings, rdp_settings_file_name);
//        g_free(rdp_settings_file_name);
//    } else {
//        rdp_settings_read_ini_file(rdp_settings);
//    }
//}

/*
 * Чтение из ini
 */
void rdp_settings_read_ini_file(VeilRdpSettings *rdp_settings, gboolean rewrite_remote_app_settings)
{
    if (rewrite_remote_app_settings) {
        rdp_settings->is_remote_app = read_int_from_ini_file("RDPSettings", "is_remote_app", 0);
        update_string_safely(&rdp_settings->remote_app_program,
                             read_str_from_ini_file("RDPSettings", "remote_app_program"));
        update_string_safely(&rdp_settings->remote_app_options,
                             read_str_from_ini_file("RDPSettings", "remote_app_options"));
    }

    rdp_settings->is_multimon = read_int_from_ini_file("RDPSettings", "is_multimon", 0);
    rdp_settings->redirectsmartcards = read_int_from_ini_file("RDPSettings", "redirectsmartcards", 1);
    rdp_settings->redirectprinters = read_int_from_ini_file("RDPSettings", "redirect_printers", 1);

    update_string_safely(&rdp_settings->alternate_shell,
                         read_str_from_ini_file("RDPSettings", "alternate_shell"));

    rdp_settings->disable_rdp_fonts = read_int_from_ini_file("RDPSettings", "disable_rdp_fonts", 0);
    rdp_settings->disable_rdp_themes = read_int_from_ini_file("RDPSettings", "disable_rdp_themes", 0);

    rdp_settings->allow_desktop_composition = read_int_from_ini_file("RDPSettings", "allow_desktop_composition", 0);
}

/*
 * Чтение настроек из стандартного RDP файла
 */
void rdp_settings_read_standard_file(VeilRdpSettings *rdp_settings, const gchar *file_name)
{
    // Если не указано, берем из ini
    g_autofree gchar *final_file_name = NULL;
    if (file_name)
        final_file_name = g_strdup(file_name);
    else
        final_file_name = read_str_from_ini_file("RDPSettings", "rdp_settings_file");
    convert_string_from_utf8_to_locale(&final_file_name);

    FILE *rdp_file = fopen(final_file_name, "r");
    if (rdp_file == NULL) {
        g_warning("%s: Cant open file %s", (const char *)__func__, final_file_name);
        return;
    }

    // reset parameters
    rdp_settings_clear(rdp_settings);

    // parse
    const guint buff_size = 1024
            ;
    char line[buff_size];

    const gchar *delimiter = ":";
    const guint tokens_num = 3;
    //fgets
    while (fgets(line, sizeof(line), rdp_file)) {
        printf("Retrieved line  %s\n", line);

        gchar **params_array = g_strsplit(line, delimiter, tokens_num);
        if (g_strv_length(params_array) == tokens_num) {
            gchar *name = params_array[0];
            gchar *value = params_array[2];
            remove_char(value, '\n');

            if (g_strcmp0(name, "domain") == 0) {
                update_string_safely(&rdp_settings->domain, value);
            } else if (g_strcmp0(name, "username") == 0) {
                update_string_safely(&rdp_settings->user_name, value);
            } else if (g_strcmp0(name, "full address") == 0) {
                update_string_safely(&rdp_settings->ip, value);

            } else if (g_strcmp0(name, "remoteapplicationmode") == 0) {
                rdp_settings->is_remote_app = atoi(value);
            } else if (g_strcmp0(name, "remoteapplicationprogram") == 0) {
                update_string_safely(&rdp_settings->remote_app_program, value);
            } else if (g_strcmp0(name, "remoteapplicationcmdline") == 0) {
                update_string_safely(&rdp_settings->remote_app_options, value);

            } else if (g_strcmp0(name, "use multimon") == 0) {
                rdp_settings->is_multimon = atoi(value);
            } else if (g_strcmp0(name, "redirectsmartcards") == 0) {
                rdp_settings->redirectsmartcards = atoi(value);
            } else if (g_strcmp0(name, "redirectprinters") == 0) {
                rdp_settings->redirectprinters = atoi(value);

            } else if (g_strcmp0(name, "alternate shell") == 0) {
                update_string_safely(&rdp_settings->alternate_shell, value);
            } else if (g_strcmp0(name, "allow font smoothing") == 0) {
                rdp_settings->disable_rdp_fonts = !((gboolean)atoi(value));
            } else if (g_strcmp0(name, "disable themes") == 0) {
                rdp_settings->disable_rdp_themes = atoi(value);
            } else if (g_strcmp0(name, "allow desktop composition") == 0) {
                rdp_settings->allow_desktop_composition = atoi(value);
            }
        }

        g_strfreev(params_array);
    }

    fclose(rdp_file);
}

void rdp_settings_clear(VeilRdpSettings *rdp_settings)
{
    rdp_settings->port = 0;
    rdp_settings->is_remote_app = 0;

    free_memory_safely(&rdp_settings->user_name);
    free_memory_safely(&rdp_settings->password);
    free_memory_safely(&rdp_settings->domain);
    free_memory_safely(&rdp_settings->ip);

    free_memory_safely(&rdp_settings->remote_app_program);
    free_memory_safely(&rdp_settings->remote_app_options);

    free_memory_safely(&rdp_settings->alternate_shell);
}