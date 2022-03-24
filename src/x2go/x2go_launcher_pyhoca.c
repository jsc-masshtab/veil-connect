/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <glib/gi18n.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "remote-viewer-util.h"
#include "settingsfile.h"
#include "settings_data.h"
#include "x2go_launcher.h"
#include "vdi_event.h"

// X2go клиент стартует довольно продолжительное время, поэтому необходимо показывать
// пользователю информациорнное окно

#define PYLIB_LOG "x2goguardian-pylib" // x2goguardian-pylib
#define MAX_PARAM_AMOUNT 30


static void x2go_launcher_clear(X2goLauncher *self)
{
    if (self->pyhoca_data.builder == NULL)
        return;

    // Release resources
    g_object_unref(self->pyhoca_data.builder);
    self->pyhoca_data.builder = NULL;
    gtk_widget_destroy(self->pyhoca_data.window);

    if (self->send_signal_upon_job_finish)
        g_signal_emit_by_name(self, "job-finished");
}

// #ifdef G_OS_WIN32   #ifdef G_OS_UNIX
void x2go_launcher_stop_pyhoca(X2goLauncher *self, int sig)
{
    // If process is running than stop it
    if (self->pyhoca_data.is_launched) {
        if (self->pyhoca_data.pid) {
#ifdef G_OS_WIN32
            (void)sig;
            TerminateProcess(self->pyhoca_data.pid, 0);
#elif __linux__
            kill(self->pyhoca_data.pid, sig);
#endif
            g_info("%s SIG %i sent", __func__, sig);

        }
    } else {
        x2go_launcher_clear(self);
    }
}

// Вызывается когда процесс завершился
static void
x2go_launcher_cb_child_watch( GPid  pid, gint status, X2goLauncher *self )
{
    g_info("FINISHED. %s Status: %i", __func__, status);
    /* Close pid */
    g_spawn_close_pid(pid);
    self->pyhoca_data.is_launched = FALSE;
    self->pyhoca_data.pid = 0;

    if (self->pyhoca_data.clear_all_upon_sig_termination) {
        x2go_launcher_clear(self);
    } else {
        gtk_widget_set_sensitive(self->pyhoca_data.btn_connect, TRUE);
        gtk_label_set_text(GTK_LABEL(self->pyhoca_data.status_label), _("Connection closed"));
    }

    vdi_event_vm_changed_notify(vdi_session_get_current_vm_id(), VDI_EVENT_TYPE_VM_DISCONNECTED);
}

static gboolean
x2go_launcher_cb_out_watch( GIOChannel *channel, GIOCondition cond, X2goLauncher *self )
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
        gtk_text_buffer_insert_at_cursor( self->pyhoca_data.output_buffer, string, -1 );

    g_free(string);

    return TRUE;
}

static gboolean
x2go_launcher_cb_err_watch( GIOChannel *channel, GIOCondition cond, X2goLauncher *self)
{
    gchar *string;
    gsize  size;

    if( cond == G_IO_HUP ) {
        g_io_channel_unref( channel );
        return FALSE;
    }

    X2goPyhocaData *data = &self->pyhoca_data;

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
        gtk_label_set_text(GTK_LABEL(data->status_label), _("Closing the connection"));
        gtk_widget_show_all(data->window);

    } else if (strstr(string, "re-trying SSH key discovery now")) {

        // Possibly wrong credentials
        gtk_label_set_text(GTK_LABEL(data->status_label), _("Authorisation Error"));
        gtk_image_set_from_stock(GTK_IMAGE(data->credentials_image),
                "gtk-dialog-error", GTK_ICON_SIZE_BUTTON);
        x2go_launcher_stop_pyhoca(self, SIGINT);
    }
    g_free(string);

    return TRUE;
}

//static gboolean
//x2go_launcher_cb_in_watch( GIOChannel *channel, GIOCondition cond, X2goPyhocaData *data )
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

static void x2go_launcher_launch_process(X2goLauncher *self)
{
    X2goPyhocaData *data = &self->pyhoca_data;
    const ConnectSettingsData *conn_data = self->p_conn_data;
    // reset gui
    gtk_widget_set_sensitive(GTK_WIDGET(data->btn_connect), TRUE);
    gtk_image_set_from_stock(GTK_IMAGE(data->credentials_image), "gtk-ok", GTK_ICON_SIZE_BUTTON);

    const gchar *user_name = gtk_entry_get_text(GTK_ENTRY(data->user_name_entry));
    if (strlen_safely(user_name) == 0) {
        // "Не указано имя"
        gtk_label_set_text(GTK_LABEL(data->status_label), _("Name is not specified"));
        return;
    }

    gchar *argv[MAX_PARAM_AMOUNT] = {};
    int index = 0;
#ifdef G_OS_WIN32
    argv[index] = g_strdup("pyhoca-cli.exe"); // we suppose it's in PATH
#elif __linux__
    argv[index] = g_strdup("pyhoca-cli");
#endif
    argv[++index] = g_strdup("-N");
    argv[++index] = g_strdup("--add-to-known-hosts");
    argv[++index] = g_strdup("--libdebug");
    argv[++index] = g_strdup("--auth-attempts=0");
    argv[++index] = g_strdup_printf("-c=%s", conn_data->x2Go_settings.x2go_session_type);
    argv[++index] = g_strdup_printf("--server=%s", conn_data->ip);
    argv[++index] = g_strdup_printf("-u=%s", user_name);
    argv[++index] = g_strdup_printf("--password=%s", gtk_entry_get_text(GTK_ENTRY(data->password_entry)));
    if (conn_data->x2Go_settings.conn_type_assigned)
        argv[++index] = g_strdup_printf("--link=%s", conn_data->x2Go_settings.x2go_conn_type);
    if (conn_data->x2Go_settings.full_screen)
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
            msg = g_strdup_printf("pyhoca-cli: %s", error->message);
            gtk_label_set_text(GTK_LABEL(data->status_label), msg);
            g_clear_error(&error);
        }
        return;
    } else {
        gtk_widget_set_sensitive(GTK_WIDGET(data->btn_connect), FALSE);
        gtk_label_set_text(GTK_LABEL(data->status_label), _("Waiting for connection"));
    }

    vdi_event_vm_changed_notify(vdi_session_get_current_vm_id(), VDI_EVENT_TYPE_VM_CONNECTED);

    /* Add watch function to catch termination of the process. This function
     * will clean any remnants of process. */
    g_child_watch_add(data->pid, (GChildWatchFunc)x2go_launcher_cb_child_watch, self);

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
    g_io_add_watch( out_ch, G_IO_IN | G_IO_HUP, (GIOFunc)x2go_launcher_cb_out_watch, self );
    g_io_add_watch( err_ch, G_IO_IN | G_IO_HUP, (GIOFunc)x2go_launcher_cb_err_watch, self );
    //g_io_add_watch( in_ch, G_IO_OUT | G_IO_HUP, (GIOFunc)x2go_launcher_cb_in_watch, data );
}

static void x2go_launcher_cancel_btn_clicked(GtkButton *button G_GNUC_UNUSED, X2goLauncher *self)
{
    g_info("%s", __func__);
    x2go_launcher_stop_pyhoca(self, SIGINT);
}

static void x2go_launcher_btn_close_clicked(GtkButton *button G_GNUC_UNUSED, X2goLauncher *self)
{
    g_info("%s", __func__);
    self->pyhoca_data.clear_all_upon_sig_termination = TRUE;
#ifdef G_OS_WIN32
    x2go_launcher_stop_pyhoca(self, 0);
#elif __linux__
    x2go_launcher_stop_pyhoca(self, SIGKILL);
#endif
}

static void x2go_launcher_btn_connect_clicked(GtkButton *button G_GNUC_UNUSED, X2goLauncher *self)
{
    x2go_launcher_launch_process(self);
}

static void x2go_launcher_setup_gui(const gchar *user, const gchar *password, X2goLauncher *self)
{
    const ConnectSettingsData *conn_data = self->p_conn_data;

    X2goPyhocaData *data = &self->pyhoca_data;
    data->builder = remote_viewer_util_load_ui("x2go_control_form.glade");

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
    gtk_label_set_text(GTK_LABEL(data->address_label), conn_data->ip);
    data->user_name_entry = get_widget_from_builder(data->builder, "user_name_entry");
    data->password_entry = get_widget_from_builder(data->builder, "password_entry");
    if (user)
        gtk_entry_set_text(GTK_ENTRY(data->user_name_entry), user);
    if (password)
        gtk_entry_set_text(GTK_ENTRY(data->password_entry), password);
    data->credentials_image = get_widget_from_builder(data->builder, "credentials_image");

    // connects
    g_signal_connect(G_OBJECT(data->cancel_btn ), "clicked",
                     G_CALLBACK(x2go_launcher_cancel_btn_clicked ), self );
    g_signal_connect(G_OBJECT(data->btn_close ), "clicked",
                     G_CALLBACK(x2go_launcher_btn_close_clicked ), self );
    g_signal_connect(G_OBJECT(data->btn_connect ), "clicked",
                     G_CALLBACK(x2go_launcher_btn_connect_clicked ), self );

    gtk_widget_show_all(data->window);
}

void x2go_launcher_start_pyhoca(X2goLauncher *self, const gchar *user, const gchar *password)
{
    // GUI
    x2go_launcher_setup_gui(user, password, self);

    // Process
    x2go_launcher_launch_process(self);
}
