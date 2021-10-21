/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <string.h>
#include <stdlib.h>

#include <glib/gi18n.h>

#include "messenger.h"
#include "vdi_session.h"
#include "remote-viewer-util.h"

#define MAX_MSG_SIZE 4096 // Максимальное число байт в сообщении. Обрезать если больше
#define MAX_CHAR_NUMBER MAX_MSG_SIZE * 1000 // Максимальное хранимое число символов чата

G_DEFINE_TYPE( VeilMessenger, veil_messenger, G_TYPE_OBJECT )

static void veil_messenger_finalize(GObject *object)
{
    g_info("%s", (const char *)__func__);
    VeilMessenger *self = VEIL_MESSENGER(object);
    //g_signal_handler_disconnect(get_vdi_session_static(), self->ws_conn_changed_handle);
    g_signal_handler_disconnect(get_vdi_session_static(), self->text_msg_received_handle);

    g_object_unref(self->builder);
    if (self->main_window)
        gtk_widget_destroy(self->main_window);

    GObjectClass *parent_class = G_OBJECT_CLASS( veil_messenger_parent_class );
    ( *parent_class->finalize )( object );
}

static void veil_messenger_class_init(VeilMessengerClass *klass )
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = veil_messenger_finalize;
}

static gboolean on_window_deleted_cb(VeilMessenger *self)
{
    g_info("%s", (const char *)__func__);
    gtk_widget_hide(self->main_window);
    return TRUE;
}

static void insert_text_to_message_main_view(VeilMessenger *self, const gchar *trimmed_msg, const gchar *author_name,
                                             const gchar *name_color)
{
    if(strlen_safely(trimmed_msg) == 0)
        return;

    /// Set text
    GtkTextBuffer *message_main_view_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->message_main_view));

    GtkTextIter message_main_view_iter;
    gtk_text_buffer_get_end_iter(message_main_view_buffer, &message_main_view_iter);

    // form final string
    GDateTime *datetime = g_date_time_new_now_local();
    g_autofree gchar *data_time_string = NULL;
    data_time_string = g_date_time_format(datetime, "%H:%M:%S"); // %d.%m
    g_date_time_unref(datetime);

    g_autofree gchar *formatted_msg = NULL; // color="green"
    formatted_msg = g_strdup_printf("<span color=\"blue\">%s</span> <span background=\"%s\">%s</span>: ",
                                    data_time_string, name_color, author_name);
    gtk_text_buffer_insert_markup(message_main_view_buffer, &message_main_view_iter, formatted_msg, -1);

    gtk_text_buffer_insert(message_main_view_buffer, &message_main_view_iter, trimmed_msg, -1);
    gtk_text_buffer_insert(message_main_view_buffer, &message_main_view_iter, "\n\n", -1);

    // Delete some text in the beginning of the chat if the limit is reached
    gint char_number = gtk_text_buffer_get_char_count(message_main_view_buffer);
    if (char_number > MAX_CHAR_NUMBER) {
        GtkTextIter start_iter;
        gtk_text_buffer_get_start_iter(message_main_view_buffer, &start_iter);

        GtkTextIter offset_from_start_iter;
        gtk_text_buffer_get_iter_at_offset(message_main_view_buffer, &offset_from_start_iter, MAX_MSG_SIZE);

        gtk_text_buffer_delete(message_main_view_buffer, &start_iter, &offset_from_start_iter);
    }

    self->scroll_down_required = TRUE; // scroll to bottom/
}

// Callback. Вызывается по завершению пост запроса отсылки сообщения
void on_send_text_msg_task_finished(GObject *source_object G_GNUC_UNUSED,
        GAsyncResult *res G_GNUC_UNUSED, gpointer user_data)
{
    TextMessageData *text_message_data = (TextMessageData *)user_data;
    VeilMessenger *self = (VeilMessenger *)text_message_data->weak_ref_to_logical_owner;

    gtk_spinner_stop(GTK_SPINNER(self->spinner_status));

    if (!text_message_data->is_successfully_sent) {

        g_autofree gchar *full_err_msg = NULL;
        const gchar *err_msg = text_message_data->error_message ? text_message_data->error_message : "";
        full_err_msg = g_strdup_printf("Send error: %s", err_msg);
        g_autofree gchar *trimmed_err_msg = NULL;
        trimmed_err_msg = g_strndup(full_err_msg, 120);

        gtk_label_set_text(GTK_LABEL(self->label_status), trimmed_err_msg);
        g_warning("%s", full_err_msg);

    } else {
        GtkTextBuffer *message_entry_view_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->message_entry_view));

        // Insert text into main view
        insert_text_to_message_main_view(self, text_message_data->message, _("You"), "#99ffeb");

        // Очищаем поле ввода текста в случае успешной отправки
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(message_entry_view_buffer, &start, &end);
        gtk_text_buffer_delete(message_entry_view_buffer, &start, &end);
        // Сообщение отправлено
        gtk_label_set_text(GTK_LABEL(self->label_status), _("Message sent"));
        g_info("Сообщение отправлено");
        g_info("%s", text_message_data->message);
    }

    // Очищаем память, выделенную перед запуском задачи
    vdi_api_session_free_text_message_data(text_message_data);
}

static void on_btn_send_message_clicked(GtkButton *button G_GNUC_UNUSED, VeilMessenger *self)
{
    /// append text to message_main_view
    /// Get text
    GtkTextBuffer *message_entry_view_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->message_entry_view));

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(message_entry_view_buffer, &start, &end);
    g_autofree gchar *msg = NULL;
    msg = gtk_text_buffer_get_text(message_entry_view_buffer, &start, &end, TRUE);

    if(strlen_safely(msg) == 0)
        return;

    TextMessageData *text_message_data = calloc(1, sizeof(TextMessageData)); // will be freed in callback
    text_message_data->message = g_strndup(msg, MAX_MSG_SIZE);
    text_message_data->weak_ref_to_logical_owner = (gpointer)self;

    // setup gui
    gtk_spinner_start(GTK_SPINNER(self->spinner_status));
    // Идет отправка сообщения
    gtk_label_set_text(GTK_LABEL(self->label_status), _("Sending message"));

    // Send to the server
    execute_async_task(vdi_session_send_text_msg_task, on_send_text_msg_task_finished,
            text_message_data, text_message_data);
}

// callback. msg from the server
static void on_text_msg_received_received(gpointer data  G_GNUC_UNUSED,
        const gchar *sender_name, const gchar *text, VeilMessenger *self)
{
    g_info("%s", (const char *)__func__);
    if(strlen_safely(text) == 0)
        return;

    g_autofree gchar *trimmed_msg = NULL;
    trimmed_msg = g_strndup(text, MAX_MSG_SIZE);

    insert_text_to_message_main_view(self, trimmed_msg, sender_name, "yellow");

    veil_messenger_show_on_top(self);
}

static void on_vertical_scroll_changed(GtkAdjustment *vertical_scroll, VeilMessenger *self)
{
    // Перемещаем скрол бегунок вниз
    if (self->scroll_down_required) {
        //g_info("%s", (const char *)__func__);
        gtk_adjustment_set_value(vertical_scroll, gtk_adjustment_get_upper(vertical_scroll));
        self->scroll_down_required = FALSE;
    }
}

static void veil_messenger_init(VeilMessenger *self)
{
    g_info("%s", (const char *)__func__);

    self->builder = remote_viewer_util_load_ui("vdi_messenger_from.glade");
    self->main_window = GTK_WIDGET(gtk_builder_get_object(self->builder, "main_window"));
    self->message_entry_view = GTK_WIDGET(gtk_builder_get_object(self->builder, "message_entry_view"));
    self->btn_send_message = GTK_WIDGET(gtk_builder_get_object(self->builder, "btn_send_message"));
    self->message_main_view = GTK_WIDGET(gtk_builder_get_object(self->builder, "message_main_view"));
    self->message_main_scrolled_window =
            GTK_WIDGET(gtk_builder_get_object(self->builder, "message_main_scrolled_window"));
    GtkAdjustment *vertical_scroll = gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(self->message_main_scrolled_window));
    self->label_status = GTK_WIDGET(gtk_builder_get_object(self->builder, "label_status"));
    self->spinner_status = GTK_WIDGET(gtk_builder_get_object(self->builder, "spinner_status"));

    // connects
    g_signal_connect_swapped(self->main_window, "delete-event", G_CALLBACK(on_window_deleted_cb), self);
    g_signal_connect(self->btn_send_message, "clicked", G_CALLBACK(on_btn_send_message_clicked), self);
    g_signal_connect(vertical_scroll, "changed", G_CALLBACK(on_vertical_scroll_changed), self);

    self->text_msg_received_handle = g_signal_connect(get_vdi_session_static(), "text-msg-received",
                                              G_CALLBACK(on_text_msg_received_received), self);

    gtk_window_resize(GTK_WINDOW(self->main_window), 800, 400);
}

VeilMessenger *veil_messenger_new()
{
    g_info("%s", (const char *)__func__);
    VeilMessenger *veil_messenger = VEIL_MESSENGER( g_object_new( TYPE_VEIL_MESSENGER, NULL ) );
    return veil_messenger;
}

void veil_messenger_show_on_top(VeilMessenger *self)
{
    // Показать окно с диалогом пользователю поверх всех
    gtk_widget_show_all(self->main_window);
    gtk_window_present(GTK_WINDOW(self->main_window));
}

void veil_messenger_hide(VeilMessenger *self)
{
    gtk_widget_hide(self->main_window);
}
