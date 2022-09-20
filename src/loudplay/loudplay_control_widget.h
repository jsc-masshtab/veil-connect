/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef VEIL_CONNECT_LOUDPLAY_CONTROL_WIDGET_H
#define VEIL_CONNECT_LOUDPLAY_CONTROL_WIDGET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "settings_data.h"
#include "gobject_gui.h"


#define TYPE_LOUDPLAY_CONTROL_WIDGET ( loudplay_control_widget_get_type( ) )
#define LOUDPLAY_CONTROL_WIDGET( obj ) \
( G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_LOUDPLAY_CONTROL_WIDGET, LoudplayControlWidget) )

typedef struct{
    GObject parent;

    GtkBuilder *builder;

    GtkWidget *main_window;
    GtkWidget *status_label;

    //GtkWidget *btn_apply_settings;
    GtkWidget *btn_stop;

    //GtkWidget *loudplay_viewport;

    ConnectSettingsData *p_conn_data;

    //GObjectGui *loudplay_config_gui;

    // invoking window
    GtkWidget *settings_invoke_window;
    GtkWidget *btn_invoke_settings;

    //VM control
    GtkWidget *btn_show_vm_control;
    GtkWidget *menu_vm_control;

#ifdef G_OS_WIN32
    HHOOK hHook;
#endif
    
} LoudplayControlWidget;

typedef struct
{
    GObjectClass parent_class;

    /* signals */
    void (*settings_change_requested)(LoudplayControlWidget *self);
    void (*process_stop_requested)(LoudplayControlWidget *self);

} LoudplayControlWidgetClass;

GType loudplay_control_widget_get_type( void ) G_GNUC_CONST;

LoudplayControlWidget *loudplay_control_widget_new(ConnectSettingsData *conn_data);

void loudplay_control_widget_show_on_top(LoudplayControlWidget *self);
void loudplay_control_widget_hide(LoudplayControlWidget *self);

//void loudplay_control_widget_update_gui(LoudplayControlWidget *self);

void loudplay_control_widget_set_status(LoudplayControlWidget *self,
        const gchar *text);
#endif //VEIL_CONNECT_LOUDPLAY_CONTROL_WIDGET_H
