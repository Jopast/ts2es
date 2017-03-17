/*
    h_ts.c
    (C) Nicholas J Humfrey <njh@aelius.com>      2008
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


#include "ts2es.h"
#include "mpa_header.h"
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _MSC_VER
#pragma warning(disable:4100)
#pragma warning(disable:4101)
#pragma warning(disable:4189)
#endif

/* ---------------------------------------------------------------------------
 */
void ts2es_report(ts2es_t *h_ts, int i_type, const char *format, ...)
{
    static const int color_1 = FOREGROUND_RED | FOREGROUND_GREEN;  // »Æ
    static const int color_2 = FOREGROUND_RED | FOREGROUND_BLUE;   // Ñóºì
    static const int color_3 = FOREGROUND_GREEN | FOREGROUND_BLUE; // Ç³À¶

    static const char s_type_info[][20] = {
        "debug", "info", "warning", "error"
    };
    static char buff[1024];
    int si_color[] = {
        color_1, color_3, color_2, FOREGROUND_RED
    };
    int k;

    va_list ap;

    if (h_ts != NULL && i_type < h_ts->param.i_log_level) {
        return;
    }

    va_start(ap, format);
    vsnprintf(buff, sizeof(buff), format, ap);
    va_end(ap);

    if ((k = strlen(buff)) > 1 && buff[k - 1] == '\n') {
        buff[k - 1] = '\0';
    }

#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), FOREGROUND_INTENSITY | si_color[i_type]);
    fprintf(stderr, "[ts2es] %s: %s\n", s_type_info[i_type], buff);
    SetConsoleTextAttribute(GetStdHandle(STD_ERROR_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN);
#else
    fprintf(stderr, "[ts2es] %s: %s\n", s_type_info[i_type], buff);
#endif
}


/* ---------------------------------------------------------------------------
 * Check to see if a PES header looks like a valid MPEG Audio one
 */
static int validate_pes_header(ts2es_t *h_ts, int pid, uint8_t *buf_ptr, int buf_len)
{
    uint8_t stream_id = 0x00;

    // Does it have the right magic?
    if (PES_PACKET_SYNC_BYTE1(buf_ptr) != 0x00 ||
        PES_PACKET_SYNC_BYTE2(buf_ptr) != 0x00 ||
        PES_PACKET_SYNC_BYTE3(buf_ptr) != 0x01) {
        ts2es_report(h_ts, TS2ES_WARNING, "Invalid PES header (pid: %d).", pid);
        return 0;
    }

    // Is it MPEG Audio?
    stream_id = PES_PACKET_STREAM_ID(buf_ptr);
    if (stream_id < 0xC0 || stream_id > 0xDF) {
        // ts2es_report(h_ts, TS2ES_WARNING, "non-mpeg audio stream (pid: %d, stream id: 0x%x).\n", pid, stream_id);
        // return 0;
    }

    // Check PES Extension header
    if (PES_PACKET_SYNC_CODE(buf_ptr) != 0x2) {
        ts2es_report(h_ts, TS2ES_WARNING, "Invalid sync code PES extension header (pid: %d, stream id: 0x%x).", pid, stream_id);
        return 0;
    }

    // Reject scrambled packets
    if (PES_PACKET_SCRAMBLED(buf_ptr)) {
        ts2es_report(h_ts, TS2ES_WARNING, "PES payload is scrambled (pid: %d, stream id: 0x%x).", pid, stream_id);
        return 0;
    }

    // It is valid
    return 1;
}

/* ---------------------------------------------------------------------------
 * Extract the PES payload and send it to the output file
 */
static void extract_pes_payload(ts2es_t *h_ts, ts2es_es_t *p_es, int cur_pid, uint8_t *pes_ptr, size_t pes_len, int start_of_pes)
{
    uint8_t *es_ptr = NULL;
    size_t es_len = 0;

    // Start of a PES header?
    if (start_of_pes) {
        uint32_t pes_total_len = PES_PACKET_LEN(pes_ptr);
        size_t pes_header_len  = PES_PACKET_HEAD_LEN(pes_ptr);
        uint8_t stream_id      = PES_PACKET_STREAM_ID(pes_ptr);
        int64_t pts            = PES_PACKET_PTS(pes_ptr);
        int64_t dts            = PES_PACKET_DTS(pes_ptr);

        if (p_es->cur_len) {
            h_ts->f_output(h_ts, p_es, h_ts->opque_output); // output the last ES stream
        }

        // Check that it has a valid header
        if (!validate_pes_header(h_ts, cur_pid, pes_ptr, pes_len)) {
            return;
        }

        // Stream IDs in range 0xC0-0xDF are MPEG audio
        if (stream_id != p_es->pes_stream_id) {
            if (p_es->pes_stream_id == -1) {
                // keep the first stream we see
                p_es->pes_stream_id = stream_id;
                ts2es_report(h_ts, TS2ES_INFO, "Found valid PES packet (offset: 0x%lx, pid: %d, stream id: 0x%x, length: %u)\n",
                    ((unsigned long)h_ts->total_packets - 1)*TS_PACKET_SIZE, cur_pid, stream_id, pes_total_len);
            } else {
                ts2es_report(h_ts, TS2ES_INFO, "Ignoring additional stream ID 0x%x (pid: %d).\n", stream_id, cur_pid);
                return;
            }
        }
        // Store the length of the PES packet payload
        p_es->pes_remaining = pes_total_len - (2 + pes_header_len);
        p_es->pts           = pts;
        p_es->dts           = dts;

        // Keep pointer to ES data in this packet
        es_ptr = pes_ptr + (9 + pes_header_len);
        es_len = pes_len - (9 + pes_header_len);
    } else if (p_es->pes_stream_id) {
        // Only output data once we have seen a PES header
        es_ptr = pes_ptr;
        es_len = pes_len;

        // Are we are the end of the PES packet?
        if (es_len > p_es->pes_remaining) {
            es_len = p_es->pes_remaining;
        }
    }

    // Got some data to write out?
    if (es_ptr) {
        // Subtract the amount remaining in current PES packet
        p_es->pes_remaining -= es_len;

        // Scan through Elementary Stream (ES)
        // and try and find MPEG audio stream header
        while (!p_es->synced && es_len >= 4) {
            mpa_header_t mpah;
            // Valid header?
            if (mpa_header_parse(h_ts, es_ptr, &mpah)) {
                // Looks good, we have gained sync.
                if (h_ts->never_synced) {
                    ts2es_report(h_ts, TS2ES_INFO, " ");
                    mpa_header_print(h_ts, &mpah);
                    ts2es_report(h_ts, TS2ES_INFO, "MPEG Audio Framesize: %d bytes\n", mpah.framesize);
                } else {
                    ts2es_report(h_ts, TS2ES_INFO, "Regained sync at 0x % lx\n", ((unsigned long)h_ts->total_packets - 1)*TS_PACKET_SIZE);
                }
                p_es->synced = 1;
                h_ts->never_synced = 0;
            } else {
                // Skip byte
                es_len--;
                es_ptr++;
            }
        }

        // If stream is synced then write the data out
        if (p_es->synced && es_len > 0) {
            memcpy(p_es->raw_data + p_es->cur_len, es_ptr, es_len);
            p_es->cur_len += es_len;

            // Write out the data
            if (p_es->pes_remaining + pes_len < TS_PACKET_SIZE - 5) {
                h_ts->f_output(h_ts, p_es, h_ts->opque_output);
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static void ts_continuity_check(ts2es_t *h_ts, ts2es_es_t *p_es, int ts_cc)
{
    if (p_es->continuity_count != ts_cc) {
        // Only display an error after we gain sync
        if (p_es->synced) {
            ts2es_report(h_ts, TS2ES_WARNING, "TS continuity error at 0x%lx, pid[%d]: (%d, %d)\n",
                ((unsigned long)h_ts->total_packets - 1) * TS_PACKET_SIZE, 
                p_es->pid, p_es->continuity_count, ts_cc);
            p_es->synced = 0;
        }
        p_es->continuity_count = ts_cc;
    }

    p_es->continuity_count++;
    if (p_es->continuity_count == 16) {
        p_es->continuity_count = 0;
    }
}

/* ---------------------------------------------------------------------------
 */
ts2es_es_t *ts2es_find_es(ts2es_t *h_ts, int pid)
{
    int i;
    for (i = 0; i < h_ts->num_es; i++) {
        if (h_ts->es[i].b_valid && h_ts->es[i].pid == pid) {
            return &h_ts->es[i];
        }
    }

    h_ts->num_es++;
    h_ts->es[i].b_valid = 1;
    h_ts->es[i].pid     = pid;
    h_ts->es[i].cur_len = 0;
    h_ts->es[i].synced  = 1;
    h_ts->es[i].continuity_count = -1;

    return &h_ts->es[i];
}

/* ---------------------------------------------------------------------------
 */
int ts2es_demux_ts_packet(ts2es_t *h_ts, uint8_t *buf, size_t buf_len)
{
    uint8_t *pes_ptr = NULL;
    size_t pes_len;
    int cur_pid;
    ts2es_es_t *p_es = NULL;

    h_ts->total_packets++;

    // Check the sync-byte
    if (TS_PACKET_SYNC_BYTE(buf) != 0x47) {
        ts2es_report(h_ts, TS2ES_WARNING, "Lost Transport Stream syncronisation - aborting (offset: 0x%lx).\n",
            ((unsigned long)h_ts->total_packets - 1)*TS_PACKET_SIZE);
        // FIXME: try and re-gain synchronisation
        return 0;
    }

    cur_pid = TS_PACKET_PID(buf);

    if (cur_pid == 0) {
        ts2es_report(h_ts, TS2ES_DEBUG, "pid: 0, PAT\n");
        ts2es_decode_pat(h_ts, buf + 5, buf_len - 5);
    } else if (cur_pid == h_ts->pmt_pid) {
        int i = 0;
        ts2es_report(h_ts, TS2ES_DEBUG, "pid: %d, PMT\n", cur_pid);
        ts2es_decode_pmt(h_ts, buf + 5, buf_len - 5);
        h_ts->b_output = 1;

        if (h_ts->param.stream_type_2_catch >= 0) {
            h_ts->param.pid_max = 1;
            h_ts->param.pid_min = 0x1FFE;
            for (i = 0; i < MAX_NUM_ES; i++) {
                int pmt_pid = h_ts->pmt[i].pid;
                if (pmt_pid == 0) {  // 'PID = 0' is used for PAT
                    break;
                }
                if (h_ts->pmt[i].stream_type == h_ts->param.stream_type_2_catch) {
                    if (h_ts->param.pid_max < pmt_pid) {
                        h_ts->param.pid_max = pmt_pid;
                    }
                    if (h_ts->param.pid_min > pmt_pid) {
                        h_ts->param.pid_min = pmt_pid;
                    }
                }
            }
        }
    }

    // Check packet validity
    if ((cur_pid >= h_ts->param.pid_min && cur_pid <= h_ts->param.pid_max) || (h_ts->param.pid_max == -1)) {
        // Scrambled?
        if (TS_PACKET_SCRAMBLING(buf)) {
            ts2es_report(h_ts, TS2ES_WARNING, "PID %d is scrambled.", cur_pid);
            return 1;
        }

        // Transport error?
        if (TS_PACKET_TRANS_ERROR(buf)) {
            ts2es_report(h_ts, TS2ES_WARNING, "transport error at 0x%lx\n",
                ((unsigned long)h_ts->total_packets - 1)*TS_PACKET_SIZE);
            p_es->synced = 0;
            return 1;
        }
    }

    // Location of and size of PES payload
    pes_ptr = &buf[4];
    pes_len = TS_PACKET_SIZE - 4;

    // Check for adaptation field?
    if (TS_PACKET_ADAPTATION(buf) == 0x1) {
        // Payload only, no adaptation field
    } else if (TS_PACKET_ADAPTATION(buf) == 0x2) {
        // Adaptation field only, no payload
        return 1;
    } else if (TS_PACKET_ADAPTATION(buf) == 0x3) {
        // Adaptation field AND payload
        pes_ptr += (TS_PACKET_ADAPT_LEN(buf) + 1);
        pes_len -= (TS_PACKET_ADAPT_LEN(buf) + 1);
    }

    // fprintf(stdout, "pid[%4d]: %5u\n", cur_pid, pes_len);
    // Check we know about the payload
    if (cur_pid == 0x1FFF) {
        // Ignore NULL package
        return 1;
    }

    // No chosen PID yet?
    if (h_ts->param.pid_max == -1 && TS_PACKET_PAYLOAD_START(buf)) {
        // Does this one look good ?
        if (TS_PACKET_PAYLOAD_START(buf) &&
            validate_pes_header(h_ts, cur_pid, pes_ptr, pes_len)) {
            // Looks good, use this one
            h_ts->param.pid_min = h_ts->param.pid_max = cur_pid;
        }
    }

    // Process the packet, if it is the PID we are interested in
    if (cur_pid >= h_ts->param.pid_min && cur_pid <= h_ts->param.pid_max) {
        p_es = ts2es_find_es(h_ts, cur_pid);
        // Continuity check
        ts_continuity_check(h_ts, p_es, TS_PACKET_CONT_COUNT(buf));
        // Extract PES payload and write it to output
        extract_pes_payload(h_ts, p_es, cur_pid, pes_ptr, pes_len, TS_PACKET_PAYLOAD_START(buf));
    }

    return 1;
}

/* ---------------------------------------------------------------------------
 */
ts2es_t *ts2es_create(ts2es_param_t *p_param, f_ts2es_output_es p_fun_out, void *opque)
{
#if ARCH_X86_64
#  define CACHE_LINE_SIZE   32        /* for x86-64 */
#  define ALIGN_POINTER(p)  (p) = (uint8_t *)((uint64_t)((p) + (CACHE_LINE_SIZE - 1)) & (~(uint64_t)(CACHE_LINE_SIZE - 1)))
#else
#  define CACHE_LINE_SIZE   32       /* for X86-32 */
#  define ALIGN_POINTER(p)  (p) = (uint8_t *)((uint32_t)((p) + (CACHE_LINE_SIZE - 1)) & (~(uint32_t)(CACHE_LINE_SIZE - 1)))
#endif
    uint8_t *mem_base;
    uint32_t mem_size;
    ts2es_t *h_ts;
    int i;

    if ((p_param->pid_min <= 0 || p_param->pid_max < p_param->pid_min) && p_param->stream_type_2_catch <= 0) {
        ts2es_report(NULL, TS2ES_WARNING, "Invalid Transport Stream PID: [%d, %d]\n", p_param->pid_min, p_param->pid_max);
        exit(-1);
    }

    mem_size = sizeof(ts2es_t) + CACHE_LINE_SIZE
        + MAX_NUM_ES * (ES_MAX_SIZE + CACHE_LINE_SIZE);

    mem_base = (uint8_t *)malloc(mem_size);
    h_ts = (ts2es_t *)mem_base;

    if (h_ts == NULL) {
        perror("Failed to allocate memory for ts2es_t");
        exit(-3);
    }

    // Zero the memory
    memset(h_ts, 0, sizeof(ts2es_t));
    memcpy(&h_ts->param, p_param, sizeof(ts2es_param_t));

    mem_base += sizeof(ts2es_t);
    ALIGN_POINTER(mem_base);

    /* init ES data */
    for (i = 0; i < MAX_NUM_ES; i++) {
        ts2es_es_t *p_es = &h_ts->es[i];
        p_es->pes_stream_id    = -1;
        p_es->pes_remaining    = 0;
        p_es->continuity_count = -1;
        p_es->synced           = 0;
        p_es->raw_data         = mem_base;
        mem_base              += ES_MAX_SIZE;
        ALIGN_POINTER(mem_base);
    }

    // Initialize defaults
    h_ts->never_synced  = 1;
    h_ts->total_bytes   = 0;
    h_ts->total_packets = 0;

    h_ts->f_output      = p_fun_out;
    h_ts->opque_output  = opque;

    return h_ts;
}

/* ---------------------------------------------------------------------------
 */
void ts2es_destroy(ts2es_t *h_ts)
{
    if (h_ts) {
        free(h_ts);
    }
}