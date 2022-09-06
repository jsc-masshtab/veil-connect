/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include <glib/gi18n.h>

#include <freerdp/locale/keyboard.h>
#include <freerdp/scancode.h>

#include "rdp_viewer_window.h"

#ifdef _WIN32
#include <gdk/gdkwin32.h>
#endif

#include "remote-viewer-util.h"
#include "config.h"

#include "rdp_display.h"
#include "vdi_session.h"
#include "about_dialog.h"

#include "settingsfile.h"
#include "usbredir_dialog.h"
#include "usbredir_controller.h"
#include "usbredir_util.h"
#include "messenger.h"
#include "vm_control_menu.h"

#define MAX_KEY_COMBO 4


struct keyComboDef {
    guint keys[MAX_KEY_COMBO];
    const char *label;
    const gchar* accel_path;
};

// static variables and constants
static const struct keyComboDef keyCombos[] = {
    { { RDP_SCANCODE_LCONTROL, RDP_SCANCODE_LMENU, RDP_SCANCODE_DELETE, GDK_KEY_VoidSymbol }, "Ctrl+Alt+_Del", NULL},
    { { RDP_SCANCODE_LCONTROL, RDP_SCANCODE_LMENU, GDK_KEY_BackSpace, GDK_KEY_VoidSymbol }, "Ctrl+Alt+_Backspace",NULL},
    { { RDP_SCANCODE_LCONTROL, RDP_SCANCODE_LMENU, RDP_SCANCODE_F1, GDK_KEY_VoidSymbol }, "Ctrl+Alt+F_1", NULL},
    { { RDP_SCANCODE_LCONTROL, RDP_SCANCODE_LMENU, RDP_SCANCODE_F2, GDK_KEY_VoidSymbol }, "Ctrl+Alt+F_2", NULL},
    { { RDP_SCANCODE_LCONTROL, RDP_SCANCODE_LMENU, RDP_SCANCODE_F3, GDK_KEY_VoidSymbol }, "Ctrl+Alt+F_3", NULL},
    { { RDP_SCANCODE_LCONTROL, RDP_SCANCODE_LMENU, RDP_SCANCODE_F4, GDK_KEY_VoidSymbol }, "Ctrl+Alt+F_4", NULL},
    { { RDP_SCANCODE_LCONTROL, RDP_SCANCODE_LMENU, RDP_SCANCODE_F5, GDK_KEY_VoidSymbol }, "Ctrl+Alt+F_5", NULL},
    { { RDP_SCANCODE_LCONTROL, RDP_SCANCODE_LMENU, RDP_SCANCODE_F6, GDK_KEY_VoidSymbol }, "Ctrl+Alt+F_6", NULL},
    { { RDP_SCANCODE_LCONTROL, RDP_SCANCODE_LMENU, RDP_SCANCODE_F7, GDK_KEY_VoidSymbol }, "Ctrl+Alt+F_7", NULL},
    { { RDP_SCANCODE_LCONTROL, RDP_SCANCODE_LMENU, RDP_SCANCODE_F8, GDK_KEY_VoidSymbol }, "Ctrl+Alt+F_8", NULL},
    { { RDP_SCANCODE_LCONTROL, RDP_SCANCODE_LMENU, RDP_SCANCODE_F9, GDK_KEY_VoidSymbol }, "Ctrl+Alt+F_9", NULL},
    { { RDP_SCANCODE_LCONTROL, RDP_SCANCODE_LMENU, RDP_SCANCODE_F10, GDK_KEY_VoidSymbol }, "Ctrl+Alt+F1_0", NULL},
    { { RDP_SCANCODE_LCONTROL, RDP_SCANCODE_LMENU, RDP_SCANCODE_F11, GDK_KEY_VoidSymbol }, "Ctrl+Alt+F11", NULL},
    { { RDP_SCANCODE_LCONTROL, RDP_SCANCODE_LMENU, RDP_SCANCODE_F12, GDK_KEY_VoidSymbol }, "Ctrl+Alt+F12", NULL},
    { { RDP_SCANCODE_LMENU, RDP_SCANCODE_F4, GDK_KEY_VoidSymbol }, "Alt+F_4", NULL},
    { { RDP_SCANCODE_LMENU, RDP_SCANCODE_TAB, GDK_KEY_VoidSymbol }, "Alt+Tab", NULL},
    { { RDP_SCANCODE_LWIN, GDK_KEY_VoidSymbol }, "WIN", NULL},
    { { RDP_SCANCODE_LWIN, RDP_SCANCODE_KEY_R, GDK_KEY_VoidSymbol }, "WIN+R", NULL},
    { { RDP_SCANCODE_LWIN, RDP_SCANCODE_KEY_P, GDK_KEY_VoidSymbol }, "WIN+P", NULL},
    { { RDP_SCANCODE_LWIN, RDP_SCANCODE_KEY_E, GDK_KEY_VoidSymbol }, "WIN+E", NULL},
}; // Добавлять новые только в конец.

#ifdef G_OS_WIN32
static HWND win32_window = NULL;
#endif

G_DEFINE_TYPE( RdpViewerWindow, rdp_viewer_window, G_TYPE_OBJECT )


static void rdp_viewer_window_finalize(GObject *object)
{
    GObjectClass *parent_class = G_OBJECT_CLASS( rdp_viewer_window_parent_class );
    ( *parent_class->finalize )( object );
}

static void rdp_viewer_window_class_init(RdpViewerWindowClass *klass )
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = rdp_viewer_window_finalize;

    // signals
    g_signal_new("stop-requested",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(RdpViewerWindowClass, stop_requested),
                 NULL, NULL,
                 NULL,
                 G_TYPE_NONE,
                 2,
                 G_TYPE_STRING,
                 G_TYPE_INT);
}

static void rdp_viewer_window_init(RdpViewerWindow *self G_GNUC_UNUSED)
{
    g_info("%s", (const char *) __func__);
}

// function implementations

// taken from spice-widget.c   https://github.com/freedesktop/spice-gtk
#ifdef G_OS_WIN32
static LRESULT CALLBACK keyboard_hook_cb(int code, WPARAM wparam, LPARAM lparam)
{
    if  (win32_window && code == HC_ACTION && wparam != WM_KEYUP) {
        KBDLLHOOKSTRUCT *hooked = (KBDLLHOOKSTRUCT*)lparam;
        DWORD dwmsg = (hooked->flags << 24) | (hooked->scanCode << 16) | 1;

        if (hooked->vkCode == VK_NUMLOCK || hooked->vkCode == VK_RSHIFT) {
            dwmsg &= ~(1 << 24);
            SendMessage(win32_window, wparam, hooked->vkCode, dwmsg);
        }
        switch (hooked->vkCode) {
            case VK_CAPITAL:
            case VK_SCROLL:
            case VK_NUMLOCK:
            case VK_LSHIFT:
            case VK_RSHIFT:
            case VK_RCONTROL:
            case VK_LMENU:
            case VK_RMENU:
                break;
            case VK_LCONTROL:
                /* When pressing AltGr, an extra VK_LCONTROL with a special
                 * scancode with bit 9 set is sent. Let's ignore the extra
                 * VK_LCONTROL, as that will make AltGr misbehave. */
                if (hooked->scanCode & 0x200)
                    return 1;
                break;
            default:
                SendMessage(win32_window, wparam, hooked->vkCode, dwmsg);
                return 1;
        }
    }
    return CallNextHookEx(NULL, code, wparam, lparam);
}
#endif

static gboolean rdp_viewer_window_grab_try(gpointer user_data)
{
    RdpViewerWindow *rdp_window = (RdpViewerWindow *)user_data;

#ifdef G_OS_WIN32
    win32_window = gdk_win32_window_get_impl_hwnd(gtk_widget_get_window(rdp_window->rdp_viewer_window));

        if (rdp_window->keyboard_hook == NULL)
            rdp_window->keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_hook_cb,
                                                GetModuleHandle(NULL), 0);
        g_warn_if_fail(rdp_window->keyboard_hook != NULL);
#endif
    GdkDisplay *display = gtk_widget_get_display(rdp_window->rdp_viewer_window);
    rdp_window->seat = gdk_display_get_default_seat(display);
    g_info("Going to grab now");
    GdkGrabStatus ggs = gdk_seat_grab(rdp_window->seat,
                                      gtk_widget_get_window(rdp_window->rdp_viewer_window),
                                      GDK_SEAT_CAPABILITY_KEYBOARD, TRUE, NULL, NULL, NULL, NULL);

    if (ggs != GDK_GRAB_SUCCESS) {
        // При неудаче GTK зачем-то скрывает окно. Поэтому показываем.
        GdkWindow *gdk_window = gtk_widget_get_window(rdp_window->rdp_viewer_window);
        gdk_window_show(gdk_window);
    }

    g_info("%s ggs: %i", (const char *)__func__, ggs);

    rdp_window->grab_try_event_source_id = 0;
    return G_SOURCE_REMOVE;
}

// Toggle keyboard grabbing mode
static void rdp_viewer_window_toggle_keyboard_grab(RdpViewerWindow *rdp_window, gboolean is_grabbed)
{
    if (is_grabbed) {
        if (rdp_window->seat)
            return;

        // Если сделать захват клавы прямо в обработчике ивента focus in, то проявляется необъяснимое поведение gtk
        // Поэтому делаем это чуть позже guint
        rdp_window->grab_try_event_source_id =
                g_timeout_add(50, (GSourceFunc)rdp_viewer_window_grab_try, rdp_window);

    } else {
        //  Отменяем запрос 'сделать захват клавы', если он внезапно присутствуюет
        if (rdp_window->grab_try_event_source_id) {
            g_source_remove(rdp_window->grab_try_event_source_id);
            rdp_window->grab_try_event_source_id = 0;
        }

        // ungrab keyboard
        if (rdp_window->seat) {
            gdk_seat_ungrab(rdp_window->seat);
            rdp_window->seat = NULL;
        }
#ifdef G_OS_WIN32
        if (rdp_window->keyboard_hook) {
            UnhookWindowsHookEx(rdp_window->keyboard_hook);
            rdp_window->keyboard_hook = NULL;
        }
#endif
    }
}

// Toggle fullscreen mode
static void rdp_viewer_window_toggle_fullscreen(RdpViewerWindow *rdp_window, gboolean is_fullscreen)
{
    if (is_fullscreen) {
        // grab keyboard
        rdp_viewer_window_toggle_keyboard_grab(rdp_window, TRUE);
        rdp_window->is_grab_keyboard_on_focus_in_mode = TRUE; // we want the keyboard to be grabbed only in focus

        // fullscreen
        gtk_window_set_resizable(GTK_WINDOW(rdp_window->rdp_viewer_window), TRUE);
        gtk_window_fullscreen(GTK_WINDOW(rdp_window->rdp_viewer_window));

        // show toolbar
        gtk_widget_hide(rdp_window->top_menu);
        gtk_widget_show(rdp_window->overlay_toolbar);
        virt_viewer_timed_revealer_force_reveal(rdp_window->revealer, TRUE);

    } else {
        // ungrab keyboard
        rdp_window->is_grab_keyboard_on_focus_in_mode = FALSE;
        rdp_viewer_window_toggle_keyboard_grab(rdp_window, FALSE);
        //  leave fulscreen
        gtk_widget_hide(rdp_window->overlay_toolbar);
        //gtk_widget_set_size_request(priv->window, -1, -1);
        gtk_window_unfullscreen(GTK_WINDOW(rdp_window->rdp_viewer_window));
        gtk_widget_show(rdp_window->top_menu);
    }
}

static guint get_nkeys(const guint *keys)
{
    guint i;

    for (i = 0; keys[i] != GDK_KEY_VoidSymbol; )
        i++;

    return i;
}

// if rdp session closed the context contains trash
static gboolean rdp_viewer_window_deleted_cb(gpointer userdata)
{
    g_info("%s", (const char *)__func__);
    RdpViewerWindow *rdp_window = (RdpViewerWindow *)userdata;
    g_signal_emit_by_name(rdp_window, "stop-requested", "quit-requested", TRUE);

    return TRUE;
}

// Oкно не перевести в фул скрин пока оно не показано. Поэтому так.
// Делаем это на первичный показ окна
static gboolean rdp_viewer_window_event_on_mapped(GtkWidget *widget G_GNUC_UNUSED, GdkEvent *event G_GNUC_UNUSED,
        gpointer user_data)
{
    g_info("%s", (const char *)__func__);

    RdpViewerWindow *rdp_window = (RdpViewerWindow *)user_data;
    if (!rdp_window->window_was_mapped) {
        gboolean full_screen = rdp_window->ex_context->p_rdp_settings->full_screen;
        rdp_viewer_window_toggle_fullscreen(rdp_window, full_screen);

        rdp_window->window_was_mapped = TRUE;
    }

    return FALSE;
}

// it seems focus-in-event and focus-out-event don’t work when keyboard is grabbed
gboolean rdp_viewer_window_on_state_event(GtkWidget *widget G_GNUC_UNUSED, GdkEventWindowState *event,
        gpointer user_data)
{
    //g_info("%s %p event->type: %i", (const char *)__func__, widget, event->type);

    RdpViewerWindow *rdp_window = (RdpViewerWindow *)user_data;

    if (event->changed_mask & GDK_WINDOW_STATE_FOCUSED) {

        if (event->new_window_state & GDK_WINDOW_STATE_FOCUSED) {
            g_info("FOCUS IN");
            if (rdp_window->is_grab_keyboard_on_focus_in_mode) {
                rdp_viewer_window_toggle_keyboard_grab(rdp_window, TRUE);
            }
        } else {
            g_info("FOCUS OUT");
            rdp_viewer_window_toggle_keyboard_grab(rdp_window, FALSE);
        }
    }
    return FALSE;
}

static void rdp_viewer_item_product_site_activated(GtkWidget *menu G_GNUC_UNUSED, gpointer userdata G_GNUC_UNUSED)
{
    gtk_show_uri_on_window(NULL, VEIL_PRODUCT_SITE, GDK_CURRENT_TIME, NULL);
}

static void rdp_viewer_item_tk_doc_activated(GtkWidget *menu G_GNUC_UNUSED, gpointer userdata G_GNUC_UNUSED)
{
    gtk_show_uri_on_window(NULL, VEIL_CONNECT_DOC_SITE, GDK_CURRENT_TIME, NULL);
}

static void rdp_viewer_item_dialog_with_admin_activated(GtkWidget *menu G_GNUC_UNUSED,
                                                        RdpViewerWindow *rdp_window)
{
    if (rdp_window->ex_context->p_conn_data->global_app_mode != GLOBAL_APP_MODE_VDI) {
        show_msg_box_dialog(GTK_WINDOW(rdp_window->rdp_viewer_window),
                            _("Messaging is supported in VDI connection mode only"));
        return;
    }

    VeilMessenger *veil_messenger = rdp_window->ex_context->p_veil_messenger;
    veil_messenger_show_on_top(veil_messenger);
}

static void rdp_viewer_window_on_vm_control_menu(GtkWidget *menu, gpointer userdata)
{
    RdpViewerWindow *rdp_window = (RdpViewerWindow *)userdata;
    gtk_widget_show_all(rdp_window->menu_vm_control);
    gtk_menu_popup_at_widget(GTK_MENU(rdp_window->menu_vm_control), menu, GDK_GRAVITY_SOUTH, GDK_GRAVITY_NORTH, NULL);
}

static void rdp_viewer_item_menu_connection_info_activated(GtkWidget *menu G_GNUC_UNUSED,
                                                           RdpViewerWindow *rdp_window)
{
    conn_info_dialog_show(rdp_window->conn_info_dialog, GTK_WINDOW(rdp_window->rdp_viewer_window));
}

static void on_vm_status_changed(gpointer data G_GNUC_UNUSED, int power_state, RdpViewerWindow *rdp_window)
{
    set_vm_power_state_on_label(
            GTK_LABEL(gtk_builder_get_object(rdp_window->builder, "vm_status_display")), power_state);
}

static void rdp_viewer_item_about_activated(GtkWidget *menu G_GNUC_UNUSED, gpointer userdata)
{
    g_info("%s", (const char *)__func__);
    GtkWindow *rdp_viewer_window = (GtkWindow *)userdata;
    show_about_dialog(rdp_viewer_window);
}

static gboolean rdp_viewer_window_is_usbredir_possible(RdpViewerWindow *rdp_window)
{
    if (rdp_window->ex_context->p_conn_data->global_app_mode != GLOBAL_APP_MODE_VDI) {
        show_msg_box_dialog(GTK_WINDOW(rdp_window->rdp_viewer_window),
                _("USBREDIR is supported only in VDI connection mode"));
        return FALSE;
    }

    // Не показывать если запрещено в админке. Проброс USB запрещен администратором
    if (!vdi_session_is_usb_redir_permitted()) {
        show_msg_box_dialog(GTK_WINDOW(rdp_window->rdp_viewer_window), _("USB redirection is not allowed"));
        return FALSE;
    }

#ifdef _WIN32
    if ( !usbredir_util_check_if_usbdk_installed(GTK_WINDOW(rdp_window->rdp_viewer_window)) )
        return FALSE;
#endif

    return TRUE;
}

static void rdp_viewer_item_menu_usb_tcp_activated(GtkWidget *menu G_GNUC_UNUSED, gpointer userdata)
{
    RdpViewerWindow *rdp_window = (RdpViewerWindow *)userdata;
#ifdef __APPLE__
    show_msg_box_dialog(GTK_WINDOW(rdp_window->rdp_viewer_window),
                        "Проброс USB не поддерживается на текущей ОС");
    return;
#endif

    // Не показывать если уже открыто
    if (usbredir_controller_is_usb_tcp_window_shown())
        return;

    if (!rdp_viewer_window_is_usbredir_possible(rdp_window))
        return;

    g_autofree gchar *title = NULL;
    title = g_strdup_printf("usbredir TCP  -  %s", APPLICATION_NAME);
    usbredir_dialog_start(GTK_WINDOW(rdp_window->rdp_viewer_window), title);
}

static void rdp_viewer_item_menu_usb_spice_activated(GtkWidget *menu G_GNUC_UNUSED, gpointer userdata)
{
    RdpViewerWindow *rdp_window = (RdpViewerWindow *)userdata;

    if (!rdp_viewer_window_is_usbredir_possible(rdp_window))
        return;

    // Проверка поддерживает ли сервер перенаправление с использованием спайс в rdp сессии
    const gchar *required_server_version = "4.1.5";
    if (virt_viewer_compare_version(get_vdi_session_static()->vdi_version.string, required_server_version) < 0) {
        g_autofree gchar *msg = NULL;
        msg = g_strdup_printf(_("The client and server are incompatible. "
                         "This client demands the server to be at least %s. "
                         "Current server version is %s."),
                                 required_server_version,
                                 get_vdi_session_static()->vdi_version.string);
        show_msg_box_dialog(GTK_WINDOW(rdp_window->rdp_viewer_window), msg);
        return;
    }

    // Create spice session
    if (usbredir_controller_get_static()->spice_usb_session == NULL)
        usbredir_controller_get_static()->spice_usb_session = usbredir_spice_new();

    // Get spice connection data and connect
    usbredir_spice_show_dialog(usbredir_controller_get_static()->spice_usb_session,
            GTK_WINDOW(rdp_window->rdp_viewer_window));
}

static void rdp_viewer_window_menu_send(GtkWidget *menu, gpointer userdata)
{
    ExtendedRdpContext* ex_context = (ExtendedRdpContext*)userdata;
    if (!ex_context || !ex_context->is_running)
        return;

    rdpContext context = ex_context->context;

    int *key_shortcut_index = (g_object_get_data(G_OBJECT(menu), "key_shortcut_index"));
    g_info("%s %i %i %i", (const char *)__func__,
           keyCombos[*key_shortcut_index].keys[0], keyCombos[*key_shortcut_index].keys[1],
            keyCombos[*key_shortcut_index].keys[2]);
    rdp_viewer_window_send_key_shortcut(&context, *key_shortcut_index);
}

static void
rdp_viewer_window_menu_close_window(GtkWidget *menu G_GNUC_UNUSED, gpointer userdata)
{
    g_info("%s", (const char *)__func__);
    RdpViewerWindow *rdp_window = (RdpViewerWindow *)userdata;
    g_signal_emit_by_name(rdp_window, "stop-requested", "quit-requested", TRUE);
}

static void
rdp_viewer_window_menu_show_usb(GtkWidget *menu, gpointer userdata)
{
    g_info("%s", (const char *)__func__);
    RdpViewerWindow *rdp_window = (RdpViewerWindow *)userdata;
    gtk_widget_show_all(rdp_window->menu_open_usb);
    gtk_menu_popup_at_widget(GTK_MENU(rdp_window->menu_open_usb), menu, GDK_GRAVITY_SOUTH, GDK_GRAVITY_NORTH, NULL);
}

static void
rdp_viewer_window_menu_disconnect(GtkWidget *menu G_GNUC_UNUSED, gpointer userdata)
{
    g_info("%s", (const char *)__func__);
    RdpViewerWindow *rdp_window = (RdpViewerWindow *)userdata;
    g_signal_emit_by_name(rdp_window, "stop-requested", "job-finished", FALSE);
}

static void
rdp_viewer_window_menu_show_shortcuts(GtkWidget *btn, gpointer userdata)
{
    g_info("%s", (const char *)__func__);
    RdpViewerWindow *rdp_window = (RdpViewerWindow *)userdata;
    gtk_widget_show_all(rdp_window->menu_send_shortcut);
    gtk_menu_popup_at_widget(GTK_MENU(rdp_window->menu_send_shortcut), btn,
                             GDK_GRAVITY_SOUTH, GDK_GRAVITY_SOUTH, NULL);
}

static void
rdp_viewer_control_menu_setup(GtkBuilder *builder, RdpViewerWindow *rdp_window)
{
    rdp_window->menu_vm_control = vm_control_menu_create();

    GtkMenuItem *menu_switch_off = GTK_MENU_ITEM(gtk_builder_get_object(builder, "menu-switch-off"));
    g_signal_connect(menu_switch_off, "activate", G_CALLBACK(rdp_viewer_window_menu_disconnect), rdp_window);
}

static void
rdp_viewer_item_fullscreen_activate_request(GtkWidget *menu G_GNUC_UNUSED, gpointer userdata)
{
    g_info("%s", (const char *)__func__);
    RdpViewerWindow *rdp_window = (RdpViewerWindow *)userdata;
    rdp_viewer_window_toggle_fullscreen(rdp_window, TRUE);
}

static void
rdp_viewer_window_toolbar_leave_fullscreen(GtkWidget *button G_GNUC_UNUSED, gpointer userdata)
{
    RdpViewerWindow *rdp_window = (RdpViewerWindow *)userdata;
    rdp_viewer_window_toggle_fullscreen(rdp_window, FALSE);
}

static void
rdp_viewer_window_toolbar_leave_fullscreen_for_all_windows(GtkWidget *button G_GNUC_UNUSED, gpointer userdata)
{
    RdpViewerWindow *rdp_window = (RdpViewerWindow *)userdata;
    GArray *rdp_windows_array = rdp_window->ex_context->rdp_windows_array;

    // leave fullscreen on all windows
    for (guint i = 0; i < rdp_windows_array->len; ++i) {
        RdpViewerWindow *rdp_window_data_ = g_array_index(rdp_windows_array, RdpViewerWindow *, i);
        rdp_viewer_window_toggle_fullscreen(rdp_window_data_, FALSE);
    }
}

static GtkWidget *
create_new_button_for_overlay_toolbar(RdpViewerWindow *rdp_window, GtkWidget *icon_widget, const gchar *icon_name,
        const gchar *text)
{
    GtkWidget *button = GTK_WIDGET(gtk_tool_button_new(icon_widget, NULL));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(button), icon_name);
    gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(button), text);
    gtk_tool_item_set_is_important(GTK_TOOL_ITEM(button), TRUE);
    gtk_widget_show(button);
    gtk_toolbar_insert(GTK_TOOLBAR(rdp_window->overlay_toolbar), GTK_TOOL_ITEM(button), -1);

    return button;
}

static void fill_shortcuts_menu(GtkMenu *sub_menu_send, ExtendedRdpContext* ex_context)
{
    int num_of_shortcuts = G_N_ELEMENTS(keyCombos);
    for (int i = 0; i < num_of_shortcuts; ++i) {
        GtkWidget *menu_item = gtk_menu_item_new_with_mnemonic(keyCombos[i].label);
        gtk_container_add(GTK_CONTAINER(sub_menu_send), menu_item);

        int *key_shortcut_index = malloc(sizeof(int));
        *key_shortcut_index = i;
        g_object_set_data_full(G_OBJECT(menu_item), "key_shortcut_index", key_shortcut_index, g_free);
        g_signal_connect(menu_item, "activate", G_CALLBACK(rdp_viewer_window_menu_send), ex_context);
    }
}

static void rdp_viewer_toolbar_setup(GtkBuilder *builder, RdpViewerWindow *rdp_window)
{
    GtkWidget *button;

    // create a toolbar which will be shown in fullscreen mode
    rdp_window->overlay_toolbar = gtk_toolbar_new();
    gtk_toolbar_set_show_arrow(GTK_TOOLBAR(rdp_window->overlay_toolbar), FALSE);
    gtk_widget_set_no_show_all(rdp_window->overlay_toolbar, TRUE);
    gtk_toolbar_set_style(GTK_TOOLBAR(rdp_window->overlay_toolbar), GTK_TOOLBAR_BOTH_HORIZ);

    // Leave fullscreen
    button = create_new_button_for_overlay_toolbar(rdp_window, NULL, "view-restore", _("Leave full screen"));
    g_signal_connect(button, "clicked", G_CALLBACK(rdp_viewer_window_toolbar_leave_fullscreen), rdp_window);

    // Leave fullscreen for all windows at once
    if (rdp_window->ex_context->context.settings->MonitorCount > 1) {
        button = create_new_button_for_overlay_toolbar(rdp_window, NULL, "view-restore",
                "Покинуть полный экран на всех окнах");
        g_signal_connect(button, "clicked",
                G_CALLBACK(rdp_viewer_window_toolbar_leave_fullscreen_for_all_windows),
                rdp_window);
    }

    // Send key combination
    button = create_new_button_for_overlay_toolbar(rdp_window, NULL, "preferences-desktop-keyboard-shortcuts",
                                                   _("Send key combination"));
    rdp_window->menu_send_shortcut = gtk_menu_new();
    fill_shortcuts_menu(GTK_MENU(rdp_window->menu_send_shortcut), rdp_window->ex_context);
    g_signal_connect(button, "clicked", G_CALLBACK(rdp_viewer_window_menu_show_shortcuts),
                     rdp_window);

    // USB
    rdp_window->menu_open_usb = gtk_menu_new();
    GtkWidget *menu_usbredir_tcp = gtk_menu_item_new_with_mnemonic("USBREDIR TCP");
    gtk_container_add(GTK_CONTAINER(rdp_window->menu_open_usb), menu_usbredir_tcp);
    g_signal_connect(menu_usbredir_tcp, "activate", G_CALLBACK(rdp_viewer_item_menu_usb_tcp_activated), rdp_window);
    GtkWidget *menu_usbredir_spice = gtk_menu_item_new_with_mnemonic("USBREDIR SPICE");
    gtk_container_add(GTK_CONTAINER(rdp_window->menu_open_usb), menu_usbredir_spice);
    g_signal_connect(menu_usbredir_spice, "activate", G_CALLBACK(rdp_viewer_item_menu_usb_spice_activated), rdp_window);

    GtkWidget *image_widget = gtk_image_new_from_resource(VIRT_VIEWER_RESOURCE_PREFIX"/icons/content/img/usb.png");
    button = create_new_button_for_overlay_toolbar(rdp_window, image_widget, "?", _("USB"));
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(button), _("USB"));
    g_signal_connect(button, "clicked", G_CALLBACK(rdp_viewer_window_menu_show_usb), rdp_window);

    // Vm control
    button = create_new_button_for_overlay_toolbar(rdp_window, NULL, "system-run", _("VM control"));
    g_signal_connect(button, "clicked", G_CALLBACK(rdp_viewer_window_on_vm_control_menu), rdp_window);

    // Disconnect
    button = create_new_button_for_overlay_toolbar(rdp_window, NULL, "window-close", _("Disconnect"));
    g_signal_connect(button, "clicked", G_CALLBACK(rdp_viewer_window_menu_disconnect), rdp_window);

    // add toolbar to overlay
    rdp_window->revealer = virt_viewer_timed_revealer_new(rdp_window->overlay_toolbar);
    GtkWidget *overlay = GTK_WIDGET(gtk_builder_get_object(builder, "viewer-overlay"));
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), GTK_WIDGET(rdp_window->revealer));
}

static void
rdp_viewer_window_usb_redir_task_finished(gpointer source G_GNUC_UNUSED, int code, const gchar *message,
                                  int usbbus, int usbaddr, RdpViewerWindow *rdp_window)
{
    if (code == USB_REDIR_FINISH_SUCCESS)
        return;

    g_autofree gchar *msg = NULL;
    // Перенаправление USB /dev/bus/usb/%03i/%03i завершилось с ошибкой. %s
    msg = g_strdup_printf(_("USB redirection /dev/bus/usb/%03i/%03i finished with errors.\n%s"),
                          usbbus, usbaddr, message);
    show_msg_box_dialog(GTK_WINDOW(rdp_window->rdp_viewer_window), msg);
}

static void rdp_viewer_window_handle_key_event(GdkEventKey *event, ExtendedRdpContext* tf, gboolean down)
{
    if (!tf || !tf->is_running)
        return;
    rdpInput *input = tf->context.input;

#ifdef G_OS_UNIX
    DWORD rdp_scancode = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(event->hardware_keycode);
#elif _WIN32
    DWORD rdp_scancode = 0;
    switch (event->keyval) {
        // keys with special treatment
        case GDK_KEY_Control_L:
            rdp_scancode = RDP_SCANCODE_LCONTROL;
            break;
        case GDK_KEY_Control_R:
            rdp_scancode = RDP_SCANCODE_RCONTROL;
            break;
        case GDK_KEY_Shift_L:
            rdp_scancode = RDP_SCANCODE_LSHIFT;
            break;
        case GDK_KEY_Shift_R:
            rdp_scancode = RDP_SCANCODE_RSHIFT;
            break;
        case GDK_KEY_Alt_L:
            rdp_scancode = RDP_SCANCODE_LMENU;
            break;
        case GDK_KEY_Alt_R:
            rdp_scancode = RDP_SCANCODE_RMENU;
            break;
        case GDK_KEY_Left:
            rdp_scancode = RDP_SCANCODE_LEFT;
            break;
        case GDK_KEY_Up:
            rdp_scancode = RDP_SCANCODE_UP;
            break;
        case GDK_KEY_Right:
            rdp_scancode = RDP_SCANCODE_RIGHT;
            break;
        case GDK_KEY_Down:
            rdp_scancode = RDP_SCANCODE_DOWN;
            break;
        case GDK_KEY_Insert:
            rdp_scancode = RDP_SCANCODE_INSERT;
            break;
        case GDK_KEY_Home:
            rdp_scancode = RDP_SCANCODE_HOME;
            break;
        case GDK_KEY_End:
            rdp_scancode = RDP_SCANCODE_END;
            break;
        case GDK_KEY_Delete:
            rdp_scancode = RDP_SCANCODE_DELETE;
            break;
        case GDK_KEY_Prior:
            rdp_scancode = RDP_SCANCODE_PRIOR;
            break;
        case GDK_KEY_Next:
            rdp_scancode = RDP_SCANCODE_NEXT;
            break;
        case GDK_KEY_KP_Divide:
            rdp_scancode = RDP_SCANCODE_DIVIDE;
            break;
        default:
            // other keys
            rdp_scancode = GetVirtualScanCodeFromVirtualKeyCode(event->hardware_keycode, 4);
            break;
    }
#endif
    // Игнорируем принтскрин по просьбе сверху
    if (rdp_scancode == RDP_SCANCODE_PRINTSCREEN || rdp_scancode == 84) // 84  - on windows
        return;

    BOOL is_success = freerdp_input_send_keyboard_event_ex(input, down, rdp_scancode);
    (void) is_success;

    //g_info("%s: hardkey: 0x%X scancode: 0x%X keyval: 0x%X down: %i\n", (const char *) __func__,
    //       event->hardware_keycode, rdp_scancode, event->keyval, down);
}

static gboolean rdp_viewer_window_key_pressed(GtkWidget *widget G_GNUC_UNUSED, GdkEventKey *event, gpointer user_data)
{
    vdi_ws_client_send_user_gui(vdi_session_get_ws_client()); // notify server

    ExtendedRdpContext* tf = (ExtendedRdpContext*)user_data;
    rdp_viewer_window_handle_key_event(event, tf, TRUE);
    return TRUE;
}

static gboolean rdp_viewer_window_key_released(GtkWidget *widget G_GNUC_UNUSED, GdkEventKey *event, gpointer user_data)
{
    ExtendedRdpContext* tf = (ExtendedRdpContext*)user_data;
    rdp_viewer_window_handle_key_event(event, tf, FALSE);

    return TRUE;
}

static void rdp_viewer_window_show(RdpViewerWindow *rdp_window, GdkRectangle geometry)
{
    g_info("%s W: %u x H:%u x:%i y:%i", (const char *)__func__,
           geometry.width, geometry.height, geometry.x, geometry.y);

    gtk_window_resize(GTK_WINDOW(rdp_window->rdp_viewer_window), geometry.width, geometry.height);
    gtk_window_move(GTK_WINDOW(rdp_window->rdp_viewer_window), geometry.x, geometry.y);
    gtk_widget_show_all(rdp_window->rdp_viewer_window);
}

RdpViewerWindow *rdp_viewer_window_create(ExtendedRdpContext *ex_context,
        int index, int monitor_num, GdkRectangle geometry)
{
    RdpViewerWindow *rdp_window = RDP_VIEWER_WINDOW( g_object_new( TYPE_RDP_VIEWER_WINDOW, NULL ) );

    rdp_window->ex_context = ex_context;
    rdp_window->monitor_index = index;
    rdp_window->monitor_number = monitor_num;

    // gui
    GtkBuilder *builder = rdp_window->builder = remote_viewer_util_load_ui("virt-viewer_veil.glade");

    GtkWidget *rdp_viewer_window = rdp_window->rdp_viewer_window =
            GTK_WIDGET(gtk_builder_get_object(builder, "viewer"));
    // ВМ: %s     Пользователь: %s  -  %s
    gchar *title = g_strdup_printf(_("VM: %s     User: %s     Monitor number: %i  -  %s"),
                                   ex_context->p_conn_data->vm_verbose_name,
                                   ex_context->p_rdp_settings->user_name,
                                   rdp_window->monitor_number, APPLICATION_NAME);
    gtk_window_set_title(GTK_WINDOW(rdp_viewer_window), title);
    free_memory_safely(&title);

    gtk_widget_add_events(rdp_viewer_window, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_FOCUS_CHANGE_MASK);
    g_signal_connect_swapped(rdp_viewer_window, "delete-event",
                             G_CALLBACK(rdp_viewer_window_deleted_cb), rdp_window);
    g_signal_connect(rdp_viewer_window, "map-event",
            G_CALLBACK(rdp_viewer_window_event_on_mapped), rdp_window);
    g_signal_connect(rdp_viewer_window, "window-state-event",
                     G_CALLBACK(rdp_viewer_window_on_state_event), rdp_window);

    rdp_window->top_menu = GTK_WIDGET(gtk_builder_get_object(builder, "top-menu"));

    // usb menu
    GtkWidget *menu_usb = GTK_WIDGET(gtk_builder_get_object(builder, "menu-file-usb"));
    // Для RDS пула используется возможности freerdp для перенаправления USB (RemoteFX)
    if (vdi_session_get_current_pool_type() == VDI_POOL_TYPE_RDS) {
        gtk_widget_set_sensitive(menu_usb, FALSE);
    } else {
        GtkWidget *submenu_usb = GTK_WIDGET(gtk_builder_get_object(builder, "usbredir_submenu"));

        GtkWidget *menu_usb_usbredir_tcp = GTK_WIDGET(gtk_builder_get_object(builder,
                "menu-file-usb-device-selection"));
        gtk_menu_item_set_label(GTK_MENU_ITEM(menu_usb_usbredir_tcp), "USBREDIR TCP");
        g_signal_connect(menu_usb_usbredir_tcp, "activate",
                G_CALLBACK(rdp_viewer_item_menu_usb_tcp_activated), rdp_window);

        // Добавить кнпку меню для spice usb
        GtkWidget *menu_usb_usbredir_spice = gtk_menu_item_new_with_label("USBREDIR SPICE");
        gtk_container_add(GTK_CONTAINER(submenu_usb), menu_usb_usbredir_spice);
        g_signal_connect(menu_usb_usbredir_spice, "activate",
                         G_CALLBACK(rdp_viewer_item_menu_usb_spice_activated), rdp_window);
    }

    // remove inapropriate items from settings menu
    gtk_widget_destroy(GTK_WIDGET(gtk_builder_get_object(builder, "menu-file-smartcard-insert")));
    gtk_widget_destroy(GTK_WIDGET(gtk_builder_get_object(builder, "menu-file-smartcard-remove")));
    gtk_widget_destroy(GTK_WIDGET(gtk_builder_get_object(builder, "menu-change-cd")));
    gtk_widget_destroy(GTK_WIDGET(gtk_builder_get_object(builder, "menu-preferences")));

    GtkMenuItem *menu_close_window = GTK_MENU_ITEM(gtk_builder_get_object(builder, "item_close_app"));
    g_signal_connect(menu_close_window, "activate", G_CALLBACK(rdp_viewer_window_menu_close_window), rdp_window);

    rdp_viewer_control_menu_setup(builder, rdp_window);
    // control menu. Управление ВМ реализовано только для VDI режима
    gboolean is_vdi_mode = ex_context->p_conn_data->global_app_mode == GLOBAL_APP_MODE_VDI;
    GtkWidget *menu_control = GTK_WIDGET(gtk_builder_get_object(builder, "menu-control"));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_control), GTK_WIDGET(rdp_window->menu_vm_control));
    gtk_widget_set_sensitive(menu_control, is_vdi_mode);

    // view menu
    gtk_widget_destroy(GTK_WIDGET(gtk_builder_get_object(builder, "menu-view-zoom")));
    gtk_widget_destroy(GTK_WIDGET(gtk_builder_get_object(builder, "menu-displays")));
    gtk_widget_destroy(GTK_WIDGET(gtk_builder_get_object(builder, "menu-view-release-cursor")));
    GtkWidget *item_fullscreen = GTK_WIDGET(gtk_builder_get_object(builder, "menu-view-fullscreen"));

    // shortcuts
    GtkWidget *menu_send_shortcut = GTK_WIDGET(gtk_builder_get_object(builder, "menu-send"));
    GtkMenu *sub_menu_send = GTK_MENU(gtk_menu_new());
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_send_shortcut), (GtkWidget*)sub_menu_send);
    fill_shortcuts_menu(sub_menu_send, ex_context);

    // help menu
    GtkWidget *item_product_site = GTK_WIDGET(gtk_builder_get_object(builder, "menu-help-guest-details"));
    GtkWidget *item_about = GTK_WIDGET(gtk_builder_get_object(builder, "imagemenuitem10"));
    GtkWidget *item_tk_doc = GTK_WIDGET(gtk_builder_get_object(builder, "menu-tk-doc"));
    GtkWidget *item_dialog_with_admin = GTK_WIDGET(gtk_builder_get_object(builder, "menu_dialog_with_admin"));
    GtkWidget *item_menu_connection_info = GTK_WIDGET(gtk_builder_get_object(builder, "menu_connection_info"));

    rdp_window->conn_info_dialog = conn_info_dialog_create();

    // control toolbar used in fullscreen
    rdp_viewer_toolbar_setup(builder, rdp_window); // for overlay control

    g_signal_connect(item_fullscreen, "activate", G_CALLBACK(rdp_viewer_item_fullscreen_activate_request),
                     rdp_window);
    g_signal_connect(item_product_site, "activate",
                     G_CALLBACK(rdp_viewer_item_product_site_activated), rdp_viewer_window);
    g_signal_connect(item_about, "activate", G_CALLBACK(rdp_viewer_item_about_activated), rdp_viewer_window);
    g_signal_connect(item_tk_doc, "activate", G_CALLBACK(rdp_viewer_item_tk_doc_activated), NULL);
    g_signal_connect(item_dialog_with_admin, "activate",
            G_CALLBACK(rdp_viewer_item_dialog_with_admin_activated), rdp_window);
    g_signal_connect(item_menu_connection_info, "activate",
            G_CALLBACK(rdp_viewer_item_menu_connection_info_activated), rdp_window);
    g_signal_connect(rdp_viewer_window, "key-press-event", G_CALLBACK(rdp_viewer_window_key_pressed), ex_context);
    g_signal_connect(rdp_viewer_window, "key-release-event",
            G_CALLBACK(rdp_viewer_window_key_released), ex_context);

    // other signals
    rdp_window->vm_changed_handle = g_signal_connect(get_vdi_session_static(), "vm-changed",
            G_CALLBACK(on_vm_status_changed), rdp_window);
    // Для первого окна добавляем сигнал для показа сообщения о проблеме USB
    if (rdp_window->monitor_index == 0) {
        rdp_window->usb_redir_finished_handle = g_signal_connect(usbredir_controller_get_static(),
                "usb-redir-finished", G_CALLBACK(rdp_viewer_window_usb_redir_task_finished), rdp_window);
    }

    // create RDP display and add to scrolled window
    rdp_window->rdp_display = rdp_display_new(ex_context, geometry);

    // Если активирована динамическая адаптация расширения, то скролы не требуются.
    GtkWidget *vbox = GTK_WIDGET(gtk_builder_get_object(builder, "viewer-box"));
    if (ex_context->p_rdp_settings->dynamic_resolution_enabled && !ex_context->p_rdp_settings->is_multimon) {
        gtk_box_pack_end(GTK_BOX(vbox), GTK_WIDGET(rdp_window->rdp_display), TRUE, TRUE, 0);
    } else {
        gtk_widget_set_size_request(GTK_WIDGET(rdp_window->rdp_display), geometry.width, geometry.height);
        GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(rdp_window->rdp_display));
        gtk_box_pack_end(GTK_BOX(vbox), GTK_WIDGET(scrolled_window), TRUE, TRUE, 0);
    }

    // position the window and show
    rdp_viewer_window_show(rdp_window, geometry);

    return rdp_window;
}

void rdp_viewer_window_destroy(RdpViewerWindow *rdp_window)
{
    // ungrab keyboard if its grabbed
    rdp_viewer_window_toggle_keyboard_grab(rdp_window, FALSE);

    if (rdp_window->vm_changed_handle)
        g_signal_handler_disconnect(get_vdi_session_static(), rdp_window->vm_changed_handle);
    if (rdp_window->usb_redir_finished_handle)
        g_signal_handler_disconnect(usbredir_controller_get_static(), rdp_window->usb_redir_finished_handle);

    conn_info_dialog_destroy(rdp_window->conn_info_dialog);

    g_object_unref(rdp_window->builder);
    gtk_widget_destroy(rdp_window->menu_send_shortcut);
    gtk_widget_destroy(rdp_window->menu_open_usb);
    gtk_widget_destroy(rdp_window->overlay_toolbar);
    gtk_widget_destroy(rdp_window->rdp_viewer_window);

    g_object_unref(rdp_window);
}

void rdp_viewer_window_send_key_shortcut(rdpContext* context, int key_shortcut_index)
{
    rdpInput *input = context->input;

    guint key_array_size = get_nkeys(keyCombos[key_shortcut_index].keys);

    for (guint i = 0; i < key_array_size; ++i)
        freerdp_input_send_keyboard_event_ex(input, TRUE, keyCombos[key_shortcut_index].keys[i]);
    for (guint i = 0; i < key_array_size; ++i)
        freerdp_input_send_keyboard_event_ex(input, FALSE, keyCombos[key_shortcut_index].keys[i]);
}