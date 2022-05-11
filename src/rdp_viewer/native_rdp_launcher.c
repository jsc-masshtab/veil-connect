/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <config.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#ifdef G_OS_WIN32
#include <winsock2.h>
#include <minwindef.h>
#include <windef.h>
#include <wincred.h>

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#endif

#include "native_rdp_launcher.h"
#include "rdp_viewer.h"
#include "remote-viewer-util.h"
#include "vdi_event.h"


G_DEFINE_TYPE( NativeRdpLauncher, native_rdp_launcher, G_TYPE_OBJECT )


static void native_rdp_launcher_finalize(GObject *object)
{
    GObjectClass *parent_class = G_OBJECT_CLASS( native_rdp_launcher_parent_class );
    ( *parent_class->finalize )( object );
}

static void native_rdp_launcher_class_init(NativeRdpLauncherClass *klass )
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = native_rdp_launcher_finalize;

    // signals
    g_signal_new("job-finished",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(NativeRdpLauncherClass, job_finished),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
}

static void native_rdp_launcher_init(NativeRdpLauncher *self G_GNUC_UNUSED)
{
    g_info("%s", (const char *) __func__);
}

#if defined(_WIN32)
// Сохраняем пароль для возможности входа через нативный клиент без ввода пароля
static void
native_rdp_launcher_store_conn_data(const VeilRdpSettings *p_rdp_settings)
{
    if (p_rdp_settings->user_name == NULL || p_rdp_settings->password == NULL)
        return;

    g_autofree gchar *user_name = NULL;
    user_name = g_strdup(p_rdp_settings->user_name);
    g_autofree gchar *domain = NULL;
    domain = g_strdup(p_rdp_settings->domain);

    g_autofree gchar *user_name_converted = NULL;
    g_autofree gchar *password_converted = NULL;
    g_autofree gchar *target_name_converted = NULL;

    // Если имя в формате name@domain то вычлиянем имя и домен
    extract_name_and_domain(user_name, &user_name, &domain);

    if (strlen_safely(domain))
        user_name_converted = g_strdup_printf("%s\\%s", domain, user_name);
    else
        user_name_converted = g_strdup(user_name);
    password_converted = g_strdup(p_rdp_settings->password);
    target_name_converted = g_strdup_printf("TERMSRV/%s", p_rdp_settings->ip);

    convert_string_from_utf8_to_locale(&user_name_converted);
    convert_string_from_utf8_to_locale(&password_converted);
    convert_string_from_utf8_to_locale(&target_name_converted);

    // Convert to wide char strings
    wchar_t *user_name_converted_w = util_multibyte_str_to_wchar_str(user_name_converted);
    wchar_t *password_converted_w = util_multibyte_str_to_wchar_str(password_converted);
    wchar_t *target_name_converted_w = util_multibyte_str_to_wchar_str(target_name_converted);

    // Store credentials
    DWORD cbCredentialBlobSize = (DWORD)(wcslen(password_converted_w) * sizeof(wchar_t));

    // create credential
    CREDENTIALW credential = { 0 };
    credential.Type = CRED_TYPE_DOMAIN_PASSWORD;
    credential.TargetName = target_name_converted_w;
    credential.CredentialBlobSize = cbCredentialBlobSize;
    credential.CredentialBlob = (LPBYTE)password_converted_w;
    credential.Persist = CRED_PERSIST_SESSION;
    credential.UserName = user_name_converted_w;

    // write credential to credential store
    WINBOOL ok = CredWriteW(&credential, 0);
    if (!ok) {
        DWORD error = GetLastError();
        g_warning("Cant save credentials for native windows client. The last error: %lu", error);
    }

    free(user_name_converted_w);
    free(password_converted_w);
    free(target_name_converted_w);
}
#endif

#if defined(_WIN32) || defined(__MACH__)

static void
native_rdp_launcher_finish_job(NativeRdpLauncher *self)
{
#if defined(_WIN32)
    vdi_event_vm_changed_notify(vdi_session_get_current_vm_id(), VDI_EVENT_TYPE_VM_DISCONNECTED);
    self->pid = NULL;
#else
    self->pid = 0;
#endif
    self->is_launched = FALSE;

    g_signal_emit_by_name(self, "job-finished");
}

static void
native_rdp_launcher_cb_child_watch(GPid pid, gint status, NativeRdpLauncher *self)
{
    g_info("FINISHED. %s Status: %i", __func__, status);
    g_spawn_close_pid(pid);
#if defined(__MACH__)
    if (status == 256)
        show_msg_box_dialog(self->parent_widget, _("Check if Microsoft Remote Client installed"));
#endif
    native_rdp_launcher_finish_job(self);
}

static void
append_rdp_data(FILE *destFile, const gchar *param_name, const gchar *param_value)
{
    if (param_value == NULL)
        return;

    gchar *full_address = g_strdup_printf("%s:%s\n", param_name, param_value);
    fputs(full_address, destFile);
    g_free(full_address);
}
#endif

NativeRdpLauncher *native_rdp_launcher_new()
{
    return NATIVE_RDP_LAUNCHER( g_object_new( TYPE_NATIVE_RDP_LAUNCHER, NULL ) );
}

void
native_rdp_launcher_start(NativeRdpLauncher *self, GtkWindow *parent, const VeilRdpSettings *p_rdp_settings)
{
#if  defined(_WIN32) || defined(__MACH__)
    //create rdp file based on template
    //open template for reading and take its content
    FILE *sourceFile;
    FILE *destFile;

    const char *rdp_template_filename = "rdp_data/rdp_template_file.txt";
    sourceFile = fopen(rdp_template_filename,"r");
    if (sourceFile == NULL) {
        const gchar *err_msg = "Unable to open file rdp_data/rdp_template_file.txt";
        show_msg_box_dialog(NULL, err_msg);
        g_signal_emit_by_name(self, "job-finished");
        return;
    }

    const gchar *app_data_dir = g_get_user_config_dir();
    g_autofree gchar *app_rdp_data_dir = NULL;
    app_rdp_data_dir = g_build_filename(app_data_dir, APP_FILES_DIRECTORY_NAME, "rdp_data", NULL);
    g_mkdir_with_parents(app_rdp_data_dir, 0755);

    g_autofree gchar *rdp_data_file_name = NULL;
    rdp_data_file_name = g_build_filename(app_rdp_data_dir, "rdp_file.rdp", NULL);

    g_autofree gchar *rdp_data_file_name_utf8 = NULL;
    rdp_data_file_name_utf8 = g_strdup(rdp_data_file_name);
    convert_string_from_utf8_to_locale(&rdp_data_file_name);
    destFile = fopen(rdp_data_file_name, "w");

    /* fopen() return NULL if unable to open file in given mode. */
    if (destFile == NULL) {
        g_autofree gchar *err_msg = NULL;
        err_msg = g_strdup_printf("Unable to open file %s. \n"
                                         "Please check if file exists and you have read/write privilege.",
                                         rdp_data_file_name);
        show_msg_box_dialog(NULL, err_msg);
        g_signal_emit_by_name(self, "job-finished");
        // Unable to open
        fclose(sourceFile);
        return;
    }
    // Copy file    char ch;
    copy_file_content(sourceFile, destFile);

    // append unique data
    g_autofree gchar *full_address = NULL;
    full_address = g_strdup_printf("%s:%i", p_rdp_settings->ip, p_rdp_settings->port);
    append_rdp_data(destFile, "full address:s", full_address);
    append_rdp_data(destFile, "username:s", p_rdp_settings->user_name);
    append_rdp_data(destFile, "domain:s", p_rdp_settings->domain);

    if (p_rdp_settings->is_remote_app) {
        append_rdp_data(destFile, "remoteapplicationmode:i", "1");
        append_rdp_data(destFile,"remoteapplicationprogram:s", p_rdp_settings->remote_app_program);
        append_rdp_data(destFile,"remoteapplicationcmdline:s", p_rdp_settings->remote_app_options);
    } else {
        append_rdp_data(destFile, "remoteapplicationmode:i", "0");
    }

    append_rdp_data(destFile, "use multimon:i", p_rdp_settings->is_multimon ? "1" : "0");
    append_rdp_data(destFile, "screen mode id:i", p_rdp_settings->full_screen ? "2" : "1");
    if (strlen_safely(p_rdp_settings->selectedmonitors))
        append_rdp_data(destFile, "selectedmonitors:s", p_rdp_settings->selectedmonitors);

    append_rdp_data(destFile, "redirectsmartcards:i", p_rdp_settings->redirectsmartcards ? "1" : "0");
    append_rdp_data(destFile, "redirectprinters:i", p_rdp_settings->redirectprinters ? "1" : "0");
    append_rdp_data(destFile, "allow font smoothing:i",
                    p_rdp_settings->disable_rdp_fonts ? "0" : "1");
    append_rdp_data(destFile, "disable themes:i", p_rdp_settings->disable_rdp_themes ? "1" : "0");

    /* Close files to release resources */
    fclose(sourceFile);
    fclose(destFile);

#if  defined(_WIN32)
    // Store credentials data
    native_rdp_launcher_store_conn_data(p_rdp_settings);
#endif

    self->parent_widget = parent;

    // launch process
    gchar *argv[3] = {};
    int index = 0;

#if  defined(_WIN32)
    argv[index] = g_strdup("mstsc");
    argv[++index] = g_strdup(rdp_data_file_name_utf8);
#elif defined(__MACH__)
    argv[index] = g_strdup("open");
    argv[++index] = g_strdup(rdp_data_file_name_utf8);
    //argv[++index] = g_strdup("--wait-apps");
#endif

    GError *error = NULL;
    self->is_launched = g_spawn_async(NULL, argv, NULL,
                                     G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL,
                                     NULL, &self->pid, &error);

    for (guint i = 0; i < (sizeof(argv) / sizeof(gchar *)); i++) {
        g_free(argv[i]);
    }

    if (!self->is_launched) {
        g_autofree gchar *err_msg = NULL;
        err_msg = g_strdup_printf("NATIVE CLIENT SPAWN FAILED");
        if (error) {
            g_warning("%s", error->message);
            g_clear_error(&error);
        }

        show_msg_box_dialog(NULL, err_msg);
        g_signal_emit_by_name(self, "job-finished");
        return;
    }

    // stop process callback
    g_child_watch_add(self->pid, (GChildWatchFunc)native_rdp_launcher_cb_child_watch, self);

#if defined(_WIN32)
    vdi_event_vm_changed_notify(vdi_session_get_current_vm_id(), VDI_EVENT_TYPE_VM_CONNECTED);
#endif

#else
    (void)self;
    (void)parent;
    (void)p_rdp_settings;
#endif
}

void native_rdp_launcher_stop(NativeRdpLauncher *self)
{
    if (self->pid) {
        terminate_process(self->pid);
    }
}
