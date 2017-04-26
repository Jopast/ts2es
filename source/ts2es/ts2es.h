
/*
    ts2es.h
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

#ifndef _TS2ES_H_
#define _TS2ES_H_

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif
/* ===========================================================================
 * constant and macro definitions
 * ==========================================================================*/
// The size of MPEG2 TS packets
#define TS_PACKET_SIZE          188
// maximum size of an ES streams
#define ES_MAX_SIZE             (4 << 20)
#define MAX_NUM_ES              32

/* Macros for accessing MPEG-2 TS packet headers */
#define TS_PACKET_SYNC_BYTE(b)      (b[0])
#define TS_PACKET_TRANS_ERROR(b)    ((b[1]&0x80)>>7)
#define TS_PACKET_PAYLOAD_START(b)  ((b[1]&0x40)>>6)
#define TS_PACKET_PRIORITY(b)       ((b[1]&0x20)>>4)
#define TS_PACKET_PID(b)            (((b[1]&0x1F)<<8) | b[2])

#define TS_PACKET_SCRAMBLING(b)     ((b[3]&0xC0)>>6)
#define TS_PACKET_ADAPTATION(b)     ((b[3]&0x30)>>4)
#define TS_PACKET_CONT_COUNT(b)     ((b[3]&0x0F)>>0)
#define TS_PACKET_ADAPT_LEN(b)      (b[4])

/* Macros for accessing MPEG-2 PES packet headers */
#define PES_PACKET_SYNC_BYTE1(b)    (b[0])
#define PES_PACKET_SYNC_BYTE2(b)    (b[1])
#define PES_PACKET_SYNC_BYTE3(b)    (b[2])
#define PES_PACKET_STREAM_ID(b)     (b[3])
#define PES_PACKET_LEN(b)           ((b[4] << 8) | b[5])

#define PES_PACKET_SYNC_CODE(b)     ((b[6] & 0xC0) >> 6)
#define PES_PACKET_SCRAMBLED(b)     ((b[6] & 0x30) >> 4)
#define PES_PACKET_PRIORITY(b)      ((b[6] & 0x08) >> 3)
#define PES_PACKET_ALIGNMENT(b)     ((b[6] & 0x04) >> 2)
#define PES_PACKET_COPYRIGHT(b)     ((b[6] & 0x02) >> 1)
#define PES_PACKET_ORIGINAL(b)      ((b[6] & 0x01) >> 0)

#define PES_PACKET_PTS_DTS(b)       ((b[7] & 0xC0) >> 6)
#define PES_PACKET_ESCR(b)          ((b[7] & 0x20) >> 5)
#define PES_PACKET_ESR(b)           ((b[7] & 0x10) >> 4)
#define PES_PACKET_DSM_TRICK(b)     ((b[7] & 0x8) >> 3)
#define PES_PACKET_ADD_COPY(b)      ((b[7] & 0x4) >> 2)
#define PES_PACKET_CRC(b)           ((b[7] & 0x2) >> 1)
#define PES_PACKET_EXTEN(b)         ((b[7] & 0x1) >> 0)
#define PES_PACKET_HEAD_LEN(b)      (b[8])

#define PES_PACKET_PTS(b)       ((uint32_t)((b[9] & 0x0E) << 29) | \
                     (uint32_t)(b[10] << 22) | \
                     (uint32_t)((b[11] & 0xFE) << 14) | \
                     (uint32_t)(b[12] << 7) | \
                     (uint32_t)(b[13] >> 1))

#define PES_PACKET_DTS(b)       ((uint32_t)((b[14] & 0x0E) << 29) | \
                     (uint32_t)(b[15] << 22) | \
                     (uint32_t)((b[16] & 0xFE) << 14) | \
                     (uint32_t)(b[17] << 7) | \
                     (uint32_t)(b[18] >> 1))

enum ts2es_report_e {
    TS2ES_DEBUG   = 0,
    TS2ES_INFO    = 1,
    TS2ES_WARNING = 2,
    TS2ES_ERROR   = 3,
    TS2ES_INFO_TYPE_MASK = 0xff,
};

/* ===========================================================================
 * type definitions
 * ==========================================================================*/

typedef struct ts2es_es_t ts2es_es_t;
typedef struct ts2es_t    ts2es_t;

typedef void(*f_ts2es_output_es)(ts2es_t *h_ts, ts2es_es_t *p_es, void *opque);

typedef struct ts2es_param_t {
    char s_input[256];
    char s_output[256];

    int  i_log_level;
    int  stream_type_2_catch;
    int  pid_min;
    int  pid_max;
} ts2es_param_t;

typedef struct ts2es_es_t {
    uint32_t b_valid;   // 数据段是否有效
    uint32_t pid;       // 保存的ES流的PID
    int64_t  pts;       // PES包的pts
    int64_t  dts;       // PES包的dts
    int      synced;
    int      continuity_count;
    int      pes_remaining;
    int      pes_stream_id;
    uint32_t total_len; // 总的ES长度
    uint32_t cur_len;   // 当前已经获取的buffer长度
    uint8_t *raw_data;  // 数据buffer的指针
} ts2es_es_t;

typedef struct ts2es_pmt_t {
    uint8_t    stream_type;        /* 8 bit, 指示特定PID的节目元素包的类型。该处PID由elementary PID指定 */
    uint16_t   pid;                /* 13 bit, 该域指示TS包的PID值。这些TS包含有相关的节目元素 */
    uint16_t   ES_info_length;     /* 12 bit, 前两位bit为00。该域指示跟随其后的描述相关节目元素的byte数 */
    uint32_t   descriptor;
} ts2es_pmt_t;

typedef struct ts2es_pat_t {
    /* PID: 0x0000 */
    uint16_t   program_id;         /* 16 bit, 频道号 */
    uint16_t   program_map_pid;    /* 13 bit, PID of PMT */
} ts2es_pat_t;

typedef struct ts2es_t {
    ts2es_param_t       param;

    int                 Interrupted;
    int                 never_synced;
    uint32_t            total_bytes;
    uint32_t            total_packets;
    int                 num_es;
    int                 b_output;
    f_ts2es_output_es   f_output;
    void               *opque_output;

    ts2es_pat_t         pat;
    int                 pmt_pid;
    ts2es_pmt_t         pmt[MAX_NUM_ES];

    ts2es_es_t          es[MAX_NUM_ES];
} ts2es_t;


/* ===========================================================================
 * interface definitions
 * ==========================================================================*/

ts2es_t *ts2es_create(ts2es_param_t *p_param, f_ts2es_output_es p_fun_out, void *opque);
int      ts2es_demux_ts_packet(ts2es_t *h_ts, uint8_t *buf, size_t buf_len);
void     ts2es_destroy(ts2es_t *h_ts);

void     ts2es_decode_pat(ts2es_t *h_ts, uint8_t *buf, int buf_len);
void     ts2es_decode_pmt(ts2es_t *h_ts, uint8_t *buf, int buf_len);

void     ts2es_report(ts2es_t *h_ts, int i_type, const char *format, ...);

#ifdef __cplusplus
};
#endif
#endif // _TS2ES_H_

