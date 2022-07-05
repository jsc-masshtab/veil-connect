/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <glib/gi18n.h>

#include "vdi_session.h"

#include "vm_control_menu.h"

static void vm_control_menu_create_menu_item(GtkWidget *main_menu, const gchar *text,
        void (*slot)(GtkWidget *, gpointer))
{
    GtkWidget *menu_item = gtk_menu_item_new_with_mnemonic(text);
    gtk_container_add(GTK_CONTAINER(main_menu), menu_item);
    g_signal_connect(menu_item, "activate", G_CALLBACK(slot), NULL);
}

static void
on_menu_start_vm(GtkWidget *menu G_GNUC_UNUSED, gpointer userdata G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
    vdi_api_session_execute_task_do_action_on_vm("start", FALSE);
}

static void
on_menu_suspend_vm(GtkWidget *menu G_GNUC_UNUSED, gpointer userdata G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
    vdi_api_session_execute_task_do_action_on_vm("suspend", FALSE);
}

static void
on_menu_shutdown_vm(GtkWidget *menu G_GNUC_UNUSED, gpointer userdata G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
    vdi_api_session_execute_task_do_action_on_vm("shutdown", FALSE);
}

static void
on_menu_shutdown_vm_force(GtkWidget *menu G_GNUC_UNUSED, gpointer userdata G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
    vdi_api_session_execute_task_do_action_on_vm("shutdown", TRUE);
}

static void
on_menu_reboot_vm(GtkWidget *menu G_GNUC_UNUSED, gpointer userdata G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
    vdi_api_session_execute_task_do_action_on_vm("reboot", FALSE);
}

static void
on_menu_reboot_vm_force(GtkWidget *menu G_GNUC_UNUSED, gpointer userdata G_GNUC_UNUSED)
{
    g_info("%s", (const char *)__func__);
    vdi_api_session_execute_task_do_action_on_vm("reboot", TRUE);
}

GtkWidget *vm_control_menu_create()
{
    GtkWidget *main_menu = gtk_menu_new();

    vm_control_menu_create_menu_item(main_menu, _("Start"), on_menu_start_vm);
    vm_control_menu_create_menu_item(main_menu, _("Suspend"), on_menu_suspend_vm);
    vm_control_menu_create_menu_item(main_menu, _("Power off"), on_menu_shutdown_vm);
    vm_control_menu_create_menu_item(main_menu, _("Power off forced"), on_menu_shutdown_vm_force);
    vm_control_menu_create_menu_item(main_menu, _("Reboot"), on_menu_reboot_vm);
    vm_control_menu_create_menu_item(main_menu, _("Reboot forced"), on_menu_reboot_vm_force);

    return main_menu;
}
