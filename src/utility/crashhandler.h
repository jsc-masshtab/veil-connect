/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef CRASHHANDLER_H
#define CRASHHANDLER_H


void install_crash_handler(const char *log_file_name, const char *cur_log_path);
void uninstall_crash_handler(void);

#endif // CRASHHANDLER_H
