//
// Created by solomin on 23.12.2020.
//

#include "vdi_session.h"
#include "app_updater.h"

#include <stdlib.h>

#ifdef _WIN32
#include <Urlmon.h>
#endif

#include <glib/gstdio.h>
#include <glib.h>

#include "remote-viewer-util.h"


G_DEFINE_TYPE( AppUpdater, app_updater, G_TYPE_OBJECT )

//==============================Static======================================

static void app_updater_finalize( GObject *object );

static void app_updater_class_init( AppUpdaterClass *klass )
{
    GObjectClass *gobject_class = G_OBJECT_CLASS( klass );

    gobject_class->finalize = app_updater_finalize;

    // signals
    g_signal_new("status-msg-changed",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(AppUpdaterClass, status_msg_changed),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE,
                 1,
                 G_TYPE_STRING);

    g_signal_new("state-changed",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(AppUpdaterClass, state_changed),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE,
                 1,
                 G_TYPE_INT);
}

static void app_updater_init( AppUpdater *self G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
    g_mutex_init(&self->priv_members_mutex);

    self->_is_working = 0;
    self->_cur_status_msg = NULL;
    self->_admin_password = NULL;
}

static void app_updater_finalize( GObject *object )
{
    g_info("%s", (const char *)__func__);
    AppUpdater *self = APP_UPDATER(object);
    // clear mem
    g_mutex_lock(&self->priv_members_mutex);
    free_memory_safely(&self->_cur_status_msg);
    free_memory_safely(&self->_admin_password);
    g_mutex_unlock(&self->priv_members_mutex);

    wair_for_mutex_and_clear(&self->priv_members_mutex);

    // если сильно захотеть можно еще сделать отмену загрузки обновления, если она происходит;

    GObjectClass *parent_class = G_OBJECT_CLASS( app_updater_parent_class );
    ( *parent_class->finalize )( object );
}

static gboolean status_msg_changed(AppUpdater *self)
{
    g_signal_emit_by_name(self, "status-msg-changed", self->_cur_status_msg);
    return FALSE;
}

static gboolean state_changed(AppUpdater *self)
{
    g_signal_emit_by_name(self, "state-changed", self->_is_working);
    return FALSE;
}

static void set_status_msg(AppUpdater *self, const gchar *status_msg)
{
    g_info("set_status_msg: %s", status_msg);
    g_mutex_lock(&self->priv_members_mutex);
    update_string_safely(&self->_cur_status_msg, status_msg);
    gdk_threads_add_idle((GSourceFunc)status_msg_changed, self);
    g_mutex_unlock(&self->priv_members_mutex);
}

#ifdef __linux__
static void
app_updater_get_linux_updates(GTask    *task G_GNUC_UNUSED,
                                gpointer     source_object G_GNUC_UNUSED,
                                gpointer     task_data,
                                GCancellable *cancellable G_GNUC_UNUSED)
{
    AppUpdater *self = (AppUpdater *)task_data;
    if (self->_is_working)
        return;

    gchar *last_version = NULL;
    gchar *standard_output = NULL;
    gchar *standard_error = NULL;
    gint exit_status = 0;
    GMatchInfo *match_info = NULL;
    GRegex *regex = NULL;
    gchar *found_match = NULL;

    const gchar *package_name = "veil-connect"; // veil-connect

    set_status_msg(self, "Проверка наличия обновлений."); // code repeat
    self->_is_working = 1;
    gdk_threads_add_idle((GSourceFunc)state_changed, self);

    // Проверка наличия обновлении в linux репозитории пакетов
    gchar *command_line = g_strdup_printf("apt list --upgradable | grep %s", package_name);
    gboolean cmd_success = g_spawn_command_line_sync(command_line, &standard_output, &standard_error, &exit_status,
                                                 NULL);
    if (!cmd_success) {
        set_status_msg(self, "Не удалось проверить наличие обновлений.");
        goto clear_mark;
    }

    // Найти строку содержащую veil-connect. Разделить по пробелам. Второй элемент - новая версия
    // ^(veil-connect.+)\n
    gchar *string_pattern = g_strdup_printf("^(%s.+)\\n", package_name);
    regex = g_regex_new(string_pattern, G_REGEX_MULTILINE, 0, NULL);
    g_regex_match(regex, standard_output, 0, &match_info);
    g_free(string_pattern);

    gboolean is_matched = g_match_info_matches(match_info);

    if (!is_matched) {
        set_status_msg(self, "Нет доступных обновлений [код: 1].");
        goto clear_mark;
    }

    found_match = g_match_info_fetch(match_info, 0);
    // вычленить версию
    gint max_tokens = 3;
    gchar **package_str_array = g_strsplit(found_match, " ", max_tokens);
    if (package_str_array == NULL) {
        set_status_msg(self, "Нет доступных обновлений [код: 2].");
        goto clear_mark;
    }

    guint array_len = g_strv_length(package_str_array);
    if (array_len == max_tokens) {
        last_version = g_strdup(package_str_array[1]);
    }
    g_strfreev(package_str_array);

    if (last_version == NULL) {
        set_status_msg(self, "Нет доступных обновлений [код: 3].");
        goto clear_mark;
    }

    // clear
    free_memory_safely(&command_line);
    free_memory_safely(&standard_output);
    free_memory_safely(&standard_error);

    // Запускаем установку обновлений
    gchar *msg = g_strdup_printf("Устанавливается новая версия %s.", last_version);
    set_status_msg(self, msg);
    g_free(msg);

    g_mutex_lock(&self->priv_members_mutex);
    command_line = g_strdup_printf("./start_client_update.sh %s %s", self->_admin_password, package_name);
    g_mutex_unlock(&self->priv_members_mutex);

    cmd_success = g_spawn_command_line_sync(command_line, &standard_output, &standard_error, &exit_status,
                                                     NULL);
    if (!cmd_success) {
        set_status_msg(self, "Не удалось запустить процесс установки новой версии ПО.");
        goto clear_mark;
    }
    g_info("App update standard_output: %s", standard_output);
    g_info("App update standard_error: %s", standard_error);
    g_info("App update exit_status: %i", exit_status);

    if (exit_status == 0)
        msg = g_strdup_printf("Процесс установки версии %s завершился.\nПерезапустите приложение.", last_version);
    else
        msg = g_strdup_printf("Процесс установки версии %s завершился\nс ошибками. См. лог файл.", last_version);
    set_status_msg(self, msg);
    g_free(msg);

    // clear
    clear_mark:
    free_memory_safely(&last_version);
    if (regex)
        g_regex_unref(regex);
    if(match_info)
        g_match_info_free(match_info);
    free_memory_safely(&command_line);
    free_memory_safely(&standard_output);
    free_memory_safely(&standard_error);
    free_memory_safely(&found_match);
    g_mutex_lock(&self->priv_members_mutex);
    free_memory_safely(&self->_admin_password);
    g_mutex_unlock(&self->priv_members_mutex);

    // notify about stop
    self->_is_working = 0;
    gdk_threads_add_idle((GSourceFunc)state_changed, self);
}
#elif _WIN32
static void
app_updater_get_windows_updates(GTask    *task G_GNUC_UNUSED,
                            gpointer     source_object G_GNUC_UNUSED,
                            gpointer     task_data,
                            GCancellable *cancellable G_GNUC_UNUSED)
{
    AppUpdater *self = (AppUpdater *)task_data;
    if (self->_is_working)
        return;

    gchar *last_version = NULL;

    // start
    set_status_msg(self, "Проверка наличия обновлений.");
    self->_is_working = 1;
    gdk_threads_add_idle((GSourceFunc)state_changed, self);

    gchar *full_file_name = NULL;
    gchar *app_data_dir = get_windows_app_data_location();
    gchar *temp_dir = g_strdup_printf("%s/temp", app_data_dir);
    g_mkdir_with_parents(temp_dir, 0755);

    // Check for updates
    const gchar *veil_connect_url = "https://veil-update.mashtab.org/files/veil-connect/latest/windows/";
    gchar *download_link = vdi_session_check_for_tk_updates(veil_connect_url, &last_version);
    if (download_link == NULL) { // download_link == NULL
        set_status_msg(self, "Нет доступных обновлений.");
        goto clear_mark;
    }

    // Download the latest version if required
    set_status_msg(self, "Скачивание новой версии ПО.");

    full_file_name = g_strdup_printf("%s/installer.exe", temp_dir);

    HRESULT res = URLDownloadToFile(NULL, download_link, full_file_name, 0, NULL);
    free_memory_safely(&download_link);

    if (res != S_OK) {
        gchar *reason_phrase = NULL;
        const gchar *common_phrase = "Не удалось загрузить обновления.";
        if (res == E_OUTOFMEMORY) {
            reason_phrase = g_strdup_printf("%s Buffer length invalid or insufficient memory.", common_phrase);
        } else if (res == INET_E_DOWNLOAD_FAILURE) {
            reason_phrase = g_strdup_printf("%s URL is invalid.", common_phrase);
        } else {
            reason_phrase = g_strdup_printf("%s Other error: %ld.", common_phrase, res);
        }

        set_status_msg(self, reason_phrase);
        g_free(reason_phrase);
        goto clear_mark;
    }

    // Launch  installer if downloaded
    set_status_msg(self, "Установка новой версии ПО.");

    gchar **argv = malloc(2 * sizeof(gchar *));
    argv[0] = full_file_name;
    argv[1] = NULL;

    GError *error = NULL;
    if (g_spawn_sync(NULL,
                     argv,
                     NULL,
                     G_SPAWN_DEFAULT, //G_SPAWN_DO_NOT_REAP_CHILD,
                     NULL,
                     NULL,
                     NULL,
                     NULL,
                     NULL,
                     &error)
            ) {
        g_info("installer successfully spawned"); // last_version
        gchar *msg = g_strdup_printf("Процесс установки версии %s завершился. Перезапустите приложение.",
                                     last_version);
        set_status_msg(self, msg);
        g_free(msg);
    } else {
        g_info("installer spawning failed");
        set_status_msg(self, "Не удалось запустить процесс установки новой версии ПО.");
    }
    free(argv);
    if (error)
        g_info("%s %s", (const char *) __func__, error->message);
    g_clear_error(&error);

    // clear
    clear_mark:
    free_memory_safely(&temp_dir);
    free_memory_safely(&app_data_dir);
    free_memory_safely(&full_file_name);
    free_memory_safely(&last_version);

    //
    self->_is_working = 0;
    gdk_threads_add_idle((GSourceFunc)state_changed, self);
}
#endif
//=============================Global=======================================

AppUpdater *app_updater_new()
{
    AppUpdater *app_updater = APP_UPDATER( g_object_new( TYPE_APP_UPDATER, NULL ) );

    return app_updater;
}

// Нужно освободить возвращаемое значение
gchar *app_updater_get_cur_status_msg(AppUpdater *self)
{
    g_mutex_lock(&self->priv_members_mutex);
    gchar *temp_cur_status_msg = g_strdup(self->_cur_status_msg);
    g_mutex_unlock(&self->priv_members_mutex);
    return temp_cur_status_msg;
}

int app_updater_is_working(AppUpdater *self)
{
    return self->_is_working;
}

void app_updater_set_admin_password(AppUpdater *self, const gchar *admin_password)
{
    g_mutex_lock(&self->priv_members_mutex);
    update_string_safely(&self->_admin_password, admin_password);
    g_mutex_unlock(&self->priv_members_mutex);
}

// Нужно освободить возвращаемое значение
gchar *app_updater_get_admin_password(AppUpdater *self)
{
    g_mutex_lock(&self->priv_members_mutex);
    gchar *temp_admin_password = g_strdup(self->_admin_password);
    g_mutex_unlock(&self->priv_members_mutex);
    return temp_admin_password;
}

#ifdef __linux__
void app_updater_execute_task_get_linux_updates(AppUpdater *self)
{
    execute_async_task(app_updater_get_linux_updates, NULL, self, NULL);
}

#elif _WIN32
void app_updater_execute_task_get_windows_updates(AppUpdater *self)
{
    execute_async_task(app_updater_get_windows_updates, NULL, self, NULL);
}
#endif
