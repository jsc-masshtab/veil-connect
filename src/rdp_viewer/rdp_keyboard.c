/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include "rdp_keyboard.h"
#include "rdp_viewer_window.h"

#ifdef __linux__

#include <X11/Xlib.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#elif _WIN32
#endif
/*
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')
*/

static UINT32 rdp_keyboard_get_toggle_keys_state()
{
    GdkKeymap *gdk_def_display = gdk_keymap_get_for_display(gdk_display_get_default());
    gboolean num_lock_state = gdk_keymap_get_num_lock_state(gdk_def_display);
    g_info("GDK num_lock_state %i", num_lock_state);

    UINT32 toggleKeysState = 0;

    if (gdk_keymap_get_scroll_lock_state(gdk_def_display))
        toggleKeysState |= KBD_SYNC_SCROLL_LOCK;


    if (gdk_keymap_get_caps_lock_state(gdk_def_display))
        toggleKeysState |= KBD_SYNC_CAPS_LOCK;

    if (gdk_keymap_get_num_lock_state(gdk_def_display))
        toggleKeysState |= KBD_SYNC_NUM_LOCK;

    return toggleKeysState;
}

void rdp_keyboard_focus_in(ExtendedRdpContext* ex_context)
{
    rdpInput* input = ex_context->context.input;
    UINT32 syncFlags = rdp_keyboard_get_toggle_keys_state();
    input->FocusInEvent(input, syncFlags);
}