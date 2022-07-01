/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#include "gobject_gui.h"
#include "remote-viewer-util.h"
#include "custom_property_data.h"

GObjectGui *gobject_gui_generate_gui(GObject *gobject)
{
    if (!gobject)
        return NULL;

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);

    guint n_pspecs;
    GParamSpec **pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (gobject), &n_pspecs);

    for (guint i = 0; i < n_pspecs; i++) {
        GParamSpec *pspec = pspecs[i];

        /* show only what we can */
        if (!(pspec->flags & G_PARAM_SHOW_ON_GUI))
            continue;

        GValue value = { 0, };
        g_value_init(&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
        g_object_get_property(gobject, pspec->name, &value);

        // control widget
        GtkWidget *control_widget = NULL;
        switch (pspec->value_type) {

            case G_TYPE_INT: {
                gint64 value_ = g_value_get_int(&value);
                GParamSpecInt *pspec_int = (GParamSpecInt *)pspec;

                GtkWidget *spin_btn = gtk_spin_button_new_with_range(
                        (gdouble)pspec_int->minimum, (gdouble)pspec_int->maximum, 1.0);
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_btn), (gdouble)value_);
                control_widget = spin_btn;
                break;
            }
            case G_TYPE_BOOLEAN: {
                gboolean value_ = g_value_get_boolean(&value);

                GtkWidget *check_btn = gtk_check_button_new();
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_btn), value_);
                control_widget = check_btn;
                break;
            }
            //case G_TYPE_DOUBLE: {
            //    gdouble value_ = g_value_get_double(&value);
            //    break;
            //}
            case G_TYPE_STRING: {
                const gchar *value_ = g_value_get_string(&value);

                GtkWidget *entry = gtk_entry_new();
                gtk_entry_set_text(GTK_ENTRY(entry), value_);
                control_widget = entry;
                break;
            }
            default:
                break;
        }

        if (control_widget) {
            gtk_widget_set_name(control_widget, pspec->name);
            // label widget
            GtkWidget *name_widget = gtk_label_new(pspec->name);
            gtk_widget_set_halign(name_widget, GTK_ALIGN_END);
            // horizontal box
            GtkWidget *horizontal_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
            gtk_box_set_homogeneous(GTK_BOX(horizontal_box), TRUE);
            gtk_box_pack_start(GTK_BOX(horizontal_box), GTK_WIDGET(name_widget), TRUE, TRUE, 0);
            gtk_box_pack_start(GTK_BOX(horizontal_box), GTK_WIDGET(control_widget), TRUE, TRUE, 0);
            // Add to yhe main container
            gtk_box_pack_start(GTK_BOX(main_box), GTK_WIDGET(horizontal_box), TRUE, TRUE, 0);
        }

        g_value_unset(&value);
    }

    g_free(pspecs);

    return main_box;
}

void gobject_gui_fill_gobject_from_gui(GObject *gobject, GObjectGui *gui)
{
    if (!gobject || !gui)
        return;

    guint n_pspecs;
    GParamSpec **pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (gobject), &n_pspecs);

    for (guint i = 0; i < n_pspecs; i++) {
        GParamSpec *pspec = pspecs[i];

        /* write only what we can */
        if (!(pspec->flags & G_PARAM_WRITABLE))
            continue;

        GtkWidget *widget = util_find_child(gui, pspec->name);
        if (!widget)
            continue;

        switch (pspec->value_type) {

            case G_TYPE_INT: {
                gdouble value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
                g_object_set(gobject, pspec->name, (int)value, NULL);
                break;
            }
            case G_TYPE_BOOLEAN: {
                gboolean value = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
                g_object_set(gobject, pspec->name, value, NULL);
                break;
            }
            case G_TYPE_STRING: {
                const gchar *value = gtk_entry_get_text(GTK_ENTRY(widget));
                g_object_set(gobject, pspec->name, value, NULL);
                break;
            }
            default:
                break;
        }
    }
}