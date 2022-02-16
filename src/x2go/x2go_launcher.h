/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_X2GO_LAUNCHER_H
#define VEIL_CONNECT_X2GO_LAUNCHER_H

#include <gtk/gtk.h>

#include "settings_data.h"


#define TYPE_X2GO_LAUNCHER ( x2go_launcher_get_type( ) )
#define X2GO_LAUNCHER( obj ) ( G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_X2GO_LAUNCHER, X2goLauncher) )

typedef struct
{
    GPid pid;
    gboolean is_launched;

    gchar *user;
    gchar *password;

    const ConnectSettingsData *p_data;

} X2goQtClientData;

typedef struct
{
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

    gboolean clear_all_upon_sig_termination;

} X2goPyhocaData;


typedef struct {
    GObject parent;

    X2goQtClientData qt_client_data;
    X2goPyhocaData pyhoca_data;

    const ConnectSettingsData *p_conn_data;

    gboolean send_signal_upon_job_finish;

} X2goLauncher;

typedef struct {
    GObjectClass parent_class;

    /* signals */
    void (*job_finished)(X2goLauncher *self);

} X2goLauncherClass;

GType x2go_launcher_get_type( void ) G_GNUC_CONST;

X2goLauncher *x2go_launcher_new(void);

void x2go_launcher_start(X2goLauncher *self, const gchar *user, const gchar *password,
        const ConnectSettingsData *conn_data);

void x2go_launcher_start_qt_client(X2goLauncher *self, const gchar *user, const gchar *password);
void x2go_launcher_start_pyhoca(X2goLauncher *self, const gchar *user, const gchar *password);

void x2go_launcher_stop(X2goLauncher *self);
void x2go_launcher_stop_pyhoca(X2goLauncher *self, int sig);


#endif //VEIL_CONNECT_X2GO_LAUNCHER_H
