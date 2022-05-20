/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <glib/gi18n.h>

#include "async.h"
#include "vdi_session.h"
#include "remote-viewer-util.h"
#include "vdi_user_settings_widget.h"
#include "qr_code_generation/qr_code_renderer.h"


static gboolean is_widget_active = FALSE; // на случай если каллбэк завершился после закрытия виджета

typedef struct{

    GMainLoop *loop;
    GtkBuilder *builder;

    GtkWidget *main_window;
    GtkWidget *qr_widget_container_box;
    GtkWidget *secret_label;

    GtkWidget *check_btn_2fa;
    GtkWidget *btn_generate_qr_code;
    GtkWidget *btn_apply;

    GtkWidget *status_msg_text_view;
    GtkWidget *spinner_status;

    QrWidget *qr_widget;

    gboolean is_2fa_enabled; // Текущее состояние. Включена ли 2fa

} VdiUserSettingsWidget;

void vdi_user_settings_widget_set_status(VdiUserSettingsWidget *self, const gchar *text)
{
    GtkTextBuffer *main_text_view_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->status_msg_text_view));
    gtk_text_buffer_set_text(main_text_view_buffer, text, -1);
}

static gboolean window_deleted_cb(VdiUserSettingsWidget *self)
{
    shutdown_loop(self->loop);
    return TRUE;
}

static void on_vdi_session_get_user_data_task_finished(GObject *source_object G_GNUC_UNUSED,
                                                       GAsyncResult *res, gpointer user_data)
{
    gpointer ptr_res = g_task_propagate_pointer(G_TASK (res), NULL);
    UserData *tk_user_data = (UserData *)ptr_res;
    if (!is_widget_active) {
        if (tk_user_data)
            vdi_api_session_free_tk_user_data(tk_user_data);
        return;
    }

    VdiUserSettingsWidget *self = (VdiUserSettingsWidget *) user_data;
    gtk_spinner_stop(GTK_SPINNER(self->spinner_status));

    if (ptr_res == NULL) {
        vdi_user_settings_widget_set_status(self, "Не удалось получить актульные данные с сервера");
        return;
    }

    if (tk_user_data->is_success) {
        // В случае успеха обновляем состояние GUI
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->check_btn_2fa), tk_user_data->two_factor);
        gtk_widget_set_sensitive(self->check_btn_2fa, TRUE);
        gtk_widget_set_sensitive(self->btn_generate_qr_code, tk_user_data->two_factor);
        gtk_widget_set_sensitive(self->btn_apply, FALSE);

        self->is_2fa_enabled = tk_user_data->two_factor;
        if (tk_user_data->two_factor) {
            // "Двухфакторная аутентификация включена.\n"
            // "Для отключения снимите галочку и нажмите Применить."
            vdi_user_settings_widget_set_status(self, _("Two-factor authentication enabled.\n"
                                                      "Uncheck 2fa and press Apply to disable."));
        } else {
            // "Двухфакторная аутентификация выключена. Для включения:\n"
            // "- поставьте галочку,\n"
            // "- сгенерируйте QR-код,\n"
            // "- отсканируйте QR-код c помощью аутентификатора "
            // "и нажмите Применить."
            vdi_user_settings_widget_set_status(self, _("Two-factor authentication disabled. To enable:\n"
                                                      "- check 2fa,\n"
                                                      "- generate QR-code,\n"
                                                      "- scan QR-code using an authenticator and press Apply.")
            );
        }

    } else {
        g_autofree gchar *error_msg = NULL;
        error_msg = g_strdup_printf("Возникла ошибка: %s", tk_user_data->error_message);
        vdi_user_settings_widget_set_status(self, error_msg);
    }

    vdi_api_session_free_tk_user_data(tk_user_data);
}

static void on_vdi_session_generate_qr_code_task_finished(GObject *source_object G_GNUC_UNUSED,
                                                         GAsyncResult *res, gpointer user_data)
{
    gpointer ptr_res = g_task_propagate_pointer(G_TASK (res), NULL);
    UserData *tk_user_data = (UserData *)ptr_res;
    if (!is_widget_active) {
        if (tk_user_data)
            vdi_api_session_free_tk_user_data(tk_user_data);
        return;
    }

    VdiUserSettingsWidget *self = (VdiUserSettingsWidget *) user_data;
    gtk_spinner_stop(GTK_SPINNER(self->spinner_status));

    if (ptr_res == NULL) { // "Не удалось сгенерировать QR-код"
        vdi_user_settings_widget_set_status(self, _("Failed to generate QR-code"));
        return;
    }

    if (tk_user_data->is_success) {
        // Generate QR image data
        qr_widget_generate_qr(self->qr_widget, tk_user_data->qr_uri);
        gtk_label_set_text(GTK_LABEL(self->secret_label), tk_user_data->secret);
        gtk_widget_set_sensitive(self->btn_apply, TRUE);

        // Если 2fa уже включена, то применять ничего не надо
        // "QR-код сгенерирован.\n"
        // "Отсканируйте QR-код c помощью аутентификатора\n"
        // "(пример Яндекс.Ключ, Google Authenticator). %s",
        // self->is_2fa_enabled ? "" : "Затем нажмите Применить."
        g_autofree gchar *msg = NULL;
        msg = g_strdup_printf(_("QR-code generated.\n"
                              "Scan QR-code using an authenticator\n"
                              "(Yandex.Key, Google Authenticator). %s"),
                              self->is_2fa_enabled ? "" : _("Then press Apply."));

        vdi_user_settings_widget_set_status(self, msg);

    } else {
        // "Возникла ошибка  %s"
        g_autofree gchar *error_msg = NULL;
        error_msg = g_strdup_printf(_("An error occurred  %s"), tk_user_data->error_message);
        vdi_user_settings_widget_set_status(self, error_msg);
    }

    vdi_api_session_free_tk_user_data(tk_user_data);
}

static void on_vdi_session_update_user_data_task_finished(GObject *source_object G_GNUC_UNUSED,
                                                          GAsyncResult *res, gpointer user_data)
{
    gpointer ptr_res = g_task_propagate_pointer(G_TASK (res), NULL);
    UserData *tk_user_data = (UserData *)ptr_res;
    if (!is_widget_active) {
        if (tk_user_data)
            vdi_api_session_free_tk_user_data(tk_user_data);
        return;
    }

    VdiUserSettingsWidget *self = (VdiUserSettingsWidget *) user_data;
    gtk_spinner_stop(GTK_SPINNER(self->spinner_status));

    if (ptr_res == NULL) { // "Не удалось применить изменения."
        vdi_user_settings_widget_set_status(self, _("Failed to apply changes."));
        return;
    }

    if (tk_user_data->is_success) {
        self->is_2fa_enabled = tk_user_data->two_factor;
        vdi_user_settings_widget_set_status(self, _("Changes applied successfully."));
    } else {
        g_autofree gchar *error_msg = NULL;
        error_msg = g_strdup_printf(_("An error occurred  %s"), tk_user_data->error_message);
        vdi_user_settings_widget_set_status(self, error_msg);
    }

    vdi_api_session_free_tk_user_data(tk_user_data);
}

static void on_btn_generate_qr_code_clicked(GtkButton *button G_GNUC_UNUSED, VdiUserSettingsWidget *self)
{
    // Отправлен запрос на генерацию QR.
    vdi_user_settings_widget_set_status(self, _("QR generation request sent."));
    gtk_spinner_start(GTK_SPINNER(self->spinner_status));
    execute_async_task(vdi_session_generate_qr_code_task,
                       (GAsyncReadyCallback)on_vdi_session_generate_qr_code_task_finished, NULL, self);
}

static void on_btn_apply_clicked(GtkButton *button G_GNUC_UNUSED, VdiUserSettingsWidget *self)
{
    // Отправлен запрос на применение изменений.
    vdi_user_settings_widget_set_status(self, _("Request to apply changes sent."));
    gtk_spinner_start(GTK_SPINNER(self->spinner_status));

    UserData *tk_user_data = calloc(1, sizeof(UserData)); // free in callback!
    tk_user_data->two_factor = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->check_btn_2fa));
    execute_async_task(vdi_session_update_user_data_task,
                       (GAsyncReadyCallback)on_vdi_session_update_user_data_task_finished, tk_user_data, self);
}

static void on_check_btn_2fa_toggled(GtkToggleButton *button, VdiUserSettingsWidget *self)
{
    gboolean is_active = gtk_toggle_button_get_active(button);
    if (is_active) {
        gtk_widget_set_sensitive(self->btn_generate_qr_code, TRUE);
        gtk_widget_set_sensitive(self->btn_apply, FALSE);
        // "Сгенерируйте QR-код."
        vdi_user_settings_widget_set_status(self, _("Generate QR-code."));
    } else {
        gtk_widget_set_sensitive(self->btn_generate_qr_code, FALSE);
        gtk_widget_set_sensitive(self->btn_apply, TRUE);
        // "Для отключения двухфакторной аутентификации нажмите Применить."
        vdi_user_settings_widget_set_status(self, _("Press Apply to disable 2fa."));
        // clear QR data
        qr_widget_clear(self->qr_widget);
        gtk_label_set_text(GTK_LABEL(self->secret_label), "");
    }
}

static void on_parent_hidden(GtkWidget *widget G_GNUC_UNUSED, gpointer user_data)
{
    VdiUserSettingsWidget *self = (VdiUserSettingsWidget *)user_data;
    shutdown_loop(self->loop);
}

void vdi_user_settings_widget_show(GtkWindow *parent)
{
    VdiUserSettingsWidget self = {};
    memset(&self, 0, sizeof(VdiUserSettingsWidget));
    is_widget_active = TRUE;

    self.builder = remote_viewer_util_load_ui("vdi_user_settings_form.glade");
    self.main_window = get_widget_from_builder(self.builder, "main_window");
    //gtk_window_set_deletable(self.main_window, FALSE);
    self.qr_widget_container_box = get_widget_from_builder(self.builder, "qr_widget_container_box");
    self.secret_label = get_widget_from_builder(self.builder, "secret_label");
    gtk_label_set_selectable(GTK_LABEL(self.secret_label), TRUE);

    self.check_btn_2fa = get_widget_from_builder(self.builder, "check_btn_2fa");
    self.btn_generate_qr_code = get_widget_from_builder(self.builder, "btn_generate_qr_code");
    self.btn_apply = get_widget_from_builder(self.builder, "btn_apply");

    self.status_msg_text_view = get_widget_from_builder(self.builder, "status_msg_text_view");
    self.spinner_status = get_widget_from_builder(self.builder, "spinner_status");

    // add QR widget
    self.qr_widget = qr_widget_new();
    gtk_box_pack_end(GTK_BOX(self.qr_widget_container_box), GTK_WIDGET(self.qr_widget), TRUE, TRUE, 0);

    // Signals
    g_signal_connect_swapped(self.main_window, "delete-event", G_CALLBACK(window_deleted_cb), &self);
    g_signal_connect(self.btn_generate_qr_code, "clicked", G_CALLBACK(on_btn_generate_qr_code_clicked), &self);
    g_signal_connect(self.btn_apply, "clicked", G_CALLBACK(on_btn_apply_clicked), &self);
    g_signal_connect(self.check_btn_2fa, "toggled", G_CALLBACK(on_check_btn_2fa_toggled), &self);
    gulong parent_hide_sig_handler = g_signal_connect(parent, "hide",
                                          G_CALLBACK(on_parent_hidden), &self);

    // Show
    gtk_window_set_transient_for(GTK_WINDOW(self.main_window), parent);
    gtk_window_set_position(GTK_WINDOW(self.main_window), GTK_WIN_POS_CENTER);
    gtk_widget_show_all(self.main_window);

    // Request user data
    execute_async_task(vdi_session_get_user_data_task,
                       (GAsyncReadyCallback)on_vdi_session_get_user_data_task_finished, NULL, &self);

    create_loop_and_launch(&self.loop);

    // clear
    g_signal_handler_disconnect(G_OBJECT(parent), parent_hide_sig_handler);
    is_widget_active = FALSE;
    g_object_unref(self.builder);
    gtk_widget_destroy(self.main_window);
}
