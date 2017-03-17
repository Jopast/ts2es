
/*
    test.c
    (C) Falei Luo          <falei.luo@gmail.com> 2017

    Copyright notice:

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "ts2es/ts2es.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 */
void ts2es_output_es(ts2es_t *h_ts, ts2es_es_t *p_es, void *opque)
{
    if (h_ts->b_output && p_es->cur_len) {
        char s_path[260];
        int written = 0;
        FILE *fp;
        
        sprintf_s(s_path, 260, "%s_%d.es", h_ts->param.s_output, p_es->pid);
        fp = fopen(s_path, "ab+");
        if (fp != NULL) {
            written = fwrite(p_es->raw_data, 1, p_es->cur_len, fp);
            fclose(fp);
        }

        p_es->cur_len = 0;
        if (written <= 0) {
            ts2es_report(h_ts, TS2ES_ERROR, "failed to write stream out");
            exit(-2);
        } else {
            ts2es_report(h_ts, TS2ES_DEBUG, "writing TS packet, PID[%d], pts: %d\n", p_es->pid, p_es->pts);
        }
        h_ts->total_bytes += written;
    }
}

/* ---------------------------------------------------------------------------
 */
int main(int argc, char **argv)
{
    FILE *fin;
    ts2es_param_t param;
    ts2es_t *h_ts;
    uint8_t buf[TS_PACKET_SIZE];

    // Parse the command-line parameters
    param.i_log_level = TS2ES_DEBUG;   // only report information whose level >= i_log_level
    param.stream_type_2_catch = 66;    // stream_type to catch, if >=0, would overwrite the pid_min and pid_max settings
    // param.pid_min = 0x166;
    // param.pid_max = param.pid_min + 16;

    strcpy(param.s_input, "test.ts");
    strcpy(param.s_output, "output.es");

    h_ts = ts2es_create(&param, &ts2es_output_es, NULL);

    // Hard work happens here
    fin = fopen(h_ts->param.s_input, "rb");
    if (fin == NULL) {
        perror("Failed to open input file");
        exit(-2);
    }

    while (!h_ts->Interrupted) {
        int count = fread(buf, 1, TS_PACKET_SIZE, fin);
        if (count == 0) {
            break;
        }

        if (ts2es_demux_ts_packet(h_ts, buf, count) == 0) {
            break;
        }
    }

    // Display statistics
    ts2es_report(NULL, TS2ES_INFO, "TS packets processed: %lu\n", h_ts->total_packets);
    ts2es_report(NULL, TS2ES_INFO, "Total written: %lu bytes\n", h_ts->total_bytes);

    ts2es_destroy(h_ts);
    h_ts = NULL;

    // Success
    return 0;
}

