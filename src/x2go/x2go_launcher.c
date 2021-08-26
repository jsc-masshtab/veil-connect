/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "remote-viewer-util.h"
#include "settingsfile.h"
#include "x2go_launcher.h"

// X2go клиент стартует довольно продолжительное время, поэтому необходимо показывать
// пользователю информациорнное окно
// WARN: passwordless login for
// re-trying SSH key discovery now with passphrase for unlocking the key
#define PYLIB_LOG "x2goguardian-pylib" // x2goguardian-pylib
#define MAX_PARAM_AMOUNT 30

typedef struct
{
    GMainLoop *loop;
    GtkBuilder *builder;

    GtkWidget *window;
    GtkWidget *main_image;
    GtkWidget *status_label;

    GtkWidget *cancel_btn;
    GtkWidget *btn_close;
    GtkWidget *btn_connect;

    GtkWidget *user_name_entry;
    GtkWidget *password_entry;
    GtkWidget *address_label;
    GtkWidget *credentials_image;

    GtkWidget *proc_output_view;
    GtkTextBuffer *output_buffer;

    GPid pid;
    gboolean is_launched;

    gboolean is_close_pressed;

    const ConnectSettingsData *p_data;

} X2goData;

static void x2go_launcher_stop_process(X2goData *data, int sig)
{
    // If process is running than stop it
    if (data->is_launched) {
        if (data->pid) {
            kill(data->pid, sig); // todo: maybe CloseHandle on windows
            g_info("%s SIG %i sent", __func__, sig);
        }
    } else {
        shutdown_loop(data->loop);
    }
}

// Вызывается когда процесс завершился
static void
x2go_launcher_cb_child_watch( GPid  pid, gint status, X2goData *data )
{
    g_info("FINISHED. %s Status: %i", __func__, status);
    /* Close pid */
    g_spawn_close_pid( pid );
    data->is_launched = FALSE;
    data->pid = 0;

    if (data->is_close_pressed) {
        shutdown_loop(data->loop);
    } else {
        gtk_widget_set_sensitive(data->btn_connect, TRUE);
        gtk_label_set_text(GTK_LABEL(data->status_label), "Подключение завершено");
    }

}

static gboolean
x2go_launcher_cb_out_watch( GIOChannel *channel, GIOCondition cond,X2goData *data )
{
    gchar *string;
    gsize  size;

    if( cond == G_IO_HUP ) {
        g_io_channel_unref( channel );
        return FALSE;
    }

    g_io_channel_read_line( channel, &string, &size, NULL, NULL );
    g_info("%s  %s", __func__, string);
    // Не показываем ненужные пользователю сообщения
    if (!strstr(string, PYLIB_LOG))
        gtk_text_buffer_insert_at_cursor( data->output_buffer, string, -1 );

    g_free(string);

    return TRUE;
}

static gboolean
x2go_launcher_cb_err_watch( GIOChannel *channel, GIOCondition cond, X2goData *data )
{
    gchar *string;
    gsize  size;

    if( cond == G_IO_HUP ) {
        g_io_channel_unref( channel );
        return FALSE;
    }

    g_io_channel_read_line( channel, &string, &size, NULL, NULL );
    g_info("%s  %s", __func__, string);
    // Не показываем Press CTRL+C чтобы не смущать пользователя
    if (!strstr(string, PYLIB_LOG) && !strstr(string, "Press CTRL+C")) {
        gtk_text_buffer_insert_at_cursor(data->output_buffer, string, -1);
        GtkAdjustment *adjustment = gtk_text_view_get_vadjustment(GTK_TEXT_VIEW(data->proc_output_view));
        gtk_adjustment_set_value(adjustment, gtk_adjustment_get_upper(adjustment));
    }

    if (strstr(string, "NX3 proxy is up and running")) {
        // Connection established. Hide control window
        gtk_label_set_text(GTK_LABEL(data->status_label), "");
        gtk_widget_hide(data->window);

    } else if (strstr(string, "Tunnel closed from")) {
        // Connection is about to end. Show control windows
        gtk_label_set_text(GTK_LABEL(data->status_label), "Завершаем соединение");
        gtk_widget_show_all(data->window);

    } else if (strstr(string, "re-trying SSH key discovery now with passphrase for unlocking the key")) {
        // Possibly wrong credentials
        gtk_label_set_text(GTK_LABEL(data->status_label), "Ошибка авторизации");
        gtk_image_set_from_stock(GTK_IMAGE(data->credentials_image),
                "gtk-dialog-error", GTK_ICON_SIZE_BUTTON);
        x2go_launcher_stop_process(data, SIGINT);
    }
    g_free(string);

    return TRUE;
}

//static gboolean
//x2go_launcher_cb_in_watch( GIOChannel *channel, GIOCondition cond, X2goData *data )
//{
//    g_info("%s", __func__);
//
//    if( cond == G_IO_HUP ) {
//        g_io_channel_unref( channel );
//        return FALSE;
//    }
//
//    //g_io_channel_write_chars( channel, "\\x04", -1, NULL, NULL);
//
//    return TRUE;
//}

static void x2go_launcher_launch_process(X2goData *data)
{
    // reset gui
    gtk_widget_set_sensitive(GTK_WIDGET(data->btn_connect), FALSE);
    gtk_image_set_from_stock(GTK_IMAGE(data->credentials_image), "gtk-ok", GTK_ICON_SIZE_BUTTON);

    const ConnectSettingsData *con_data = data->p_data;

    // Parameters
    g_autofree gchar *x2go_session_type = NULL;
    x2go_session_type = read_str_from_ini_file_with_def("X2GoSettings", "session_type", "XFCE");
    gboolean conn_type_assigned = read_int_from_ini_file("X2GoSettings", "conn_type_assigned", 0);
    g_autofree gchar *x2go_conn_type = NULL;
    x2go_conn_type = read_str_from_ini_file_with_def("X2GoSettings", "conn_type", "modem");
    gboolean full_screen = read_int_from_ini_file("X2GoSettings", "full_screen", 0);

    gchar *argv[MAX_PARAM_AMOUNT] = {};
    int index = 0;
    argv[index] = g_strdup("pyhoca-cli");
    argv[++index] = g_strdup("-N");
    argv[++index] = g_strdup("--add-to-known-hosts");
    argv[++index] = g_strdup("--libdebug");
    argv[++index] = g_strdup("--auth-attempts=0");
    argv[++index] = g_strdup_printf("-c=%s", x2go_session_type);
    argv[++index] = g_strdup_printf("--server=%s", con_data->ip);
    argv[++index] = g_strdup_printf("-u=%s", gtk_entry_get_text(GTK_ENTRY(data->user_name_entry)));
    argv[++index] = g_strdup_printf("--password=%s", gtk_entry_get_text(GTK_ENTRY(data->password_entry)));
    if (conn_type_assigned)
        argv[++index] = g_strdup_printf("--link=%s", x2go_conn_type);
    if (full_screen)
        argv[++index] = g_strdup("--geometry=fullscreen");

    gint out_fd, err_fd;//, in_fd;
    GIOChannel *out_ch, *err_ch;//, *in_ch;
    GError *error = NULL;

    /* Spawn child process */
    data->is_launched = g_spawn_async_with_pipes(NULL, argv, NULL,
            G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH , NULL,
            NULL, &data->pid, NULL, &out_fd, &err_fd, &error);

    for (guint i = 0; i < (sizeof(argv) / sizeof(gchar *)); i++) {
        g_free(argv[i]);
    }

    if(!data->is_launched || data->pid <= 0) {
        g_warning("X2GO SPAWN FAILED");
        if (error) {
            g_warning("%s", error->message);
            g_autofree gchar *msg = NULL;
            msg = g_strdup_printf("%s", error->message);
            gtk_label_set_text(GTK_LABEL(data->status_label), msg);
            g_clear_error(&error);
        }
        return;
    } else {
        gtk_label_set_text(GTK_LABEL(data->status_label), "Ожидаем подключение");
    }

    /* Add watch function to catch termination of the process. This function
     * will clean any remnants of process. */
    g_child_watch_add(data->pid, (GChildWatchFunc)x2go_launcher_cb_child_watch, data);

    /* Create channels that will be used to read data from pipes. */
#ifdef G_OS_WIN32
    out_ch = g_io_channel_win32_new_fd( out_fd );
    err_ch = g_io_channel_win32_new_fd( err_fd );
    //in_ch = g_io_channel_win32_new_fd( in_fd );
#else
    out_ch = g_io_channel_unix_new( out_fd );
    err_ch = g_io_channel_unix_new( err_fd );
    //in_ch = g_io_channel_unix_new( in_fd );
#endif

    /* Add watches to channels */
    g_io_add_watch( out_ch, G_IO_IN | G_IO_HUP, (GIOFunc)x2go_launcher_cb_out_watch, data );
    g_io_add_watch( err_ch, G_IO_IN | G_IO_HUP, (GIOFunc)x2go_launcher_cb_err_watch, data );
    //g_io_add_watch( in_ch, G_IO_OUT | G_IO_HUP, (GIOFunc)x2go_launcher_cb_in_watch, data );
}

static void x2go_launcher_cancel_btn_clicked(GtkButton *button G_GNUC_UNUSED, X2goData *data)
{
    g_info("%s", __func__);
    x2go_launcher_stop_process(data, SIGINT);
}

static void x2go_launcher_btn_close_clicked(GtkButton *button G_GNUC_UNUSED, X2goData *data)
{
    g_info("%s", __func__);
    x2go_launcher_stop_process(data, SIGKILL);
}

static void x2go_launcher_btn_connect_clicked(GtkButton *button G_GNUC_UNUSED, X2goData *data)
{
    x2go_launcher_launch_process(data);
}

static void x2go_launcher_setup_gui(X2goData *data)
{
    const ConnectSettingsData *con_data = data->p_data;

    data->builder = remote_viewer_util_load_ui("x2go_control_form.ui");

    data->window = get_widget_from_builder(data->builder, "main_window");
    gtk_window_set_default_size(GTK_WINDOW(data->window), 700, 400);

    data->main_image = get_widget_from_builder(data->builder, "main_image");
    gtk_image_set_from_resource(GTK_IMAGE(data->main_image),
                                VIRT_VIEWER_RESOURCE_PREFIX"/icons/content/img/x2go-logo-with-text.png");

    data->proc_output_view = get_widget_from_builder(data->builder, "msg_text_view");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(data->proc_output_view), FALSE);
    data->output_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->proc_output_view));

    data->status_label = get_widget_from_builder(data->builder, "status_label");

    data->cancel_btn = get_widget_from_builder(data->builder, "btn_cancel");
    data->btn_close = get_widget_from_builder(data->builder, "btn_close");
    data->btn_connect = get_widget_from_builder(data->builder, "btn_connect");

    data->address_label = get_widget_from_builder(data->builder, "address_label");
    gtk_label_set_text(GTK_LABEL(data->address_label), con_data->ip);
    data->user_name_entry = get_widget_from_builder(data->builder, "user_name_entry");
    data->password_entry = get_widget_from_builder(data->builder, "password_entry");
    gtk_entry_set_text(GTK_ENTRY(data->user_name_entry), con_data->user);
    gtk_entry_set_text(GTK_ENTRY(data->password_entry), con_data->password);
    data->credentials_image = get_widget_from_builder(data->builder, "credentials_image");

    // connects
    g_signal_connect(G_OBJECT(data->cancel_btn ), "clicked",
                     G_CALLBACK(x2go_launcher_cancel_btn_clicked ), data );
    g_signal_connect(G_OBJECT(data->btn_close ), "clicked",
                     G_CALLBACK(x2go_launcher_btn_close_clicked ), data );
    g_signal_connect(G_OBJECT(data->btn_connect ), "clicked",
                     G_CALLBACK(x2go_launcher_btn_connect_clicked ), data );

    gtk_widget_show_all(data->window);
}

void x2go_launcher_start(const ConnectSettingsData *con_data)
{
    X2goData data = {};
    data.p_data = con_data;

    // GUI
    x2go_launcher_setup_gui(&data);

    // Process
    x2go_launcher_launch_process(&data);

    create_loop_and_launch(&data.loop);

    // Release resources
    g_object_unref(data.builder);
    gtk_widget_destroy(data.window);
}
