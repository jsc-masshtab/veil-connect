//
// Created by ubuntu on 19.04.2021.
//

#ifndef VEIL_CONNECT_LOGGER_H
#define VEIL_CONNECT_LOGGER_H

#include <gtk/gtk.h>

typedef enum {
    CLIPBOARD_LOGGER_FROM_CLIENT_TO_VM,
    CLIPBOARD_LOGGER_FROM_VM_TO_CLIENT
} ClipboardLoggerDataType;

void veil_logger_setup(void);
void veil_logger_free(void);
const gchar *veil_logger_get_log_start_date(void);

void logger_save_clipboard_data(const gchar *data, guint size, ClipboardLoggerDataType data_type);

#endif //VEIL_CONNECT_LOGGER_H
