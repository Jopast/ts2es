/*
 *  MPEG Audio Header Parser
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _MPA_HEADER_H
#define _MPA_HEADER_H

#include <stdint.h>
#include "ts2es.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t syncword;
    uint32_t layer;
    uint32_t version;
    uint32_t error_protection;
    uint32_t bitrate_index;
    uint32_t samplerate_index;
    uint32_t padding;
    uint32_t extension;
    uint32_t mode;
    uint32_t mode_ext;
    uint32_t copyright;
    uint32_t original;
    uint32_t emphasis;
    uint32_t channels;
    uint32_t bitrate;
    uint32_t samplerate;
    uint32_t samples;
    uint32_t framesize;
} mpa_header_t;

// Get parse the header of a frame of mpeg audio
int mpa_header_parse(ts2es_t *h_ts, const uint8_t *buf, mpa_header_t *mh);

void mpa_header_print(ts2es_t *h_ts, mpa_header_t *mh);

#ifdef __cplusplus
};
#endif
#endif
