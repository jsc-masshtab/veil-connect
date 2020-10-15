cmake_minimum_required(VERSION 3.7)
project(veil_connect C)

set(CMAKE_C_STANDARD 99)

######################CUSTOM CMAKE OPTIONS#############################
option(BUILD_x32 "Use path to 32bit dependencies" OFF)

######################COMPILER OPTIONS#################################
add_compile_options(-Wno-deprecated-declarations)

INCLUDE_DIRECTORIES(
        .
        src
        src/virt_viewer
        src/utility
        src/vdi_client
        src/rdp_viewer
        src/usb_redirection
)

#############################APP FILES#################################
list(APPEND SRC_CODE
        config.h
        src/connect_settings_data.h
        src/remote-viewer.h
        src/remote-viewer.c
        src/remote-viewer-connect.c
        src/remote-viewer-connect.h
        src/remote-viewer-util.h
        src/remote-viewer-util.c
        src/remote-viewer-timed-revealer.c
        src/remote-viewer-timed-revealer.h
        src/remote_viewer_start_settings.h
        src/remote_viewer_start_settings.c
        src/main.c

# virt_viewer
        src/virt_viewer/virt-viewer-resources.h
        src/virt_viewer/virt-viewer-resources.c
        src/virt_viewer/virt-viewer-enums.h
        src/virt_viewer/virt-viewer-enums.c

        src/virt_viewer/virt-viewer-auth.h
        src/virt_viewer/virt-viewer-auth.c
        src/virt_viewer/virt-viewer-app.h
        src/virt_viewer/virt-viewer-app.c
        src/virt_viewer/virt-viewer-file.h
        src/virt_viewer/virt-viewer-file.c
        src/virt_viewer/virt-viewer-session.h
        src/virt_viewer/virt-viewer-session.c
        src/virt_viewer/virt-viewer-display.h
        src/virt_viewer/virt-viewer-display.c
        src/virt_viewer/virt-viewer-notebook.h
        src/virt_viewer/virt-viewer-notebook.c
        src/virt_viewer/virt-viewer-window.h
        src/virt_viewer/virt-viewer-window.c
        src/virt_viewer/virt-viewer-vm-connection.h
        src/virt_viewer/virt-viewer-vm-connection.c
        #HAVE_SPICE_GTK
        src/virt_viewer/virt-viewer-session-spice.h
        src/virt_viewer/virt-viewer-session-spice.c
        src/virt_viewer/virt-viewer-display-spice.h
        src/virt_viewer/virt-viewer-display-spice.c
        src/virt_viewer/virt-viewer-file-transfer-dialog.h
        src/virt_viewer/virt-viewer-file-transfer-dialog.c

#rdp_viewer
        src/rdp_viewer/rdp_viewer.h
        src/rdp_viewer/rdp_viewer.c
        src/rdp_viewer/rdp_channels.h
        src/rdp_viewer/rdp_channels.c
        src/rdp_viewer/rdp_client.h
        src/rdp_viewer/rdp_client.c
        src/rdp_viewer/rdp_cursor.h
        src/rdp_viewer/rdp_cursor.c
        src/rdp_viewer/rdp_display.h
        src/rdp_viewer/rdp_display.c
        src/rdp_viewer/rdp_viewer_window.h
        src/rdp_viewer/rdp_viewer_window.c
        src/rdp_viewer/windows_rdp_launcher.h
        src/rdp_viewer/windows_rdp_launcher.c
        src/rdp_viewer/rdp_rail.h
        src/rdp_viewer/rdp_rail.c
        src/rdp_viewer/rdp_keyboard.h
        src/rdp_viewer/rdp_keyboard.c
        src/rdp_viewer/rdp_clipboard.h
        src/rdp_viewer/rdp_clipboard.c

#utility
        src/utility/crashhandler.h
        src/utility/crashhandler.c
        src/utility/jsonhandler.h
        src/utility/jsonhandler.c
        src/utility/settingsfile.c
        src/utility/settingsfile.h
        src/utility/async.h
        src/utility/async.c

# vdi client
        src/vdi_client/vdi_manager.h
        src/vdi_client/vdi_manager.c
        src/vdi_client/vdi_session.h
        src/vdi_client/vdi_session.c
        src/vdi_client/vdi_pool_widget.h
        src/vdi_client/vdi_pool_widget.c
        src/vdi_client/vdi_ws_client.h
        src/vdi_client/vdi_ws_client.c
        src/vdi_client/vdi_redis_client.h
        src/vdi_client/vdi_redis_client.c
)

## usb_redirection
list(APPEND SRC_CODE
        src/usb_redirection/usbredirserver.h
        src/usb_redirection/usbredirserver_data.h
        src/usb_redirection/usbredirhost.h
        src/usb_redirection/usbredirhost.c
        src/usb_redirection/usbredir_controller.h
        src/usb_redirection/usbredir_controller.c
        src/usb_redirection/usbredir_dialog.h
        src/usb_redirection/usbredir_dialog.c
        src/usb_redirection/usbredir_util.h
        src/usb_redirection/usbredir_util.c
)
if (UNIX)
list(APPEND SRC_CODE
        src/usb_redirection/usbredirserver_lin.c
)
endif (UNIX)
if (WIN32)
list(APPEND SRC_CODE
        src/usb_redirection/usbredirserver_win.c
)
endif (WIN32)

###############################LIBRARIES###############################
    # GLIB
    find_package(PkgConfig REQUIRED)
    pkg_search_module(GLIB REQUIRED glib-2.0)
    INCLUDE_DIRECTORIES(${GLIB_INCLUDE_DIRS})

    # GTK
    PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)
    INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
    SET(LIBS ${LIBS} ${GTK3_LIBRARIES})
    ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})

if (UNIX)
    SET(LIBS_INCLUDE_PATH /usr/include/)

    # redis hiredis
    INCLUDE_DIRECTORIES(${LIBS_INCLUDE_PATH}hiredis)
    SET(LIBS ${LIBS} hiredis)

    # freerdp
    INCLUDE_DIRECTORIES(${LIBS_INCLUDE_PATH}freerdp2)
    SET(LIBS ${LIBS} freerdp2 freerdp-client2)

    # winpr2
    INCLUDE_DIRECTORIES(${LIBS_INCLUDE_PATH}winpr2)
    SET(LIBS ${LIBS} winpr2 winpr-tools2)

    # other libs
    SET(LIBS ${LIBS}
            m
            -rdynamic
            -pthread
            -lX11
    )

    SET(GUI_TYPE)

endif (UNIX)

if (WIN32)
    if (BUILD_x32) # 32bit
        message(STATUS "Using 32bit dependencies")
        SET(LIBS_INCLUDE_PATH C:/msys32/mingw32/include/)
        SET(FREERDP_PATH C:/job/FreeRDP-2.0.0-rc4/)
        SET(HIREDIS_PATH C:/job/hiredis-mingw/)
    else(BUILD_x32) # 64bit
        message(STATUS "Using 64bit dependencies")
        SET(LIBS_INCLUDE_PATH C:/msys32/mingw64/include/)
        SET(FREERDP_PATH C:/job/FreeRDP-2.2.0/)
        SET(HIREDIS_PATH C:/job/hiredis-mingw/)
    endif(BUILD_x32)

    # hiredis port to Windows
    INCLUDE_DIRECTORIES(${HIREDIS_PATH})
    SET(LIBS ${LIBS} ${HIREDIS_PATH}cmake-build-release/libhiredis.a)

    # freerdp
    INCLUDE_DIRECTORIES(${FREERDP_PATH}include)
    SET(LIBS ${LIBS} ${FREERDP_PATH}Release/freerdp2.dll)
    SET(LIBS ${LIBS} ${FREERDP_PATH}Release/freerdp-client2.dll)

    # winpr2
    INCLUDE_DIRECTORIES(${FREERDP_PATH}winpr/include)
    SET(LIBS ${LIBS} ${FREERDP_PATH}Release/winpr-tools2.dll)
    SET(LIBS ${LIBS} ${FREERDP_PATH}Release/winpr2.dll)

    # other libs
    SET(LIBS ${LIBS}
            m
            -pthread
            -lwsock32
            -lws2_32
            -liphlpapi
            -lssp
    )

    SET(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    SET(GUI_TYPE WIN32) # switch off console
endif (WIN32)

    # libxml
    INCLUDE_DIRECTORIES(${LIBS_INCLUDE_PATH}libxml2)
    SET(LIBS ${LIBS} xml2)

    # SPICE_GTK
    INCLUDE_DIRECTORIES(${LIBS_INCLUDE_PATH}spice-client-gtk-3.0)
    INCLUDE_DIRECTORIES(${LIBS_INCLUDE_PATH}spice-client-glib-2.0)
    INCLUDE_DIRECTORIES(${LIBS_INCLUDE_PATH}spice-1)
    SET(LIBS ${LIBS} spice-client-gtk-3.0 spice-client-glib-2.0)

    # libsoup
    INCLUDE_DIRECTORIES(${LIBS_INCLUDE_PATH}libsoup-2.4)
    SET(LIBS ${LIBS} soup-2.4)

    # json glib
    INCLUDE_DIRECTORIES(${LIBS_INCLUDE_PATH}json-glib-1.0)
    SET(LIBS ${LIBS} json-glib-1.0)

    # libusb
    INCLUDE_DIRECTORIES(${LIBS_INCLUDE_PATH}libusb-1.0)
    SET(LIBS ${LIBS} usb-1.0) # libusb-1.0.a

    # usbredirparser
    SET(LIBS ${LIBS} usbredirparser) #

#############################LINKING###################################
ADD_EXECUTABLE(veil_connect ${GUI_TYPE} ${SRC_CODE})
target_link_libraries(
    veil_connect # exec
    ${LIBS} # libs
)

#############################CONFIGS###################################
# rdp template file for Windows native client
if (WIN32)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/doc/rdp_data
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/doc/usb.ids
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/share/hwdata)
endif (WIN32)

# translations
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/po/ru
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/locale)
# CSS
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/doc/css_style.css
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
# start script
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/doc/start_vdi_client.sh
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
## ini  Приложение генерирует ini файл при первом запуске если его нет, что предпочтительнее
#file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/doc/veil_client_settings.ini
#     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
