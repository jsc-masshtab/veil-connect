//
// Created by ubuntu on 19.04.2021.
//

#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include <config.h>
#include "veil_logger.h"
#include "remote-viewer-util.h"
#include "crashhandler.h"
#include "settingsfile.h"

#define MAX_STRING_LENGTH 10485761 // 1024 X 1024

static FILE *err_file_desc = NULL;
static FILE *out_file_desc = NULL;
static gchar *data_time_string_s = NULL;
static gchar *cur_log_path = NULL;
static gchar *prev_clipboard_text = NULL;
static gint clipboard_log_enabled = 1;

void veil_logger_setup()
{
    // read settings from ini
    clipboard_log_enabled = read_int_from_ini_file("General", "clipboard_log_enabled", 1);

    g_autofree gchar *log_dir = NULL;
    log_dir = get_log_dir_path();

    // current log dir
    GDateTime *datetime = g_date_time_new_now_local();;
    data_time_string_s = g_date_time_format(datetime, "%Y_%m_%d___%H_%M_%S");
    g_date_time_unref(datetime);
    cur_log_path = g_build_filename(log_dir, data_time_string_s, NULL);

    // create log dir
    g_mkdir_with_parents(cur_log_path, 0755);

    // Return if we cant write to log dir
    if (g_access(cur_log_path, W_OK) != 0) {
        g_warning("Log directory is protected from writing");
        return;
    }

#ifdef NDEBUG // logging errors  in release mode
    //error output
    gchar *stderr_file_name = g_build_filename(cur_log_path, "stderr.txt", NULL);
    convert_string_from_utf8_to_locale(&stderr_file_name);
    err_file_desc = freopen(stderr_file_name, "w", stderr);
    g_free(stderr_file_name);

    //stdout output
    gchar *stdout_file_name = g_build_filename(cur_log_path, "stdout.txt", NULL);
    convert_string_from_utf8_to_locale(&stdout_file_name);
    out_file_desc = freopen(stdout_file_name, "w", stdout);
    g_free(stdout_file_name);
#endif

    // crash handler
    gchar *bt_file_name = g_build_filename(cur_log_path, "backtrace.txt", NULL);
    convert_string_from_utf8_to_locale(&bt_file_name);
    convert_string_from_utf8_to_locale(&cur_log_path);
    install_crash_handler(bt_file_name, cur_log_path);
    g_free(bt_file_name);
}

void veil_logger_free()
{
    uninstall_crash_handler();

    if(err_file_desc)
        fclose(err_file_desc);
    if(out_file_desc)
        fclose(out_file_desc);
    g_free(cur_log_path);
    g_free(prev_clipboard_text);
    g_free(data_time_string_s);
}

const gchar *veil_logger_get_log_start_date()
{
    return data_time_string_s;
}

void logger_save_clipboard_data(const gchar *data, guint size, ClipboardLoggerDataType data_type)
{
    if (clipboard_log_enabled != 1)
        return;

    if (data == NULL)
        return;

    // open file
    g_autofree gchar *file_name = NULL;
    file_name = g_build_filename(cur_log_path, "clipboard.txt", NULL);
    FILE *file = fopen(file_name, "a");

    if(!file)
        return;

    // trim and compare
    guint final_size = MIN(size, MAX_STRING_LENGTH);
    gchar *clipboard_text = g_strndup(data, final_size);

    if (prev_clipboard_text == NULL || strcmp(clipboard_text, prev_clipboard_text) != 0) {

        // write time
        GDateTime *datetime = g_date_time_new_now_local();
        g_autofree gchar *data_time_string = NULL;
        data_time_string = g_date_time_format(datetime, "%T");
        g_date_time_unref(datetime);

        fwrite(data_time_string, sizeof(gchar), strlen(data_time_string), file);

        // write type
        if (data_type == CLIPBOARD_LOGGER_FROM_CLIENT_TO_VM) {
            const gchar *type = " FROM_CLIENT_TO_VM:\n";
            fwrite(type, sizeof(gchar), strlen(type), file);

        } else if (data_type == CLIPBOARD_LOGGER_FROM_VM_TO_CLIENT) {
            const gchar *type = " FROM_VM_TO_CLIENT:\n";
            fwrite(type, sizeof(gchar), strlen(type), file);
        }

        // write text
        fwrite(clipboard_text, sizeof(gchar), final_size, file);
        putc ('\n' , file);
    }

    g_free(prev_clipboard_text);
    prev_clipboard_text = clipboard_text;

    // close file
    fclose(file);
}
