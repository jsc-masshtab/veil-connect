/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include "vdi_session.h"
#include "app_updater.h"
#include "config.h"
#include "settingsfile.h"

#include <stdlib.h>

#ifdef __linux__
#include <sys/utsname.h>
#elif _WIN32
#include <Urlmon.h>
#endif

#include <glib/gstdio.h>
#include <glib.h>

#include "remote-viewer-util.h"

G_DEFINE_TYPE( AppUpdater, app_updater, G_TYPE_OBJECT )

//==============================Static======================================

static void app_updater_finalize( GObject *object );

#ifdef _WIN32
static void app_updater_read_windows_updates_url_from_file(AppUpdater *self)
{
    gchar *windows_updates_url = read_str_from_ini_file("ServiceSettings", "windows_updates_url");
    if (windows_updates_url) {
        app_updater_set_windows_releases_url(self, windows_updates_url);
        g_free(windows_updates_url);
    }
}
#endif

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

    g_signal_new("updates-checked",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(AppUpdaterClass, updates_checked),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE,
                 1,
                 G_TYPE_INT);
}

static void app_updater_init( AppUpdater *self )
{
    g_info("%s", (const char *)__func__);
    g_mutex_init(&self->priv_members_mutex);

    g_mutex_lock(&self->priv_members_mutex);

    self->_is_checking_updates = FALSE;
    self->_is_getting_updates = FALSE;
    self->_last_exit_status = 0;
    self->_cur_status_msg = NULL;
    self->_admin_password = NULL;
    self->_last_standard_output = NULL;
    self->_last_standard_error = NULL;
    self->_windows_releases_url = g_strdup("https://veil-update.mashtab.org/veil-connect/windows/latest/");

    g_mutex_unlock(&self->priv_members_mutex);

#ifdef __linux__
    // detect CentOS (if release contains .el)
    struct utsname name;
    if (uname(&name) == 0 && strstr(name.release, ".el"))
        self->_linux_distro = LINUX_DISTRO_CENTOS_LIKE;
    else
        self->_linux_distro = LINUX_DISTRO_DEBIAN_LIKE;
#endif
}

static void app_updater_finalize( GObject *object )
{
    g_info("%s", (const char *)__func__);
    AppUpdater *self = APP_UPDATER(object);
    // clear mem
    g_mutex_lock(&self->priv_members_mutex);
    free_memory_safely(&self->_cur_status_msg);
    free_memory_safely(&self->_admin_password);
    free_memory_safely(&self->_last_standard_output);
    free_memory_safely(&self->_last_standard_error);
    free_memory_safely(&self->_windows_releases_url);
    g_mutex_unlock(&self->priv_members_mutex);

    wair_for_mutex_and_clear(&self->priv_members_mutex);

    // ???????? ???????????? ???????????????? ?????????? ?????? ?????????????? ???????????? ???????????????? ????????????????????, ???????? ?????? ????????????????????;

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
    g_signal_emit_by_name(self, "state-changed", self->_is_getting_updates);
    return FALSE;
}

static void set_status_msg(AppUpdater *self, const gchar *status_msg)
{
    g_info("%s: %s", (const char *)__func__, status_msg);
    g_mutex_lock(&self->priv_members_mutex);
    update_string_safely(&self->_cur_status_msg, status_msg);
    gdk_threads_add_idle((GSourceFunc)status_msg_changed, self);
    g_mutex_unlock(&self->priv_members_mutex);
}

// callback for app_updater_check_updates_task
static void
app_updater_on_check_updates_task_finished(GObject *source_object G_GNUC_UNUSED,
                                           GAsyncResult *res,
                                           gpointer user_data)
{
    AppUpdater *self = (AppUpdater *)user_data;
    gboolean is_available = g_task_propagate_boolean(G_TASK(res), NULL);
    g_signal_emit_by_name(self, "updates-checked", is_available);
}

#ifdef __linux__

// ?????????????????? ?????????????? ?????????? ????????????. ???????????????????? TRUE ???????? ???????? ?????????? ????????????
static gboolean app_updater_check_linux_updates(AppUpdater *self,
        gchar **command_line, gchar **standard_output, gchar **standard_error, gchar **last_version,
                                            GMatchInfo **match_info)
{
    if (self->_linux_distro == LINUX_DISTRO_DEBIAN_LIKE)
        *command_line = g_strdup_printf("apt list --upgradable");
    else if (self->_linux_distro == LINUX_DISTRO_CENTOS_LIKE)
        *command_line = g_strdup_printf("yum check-update | grep %s", PACKAGE);
    else
        return FALSE;

    gboolean cmd_success = g_spawn_command_line_sync(*command_line, standard_output, standard_error,
                                                     &self->_last_exit_status, NULL);
    if (!cmd_success) {
        set_status_msg(self, "???? ?????????????? ?????????????????? ?????????????? ????????????????????.");
        return FALSE;
    }

    // ?????????? ???????????? ???????????????????? veil-connect. ?????????????????? ???? ????????????????. ???????????? ?????????????? - ?????????? ????????????
    // ^(veil-connect.+)\n
    gchar *string_pattern = g_strdup_printf("^(%s.+)\\n", PACKAGE);
    GRegex *regex = g_regex_new(string_pattern, G_REGEX_MULTILINE, 0, NULL);
    g_regex_match(regex, *standard_output, 0, match_info);
    g_free(string_pattern);

    gboolean is_matched = g_match_info_matches(*match_info);

    g_regex_unref(regex);

    if (!is_matched) {
        set_status_msg(self, "?????? ?????????????????? ???????????????????? [??????: 1].");
        return FALSE;
    }

    gchar *found_match = g_match_info_fetch(*match_info, 0);
    // ?????????????????? ????????????
    const guint requied_tokens = 3;
    //gchar **package_str_array = g_strsplit(found_match, " ", max_tokens);
    gchar **package_str_array = g_regex_split_simple("\\s+", found_match, 0, 0);
    free_memory_safely(&found_match);
    if (package_str_array == NULL) {
        set_status_msg(self, "?????? ?????????????????? ???????????????????? [??????: 2].");
        return FALSE;
    }

    guint array_len = g_strv_length(package_str_array);
    if (array_len >= requied_tokens) {
        *last_version = g_strdup(package_str_array[1]);
    }
    g_strfreev(package_str_array);

    if (*last_version == NULL) {
        set_status_msg(self, "?????? ?????????????????? ???????????????????? [??????: 3].");
        return FALSE;
    }

    return TRUE;
}

// ?????????????????? ????????????????????
static void
app_updater_check_linux_updates_task(GTask    *task G_GNUC_UNUSED,
                                   gpointer     source_object G_GNUC_UNUSED,
                                   gpointer     task_data,
                                   GCancellable *cancellable G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);

    AppUpdater *self = (AppUpdater *)task_data;
    if (self->_is_checking_updates)
        return;
    self->_is_checking_updates = TRUE;

    gchar *last_version = NULL;
    gchar *standard_output = NULL;
    gchar *standard_error = NULL;
    gchar *command_line = NULL;
    GMatchInfo *match_info = NULL;

    gboolean is_updates_available = app_updater_check_linux_updates(self,
                                    &command_line, &standard_output, &standard_error, &last_version, &match_info);

    if(match_info)
        g_match_info_free(match_info);
    free_memory_safely(&command_line);
    free_memory_safely(&last_version);
    free_memory_safely(&standard_output);
    free_memory_safely(&standard_error);

    g_task_return_boolean(task, is_updates_available);

    self->_is_checking_updates = FALSE;
}

// ?????????????????? ?? ???????????? ????????????????????
static void
app_updater_get_linux_updates_task(GTask    *task G_GNUC_UNUSED,
                                gpointer     source_object G_GNUC_UNUSED,
                                gpointer     task_data,
                                GCancellable *cancellable G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);

    AppUpdater *self = (AppUpdater *)task_data;
    if (self->_is_getting_updates || self->_linux_distro == LINUX_DISTRO_UNKNOWN)
        return;

    gchar *last_version = NULL;
    gchar *standard_output = NULL;
    gchar *standard_error = NULL;
    gchar *command_line = NULL;
    GMatchInfo *match_info = NULL;

    g_mutex_lock(&self->priv_members_mutex);
    free_memory_safely(&self->_last_standard_output);
    free_memory_safely(&self->_last_standard_error);
    g_mutex_unlock(&self->priv_members_mutex);

    // check if update script exists
    if (!g_file_test("update_script.sh", G_FILE_TEST_EXISTS)) {
        set_status_msg(self, "?????????????????????? ???????? ???????????? ???????????????????? update_script.sh!\n"
                             "???????????????????? ???????????????????????????? ????????????????????");
        goto clear_mark;
    }

    // ???????????????? ????????????
    set_status_msg(self, "???????????????? sudo ????????????.");
    command_line = g_strdup_printf("./update_script.sh %s %s", "check_sudo_pass", self->_admin_password);
    GError *error = NULL;
    g_spawn_command_line_sync(command_line, &standard_output, &standard_error, &self->_last_exit_status, &error);

    if (error) {
        gchar *msg = g_strdup_printf("???? ?????????????? ?????????????????? ????????????.\n%s", error->message);
        set_status_msg(self, msg);
        g_free(msg);
        g_error_free(error);
        goto clear_mark;
    }

    if (standard_output && !strstr(standard_output, "success_pass")) {
        set_status_msg(self, "???????????????????????? ????????????.");
        goto clear_mark;
    }

    g_info("0App update standard_output: %s", standard_output);
    g_info("0App update standard_error: %s", standard_error);
    free_memory_safely(&command_line);
    free_memory_safely(&standard_output);
    free_memory_safely(&standard_error);

    // ?????????????????? ???????????? ?????????????????? ????????????????????
    set_status_msg(self, "???????????????? ?????????????? ????????????????????.");
    self->_is_getting_updates = 1;
    gdk_threads_add_idle((GSourceFunc)state_changed, self);

    if (self->_linux_distro == LINUX_DISTRO_DEBIAN_LIKE)
        command_line = g_strdup_printf("./update_script.sh %s %s", "apt_update", self->_admin_password);
    else if (self->_linux_distro == LINUX_DISTRO_CENTOS_LIKE)
        command_line = g_strdup_printf("./update_script.sh %s %s", "yum_makecache", self->_admin_password);

    g_spawn_command_line_sync(command_line, &standard_output, &standard_error, &self->_last_exit_status, NULL);
    g_info("1App update standard_output: %s", standard_output);
    g_info("1App update standard_error: %s", standard_error);
    free_memory_safely(&command_line);
    free_memory_safely(&standard_output);
    free_memory_safely(&standard_error);

    // ???????????????? ?????????????? ????????????????????
    if (!app_updater_check_linux_updates(self,
            &command_line, &standard_output, &standard_error, &last_version, &match_info))
        goto clear_mark;

    // clear
    free_memory_safely(&command_line);
    free_memory_safely(&standard_output);
    free_memory_safely(&standard_error);

    // ?????????????????? ?????????????????? ????????????????????
    gchar *msg = g_strdup_printf("?????????????????????????????? ?????????? ???????????? %s.", last_version);
    set_status_msg(self, msg);
    g_free(msg);

    g_mutex_lock(&self->priv_members_mutex);
    if (self->_linux_distro == LINUX_DISTRO_DEBIAN_LIKE)
        command_line = g_strdup_printf("./update_script.sh %s %s %s", "apt_install", self->_admin_password, PACKAGE);
    else if (self->_linux_distro == LINUX_DISTRO_CENTOS_LIKE)
        command_line = g_strdup_printf("./update_script.sh %s %s %s", "yum_update", self->_admin_password, PACKAGE);
    g_mutex_unlock(&self->priv_members_mutex);

    gboolean cmd_success = g_spawn_command_line_sync(command_line, &standard_output, &standard_error,
            &self->_last_exit_status, NULL);
    if (!cmd_success) {
        set_status_msg(self, "???? ?????????????? ?????????????????? ?????????????? ?????????????????? ?????????? ???????????? ????.");
        goto clear_mark;
    }
    g_info("2App update standard_output: %s", standard_output);
    g_info("2App update standard_error: %s", standard_error);
    g_info("App update exit_status: %i", self->_last_exit_status);

    if (self->_last_exit_status == 0)
        msg = g_strdup_printf("?????????????? ?????????????????? ???????????? %s ????????????????????.\n?????????????????????????? ????????????????????.", last_version);
    else
        msg = g_strdup_printf("?????????????? ?????????????????? ???????????? %s ????????????????????\n?? ????????????????.", last_version);
    set_status_msg(self, msg);
    g_free(msg);

    // clear
    clear_mark:

    if(match_info)
        g_match_info_free(match_info);

    g_mutex_lock(&self->priv_members_mutex);
    free_memory_safely(&self->_admin_password);

    free_memory_safely(&self->_last_standard_output);
    self->_last_standard_output = g_strdup(standard_output);

    free_memory_safely(&self->_last_standard_error);
    self->_last_standard_error = g_strdup(standard_error);
    g_mutex_unlock(&self->priv_members_mutex);

    free_memory_safely(&command_line);
    free_memory_safely(&last_version);
    free_memory_safely(&standard_output);
    free_memory_safely(&standard_error);

    // notify about stop
    self->_is_getting_updates = 0;
    gdk_threads_add_idle((GSourceFunc)state_changed, self);
}
#elif _WIN32
// ?????????????????? ????????????????????
static void
app_updater_check_windows_updates_task(GTask    *task G_GNUC_UNUSED,
                                     gpointer     source_object G_GNUC_UNUSED,
                                     gpointer     task_data,
                                     GCancellable *cancellable G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);

    AppUpdater *self = (AppUpdater *)task_data;
    if (self->_is_checking_updates)
        return;
    self->_is_checking_updates = TRUE;

    gchar *last_version = NULL;
    gchar *windows_releases_url = app_updater_get_windows_releases_url(self);
    gchar *download_link = vdi_session_check_for_tk_updates(windows_releases_url, &last_version);
    free_memory_safely(&windows_releases_url);

    gboolean is_updates_available = (download_link != NULL);
    free_memory_safely(&download_link);

    g_task_return_boolean(task, is_updates_available);

    self->_is_checking_updates = FALSE;
}

static void
app_updater_get_windows_updates_task(GTask    *task G_GNUC_UNUSED,
                            gpointer     source_object G_GNUC_UNUSED,
                            gpointer     task_data,
                            GCancellable *cancellable G_GNUC_UNUSED)
{
    AppUpdater *self = (AppUpdater *)task_data;
    if (self->_is_getting_updates)
        return;

    gchar *last_version = NULL;

    // start
    set_status_msg(self, "???????????????? ?????????????? ????????????????????.");
    self->_is_getting_updates = 1;
    gdk_threads_add_idle((GSourceFunc)state_changed, self);

    gchar *full_file_name = NULL;
    gchar *temp_dir = get_windows_app_temp_location();

    // Check for updates
    gchar *windows_releases_url = app_updater_get_windows_releases_url(self);
    gchar *download_link = vdi_session_check_for_tk_updates(windows_releases_url, &last_version);
    free_memory_safely(&windows_releases_url);

    if (download_link == NULL) { // download_link == NULL
        set_status_msg(self, "?????? ?????????????????? ????????????????????.");
        goto clear_mark;
    }

    // Download the latest version
    set_status_msg(self, "???????????????????? ?????????? ???????????? ????.");

    full_file_name = g_strdup_printf("%s/installer.exe", temp_dir);

    HRESULT res = URLDownloadToFile(NULL, download_link, full_file_name, 0, NULL);
    free_memory_safely(&download_link);

    if (res != S_OK) {
        gchar *reason_phrase = NULL;
        const gchar *common_phrase = "???? ?????????????? ?????????????????? ????????????????????.";
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
    set_status_msg(self, "?????????????????? ?????????? ???????????? ????.");

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );

    // Start the child process.
    if( CreateProcess( NULL,   // No module name (use command line)
                        full_file_name,        // Command line
                        NULL,           // Process handle not inheritable
                        NULL,           // Thread handle not inheritable
                        FALSE,          // Set handle inheritance to FALSE
                        DETACHED_PROCESS,
                        NULL,           // Use parent's environment block
                        NULL,           // Use parent's starting directory
                        &si,            // Pointer to STARTUPINFO structure
                        &pi )           // Pointer to PROCESS_INFORMATION structure
            )
    {
        gchar *msg = g_strdup_printf("?????????????? ?????????????????? ???????????? %s ??????????????. ?????????????????? ????????????????????.",
                                         last_version);
        set_status_msg(self, msg);
        g_free(msg);
        exit(0);
    } else {
        g_info("installer spawning failed");
        set_status_msg(self, "???? ?????????????? ?????????????????? ?????????????? ?????????????????? ?????????? ???????????? ????.");
    }

    // clear
    clear_mark:
    free_memory_safely(&temp_dir);
    free_memory_safely(&full_file_name);
    free_memory_safely(&last_version);

    //
    self->_is_getting_updates = 0;
    gdk_threads_add_idle((GSourceFunc)state_changed, self);
}
#endif
//=============================Global=======================================

AppUpdater *app_updater_new()
{
    AppUpdater *app_updater = APP_UPDATER( g_object_new( TYPE_APP_UPDATER, NULL ) );
    return app_updater;
}

// ?????????? ???????????????????? ???????????????????????? ????????????????
gchar *app_updater_get_cur_status_msg(AppUpdater *self)
{
    g_mutex_lock(&self->priv_members_mutex);
    gchar *temp_cur_status_msg = g_strdup(self->_cur_status_msg);
    g_mutex_unlock(&self->priv_members_mutex);
    return temp_cur_status_msg;
}

int app_updater_is_getting_updates(AppUpdater *self)
{
    return self->_is_getting_updates;
}

void app_updater_set_admin_password(AppUpdater *self, const gchar *admin_password)
{
    g_mutex_lock(&self->priv_members_mutex);
    update_string_safely(&self->_admin_password, admin_password);
    g_mutex_unlock(&self->priv_members_mutex);
}

// ?????????? ???????????????????? ???????????????????????? ????????????????
gchar *app_updater_get_admin_password(AppUpdater *self)
{
    g_mutex_lock(&self->priv_members_mutex);
    gchar *temp_admin_password = g_strdup(self->_admin_password);
    g_mutex_unlock(&self->priv_members_mutex);
    return temp_admin_password;
}

void app_updater_set_windows_releases_url(AppUpdater *self, const gchar *windows_releases_url)
{
    g_mutex_lock(&self->priv_members_mutex);
    update_string_safely(&self->_windows_releases_url, windows_releases_url);
    g_mutex_unlock(&self->priv_members_mutex);
}

gchar *app_updater_get_windows_releases_url(AppUpdater *self)
{
    g_mutex_lock(&self->priv_members_mutex);
    gchar *temp = g_strdup(self->_windows_releases_url);
    g_mutex_unlock(&self->priv_members_mutex);
    return temp;
}

void app_updater_execute_task_check_updates(AppUpdater *self)
{
#ifdef __linux__
    execute_async_task(app_updater_check_linux_updates_task,
                       app_updater_on_check_updates_task_finished, self, self);
#elif _WIN32
    app_updater_read_windows_updates_url_from_file(self);
    execute_async_task(app_updater_check_windows_updates_task,
                       app_updater_on_check_updates_task_finished, self, self);
#endif
}

void app_updater_execute_task_get_updates(AppUpdater *self)
{
#ifdef __linux__
    execute_async_task(app_updater_get_linux_updates_task, NULL, self, NULL);
#elif _WIN32
    app_updater_read_windows_updates_url_from_file(self);
    execute_async_task(app_updater_get_windows_updates_task, NULL, self, NULL);
#endif
}

// ???????????????????? ?????????? ???????????????????? ???????????????????????? ????????????????. ???????????????????? ???????????????????? ????????????.
gchar *app_updater_get_last_process_output(AppUpdater *self)
{
    g_mutex_lock(&self->priv_members_mutex);
    gchar *process_output = NULL;

    if (self->_last_standard_output || self->_last_standard_error)
        process_output = g_strdup_printf("%s\n%s", self->_last_standard_error, self->_last_standard_output);

    g_mutex_unlock(&self->priv_members_mutex);

    return process_output;
}
