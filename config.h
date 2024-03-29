/* Build version details */
#define BUILDID ""

/* always defined to indicate that i18n is enabled */
#define ENABLE_NLS 1

/* Enable compile-time and run-time bounds-checking, and some warnings. */
                 #if !defined _FORTIFY_SOURCE && defined __OPTIMIZE__ && __OPTIMIZE__
                 # define _FORTIFY_SOURCE 2
                 #endif

/* GETTEXT package name */
#define GETTEXT_PACKAGE "veil-connect"
#define  LOCALE_DIR "locale" // folder that placed next to the binary

/* Define to 1 if you have the `bind_textdomain_codeset' function. */
#define HAVE_BIND_TEXTDOMAIN_CODESET 1

/* Define to 1 if you have the `dcgettext' function. */
#define HAVE_DCGETTEXT 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the `fork' function. */
#define HAVE_FORK 1

/* Define if the GNU gettext() function is already present or preinstalled. */
#define HAVE_GETTEXT 1

/* Have gtk-vnc? */
/* #undef HAVE_GTK_VNC */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if your <locale.h> file defines LC_MESSAGES. */
#define HAVE_LC_MESSAGES 1

/* Have libvirt? */
#define HAVE_LIBVIRT 0

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Have rest_proxy_auth_cancel and OVIRT_REST_CALL_ERROR_CANCELLED? */
#define HAVE_OVIRT_CANCEL 1

/* Have spice-gtk? */
#define HAVE_SPICE_GTK 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#ifdef __linux__
#define HAVE_SYS_SOCKET_H 1
#endif
/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/un.h> header file. */
//#define HAVE_SYS_UN_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#ifdef __linux__
#define HAVE_UNISTD_H 1
#endif

/* Have virDomainOpenGraphicsFD? */
#define HAVE_VIR_DOMAIN_OPEN_GRAPHICS_FD 1

/* Define to 1 if you have the <windows.h> header file. */
/* #undef HAVE_WINDOWS_H */

/*Spice controller*/
//#define HAVE_SPICE_CONTROLLER

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Name of application settings and config files directory */
#define APP_FILES_DIRECTORY_NAME "VeilConnect"

/* Name of application */
#define APPLICATION_NAME "VeiL Connect"

/* Name of package */
#define PACKAGE GETTEXT_PACKAGE

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.14.9"

/* OS ID for this build */
/* #undef REMOTE_VIEWER_OS_ID */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION PACKAGE_VERSION

/* Enable GNU extensions */
#define _GNU_SOURCE /**/

/*Endpoint lka thin client in nginx*/
#define NGINX_VDI_API_PREFIX "api"

/*Product links*/
#define VEIL_PRODUCT_SITE "https://mashtab.org/files/vdi/index.html"

#define VEIL_CONNECT_DOC_SITE "https://veil.mashtab.org/vdi-docs/connect/operator_guide/annotate/"

#define VEIL_CONNECT_WIN_RELEASE_URL "https://veil-update.mashtab.org/veil-connect/windows/latest/"
