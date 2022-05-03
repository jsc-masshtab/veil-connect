/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */
#include <gtk/gtk.h>
#include <json-glib/json-glib.h>

#include "settings_data.h"

#ifndef VEIL_CONNECT_VDI_APP_SELECTOR_H
#define VEIL_CONNECT_VDI_APP_SELECTOR_H

typedef enum{
    APP_SELECTOR_RESULT_NONE, // Ничего не выбрано
    APP_SELECTOR_RESULT_APP, // Выбрано приложение
    APP_SELECTOR_RESULT_DESKTOP, // Выбрано обычное подключение к рабочему столу
} AppSelectorResultType;

typedef struct{
    gboolean is_remote_app;
    gchar *remote_app_program;
    gchar *remote_app_options;
    AppSelectorResultType result_type;

} AppSelectorResult;

AppSelectorResult vdi_app_selector_start(VeilVmData *p_vdi_vm_data, GtkWindow *parent,
                                         RemoteApplicationFormat remote_application_format);

//void vdi_app_selector_free_selector_result(AppSelectorResult *selector_result);

#endif //VEIL_CONNECT_VDI_APP_SELECTOR_H
