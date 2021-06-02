/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifndef REMOTE_VIEWER_H
#define REMOTE_VIEWER_H

#include <glib-object.h>
#include "virt-viewer-app.h"
#include "app_updater.h"
#include "vdi_manager.h"
#include "messenger.h"
#include "net_speedometer.h"

G_BEGIN_DECLS

#define REMOTE_VIEWER_TYPE remote_viewer_get_type()
#define REMOTE_VIEWER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), REMOTE_VIEWER_TYPE, RemoteViewer))
#define REMOTE_VIEWER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), REMOTE_VIEWER_TYPE, RemoteViewerClass))
#define REMOTE_VIEWER_IS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REMOTE_VIEWER_TYPE))
#define REMOTE_VIEWER_IS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), REMOTE_VIEWER_TYPE))
#define REMOTE_VIEWER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), REMOTE_VIEWER_TYPE, RemoteViewerClass))

typedef struct _RemoteViewerPrivate RemoteViewerPrivate;

typedef struct {
    GtkApplication parent;

    RemoteViewerPrivate *priv;

    VirtViewerApp *virt_viewer_obj;
    AppUpdater *app_updater;
    VdiManager *vdi_manager;
    VeilMessenger *veil_messenger;
    NetSpeedometer *net_speedometer;

} RemoteViewer;

typedef struct {
    GtkApplicationClass parent_class;
} RemoteViewerClass;

GType remote_viewer_get_type(void);

RemoteViewer *remote_viewer_new(void);
void remote_viewer_free_resources(RemoteViewer *self);

G_END_DECLS

#endif /* REMOTE_VIEWER_H */
/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  indent-tabs-mode: nil
 * End:
 */
