/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef THIN_CLIENT_VEIL_SETTINGSFILE_H
#define THIN_CLIENT_VEIL_SETTINGSFILE_H

#include <gtk/gtk.h>
#include <config.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gtypes.h>

GKeyFile *get_ini_keyfile(void);
const gchar *get_ini_file_name(void);
void close_ini_file(void);

gchar *read_str_from_ini_file(const gchar *group_name,  const gchar *key);
gchar *read_str_from_ini_file_with_def(const gchar *group_name,  const gchar *key, const gchar *default_val);
gchar *read_str_from_ini_file_default(const gchar *group_name,  const gchar *key, const gchar *default_str);
void write_str_to_ini_file(const gchar *group_name,  const gchar *key, const gchar *str_value);

gint read_int_from_ini_file(const gchar *group_name,  const gchar *key, gint def_value);
void write_int_to_ini_file(const gchar *group_name,  const gchar *key, gint value);

GList *read_string_list_from_ini_file(const gchar *group_name,  const gchar *key);
void write_string_list_to_ini_file(const gchar *group_name, const gchar *key, const gchar * const list[], gsize length);
void write_string_list_to_ini_file_v2(const gchar *group_name, const gchar *key, GList *strings);

#endif //THIN_CLIENT_VEIL_SETTINGSFILE_H
