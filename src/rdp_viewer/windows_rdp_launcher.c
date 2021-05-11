/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <config.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#ifdef G_OS_WIN32
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#endif

#include "rdp_viewer.h"
#include "remote-viewer-util.h"

#ifdef _WIN32
static void append_rdp_data(FILE *destFile, const gchar *param_name, const gchar *param_value)
{
    gchar *full_address = g_strdup_printf("%s:%s\n", param_name, param_value);
    fputs(full_address, destFile);
    g_free(full_address);
}
#endif

void
launch_windows_rdp_client(const gchar *user_name, const gchar *password G_GNUC_UNUSED,
                          const gchar *ip, int port G_GNUC_UNUSED, const gchar *domain,
                          const VeilRdpSettings *p_rdp_settings G_GNUC_UNUSED)
{
#ifdef __linux__
    (void)user_name;
    (void)ip;
    (void)domain;
#elif defined _WIN32
    //create rdp file based on template
    //open template for reading and take its content
    FILE *sourceFile;
    FILE *destFile;

    const char *rdp_template_filename = "rdp_data/rdp_template_file.txt";
    sourceFile = fopen(rdp_template_filename,"r");
    if (sourceFile == NULL) {
        g_info("Unable to open file rdp_data/rdp_template_file.txt");
        return;
    }

    gchar *app_data_dir = get_windows_app_data_location();
    gchar *app_rdp_data_dir = g_strdup_printf("%s/rdp_data", app_data_dir);
    g_mkdir_with_parents(app_rdp_data_dir, 0755);

    char *rdp_data_file_name = g_strdup_printf("%s/rdp_file.rdp", app_rdp_data_dir);
    g_free(app_rdp_data_dir);
    g_free(app_data_dir);

    convert_string_from_utf8_to_locale(&rdp_data_file_name);
    destFile = fopen(rdp_data_file_name, "w");

    /* fopen() return NULL if unable to open file in given mode. */
    if (destFile == NULL)
    {
        // Unable to open
        g_info("\nUnable to open file rdp_file.rd.");
        g_info("Please check if file exists and you have read/write privilege.");
        fclose(sourceFile);
        return;
    }
    // Copy file
    char ch;
    /* Copy file contents character by character. */
    while ((ch = fgetc(sourceFile)) != EOF)
    {
        fputc(ch, destFile);
        //g_info("symbol: %i.", ch);
    }

    // apend unique data
    append_rdp_data(destFile, "full address:s", ip);
    append_rdp_data(destFile, "username:s", user_name);
    append_rdp_data(destFile, "domain:s", domain);

    if (p_rdp_settings->is_remote_app) {
        append_rdp_data(destFile, "remoteapplicationmode:i", "1");
        append_rdp_data(destFile,"remoteapplicationprogram:s", p_rdp_settings->remote_app_name);
        append_rdp_data(destFile,"remoteapplicationcmdline:s", p_rdp_settings->remote_app_options);
    } else {
        append_rdp_data(destFile, "remoteapplicationmode:i", "0");
    }

    /* Close files to release resources */
    fclose(sourceFile);
    fclose(destFile);

    // launch process
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    ZeroMemory( &pi, sizeof(pi) );

    // Start the child process.
    gchar *cmd_line = g_strdup_printf("mstsc %s", rdp_data_file_name);
//    gchar *cmd_line = g_strdup(
//            "mstsc C:\\job\\vdiserver\\desktop-client-c\\cmake-build-release\\rdp_datardp_file.rdp");
    if( !CreateProcess( NULL,   // No module name (use command line)
                        cmd_line,        // Command line
                        NULL,           // Process handle not inheritable
                        NULL,           // Thread handle not inheritable
                        FALSE,          // Set handle inheritance to FALSE
                        0,              // No creation flags
                        NULL,           // Use parent's environment block
                        NULL,           // Use parent's starting directory
                        &si,            // Pointer to STARTUPINFO structure
                        &pi )           // Pointer to PROCESS_INFORMATION structure
            )
    {
        g_info( "CreateProcess failed (%lu).", GetLastError() );
        g_free(cmd_line);
        return;
    }
    g_free(cmd_line);

    // Wait until child process exits.
    WaitForSingleObject( pi.hProcess, INFINITE );

    // Close process and thread handles.
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
#endif
}
