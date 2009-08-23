/*
 * General components used by playlist formats
 * Copyright (c) 2009 Geza Kovacs
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

/** @file libavformat/avplaylist.c
 *  @author Geza Kovacs ( gkovacs mit edu )
 *
 *  @brief General components used by playlist formats
 *
 *  @details These functions are used to initialize and manipulate playlists
 *  (AVPlaylistContext) and their individual playlist elements (PlayElem), each
 *  of which encapsulates its own AVFormatContext. This abstraction is used for
 *  implementing file concatenation and support for playlist formats.
 */

#include "libavformat/avplaylist.h"
#include "riff.h"
#include "libavutil/avstring.h"
#include "internal.h"
#include "concat.h"

AVFormatContext *av_playlist_alloc_formatcontext(char *filename)
{
    int err;
    AVFormatContext *ic = avformat_alloc_context();
    err = av_open_input_file(&(ic), filename, ic->iformat, 0, NULL);
    if (err < 0)
        av_log(ic, AV_LOG_ERROR, "Error during av_open_input_file\n");
    err = av_find_stream_info(ic);
    if (err < 0)
        av_log(ic, AV_LOG_ERROR, "Could not find stream info\n");
    return ic;
}

void av_playlist_populate_context(AVPlaylistContext *ctx, int pe_curidx)
{
    ctx->icl = av_realloc(ctx->icl, sizeof(*(ctx->icl)) * (pe_curidx+2));
    ctx->icl[pe_curidx+1] = NULL;
    ctx->icl[pe_curidx] = av_playlist_alloc_formatcontext(ctx->flist[pe_curidx]);
    ctx->nb_streams_list[pe_curidx] = ctx->icl[pe_curidx]->nb_streams;
}

void av_playlist_set_streams(AVFormatContext *s)
{
    int i;
    int offset;
    AVFormatContext *ic;
    AVPlaylistContext *ctx = s->priv_data;
    ic = ctx->icl[ctx->pe_curidx];
    offset = av_playlist_streams_offset_from_playidx(ctx, ctx->pe_curidx);
    ic->iformat->read_header(ic, NULL);
    for (i = 0; i < ic->nb_streams; ++i) {
        s->streams[offset + i] = ic->streams[i];
        ic->streams[i]->index += offset;
        if (!ic->streams[i]->codec->codec) {
            AVCodec *codec = avcodec_find_decoder(ic->streams[i]->codec->codec_id);
            if (!codec) {
                av_log(ic->streams[i]->codec, AV_LOG_ERROR, "Decoder (codec id %d) not found for input stream #%d\n",
                       ic->streams[i]->codec->codec_id, ic->streams[i]->index);
                return;
             }
             if (avcodec_open(ic->streams[i]->codec, codec) < 0) {
                av_log(ic->streams[i]->codec, AV_LOG_ERROR, "Error while opening decoder for input stream #%d\n",
                       ic->streams[i]->index);
                return;
             }
        }
    }
    s->nb_streams        = ic->nb_streams + offset;
    s->packet_buffer     = ic->packet_buffer;
    s->packet_buffer_end = ic->packet_buffer_end;
}

AVPlaylistContext *av_playlist_get_context(AVFormatContext *ic)
{
    if (ic && ic->iformat && ic->iformat->long_name && ic->priv_data &&
        !strncmp(ic->iformat->long_name, "CONCAT", 6))
        return ic->priv_data;
    else
        return NULL;
}

AVFormatContext *av_playlist_formatcontext_from_filelist(const char **flist, int len)
{
    AVPlaylistContext *ctx;
    AVFormatContext *ic;
    ctx = av_playlist_from_filelist(flist, len);
    if (!ctx) {
        av_log(NULL, AV_LOG_ERROR, "failed to create AVPlaylistContext in av_playlist_formatcontext_from_filelist\n");
        return NULL;
    }
    ic = avformat_alloc_context();
    ic->iformat = ff_concat_alloc_demuxer();
    ic->priv_data = ctx;
    return ic;
}

void av_playlist_split_encodedstring(const char *s,
                                     const char sep,
                                     char ***flist_ptr,
                                     int *len_ptr)
{
    char c, *ts, **flist;
    int i, len, buflen, *sepidx;
    sepidx = NULL;
    buflen = len = 0;
    sepidx = av_fast_realloc(sepidx, &buflen, ++len);
    sepidx[0] = 0;
    ts = s;
    while ((c = *ts++) != 0) {
        if (c == sep) {
            sepidx[len] = ts-s;
            sepidx = av_fast_realloc(sepidx, &buflen, ++len);
            if (!sepidx) {
                av_log(NULL, AV_LOG_ERROR, "av_fast_realloc error in av_playlist_split_encodedstring\n");
                continue;
            }
        }
    }
    sepidx[len] = ts-s;
    ts = s;
    *len_ptr = len;
    *flist_ptr = flist = av_malloc(sizeof(*flist) * (len+1));
    flist[len] = 0;
    for (i = 0; i < len; ++i) {
        flist[i] = av_malloc(sepidx[i+1]-sepidx[i]);
        if (!flist[i]) {
            av_log(NULL, AV_LOG_ERROR, "av_malloc error in av_playlist_split_encodedstring\n");
            continue;
        }
        av_strlcpy(flist[i], ts+sepidx[i], sepidx[i+1]-sepidx[i]);
    }
    av_free(sepidx);
}

AVPlaylistContext *av_playlist_from_filelist(const char **flist, int len)
{
    int i;
    AVPlaylistContext *ctx;
    ctx = av_mallocz(sizeof(*ctx));
    if (!ctx) {
        av_log(NULL, AV_LOG_ERROR, "av_mallocz error in av_playlist_from_encodedstring\n");
        return NULL;
    }
    for (i = 0; i < len; ++i)
        av_playlist_add_path(ctx, flist[i]);
    return ctx;
}

AVPlaylistContext *av_playlist_from_encodedstring(const char *s, const char sep)
{
    AVPlaylistContext *ctx;
    char **flist;
    int i, len;
    av_playlist_split_encodedstring(s, sep, &flist, &len);
    if (len <= 1) {
        for (i = 0; i < len; ++i)
            av_free(flist[i]);
        av_free(flist);
        return NULL;
    }
    ctx = av_playlist_from_filelist(flist, len);
    av_free(flist);
    return ctx;
}

void av_playlist_add_path(AVPlaylistContext *ctx, const char *itempath)
{
    ctx->flist = av_realloc(ctx->flist, sizeof(*(ctx->flist)) * (++ctx->pelist_size+1));
    ctx->flist[ctx->pelist_size] = NULL;
    ctx->flist[ctx->pelist_size-1] = itempath;
    ctx->durations = av_realloc(ctx->durations,
                                sizeof(*(ctx->durations)) * (ctx->pelist_size+1));
    ctx->durations[ctx->pelist_size] = 0;
    ctx->nb_streams_list = av_realloc(ctx->nb_streams_list,
                                      sizeof(*(ctx->nb_streams_list)) * (ctx->pelist_size+1));
    ctx->nb_streams_list[ctx->pelist_size] = 0;
}

void av_playlist_relative_paths(char **flist,
                                int len,
                                const char *workingdir)
{
    int i;
    for (i = 0; i < len; ++i) { // determine if relative paths
        char *fullfpath;
        int wdslen = strlen(workingdir);
        int flslen = strlen(flist[i]);
        fullfpath = av_malloc(wdslen+flslen+2);
        av_strlcpy(fullfpath, workingdir, wdslen+1);
        fullfpath[wdslen] = '/';
        fullfpath[wdslen+1] = 0;
        av_strlcat(fullfpath, flist[i], wdslen+flslen+2);
        if (url_exist(fullfpath))
            flist[i] = fullfpath;
    }
}

int64_t av_playlist_time_offset(const int64_t *durations, int pe_curidx)
{
    int i;
    int64_t total = 0;
    for (i = 0; i < pe_curidx; ++i) {
        total += durations[i];
    }
    return total;
}

int av_playlist_stream_index_from_time(AVPlaylistContext *ctx,
                                       int64_t pts,
                                       int64_t *localpts)
{
    int i;
    int64_t total;
    i = total = 0;
    while (pts >= total) {
        if (i >= ctx->pelist_size)
            break;
        total += ctx->durations[i++];
    }
    if (localpts)
        *localpts = pts-(total-ctx->durations[i-1]);
    return i;
}

int av_playlist_playidx_from_streamidx(AVPlaylistContext *ctx, int stream_index)
{
    int i, total;
    i = total = 0;
    while (stream_index >= total)
        total += ctx->nb_streams_list[i++];
    return i-1;
}

int av_playlist_localstidx_from_streamidx(AVPlaylistContext *ctx, int stream_index)
{
    int i, total;
    i = total = 0;
    while (stream_index >= total)
        total += ctx->nb_streams_list[i++];
    return stream_index - (total - ctx->nb_streams_list[i-1]);
}

int av_playlist_streams_offset_from_playidx(AVPlaylistContext *ctx, int playidx)
{
    int i, total;
    i = total = 0;
    while (playidx > i)
        total += ctx->nb_streams_list[i++];
    return total;
}
