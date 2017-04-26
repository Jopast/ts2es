
/*
    ts_table.c
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

#ifdef _MSC_VER
#pragma warning(disable:4100)
#pragma warning(disable:4101)
#pragma warning(disable:4189)
#endif

/* ---------------------------------------------------------------------------
 */
void ts2es_decode_pat(ts2es_t *h_ts, uint8_t *buf, int buf_len)
{
    int table_id = buf[0];
    int section_syntax_indicator = buf[1] >> 7;
    int zero = buf[1] >> 6 & 0x1;
    int reserved_1 = buf[1] >> 4 & 0x3;
    int section_length = ((buf[1] & 0x0F) << 8) | buf[2];

    int transport_stream_id = buf[3] << 8 | buf[4];

    int reserved_2 = buf[5] >> 6;
    int version_number = buf[5] >> 1 & 0x1F;
    int current_next_indicator = (buf[5] << 7) >> 7;
    int section_number = buf[6];
    int last_section_number = buf[7];

    int len = 0;
    int n = 0;

    h_ts->pmt_pid = -1;
    memset(h_ts->pmt, 0, sizeof(h_ts->pmt));

    len = 3 + section_length;
    int CRC_32 = (buf[len - 4] & 0x000000FF) << 24
        | (buf[len - 3] & 0x000000FF) << 16
        | (buf[len - 2] & 0x000000FF) << 8
        | (buf[len - 1] & 0x000000FF);

    for (n = 0; n < section_length - 12; n += 4) {
        unsigned  program_num = buf[8 + n] << 8 | buf[9 + n];
        int reserved_3 = buf[10 + n] >> 5;
        int network_PID = (buf[10 + n] & 0x1F) << 8 | buf[11 + n];

        if (program_num == 0x00) {
            int TS_network_Pid = network_PID; //记录该TS流的网络PID  

            ts2es_report(h_ts, TS2ES_INFO, "program num %d, network_PID: %d\n", program_num, TS_network_Pid);
        } else {
            int program_map_PID = network_PID;
            int program_number = program_num;

            h_ts->pmt_pid = program_map_PID;
            ts2es_report(h_ts, TS2ES_INFO, "program num %d, PMT pid: %d\n", program_number, program_map_PID);
            // 向全局PAT节目数组中添加PAT节目信息
        }
    }
}

/* ---------------------------------------------------------------------------
 */
void ts2es_decode_pmt(ts2es_t *h_ts, uint8_t *buf, int buf_len)
{
    int table_id = buf[0];
    int section_syntax_indicator = buf[1] >> 7;
    int zero = buf[1] >> 6 & 0x01;
    int reserved_1 = buf[1] >> 4 & 0x03;
    int section_length = (buf[1] & 0x0F) << 8 | buf[2];
    int program_number = buf[3] << 8 | buf[4];
    int reserved_2 = buf[5] >> 6;
    int version_number = buf[5] >> 1 & 0x1F;
    int current_next_indicator = (buf[5] << 7) >> 7;
    int section_number = buf[6];
    int last_section_number = buf[7];
    int reserved_3 = buf[8] >> 5;
    int PCR_PID = ((buf[8] << 8) | buf[9]) & 0x1FFF;

    int PCRID = PCR_PID;

    int reserved_4 = buf[10] >> 4;
    int program_info_length = (buf[10] & 0x0F) << 8 | buf[11];
    // Get CRC_32  
    int len = section_length + 3;
    // int CRC_32 = (buf[len - 4] & 0x000000FF) << 24
    //     | (buf[len - 3] & 0x000000FF) << 16
    //     | (buf[len - 2] & 0x000000FF) << 8
    //     | (buf[len - 1] & 0x000000FF);

    int pos = 12;
    int i = 0;

    // program info descriptor  
    if (program_info_length != 0) {
        pos += program_info_length;
    }

    // Get stream type and PID      
    for (; pos <= (section_length + 2) - 4;) {
        int stream_type = buf[pos];
        int reserved_5 = buf[pos + 1] >> 5;
        int elementary_PID = ((buf[pos + 1] << 8) | buf[pos + 2]) & 0x1FFF;
        int reserved_6 = buf[pos + 3] >> 4;
        int ES_info_length = (buf[pos + 3] & 0x0F) << 8 | buf[pos + 4];

        int descriptor = 0x00;

        if (ES_info_length != 0) {
            descriptor = buf[pos + 5];

            for (int len = 2; len <= ES_info_length; len++) {
                descriptor = descriptor << 8 | buf[pos + 4/* + len*/];
            }
            pos += ES_info_length;
        }
        pos += 5;

        h_ts->pmt[i].pid = elementary_PID;
        h_ts->pmt[i].stream_type = stream_type;
        h_ts->pmt[i].ES_info_length = ES_info_length;
        h_ts->pmt[i].descriptor = descriptor;
        i++;

        // ts2es_report(h_ts, TS2ES_INFO, "PID %d, stream_type %d, length %d, descriptor %u",
        //     elementary_PID, stream_type, ES_info_length, descriptor);
    }
}

