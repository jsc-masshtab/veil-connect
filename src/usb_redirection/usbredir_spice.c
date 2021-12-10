/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "usbredir_spice.h"
#include "remote-viewer-util.h"
#include "vdi_session.h"


static void usbredir_spice_set_status(SpiceUsbSession *self, const gchar *status)
{
    if (self->status_label)
        gtk_label_set_text(GTK_LABEL(self->status_label), status);
}

static void usbredir_spice_remove_cb(GtkContainer *container G_GNUC_UNUSED,
        GtkWidget *widget G_GNUC_UNUSED, void *data)
{
    gtk_window_resize(GTK_WINDOW(data), 1, 1);
}

static void usbredir_spice_usb_connect_failed(GObject               *object G_GNUC_UNUSED,
                               SpiceUsbDevice        *device G_GNUC_UNUSED,
                               GError                *error,
                               gpointer              data)
{
    GtkWidget *dialog;

    if (error && (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_CANCELLED))
        return;
    
    SpiceUsbSession *self = (SpiceUsbSession *)data;

    GtkWindow *parent = self->dialog ? GTK_WINDOW(self->dialog) : NULL;
    dialog = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_CLOSE, "USB redirection error");
    if (error)
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                             "%s", error->message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void usbredir_spice_show_usb_widget(SpiceUsbSession *self)
{
    if (self->main_container && self->usb_device_widget == NULL) {
        self->usb_device_widget = spice_usb_device_widget_new(self->session, NULL); /* default format */
        g_signal_connect(self->usb_device_widget, "connect-failed",
                         G_CALLBACK(usbredir_spice_usb_connect_failed), self);
        gtk_box_pack_start(GTK_BOX(self->main_container), self->usb_device_widget, TRUE, TRUE, 0);

        /* This shrinks the dialog when USB devices are unplugged */
        g_signal_connect(self->usb_device_widget, "remove",
                         G_CALLBACK(usbredir_spice_remove_cb), self->dialog);

        gtk_widget_show_all(self->dialog);
    }

    usbredir_spice_set_status(self, "");
}

// Отложенное создание usb окна. Если создать сразу после подключения, то не применяются USB фильтры со стоторы spice
// сервера
static gboolean usbredir_spice_delayed_show_usb_widget(gpointer data)
{
    SpiceUsbSession *self = (SpiceUsbSession *)data;
    usbredir_spice_show_usb_widget(self);
    return G_SOURCE_REMOVE;
}

static void usbredir_spice_main_channel_event(SpiceChannel *channel G_GNUC_UNUSED, SpiceChannelEvent event,
                               gpointer data)
{
    SpiceUsbSession *self = (SpiceUsbSession *)data;

    if (event == SPICE_CHANNEL_OPENED) {
        self->is_connected = TRUE;
        usbredir_spice_set_status(self, "Установлено SPICE подключение");
        g_timeout_add(1000, usbredir_spice_delayed_show_usb_widget, data);
    } else { // some conn error
        g_autofree gchar *msg = NULL;
        msg = g_strdup_printf("Не удалось установить соединение. %s\n"
                              "Убедитесь, что у ВМ включен удаленный доступ по протоколу SPICE.\n",
                              util_spice_channel_event_to_str(event));
        g_warning("SUP: main channel event: %u", event);
        usbredir_spice_set_status(self, msg);
        self->is_connected = FALSE;
    }
}

static void usbredir_spice_usb_channel_event(SpiceChannel *channel G_GNUC_UNUSED,
        SpiceChannelEvent event, gpointer data)
{
    if (event == SPICE_CHANNEL_OPENED) {
        g_timeout_add(200, usbredir_spice_delayed_show_usb_widget, data);
    }
}

static void usbredir_spice_channel_new(SpiceSession *s G_GNUC_UNUSED, SpiceChannel *channel, gpointer *data)
{
    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        g_signal_connect(channel, "channel-event",
                         G_CALLBACK(usbredir_spice_main_channel_event), data);

    } else if (SPICE_IS_USBREDIR_CHANNEL(channel)) {
        g_signal_connect(channel, "channel-event",
                         G_CALLBACK(usbredir_spice_usb_channel_event), data);
    }
}

static void usbredir_spice_channel_destroy(SpiceSession *session G_GNUC_UNUSED,
                            SpiceChannel *channel,
                            gpointer data)
{
    SpiceUsbSession *self = (SpiceUsbSession *)data;

    if (SPICE_IS_MAIN_CHANNEL(channel)) {
        usbredir_spice_set_status(self, "SPICE подключение завершилось");
        self->is_connected = FALSE;
    }
}

static void on_vdi_session_get_vm_data_task_finished(GObject *source_object G_GNUC_UNUSED,
        GAsyncResult *res, SpiceUsbSession *self)
{
    self->is_fetching_vm_data = FALSE;

    gpointer ptr_res = g_task_propagate_pointer(G_TASK(res), NULL); // take ownership
    g_autofree gchar *err_msg = NULL;
    err_msg = g_strdup("Не удалось получить данные для подключения SPICE\n"
                       "Убедитесь, что:\n"
                       "VeiL VDI имеет версию 3.2 и выше;\n"
                       "У ВМ включен удаленный доступ по протоколу SPICE.");
    if(ptr_res == NULL) { // "Не удалось получить список пулов"
        usbredir_spice_set_status(self, err_msg);
        return;
    }

    VdiVmData *vdi_vm_data = (VdiVmData *)ptr_res;
    if (vdi_vm_data->server_reply_type == SERVER_REPLY_TYPE_DATA) {

        usbredir_spice_set_status(self, "Получены данные для подключения SPICE");

        // Set spice credentials and connect
        usbredir_spice_set_credentials(self,
                                       vdi_vm_data->vm_host,
                                       vdi_vm_data->vm_password,
                                       vdi_vm_data->vm_port);
        usbredir_spice_connect(self);

    } else {
        const gchar *user_message = (strlen_safely(vdi_vm_data->message) != 0) ?
                                    vdi_vm_data->message : err_msg;
        usbredir_spice_set_status(self, user_message);
    }

    vdi_api_session_free_vdi_vm_data(vdi_vm_data);
}

SpiceUsbSession *usbredir_spice_new()
{
    SpiceUsbSession *self = calloc(1, sizeof(SpiceUsbSession));

    self->session = spice_session_new();
    g_signal_connect(self->session, "channel-new",
                     G_CALLBACK(usbredir_spice_channel_new), self);
    g_signal_connect(self->session, "channel-destroy",
                     G_CALLBACK(usbredir_spice_channel_destroy), self);

    return self;
}

void usbredir_spice_destroy(SpiceUsbSession *self)
{
    usbredir_spice_disconnect(self);

    g_object_unref(G_OBJECT(self->session));
    free_memory_safely(&self->host);
    free_memory_safely(&self->password);
    free(self);
}

void usbredir_spice_show_dialog(SpiceUsbSession *self, GtkWindow *parent)
{
    // create main GUI
    self->builder = remote_viewer_util_load_ui("usb_spice_dialog_form.glade");

    self->dialog = GTK_WIDGET(gtk_builder_get_object(self->builder, "main_dialog"));
    gtk_dialog_set_default_response(GTK_DIALOG(self->dialog), GTK_RESPONSE_ACCEPT);
    gtk_dialog_add_button(GTK_DIALOG(self->dialog), "Close", GTK_RESPONSE_ACCEPT);
    g_autofree gchar *title = NULL;
    title = g_strdup_printf("usbredir SPICE  -  %s", APPLICATION_NAME);
    gtk_window_set_title(GTK_WINDOW(self->dialog), title);
    gtk_window_set_default_size (GTK_WINDOW(self->dialog),  250, 100);
    gtk_window_set_transient_for(GTK_WINDOW(self->dialog), parent);

    self->main_container = GTK_WIDGET(gtk_builder_get_object(self->builder, "main_container"));
    self->status_label = GTK_WIDGET(gtk_builder_get_object(self->builder, "status_label"));

    // Spice Connect
    if (self->is_connected) {
        usbredir_spice_show_usb_widget(self);
    } else {
        // Sent request to get spice connect credentials
        if (!self->is_fetching_vm_data) {
            usbredir_spice_set_status(self, "Отправлен запрос данных для подключения SPICE");
            self->is_fetching_vm_data = TRUE;
            execute_async_task(vdi_session_get_vm_data_task,
                               (GAsyncReadyCallback)on_vdi_session_get_vm_data_task_finished,
                               NULL, self);
        }
    }

    /* show and run */
    gtk_widget_show_all(self->dialog);
    gtk_dialog_run(GTK_DIALOG(self->dialog));

    g_object_unref(self->builder);
    gtk_widget_destroy(self->dialog);
    self->dialog = NULL;
    self->main_container = NULL;
    self->usb_device_widget = NULL;
    self->status_label = NULL;
}

void usbredir_spice_set_credentials(SpiceUsbSession *self,
                                    const gchar *host, const gchar *password, guint port)
{
    if (host)
        g_object_set(self->session, "host", host, NULL);
    if (password)
        g_object_set(self->session, "password", password, NULL);
    if (port) {
        g_autofree gchar *port_str = NULL;
        port_str = g_strdup_printf("%i", port);
        g_object_set(self->session, "port", port_str, NULL);
    }
}

void usbredir_spice_connect(SpiceUsbSession *self)
{
    if (self->is_connected)
        return;

    usbredir_spice_set_status(self, "Установка SPICE подключения");
    if (!spice_session_connect(self->session)) {
        g_warning("spice_session_connect failed");
        usbredir_spice_set_status(self, "Не удалось подключиться по SPICE");
    }
}

void usbredir_spice_disconnect(SpiceUsbSession *self)
{
    if (!self->is_connected)
        return;

    spice_session_disconnect(self->session);
}