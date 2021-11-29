/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include "x2go_launcher.h"
#include "vdi_event.h"

extern void x2go_launcher_start_qt_client(const gchar *user, const gchar *password,
        const ConnectSettingsData*con_data);
extern void x2go_launcher_start_pyhoca(const gchar *user, const gchar *password,
        const ConnectSettingsData *conn_data);


void x2go_launcher_start(const gchar *user, const gchar * password, const ConnectSettingsData *conn_data)
{
    vdi_event_vm_changed_notify(vdi_session_get_current_vm_id());

    switch (conn_data->x2Go_settings.app_type) {
        case X2GO_APP_QT_CLIENT:
            x2go_launcher_start_qt_client(user, password, conn_data);
            break;
        case X2GO_APP_PYHOCA_CLI:
            x2go_launcher_start_pyhoca(user, password, conn_data);
            break;
    }

    vdi_event_vm_changed_notify(NULL);
}


