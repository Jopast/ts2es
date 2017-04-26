/*
 *  MPEG Audio Header Parser
 (C) Nicholas J Humfrey <njh@aelius.com>      2008
 (C) Falei Luo          <falei.luo@gmail.com> 2017
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


/*
    MPEG Audio frame handling courtesy of Scott Manley
    Borrowed from libshout 2.1 / mp3.c
    With a few changes and fixes
    */

#include "mpa_header.h"


#define MPA_MODE_STEREO     0
#define MPA_MODE_JOINT      1
#define MPA_MODE_DUAL       2
#define MPA_MODE_MONO       3


static const uint32_t bitrate[3][3][16] = {
    {
        // MPEG 1
        { 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 }, // Layer 1
        { 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0 }, // Layer 2
        { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 }  // Layer 3
    },
    {
        // MPEG 2
        { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0 }, // Layer 1
        { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 }, // Layer 2
        { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 }  // Layer 3
    },
    {
        // MPEG 2.5
        { 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0 }, // Layer 1
        { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 }, // Layer 2
        { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 }  // Layer 3
    }
};

static const uint32_t samplerate[3][4] = {
    { 44100, 48000, 32000, 0 }, // MPEG 1
    { 22050, 24000, 16000, 0 }, // MPEG 2
    { 11025, 12000, 8000, 0 }  // MPEG 2.5
};


/* ---------------------------------------------------------------------------
 */
static void parse_header(mpa_header_t *mh, uint32_t header)
{
    mh->syncword = (header >> 20) & 0x0fff;

    mh->version = 2 - ((header >> 19) & 0x01);
    if ((mh->syncword & 0x01) == 0) {
        mh->version = 3;
    }

    mh->layer = 4 - ((header >> 17) & 0x03);
    if (mh->layer == 4) {
        mh->layer = 0;
    }

    mh->error_protection = ((header >> 16) & 0x01) ? 0 : 1;
    mh->bitrate_index = (header >> 12) & 0x0F;
    mh->samplerate_index = (header >> 10) & 0x03;
    mh->padding = (header >> 9) & 0x01;
    mh->extension = (header >> 8) & 0x01;
    mh->mode = (header >> 6) & 0x03;
    mh->mode_ext = (header >> 4) & 0x03;
    mh->copyright = (header >> 3) & 0x01;
    mh->original = (header >> 2) & 0x01;
    mh->emphasis = header & 0x03;

    if (mh->layer && mh->version) {
        mh->bitrate = bitrate[mh->version - 1][mh->layer - 1][mh->bitrate_index];
        mh->samplerate = samplerate[mh->version - 1][mh->samplerate_index];
    } else {
        mh->bitrate = 0;
        mh->samplerate = 0;
    }

    if (mh->mode == MPA_MODE_MONO) {
        mh->channels = 1;
    } else {
        mh->channels = 2;
    }

    if (mh->version == 1) {
        mh->samples = 1152;
    } else {
        mh->samples = 576;
    }

    if (mh->samplerate) {
        mh->framesize = (mh->samples * mh->bitrate * 1000 / mh->samplerate) / 8 + mh->padding;
    }
}

/* ---------------------------------------------------------------------------
 * concise informational string
 */
void mpa_header_print(ts2es_t *h_ts, mpa_header_t *mh)
{
    char *s_version;
    char *s_mode;
    if (mh->version == 1) {
        s_version = "MPEG-1";
    } else if (mh->version == 2) {
        s_version = "MPEG-2";
    } else if (mh->version == 3) {
        s_version = "MPEG-2.5";
    } else {
        s_version = "unknown";
    }

    if (mh->mode == MPA_MODE_STEREO) {
        s_mode = "Stereo";
    } else if (mh->mode == MPA_MODE_JOINT) {
        s_mode = "Joint Stereo";
    } else if (mh->mode == MPA_MODE_DUAL) {
        s_mode = "Dual";
    } else if (mh->mode == MPA_MODE_MONO) {
        s_mode = "Mono";
    } else {
        s_mode = "unknown";
    }

    ts2es_report(h_ts, TS2ES_INFO, "version %s,  mode %s", s_version, s_mode);
    ts2es_report(h_ts, TS2ES_INFO, "layer %d, %d kbps, %d Hz", mh->layer, mh->bitrate, mh->samplerate);

    // ts2es_report(h_ts, TS2ES_DEBUG, "error_protection=%d", mh->error_protection);
    // ts2es_report(h_ts, TS2ES_DEBUG, "padding=%d", mh->padding);
    // ts2es_report(h_ts, TS2ES_DEBUG, "extension=%d", mh->extension);
    // ts2es_report(h_ts, TS2ES_DEBUG, "mode_ext=%d", mh->mode_ext);
    // ts2es_report(h_ts, TS2ES_DEBUG, "copyright=%d", mh->copyright);
    // ts2es_report(h_ts, TS2ES_DEBUG, "original=%d", mh->original);
    // ts2es_report(h_ts, TS2ES_DEBUG, "channels=%d", mh->channels);
    // ts2es_report(h_ts, TS2ES_DEBUG, "bitrate=%d", mh->bitrate);
    // ts2es_report(h_ts, TS2ES_DEBUG, "samplerate=%d", mh->samplerate);
    // ts2es_report(h_ts, TS2ES_DEBUG, "samples=%d", mh->samples);
    // ts2es_report(h_ts, TS2ES_DEBUG, "framesize=%d", mh->framesize);
}


/* ---------------------------------------------------------------------------
 * Parse mpeg audio header
 * returns 1 if valid, or 0 if invalid
 */
int mpa_header_parse(ts2es_t *h_ts, const uint8_t *buf, mpa_header_t *mh)
{
    unsigned long head;

    /* Quick check */
    if (buf[0] != 0xFF) {
        return 0;
    }

    /* Put the first four bytes into an integer */
    head = ((uint32_t)buf[0] << 24) |
        ((uint32_t)buf[1] << 16) |
        ((uint32_t)buf[2] << 8) |
        ((uint32_t)buf[3]);


    /* fill out the header struct */
    parse_header(mh, head);

    /* check for syncword */
    if ((mh->syncword & 0x0ffe) != 0x0ffe) {
        return 0;
    }

    /* make sure layer is sane */
    if (mh->layer == 0) {
        return 0;
    }

    /* make sure version is sane */
    if (mh->version == 0) {
        return 0;
    }

    /* make sure bitrate is sane */
    if (mh->bitrate == 0) {
        return 0;
    }

    /* make sure samplerate is sane */
    if (mh->samplerate == 0) {
        return 0;
    }

    return 1;
}


