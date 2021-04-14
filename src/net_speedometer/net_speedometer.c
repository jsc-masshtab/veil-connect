/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifdef _WIN32
#include <stdlib.h>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#endif

#include "net_speedometer.h"
#include "remote-viewer-util.h"
#include "vdi_session.h"

#include "virt-viewer-session-spice.h"

G_DEFINE_TYPE( NetSpeedometer, net_speedometer, G_TYPE_OBJECT )

#define PING_JOB_TIMEOUT 30000000 // microseconds
#define STATS_CHECK_TIMEOUT 5 // sec

#ifdef G_OS_UNIX
static void* net_speedometer_ping_job_linux(NetSpeedometer *self)
{
    g_info("%s", (const char *) __func__);

    gchar *standard_output = NULL;
    gchar *standard_error = NULL;
    gint exit_status = 0;

    while(self->is_ping_job_running) {

        g_autofree gchar *vm_ip = NULL;
        vm_ip = atomic_string_get(&self->vm_ip);
        if (vm_ip == NULL) {
            cancellable_sleep(PING_JOB_TIMEOUT, &self->is_ping_job_running);
            continue;
        }

        g_autofree gchar *command_line = NULL;
        command_line = g_strdup_printf("ping -W 2 -i 0.5 -c 5 %s", vm_ip);

        gboolean cmd_res = g_spawn_command_line_sync(command_line,
                                                     &standard_output,
                                                     &standard_error,
                                                     &exit_status,
                                                     NULL);
        // parse
        if (cmd_res) {
            //g_info("TEST PING: %s", standard_output);

            /// rtt min/avg/max parsing
            GRegex *regex = g_regex_new("(\\d+.\\d+)/(\\d+.\\d+)/(\\d+.\\d+)/(\\d+.\\d+)",
                    G_REGEX_MULTILINE, 0, NULL);
            GMatchInfo *match_info;
            g_regex_match(regex, standard_output, 0, &match_info);

            gboolean is_success = g_match_info_matches(match_info);
            if (is_success) {

                g_autofree gchar *ping_data_str = NULL;
                ping_data_str = g_match_info_fetch(match_info, 0);

                if (ping_data_str) {
                    gchar *ping_data_str_with_commas = NULL;
                    ping_data_str_with_commas = replace_str(ping_data_str, ".", ",");

                    gchar **ping_stats_array = g_strsplit(ping_data_str_with_commas, "/", 4);
                    if (g_strv_length(ping_stats_array) > 3) {

                        self->min_rtt = (float) atof(ping_stats_array[0]);
                        self->avg_rtt = (float) atof(ping_stats_array[1]);
                        self->max_rtt = (float) atof(ping_stats_array[2]);
                        g_info("rtt min/avg/max parsed: %s", ping_data_str_with_commas);
                        //self->is_ip_reachable = TRUE;
                    }
                    g_strfreev(ping_stats_array);
                }
            }

            if(match_info)
                g_match_info_free(match_info);
            g_regex_unref(regex);

            /// Loss parsing
            regex = g_regex_new("\\d+% packet loss", G_REGEX_MULTILINE, 0, NULL);
            g_regex_match(regex, standard_output, 0, &match_info);
            is_success = g_match_info_matches(match_info);
            //g_info("IS SUC: %i", is_success);
            if (is_success) {

                g_autofree gchar *ping_loss_data_str = NULL;
                ping_loss_data_str = g_match_info_fetch(match_info, 0);

                if (ping_loss_data_str) {
                    //g_info("ping_loss_data_str: %s", ping_loss_data_str);
                    gchar **ping_loss_array = g_strsplit(ping_loss_data_str, "%", 2);
                    if (g_strv_length(ping_loss_array) > 1) {
                        self->loss_percentage = atoi(ping_loss_array[0]);
                        g_info("Cur loss: %i", self->loss_percentage);
                    }

                    g_strfreev(ping_loss_array);
                }
            }

            if(match_info)
                g_match_info_free(match_info);
            g_regex_unref(regex);
        }

        free_memory_safely(&standard_output);
        free_memory_safely(&standard_error);

        cancellable_sleep(PING_JOB_TIMEOUT, &self->is_ping_job_running);
    }

    return NULL;
}
#elif _WIN32
static void* net_speedometer_ping_job_win(NetSpeedometer *self)
{
    g_info("%s", (const char *) __func__);

    gchar *standard_output = NULL;
    gchar *standard_error = NULL;
    gint exit_status = 0;

    while(self->is_ping_job_running) {

        g_autofree gchar *vm_ip = NULL;
        vm_ip = atomic_string_get(&self->vm_ip);
        if (vm_ip == NULL)
            continue;

        g_autofree gchar *command_line = NULL;
        command_line = g_strdup_printf("ping -w 500 -n 4 %s", vm_ip);

        gboolean cmd_res = g_spawn_command_line_sync(command_line,
                                                     &standard_output,
                                                     &standard_error,
                                                     &exit_status,
                                                     NULL);
        // parse
        if (cmd_res && standard_output) {
            //fprintf(stdout, "standard_output: %s \n", standard_output);

            GRegex *regex = g_regex_new(" = \\d+", 0, 0, NULL);
            GMatchInfo *match_info;
            g_regex_match(regex, standard_output, 0, &match_info);

            int index = 0;
            int p_sent = 0;
            int p_recv = 0;

            while (g_match_info_matches(match_info))
            {
                gchar *pind_data_str = g_match_info_fetch(match_info, 0);

                if (strlen_safely(pind_data_str) > 2) {
                    // parse loss
                    if (index == 0)
                        p_sent = atoi(&pind_data_str[2]);
                    else if(index == 1)
                        p_recv = atoi(&pind_data_str[2]);
                    // parse RTT
                    else if (index == 3)
                        self->min_rtt = (float) atof(&pind_data_str[2]);
                    else if (index == 4)
                        self->max_rtt = (float) atof(&pind_data_str[2]);
                    else if (index == 5)
                        self->avg_rtt = (float) atof(&pind_data_str[2]);
                }

                g_free(pind_data_str);
                g_match_info_next(match_info, NULL);

                index++;
            }

            // calculate loss
            if (p_sent == 0)
                self->loss_percentage = 100;
            else
                self->loss_percentage = (int) (1.0 - (float)p_recv / (float)p_sent) * 100;

            g_regex_unref(regex);

            if(match_info)
                g_match_info_free(match_info);
        }

        free_memory_safely(&standard_output);
        free_memory_safely(&standard_error);

        cancellable_sleep(PING_JOB_TIMEOUT, &self->is_ping_job_running);
    }

    return NULL;
}
#endif

static inline guint64 get_speed(guint64 cur_bytes, guint64 prev_bytes)
{
    return (cur_bytes - prev_bytes) / (guint64)STATS_CHECK_TIMEOUT;
}

static gboolean net_speedometer_check_stats(NetSpeedometer *self)
{
    // net_speedometer_check_stats вызывается каждые STATS_CHECK_TIMEOUT секунд
    //RDP
    if (self->p_rdp) {
        UINT64 in_bytes = 0;
        UINT64 out_bytes = 0;
        UINT64 in_packets = 0;
        UINT64 out_packets = 0;
        freerdp_get_stats(self->p_rdp, &in_bytes, &out_bytes, &in_packets, &out_packets);

        self->rdp_read_speed = get_speed(in_bytes, self->rdp_read_bytes);
        self->rdp_read_bytes = in_bytes;

        self->rdp_write_speed = get_speed(out_bytes, self->rdp_write_bytes);
        self->rdp_write_bytes = out_bytes;
        //g_info("STATs: in_bytes  %lu out_bytes  %lu  self->rdp_read_speed  %lu  self->rdp_write_speed  %lu",
        //       in_bytes, out_bytes, self->rdp_read_speed, self->rdp_write_speed);
        // Send to VDI server
        vdi_ws_client_send_rdp_network_stats(vdi_session_get_ws_client(), self->rdp_read_speed, self->rdp_write_speed,
                                             self->min_rtt, self->avg_rtt, self->max_rtt, self->loss_percentage);
    } else {
        self->rdp_read_bytes = 0;
        self->rdp_write_bytes = 0;
    }

    // Spice
    if (virt_viewer_app_is_active(self->p_virt_viewer_app)) {

        VirtViewerSession *virt_viewer_session = virt_viewer_app_get_session(self->p_virt_viewer_app);
        VirtViewerSessionSpice *spice_session = VIRT_VIEWER_SESSION_SPICE(virt_viewer_session);

        if (spice_session) {
            SpiceReadBytes cur_spice_read_bytes = {};
            virt_viewer_session_spice_get_stats(spice_session, &cur_spice_read_bytes);

            self->spice_read_speeds.bytes_inputs =
                    get_speed(cur_spice_read_bytes.bytes_inputs, self->spice_read_bytes.bytes_inputs);
            self->spice_read_speeds.bytes_webdav =
                    get_speed(cur_spice_read_bytes.bytes_webdav, self->spice_read_bytes.bytes_webdav);
            self->spice_read_speeds.bytes_cursor =
                    get_speed(cur_spice_read_bytes.bytes_cursor, self->spice_read_bytes.bytes_cursor);
            self->spice_read_speeds.bytes_display =
                    get_speed(cur_spice_read_bytes.bytes_display, self->spice_read_bytes.bytes_display);
            self->spice_read_speeds.bytes_record =
                    get_speed(cur_spice_read_bytes.bytes_record, self->spice_read_bytes.bytes_record);
            self->spice_read_speeds.bytes_playback =
                    get_speed(cur_spice_read_bytes.bytes_playback, self->spice_read_bytes.bytes_playback);
            self->spice_read_speeds.bytes_main =
                    get_speed(cur_spice_read_bytes.bytes_main, self->spice_read_bytes.bytes_main);

            self->spice_total_read_speed = self->spice_read_speeds.bytes_inputs +
                    self->spice_read_speeds.bytes_webdav +
                    self->spice_read_speeds.bytes_cursor +
                    self->spice_read_speeds.bytes_display +
                    self->spice_read_speeds.bytes_record +
                    self->spice_read_speeds.bytes_playback +
                    self->spice_read_speeds.bytes_main;


            self->spice_read_bytes = cur_spice_read_bytes;
            // Send to VDI server
            vdi_ws_client_send_spice_network_stats(vdi_session_get_ws_client(), &self->spice_read_speeds,
                                                   self->spice_total_read_speed,
                                                   self->min_rtt, self->avg_rtt, self->max_rtt, self->loss_percentage);
        }
    } else {
        memset(&self->spice_read_bytes, 0, sizeof(SpiceReadBytes));
    }

    return G_SOURCE_CONTINUE;
}

static void net_speedometer_finalize(GObject *object)
{
    g_info("%s", (const char *)__func__);
    NetSpeedometer *self = NET_SPEEDOMETER(object);
    // stop all
    self->is_ping_job_running = FALSE;
    if (self->ping_job_thread)
        g_thread_join(self->ping_job_thread);

    // signal handlers disconnect
    g_source_remove(self->stats_check_event_source_id);

    // unref
    atomic_string_deinit(&self->vm_ip);

    GObjectClass *parent_class = G_OBJECT_CLASS( net_speedometer_parent_class );
    ( *parent_class->finalize )( object );
}

static void net_speedometer_class_init(NetSpeedometerClass *klass )
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = net_speedometer_finalize;
}

static void net_speedometer_init(NetSpeedometer *self)
{
    g_info("%s", (const char *) __func__);

    self->p_virt_viewer_app = NULL;
    self->p_rdp = NULL;

    atomic_string_init(&self->vm_ip);
    //atomic_string_set(&self->vm_ip, "8.8.8.8");
    // quality
    self->min_rtt = 0; // ms
    self->avg_rtt = 0;
    self->max_rtt = 0;
    self->loss_percentage = 0; // 0 - 100
    self->is_ip_reachable = FALSE;

    // spice speed
    memset(&self->spice_read_bytes, 0, sizeof(SpiceReadBytes));

    // rdp speed
    self->rdp_read_bytes = 0;
    self->rdp_write_bytes = 0;

    // start ping job
    self->is_ping_job_running = TRUE;
#ifdef G_OS_UNIX
    self->ping_job_thread = g_thread_new(NULL, (GThreadFunc)net_speedometer_ping_job_linux, self);
#elif _WIN32
    self->ping_job_thread = NULL;// g_thread_new(NULL, (GThreadFunc)net_speedometer_ping_job_win, self);
#endif

    // Start net stats checker timeout
    self->stats_check_event_source_id =
            g_timeout_add(STATS_CHECK_TIMEOUT * 1000, (GSourceFunc)net_speedometer_check_stats, self);
}

NetSpeedometer *net_speedometer_new()
{
    g_info("%s", (const char *)__func__);
    NetSpeedometer *net_speedometer = NET_SPEEDOMETER( g_object_new( TYPE_NET_SPEEDOMETER, NULL ) );
    return net_speedometer;
}

void net_speedometer_update_vm_ip(NetSpeedometer *self, const gchar *vm_ip)
{
    atomic_string_set(&self->vm_ip, vm_ip);
}

void net_speedometer_set_pointer_to_virt_viewer_app(NetSpeedometer *self, VirtViewerApp *app)
{
    self->p_virt_viewer_app = app;
}

void net_speedometer_set_pointer_rdp_context(NetSpeedometer *self, rdpRdp *rdp)
{
    self->p_rdp = rdp;
}