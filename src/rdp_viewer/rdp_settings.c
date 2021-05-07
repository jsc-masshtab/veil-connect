//
// Created by solomin on 07.05.2021.
//

#include "rdp_settings.h"
#include "remote-viewer-util.h"

void rdp_settings_clear(VeilRdpSettings *rdp_settings)
{
    free_memory_safely(&rdp_settings->remote_app_name);
    free_memory_safely(&rdp_settings->remote_app_options);
}