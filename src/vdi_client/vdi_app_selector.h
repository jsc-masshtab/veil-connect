/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */
#include <gtk/gtk.h>
#include <json-glib/json-glib.h>

#include "rdp_settings.h"

#ifndef VEIL_CONNECT_VDI_APP_SELECTOR_H
#define VEIL_CONNECT_VDI_APP_SELECTOR_H

typedef enum{
    APP_SELECTOR_RESULT_NONE, // Ничего не выбрано
    APP_SELECTOR_RESULT_APP, // Выбрано приложение
    APP_SELECTOR_RESULT_DESKTOP, // Выбрано обычное подключение к рабочему столу
} AppSelectorResultType;

typedef struct{
    VeilRdpSettings rdp_settings;
    AppSelectorResultType result_type;

} AppSelectorResult;

AppSelectorResult vdi_app_selector_start(GArray *farm_array, GtkWindow *parent);

//void vdi_app_selector_free_selector_result(AppSelectorResult *selector_result);

#endif //VEIL_CONNECT_VDI_APP_SELECTOR_H
