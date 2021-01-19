//
// Created by ubuntu on 10.09.2020.
//

#include <glib-object.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef __linux__
#include <unistd.h>
#include <sys/sysmacros.h>
#ifndef major /* major and minor macros were moved to sys/sysmacros.h from sys/types.h */
#include <sys/types.h>
#endif
#include <sys/stat.h>
#endif

#include "usbredir_util.h"
#include "remote-viewer-util.h"

#define VENDOR_NAME_LEN (122 - sizeof(void *))
#define PRODUCT_NAME_LEN 126

typedef struct _usb_product_info {
    guint16 product_id;
    char name[PRODUCT_NAME_LEN];
} usb_product_info;

typedef struct _usb_vendor_info {
    usb_product_info *product_info;
    int product_count;
    guint16 vendor_id;
    char name[VENDOR_NAME_LEN];
} usb_vendor_info;


static GMutex usbids_load_mutex;
static int usbids_vendor_count = 0; /* < 0: failed, 0: empty, > 0: loaded */
static usb_vendor_info *usbids_vendor_info = NULL;


static gboolean usbredir_util_parse_usbids(gchar *path)
{
    gchar *contents, *line, **lines;
    usb_product_info *product_info;
    int i, j, id, product_count = 0;

    usbids_vendor_count = 0;
    if (!g_file_get_contents(path, &contents, NULL, NULL)) {
        usbids_vendor_count = -1;
        return FALSE;
    }

    lines = g_strsplit(contents, "\n", -1);

    for (i = 0; lines[i]; i++) {
        if (!isxdigit(lines[i][0]) || !isxdigit(lines[i][1]))
            continue;

        for (j = 1; lines[i + j] &&
                    (lines[i + j][0] == '\t' ||
                     lines[i + j][0] == '#'  ||
                     lines[i + j][0] == '\0'); j++) {
            if (lines[i + j][0] == '\t' && isxdigit(lines[i + j][1]))
                product_count++;
        }
        i += j - 1;

        usbids_vendor_count++;
    }

    usbids_vendor_info = g_new(usb_vendor_info, usbids_vendor_count);
    product_info = g_new(usb_product_info, product_count);

    usbids_vendor_count = 0;
    for (i = 0; lines[i]; i++) {
        line = lines[i];

        if (!isxdigit(line[0]) || !isxdigit(line[1]))
            continue;

        id = strtoul(line, &line, 16);
        while (isspace(line[0]))
            line++;

        usbids_vendor_info[usbids_vendor_count].vendor_id = id;
        snprintf(usbids_vendor_info[usbids_vendor_count].name,
                 VENDOR_NAME_LEN, "%s", line);

        product_count = 0;
        for (j = 1; lines[i + j] &&
                    (lines[i + j][0] == '\t' ||
                     lines[i + j][0] == '#'  ||
                     lines[i + j][0] == '\0'); j++) {
            line = lines[i + j];

            if (line[0] != '\t' || !isxdigit(line[1]))
                continue;

            id = strtoul(line + 1, &line, 16);
            while (isspace(line[0]))
                line++;
            product_info[product_count].product_id = id;
            snprintf(product_info[product_count].name,
                     PRODUCT_NAME_LEN, "%s", line);

            product_count++;
        }
        i += j - 1;

        usbids_vendor_info[usbids_vendor_count].product_count = product_count;
        usbids_vendor_info[usbids_vendor_count].product_info  = product_info;
        product_info += product_count;
        usbids_vendor_count++;
    }

    g_strfreev(lines);
    g_free(contents);

#if 0 /* Testing only */
    for (i = 0; i < usbids_vendor_count; i++) {
        printf("%04x  %s\n", usbids_vendor_info[i].vendor_id,
               usbids_vendor_info[i].name);
        product_info = usbids_vendor_info[i].product_info;
        for (j = 0; j < usbids_vendor_info[i].product_count; j++) {
            printf("\t%04x  %s\n", product_info[j].product_id,
                   product_info[j].name);
        }
    }
#endif

    return TRUE;
}


#ifdef __linux__
/* <Sigh> libusb does not allow getting the manufacturer and product strings
   without opening the device, so grab them directly from sysfs */
static gchar *usbredir_util_get_sysfs_attribute(int bus, int address,
                                                const char *attribute)
{
    struct stat stat_buf;
    char filename[256];
    gchar *contents;

    snprintf(filename, sizeof(filename), "/dev/bus/usb/%03d/%03d",
             bus, address);
    if (stat(filename, &stat_buf) != 0)
        return NULL;

    snprintf(filename, sizeof(filename), "/sys/dev/char/%u:%u/%s",
            major(stat_buf.st_rdev), minor(stat_buf.st_rdev), attribute);
    if (!g_file_get_contents(filename, &contents, NULL, NULL))
        return NULL;

    /* Remove the newline at the end */
    contents[strlen(contents) - 1] = '\0';

    return contents;
}
#endif

static gboolean usbredir_util_load_usbids(void)
{
    gboolean success = FALSE;

    g_mutex_lock(&usbids_load_mutex);
    if (usbids_vendor_count) {
        success = usbids_vendor_count > 0;
        goto leave;
    }

#ifdef WITH_USBIDS
    success = usbredir_util_parse_usbids(USB_IDS);
#else
    {
        const gchar * const *dirs = g_get_system_data_dirs();
        gchar *path = NULL;
        int i;

        for (i = 0; dirs[i]; ++i) {
            path = g_build_filename(dirs[i], "hwdata", "usb.ids", NULL);
            success = usbredir_util_parse_usbids(path);
            //if (!success)
            //    g_info("loading %s success: %i", path, success);
            g_free(path);

            if (success)
                goto leave;
        }
    }
#endif

    leave:
    g_mutex_unlock(&usbids_load_mutex);
    return success;
}

void usbredir_util_get_device_strings(int bus, int address,
                                       int vendor_id, int product_id,
                                       gchar **manufacturer, gchar **product) {
    usb_product_info *product_info;
    int i, j;

    g_return_if_fail(manufacturer != NULL);
    g_return_if_fail(product != NULL);

    *manufacturer = NULL;
    *product = NULL;

#if __linux__
    *manufacturer = usbredir_util_get_sysfs_attribute(bus, address, "manufacturer");
    *product = usbredir_util_get_sysfs_attribute(bus, address, "product");
#endif

    if ((!*manufacturer || !*product) &&
        usbredir_util_load_usbids()) {

        for (i = 0; i < usbids_vendor_count; i++) {
            if ((int)usbids_vendor_info[i].vendor_id != vendor_id)
                continue;

            if (!*manufacturer && usbids_vendor_info[i].name[0])
                *manufacturer = g_strdup(usbids_vendor_info[i].name);

            product_info = usbids_vendor_info[i].product_info;
            for (j = 0; j < usbids_vendor_info[i].product_count; j++) {
                if ((int)product_info[j].product_id != product_id)
                    continue;

                if (!*product && product_info[j].name[0])
                    *product = g_strdup(product_info[j].name);

                break;
            }
            break;
        }
    }

    if (!*manufacturer)
        *manufacturer = g_strdup("USB");
    if (!*product)
        *product = g_strdup("Device");

    /* Some devices have unwanted whitespace in their strings */
    g_strstrip(*manufacturer);
    g_strstrip(*product);

    /* Some devices repeat the manufacturer at the beginning of product */
    if (g_str_has_prefix(*product, *manufacturer) &&
        strlen(*product) > strlen(*manufacturer)) {
        gchar *tmp = g_strdup(*product + strlen(*manufacturer));
        g_free(*product);
        *product = tmp;
        g_strstrip(*product);
    }
}

gboolean usbredir_util_check_if_usbdk_installed(GtkWindow *parent)
{
    // На Windows для проброса USB необходим дополнительный софт USBdk.
    // Проброс USB без этого софта может привести к крашу

    const gchar *program_files_path = g_getenv("PROGRAMFILES");
    gchar *usbdk_file = g_strdup_printf("%s/%s", program_files_path,
                                        "UsbDk Runtime Library/UsbDkController.exe");

    gboolean is_usbdk_installed = (g_file_test(usbdk_file, G_FILE_TEST_EXISTS));
    g_free(usbdk_file);

    if (!is_usbdk_installed)
        show_msg_box_dialog(parent, "Для корректной работы проброса USB-устройств требуется "
                                    "USB Development Kit (UsbDK). "
                                    "Убедитесь, что он установлен");

    return is_usbdk_installed;
}

int usbredir_util_init_libusb_and_set_options(libusb_context **ctx, int verbose)
{
    if (libusb_init(ctx)) {
        fprintf(stderr, "Could not init libusb\n");
        return -1;
    }

    // set libusb options
#ifdef _WIN32
    libusb_set_option(*ctx, LIBUSB_OPTION_USE_USBDK);
#endif
//#if LIBUSB_API_VERSION >= 0x01000106
//    libusb_set_option(*ctx, LIBUSB_OPTION_LOG_LEVEL, verbose);
//#else
//    libusb_set_debug(*ctx, verbose);
//#endif
    libusb_set_debug(*ctx, verbose);
    return 0;
}