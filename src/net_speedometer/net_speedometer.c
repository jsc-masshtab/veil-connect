/*
 * VeiL Connect
 * VeiL VDI Client
 * Based on virt-viewer and freerdp
 *
 * Author: http://mashtab.org/
 */

#ifdef _WIN32
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#endif

#include <string.h>

#include <freerdp/version.h>

#include "net_speedometer.h"
#include "remote-viewer-util.h"

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

        // reset data
        self->nw.min_rtt = self->nw.max_rtt = self->nw.avg_rtt = 0;
        self->nw.loss_percentage = 0;
        // parse
        if (cmd_res) {
            /// rtt min/avg/max parsing
            GRegex *regex = g_regex_new("(\\d+.\\d+)/(\\d+.\\d+)/(\\d+.\\d+)/(\\d+.\\d+)",
                    G_REGEX_MULTILINE, 0, NULL);
            GMatchInfo *match_info = NULL;
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

                        self->nw.min_rtt = (float) atof(ping_stats_array[0]);
                        self->nw.avg_rtt = (float) atof(ping_stats_array[1]);
                        self->nw.max_rtt = (float) atof(ping_stats_array[2]);
                        //g_info("rtt min/avg/max parsed: %s", ping_data_str_with_commas);
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
            int loss_parse_error_code = 0;
            if (is_success) {
                g_autofree gchar *ping_loss_data_str = NULL;
                ping_loss_data_str = g_match_info_fetch(match_info, 0);

                if (ping_loss_data_str) {
                    gchar **ping_loss_array = g_strsplit(ping_loss_data_str, "%", 2);
                    if (g_strv_length(ping_loss_array) > 1) {
                        self->nw.loss_percentage = atoi(ping_loss_array[0]);
                        //g_info("Cur loss: %i", self->loss_percentage);
                    } else {
                        loss_parse_error_code = 1;
                    }
                    g_strfreev(ping_loss_array);
                } else {
                    loss_parse_error_code = 2;
                }
            } else {
                loss_parse_error_code = 3;
            }
            // Ошибки быть не должно. Если пристутствует, значит ping выдал неожидаемый std output
            if (loss_parse_error_code)
                g_warning("%s: Cant parse packet loss from ping output. Error code: %i\n%s",
                        (const char *)__func__, loss_parse_error_code, standard_output);


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
    // На Windows если вызвать пинг, то откроется консоль, что неприемлимо.
    // Поэтому вызываем пинг через VBS скрипт и пишим результат в файл.
    // Затем забираем данные из файла

    g_info("%s", (const char *) __func__);

    gchar *standard_output = NULL;
    gchar *standard_error = NULL;
    gint exit_status = 0;

    g_autofree gchar *temp_files_dir = NULL;
    temp_files_dir = get_windows_app_temp_location();
    g_autofree gchar *temp_files_dir_2 = NULL;
    temp_files_dir_2 = replace_str(temp_files_dir, "\\", "/"); // VBScript wants this
    g_autofree gchar *ping_output_file = NULL;
    ping_output_file = g_strdup_printf("%s/ping_output.txt", temp_files_dir_2);

    while(self->is_ping_job_running) {

        g_autofree gchar *vm_ip = NULL;
        vm_ip = atomic_string_get(&self->vm_ip);
        if (vm_ip == NULL) {
            cancellable_sleep(PING_JOB_TIMEOUT, &self->is_ping_job_running);
            continue;
        }

        g_autofree gchar *command_line = NULL;
        command_line = g_strdup_printf("wscript.exe ping_no_console.vbs %s %s",
                                       vm_ip, ping_output_file);
        //GError *error = NULL;
        gboolean cmd_res = g_spawn_command_line_sync(command_line,
                                                     &standard_output,
                                                     &standard_error,
                                                     &exit_status,
                                                     NULL);
        // parse
        GMappedFile *file = g_mapped_file_new(ping_output_file, false, NULL);
        gchar *contents = g_mapped_file_get_contents(file);

        // reset data
        self->nw.min_rtt = self->nw.max_rtt = self->nw.avg_rtt = 0;
        self->nw.loss_percentage = 0;
        if (cmd_res && contents) {
            //fprintf(stdout, "standard_output: %s \n", standard_output);

            GRegex *regex = g_regex_new(" = \\d+", 0, 0, NULL);
            GMatchInfo *match_info;
            g_regex_match(regex, contents, 0, &match_info);

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
                        self->nw.min_rtt = (float) atof(&pind_data_str[2]);
                    else if (index == 4)
                        self->nw.max_rtt = (float) atof(&pind_data_str[2]);
                    else if (index == 5)
                        self->nw.avg_rtt = (float) atof(&pind_data_str[2]);
                }

                g_free(pind_data_str);
                g_match_info_next(match_info, NULL);

                index++;
            }

            // calculate loss
            if (p_sent != 0)
                self->nw.loss_percentage = (int) (1.0 - (float)p_recv / (float)p_sent) * 100;

            g_regex_unref(regex);

            if(match_info)
                g_match_info_free(match_info);
        }

        g_mapped_file_unref(file);
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
#if FREERDP_VERSION_MINOR >= 1
    if (self->p_rdp) {
        UINT64 in_bytes = 0;
        UINT64 out_bytes = 0;
        UINT64 in_packets = 0;
        UINT64 out_packets = 0;
        freerdp_get_stats(self->p_rdp, &in_bytes, &out_bytes, &in_packets, &out_packets);

        self->nw.rdp_read_speed = get_speed(in_bytes, self->nw.rdp_read_bytes);
        self->nw.rdp_read_bytes = in_bytes;

        self->nw.rdp_write_speed = get_speed(out_bytes, self->nw.rdp_write_bytes);
        self->nw.rdp_write_bytes = out_bytes;
        //g_info("STATs: in_bytes  %lu out_bytes  %lu  self->rdp_read_speed  %lu  self->rdp_write_speed  %lu",
        //       in_bytes, out_bytes, self->rdp_read_speed, self->rdp_write_speed);
        // Send to VDI server
        vdi_ws_client_send_rdp_network_stats(vdi_session_get_ws_client(),
                self->nw.rdp_read_speed, self->nw.rdp_write_speed, self->nw.min_rtt,
                self->nw.avg_rtt, self->nw.max_rtt, self->nw.loss_percentage);

        g_signal_emit_by_name(self, "stats-data-updated", RDP_PROTOCOL, &self->nw);
    } else {
        self->nw.rdp_read_bytes = 0;
        self->nw.rdp_write_bytes = 0;
    }
#else
    self->nw.rdp_read_speed = 0;
    self->nw.rdp_write_speed = 0;
#endif
    // Spice
    if (self->p_virt_viewer_app && virt_viewer_app_is_active(self->p_virt_viewer_app)) {

        VirtViewerSession *virt_viewer_session = virt_viewer_app_get_session(self->p_virt_viewer_app);
        VirtViewerSessionSpice *spice_session = VIRT_VIEWER_SESSION_SPICE(virt_viewer_session);

        if (spice_session) {
            SpiceReadBytes cur_spice_read_bytes = {};
            virt_viewer_session_spice_get_stats(spice_session, &cur_spice_read_bytes);

            self->nw.spice_read_speeds.bytes_inputs =
                    get_speed(cur_spice_read_bytes.bytes_inputs, self->nw.spice_read_bytes.bytes_inputs);
            self->nw.spice_read_speeds.bytes_webdav =
                    get_speed(cur_spice_read_bytes.bytes_webdav, self->nw.spice_read_bytes.bytes_webdav);
            self->nw.spice_read_speeds.bytes_cursor =
                    get_speed(cur_spice_read_bytes.bytes_cursor, self->nw.spice_read_bytes.bytes_cursor);
            self->nw.spice_read_speeds.bytes_display =
                    get_speed(cur_spice_read_bytes.bytes_display, self->nw.spice_read_bytes.bytes_display);
            self->nw.spice_read_speeds.bytes_record =
                    get_speed(cur_spice_read_bytes.bytes_record, self->nw.spice_read_bytes.bytes_record);
            self->nw.spice_read_speeds.bytes_playback =
                    get_speed(cur_spice_read_bytes.bytes_playback, self->nw.spice_read_bytes.bytes_playback);
            self->nw.spice_read_speeds.bytes_main =
                    get_speed(cur_spice_read_bytes.bytes_main, self->nw.spice_read_bytes.bytes_main);

            self->nw.spice_total_read_speed = self->nw.spice_read_speeds.bytes_inputs +
                                              self->nw.spice_read_speeds.bytes_webdav +
                                              self->nw.spice_read_speeds.bytes_cursor +
                                              self->nw.spice_read_speeds.bytes_display +
                                              self->nw.spice_read_speeds.bytes_record +
                                              self->nw.spice_read_speeds.bytes_playback +
                                              self->nw.spice_read_speeds.bytes_main;


            self->nw.spice_read_bytes = cur_spice_read_bytes;
            // Send to VDI server
            vdi_ws_client_send_spice_network_stats(vdi_session_get_ws_client(), &self->nw.spice_read_speeds,
                                                   self->nw.spice_total_read_speed,
                                                   self->nw.min_rtt, self->nw.avg_rtt, self->nw.max_rtt,
                                                   self->nw.loss_percentage);

            g_signal_emit_by_name(self, "stats-data-updated", SPICE_PROTOCOL, &self->nw);
        }
    } else {
        memset(&self->nw.spice_read_bytes, 0, sizeof(SpiceReadBytes));
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
    if (self->stats_check_event_source_id)
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

    // signals
    g_signal_new("stats-data-updated",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(NetSpeedometerClass, stats_data_updated),
                 NULL, NULL,
                 NULL,
                 G_TYPE_NONE,
                 2,
                 G_TYPE_INT, G_TYPE_POINTER);

    g_signal_new("address-changed",
                 G_OBJECT_CLASS_TYPE(gobject_class),
                 G_SIGNAL_RUN_FIRST,
                 G_STRUCT_OFFSET(NetSpeedometerClass, address_changed),
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
}

static void net_speedometer_init(NetSpeedometer *self)
{
    g_info("%s", (const char *) __func__);

    self->p_virt_viewer_app = NULL;
    self->p_rdp = NULL;

    atomic_string_init(&self->vm_ip);
    //atomic_string_set(&self->vm_ip, "8.8.8.9");
    // quality
    self->nw.min_rtt = 0; // ms
    self->nw.avg_rtt = 0;
    self->nw.max_rtt = 0;
    self->nw.loss_percentage = 0; // 0 - 100
    self->is_ip_reachable = FALSE;

    // spice speed
    memset(&self->nw.spice_read_bytes, 0, sizeof(SpiceReadBytes));

    // rdp speed
    self->nw.rdp_read_bytes = 0;
    self->nw.rdp_write_bytes = 0;

    // start ping job
    self->is_ping_job_running = TRUE;
#ifdef G_OS_UNIX
    self->ping_job_thread = g_thread_new(NULL, (GThreadFunc)net_speedometer_ping_job_linux, self);
#elif _WIN32
    self->ping_job_thread = g_thread_new(NULL, (GThreadFunc)net_speedometer_ping_job_win, self);
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
    g_signal_emit_by_name(self, "address-changed");
}

void net_speedometer_set_pointer_to_virt_viewer_app(NetSpeedometer *self, VirtViewerApp *app)
{
    self->p_virt_viewer_app = app;
}

void net_speedometer_set_pointer_rdp_context(NetSpeedometer *self, rdpRdp *rdp)
{
    self->p_rdp = rdp;
}