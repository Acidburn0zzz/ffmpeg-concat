/*
 * M3U muxer and demuxer
 * Copyright (c) 2001 Geza Kovacs
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * Based on AU muxer and demuxer in au.c
 */

#include "avformat.h"
#include "raw.h"
#include "riff.h"

/* if we don't know the size in advance */
#define PLAYLIST_UNKNOWN_SIZE ((uint32_t)(~0))

/* The ffmpeg codecs we support, and the IDs they have in the file */
static const AVCodecTag codec_playlist_tags[] = {
    { CODEC_ID_PCM_MULAW, 1 },
    { CODEC_ID_PCM_S8, 2 },
    { CODEC_ID_PCM_S16BE, 3 },
    { CODEC_ID_PCM_S24BE, 4 },
    { CODEC_ID_PCM_S32BE, 5 },
    { CODEC_ID_PCM_F32BE, 6 },
    { CODEC_ID_PCM_F64BE, 7 },
    { CODEC_ID_PCM_ALAW, 27 },
    { 0, 0 },
};

typedef struct PlayElem {
    AVFormatContext *ic;
    char *filename;
    AVInputFormat *fmt;
    int buf_size;
    AVFormatParameters *ap;
} PlayElem;

typedef struct PlaylistD {
    PlayElem **pelist;
    int pelist_size;
    int pe_curidx;
} PlaylistD;

static int av_open_input_playelem(PlayElem *pe)
{
    return av_open_input_file(&(pe->ic), pe->filename, pe->fmt, pe->buf_size, pe->ap);
}

// based on decode_thread() in ffplay.c
static int av_alloc_playelem(unsigned char *filename, PlayElem *pe)
{
//    AVProbeData *pd;
    AVFormatContext *ic;
    AVFormatParameters *ap;
//    pd = av_malloc(sizeof(AVProbeData));
//    ic = av_malloc(sizeof(AVFormatContext));
    ap = av_malloc(sizeof(AVFormatParameters));
    memset(ap, 0, sizeof(AVFormatParameters));
    ap->width = 0;
    ap->height = 0;
    ap->time_base = (AVRational){1, 25};
    ap->pix_fmt = 0;
//    pd->filename = filename;
//    pd->buf = NULL;
//    pd->buf_size = 0;
    printf("survived av_alloc_playelem 0\n");
    fflush(stdout);
    pe->ic = ic;
    pe->filename = filename;
    pe->fmt = 0;
    printf("survived av_alloc_playelem 1\n");
    fflush(stdout);
    pe->buf_size = 0;
    pe->ap = ap;
    printf("survived av_alloc_playelem 2\n");
    fflush(stdout);
//    pe->fmt = pe->ic->iformat;
    printf("survived av_alloc_playelem 3\n");
    fflush(stdout);
    return 0;
}

static PlayElem* av_make_playelem(unsigned char *filename)
{
    int err;
    PlayElem *pe = av_malloc(sizeof(PlayElem));
    printf("survived av_make_playelem -1\n");
    fflush(stdout);
    err = av_alloc_playelem(filename, pe);
    if (err < 0)
        print_error("during-av_alloc_playelem", err);
    printf("survived av_make_playelem 0\n");
    fflush(stdout);
    err = av_open_input_playelem(pe);
    if (err < 0)
        print_error("during-open_input_playelem", err);
    printf("survived av_make_playelem 1\n");
    fflush(stdout);
    pe->fmt = pe->ic->iformat;
    if (pe->fmt != 0)
    {
        printf("pefmt set\n");
    }
    else
    {
        printf("pefmt not set\n");
    }
    fflush(stdout);
    err = av_find_stream_info(pe->ic);
    if (err < 0)
    {
        fprintf(stderr, "failed codec probe av_find_stream_info");
        fflush(stderr);
    }
    if(pe->ic->pb)
    {
        pe->ic->pb->eof_reached = 0;
    }
    else
    {
        fprintf(stderr, "failed pe ic pb not set");
        fflush(stderr);
    }
    if(!pe->fmt)
    {
        fprintf(stderr, "failed pe ic fmt not set");
        fflush(stderr);
    }
    printf("survived av_make_playelem 2\n");
    fflush(stdout);
    return pe;
}

static PlaylistD* av_make_playlistd(unsigned char **flist, int flist_len)
{
    int i;
    PlaylistD *playld = av_malloc(sizeof(PlaylistD));
    playld->pe_curidx = 0;
    playld->pelist_size = flist_len;
    for (i = 0; i < playld->pelist_size; ++i)
    {
        printf(flist[i]);
        putchar('\n');
        fflush(stdout);
    }
    printf("playlistd 0\n");
    fflush(stdout);
    playld->pelist = av_malloc(playld->pelist_size * sizeof(PlayElem*));
    memset(playld->pelist, 0, playld->pelist_size * sizeof(PlayElem*));
    printf("playlistd 1 flist is %s\n", flist[0]);
    fflush(stdout);
    for (int i = 0; i < playld->pelist_size; ++i)
    {
        playld->pelist[i] = av_make_playelem(flist[i]);
    }
    printf("playlistd 2\n");
    fflush(stdout);
    return playld;
}

#if CONFIG_PLAYLIST_MUXER
/* AUDIO_FILE header */
static int put_playlist_header(ByteIOContext *pb, AVCodecContext *enc)
{
    if(!enc->codec_tag)
        return -1;
    put_tag(pb, ".snd");       /* magic number */
    put_be32(pb, 24);           /* header size */
    put_be32(pb, PLAYLIST_UNKNOWN_SIZE); /* data size */
    put_be32(pb, (uint32_t)enc->codec_tag);     /* codec ID */
    put_be32(pb, enc->sample_rate);
    put_be32(pb, (uint32_t)enc->channels);
    return 0;
}

static int playlist_write_header(AVFormatContext *s)
{
    ByteIOContext *pb = s->pb;

    s->priv_data = NULL;

    /* format header */
    if (put_playlist_header(pb, s->streams[0]->codec) < 0) {
        return -1;
    }

    put_flush_packet(pb);

    return 0;
}

static int playlist_write_packet(AVFormatContext *s, AVPacket *pkt)
{
    ByteIOContext *pb = s->pb;
    put_buffer(pb, pkt->data, pkt->size);
    return 0;
}

static int playlist_write_trailer(AVFormatContext *s)
{
    ByteIOContext *pb = s->pb;
    int64_t file_size;

    if (!url_is_streamed(s->pb)) {

        /* update file size */
        file_size = url_ftell(pb);
        url_fseek(pb, 8, SEEK_SET);
        put_be32(pb, (uint32_t)(file_size - 24));
        url_fseek(pb, file_size, SEEK_SET);

        put_flush_packet(pb);
    }

    return 0;
}
#endif /* CONFIG_PLAYLIST_MUXER */

static int check_file_extn(char *cch, char *extn)
{
    int pos;
    int extnl;
    pos = -1;
    extnl = 0;
    while (extn[extnl] != 0)
       ++extnl;
    printf("extnl is %i", extnl);
    fflush(stdout);
    pos = -1;
    if (!cch)
    {
        printf("filename not provided, null pointer\n");
        return 0;
    }
    if (*cch == 0)
    {
        printf("filename not provided\n");
        return 0;
    }
    while (*cch != 0)
    {
        if (*cch == '.')
        {
            pos = 0;
        }
        else if (pos >= 0)
        {
            if (*cch == extn[pos])
                ++pos;
            else
                pos = -1;
            if (pos == extnl)
            {
                printf("correct");
                return 1;
            }
        }
        ++cch;
    }
    printf("incorrect");
    return 0;
}

static int compare_bufs(unsigned char *buf, unsigned char *rbuf)
{
    while (*rbuf != 0)
    {
        if (*rbuf != *buf)
            return 0;
        ++buf;
        ++rbuf;
    }
    printf("buffer matched\n");
    return 1;
}

static int playlist_probe(AVProbeData *p)
{
    printf("buffer size %i\n", p->buf_size);
    if (p->buf_size >= 7 && p->buf != 0)
    {
        if (compare_bufs(p->buf, "#EXTM3U"))
            return AVPROBE_SCORE_MAX;
        else
            printf("buffer did not match\n");
    }
    if (check_file_extn(p->filename, "m3u"))
        return AVPROBE_SCORE_MAX;
    else
        return 0;

    /* check file header */
//    if (p->buf[0] == '.' && p->buf[1] == 's' &&
//        p->buf[2] == 'n' && p->buf[3] == 'd')
//        return AVPROBE_SCORE_MAX;
//    else
//        return 0;
}

static int playlist_list_files(unsigned char *buffer, int buffer_size, unsigned char **file_list, unsigned int *lfx_ptr)
{
    unsigned int i;
    unsigned int lfx = 0;
    unsigned int fx = 0;
    unsigned char hashed_out = 0;
    unsigned char fldata[262144];
    memset(fldata, 0, 262144);
    memset(file_list, 0, 512 * sizeof(unsigned char*));
    for (i = 0; i < 512; ++i)
    {
        file_list[i] = fldata+i*512;
    }
    for (i = 0; i < buffer_size; ++i)
    {
        if (buffer[i] == 0)
            break;
        if (buffer[i] == '#')
        {
            hashed_out = 1;
            continue;
        }
        if (buffer[i] == '\n')
        {
            hashed_out = 0;
            if (fx != 0)
            {
                fx = 0;
                ++lfx;
            }
            continue;
        }
        if (hashed_out)
            continue;
        file_list[lfx][fx] = buffer[i];
        ++fx;
    }
    *lfx_ptr = lfx;
    return 0;
}

static int playlist_populate_context(PlaylistD *playld, AVFormatContext *s)
{
    int i;
    AVFormatContext *ic = playld->pelist[playld->pe_curidx]->ic;
    AVFormatParameters *nap = playld->pelist[playld->pe_curidx]->ap;
    ic->iformat->read_header(ic, nap);
    s->nb_streams = ic->nb_streams;
    for (i = 0; i < ic->nb_streams; ++i)
    {
        s->streams[i] = ic->streams[i];
    }
    return 0;
}

/* playlist input */
static int playlist_read_header(AVFormatContext *s,
                          AVFormatParameters *ap)
{
    unsigned char *flist[512];
    int flist_len;
    ByteIOContext *pb;
    PlaylistD *playld;
    pb = s->pb;
    playlist_list_files(pb->buffer, pb->buffer_size, flist, &flist_len);
    playld = av_make_playlistd(flist, flist_len);
    s->priv_data = playld;
    playlist_populate_context(playld, s);
    return 0;

    int size;
    unsigned int i;
    unsigned int tag;
    unsigned int id, channels, rate;
    enum CodecID codec;
    AVStream *st;
    /* check ".snd" header */
    tag = get_le32(pb);
    if (tag != MKTAG('.', 's', 'n', 'd'))
        return -1;
    size = get_be32(pb); /* header size */
    get_be32(pb); /* data size */

    id = get_be32(pb);
    rate = get_be32(pb);
    channels = get_be32(pb);

    codec = codec_get_id(codec_playlist_tags, id);

    if (size >= 24) {
        /* skip unused data */
        url_fseek(pb, size - 24, SEEK_CUR);
    }

    /* now we are ready: build format streams */
    st = av_new_stream(s, 0);
    if (!st)
        return -1;
    st->codec->codec_type = CODEC_TYPE_AUDIO;
    st->codec->codec_tag = id;
    st->codec->codec_id = codec;
    st->codec->channels = channels;
    st->codec->sample_rate = rate;
    av_set_pts_info(st, 64, 1, rate);
    return 0;
}

#define MAX_SIZE 4096

static int playlist_read_packet(AVFormatContext *s,
                          AVPacket *pkt)
{
    int ret;
    PlaylistD *playld;
    AVFormatContext *ic;
    playld = s->priv_data;
    retr:
    ic = playld->pelist[playld->pe_curidx]->ic;
    ret = ic->iformat->read_packet(ic, pkt);
    if (ret < 0 && playld->pe_curidx < playld->pelist_size - 1)
    {
        ++playld->pe_curidx;
        // TODO clear all existing streams before repopulating
        playlist_populate_context(playld, s);
        goto retr;
    }
    return ret;

    if (url_feof(s->pb))
        return AVERROR(EIO);
    ret= av_get_packet(s->pb, pkt, MAX_SIZE);
    if (ret < 0)
        return AVERROR(EIO);
    pkt->stream_index = 0;

    /* note: we need to modify the packet size here to handle the last
       packet */
    pkt->size = ret;
    return 0;
}

#if CONFIG_PLAYLIST_DEMUXER
AVInputFormat playlist_demuxer = {
    "playlist",
    NULL_IF_CONFIG_SMALL("PLAYLIST format"),
    0,
    playlist_probe,
    playlist_read_header,
    playlist_read_packet,
    NULL,
    pcm_read_seek,
    .codec_tag= (const AVCodecTag* const []){codec_playlist_tags, 0},
};
#endif

#if CONFIG_PLAYLIST_MUXER
AVOutputFormat playlist_muxer = {
    "playlist",
    NULL_IF_CONFIG_SMALL("PLAYLIST format"),
    "audio/x-mpegurl",
    "m3u",
    0,
    CODEC_ID_PCM_S16BE,
    CODEC_ID_NONE,
    playlist_write_header,
    playlist_write_packet,
    playlist_write_trailer,
    .codec_tag= (const AVCodecTag* const []){codec_playlist_tags, 0},
};
#endif //CONFIG_PLAYLIST_MUXER
