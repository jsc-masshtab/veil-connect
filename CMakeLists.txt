cmake_minimum_required(VERSION 3.7)
project(veil_connect C)

set(CMAKE_C_STANDARD 99)

######################CUSTOM CMAKE OPTIONS#############################
option(BUILD_x32 "Use path to 32bit dependencies" OFF)
######################COMPILER OPTIONS#################################
add_compile_options(-Wno-deprecated-declarations) # -Wall -Wextra

INCLUDE_DIRECTORIES(
        .
        src
        src/virt_viewer
        src/utility
        src/vdi_client
        src/rdp_viewer
        src/usb_redirection
        src/app_updater
        src/messenger
        src/net_speedometer
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
        src/about_dialog.h
        src/about_dialog.c
        src/conn_info_dialog.h
        src/conn_info_dialog.c

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
        src/rdp_viewer/rdp_settings.h
        src/rdp_viewer/rdp_settings.c
        src/rdp_viewer/rdp_util.h
        src/rdp_viewer/rdp_util.c

#utility
        src/utility/crashhandler.h
        src/utility/crashhandler.c
        src/utility/jsonhandler.h
        src/utility/jsonhandler.c
        src/utility/settingsfile.c
        src/utility/settingsfile.h
        src/utility/async.h
        src/utility/async.c
        src/utility/atomic_string.h
        src/utility/atomic_string.c
        src/utility/veil_logger.h
        src/utility/veil_logger.c

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
        src/vdi_client/vdi_app_selector.c
        src/vdi_client/vdi_app_selector.h
        src/vdi_client/vdi_user_settings_widget.h
        src/vdi_client/vdi_user_settings_widget.c

# app_updater
        src/app_updater/app_updater.h
        src/app_updater/app_updater.c

# messenger
        src/messenger/messenger.h
        src/messenger/messenger.c

# net_speedometer
        src/net_speedometer/net_speedometer.h
        src/net_speedometer/net_speedometer.c

# qr_code_generation
        src/qr_code_generation/qr_code_gen.h
        src/qr_code_generation/qr_code_gen.c
        src/qr_code_generation/qr_code_renderer.h
        src/qr_code_generation/qr_code_renderer.c
)

# usb_redirection
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
        src/usb_redirection/usb_selector_widget.h
        src/usb_redirection/usb_selector_widget.c
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
if (APPLE)
#    # Quartz
#    SET(LIBS_INCLUDE_PATH /usr/local/include/)
#    INCLUDE_DIRECTORIES(${LIBS_INCLUDE_PATH})
#    LINK_DIRECTORIES(/usr/local/lib)

    # X11
    SET(LIBS_INCLUDE_PATH /opt/local/include/)
    INCLUDE_DIRECTORIES(${LIBS_INCLUDE_PATH})
    LINK_DIRECTORIES(/opt/local/lib)
endif (APPLE)

if(UNIX AND NOT APPLE)
    SET(LIBS_INCLUDE_PATH /usr/include/)
endif (UNIX AND NOT APPLE)

if (UNIX)
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
        SET(FREERDP_PATH C:/job/FreeRDP-2.3.2/)
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
            -lUrlmon
    )

    SET(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    SET(GUI_TYPE WIN32) # switch off console
endif (WIN32)

    # GLIB
    find_package(PkgConfig REQUIRED)
    pkg_search_module(GLIB REQUIRED glib-2.0)
    INCLUDE_DIRECTORIES(${GLIB_INCLUDE_DIRS})

    # GTK
    PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)
    INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
    LINK_DIRECTORIES(${GTK3_LIBRARY_DIRS})
    SET(LIBS ${LIBS} ${GTK3_LIBRARIES})
    ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})

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

###########################APP LINKING#################################
ADD_EXECUTABLE(veil_connect ${GUI_TYPE} ${SRC_CODE} src/main.c)
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
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/doc/ping_no_console.vbs
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif (WIN32)

# translations
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/po/ru
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/locale)
# CSS
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/doc/css_style.css
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

if (UNIX)
# start script
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/doc/start_vdi_client.sh
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
# update script
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/doc/update_script.sh
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif (UNIX)

#############################TESTS###################################
if (BUILD_TESTING)
    enable_testing()
    ADD_EXECUTABLE(test_networking ${SRC_CODE} tests/test_networking.c)
    set_target_properties(test_networking PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    target_link_libraries(
            test_networking # exec
            ${LIBS} # libs
    )
    add_test(NAME test_networking COMMAND test_networking --verbose)
endif ()
