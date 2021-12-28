/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include "about_dialog.h"
#include "remote-viewer-util.h"
#include "config.h"

static void
msg_box_parent_hidden(GtkWidget *widget G_GNUC_UNUSED, gpointer user_data)
{
    GtkWidget *dialog_msg = (GtkWidget *)user_data;
    gtk_widget_hide(dialog_msg);
}

void show_about_dialog(GtkWindow *parent_window)
{
    GtkBuilder *about;
    GtkWidget *dialog;
    GdkPixbuf *icon;

    about = remote_viewer_util_load_ui("virt-viewer-about.glade");

    dialog = GTK_WIDGET(gtk_builder_get_object(about, "about"));
    gtk_about_dialog_set_version (GTK_ABOUT_DIALOG(dialog), VERSION);
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), APPLICATION_NAME);

    icon = gdk_pixbuf_new_from_resource(VIRT_VIEWER_RESOURCE_PREFIX"/icons/content/img/veil-32x32.png", NULL);
    if (icon != NULL) {
        gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(dialog), icon);
        g_object_unref(icon);
    } else {
        gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(dialog), "virt-viewer_veil");
    }

    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent_window);

    gulong sig_handler = g_signal_connect(parent_window, "hide",
            G_CALLBACK(msg_box_parent_hidden), dialog);

    gtk_widget_show(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));

    g_signal_handler_disconnect(G_OBJECT(parent_window), sig_handler);
    gtk_widget_destroy(dialog);

    g_object_unref(G_OBJECT(about));
}