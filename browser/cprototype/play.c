/*
 * Copyright (c) 2014 Stefano Sabatini
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/**
 * @file
 * libavformat AVIOContext API example.
 *
 * Make libavformat demuxer access media content through a custom
 * AVIOContext read callback.
 * @example avio_reading.c
 */
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/file.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>

#include <SDL.h>
#ifdef AUDIO
#include <libswresample/swresample.h>
#include <libavutil/samplefmt.h>
#endif

struct ListElem;
typedef struct ListElem {
    int seqno;
    void *data;
    struct ListElem *next;
} ListElem;

typedef struct {
    ListElem *first;
    ListElem *last;
} List;

List *List_new (void)
{
    List *l = calloc (1, sizeof (List));
    return l;
}

ListElem *List_newelem (void * data, int seqno)
{
    ListElem *el = calloc (1, sizeof (ListElem));
    if (el) {
        el->data = data;
        el->seqno = seqno;
    }

    return el;
}

// wrapper around av_frame_free, so we can use it with List_free
inline void free_frame (void * frame)
{
    av_frame_free ( (AVFrame **) &frame );
}

typedef void (*List_free_data) (void *);
void List_free (List *l, List_free_data free_d)
{
    if (!l) return;

    ListElem *el, *o = NULL;
    for (el = l->first; el != l->last; el = el->next) {
        if (free_d) free_d (el->data);
        free (o);
        o = el;
    }
    if (free_d) free_d (l->last->data);
    free (l->last);

    free (l);
}

void List_append (List *l, void *data, int no)
{
    if (!l) return;

    ListElem *el = List_newelem (data, no);

    if (l->first == NULL) {
        l->first = el;
    } else {
        l->last->next = el;
    }
    l->last = el;
}

void List_insert (List *l, void *data, int no)
{
    if (!l) return;

    ListElem *el = List_newelem (data, no);
    if (l->first == NULL) {
        l->last = el;
    }  else {
        el->next = l->first;
    }
    l->first = el;
}

void List_take (List *l, void **data, int *seqno)
{
    if (!l || !l->first) return;

    ListElem *el = l->first;
    if (l->first == l->last) {
        l->last = NULL;
    }
    l->first = el->next;

    *data = el->data;
    if (seqno) *seqno = el->seqno;

    free (el);
}

void paint_frame (SDL_Overlay *overlay, AVFrame *frame)
{
    // more or less copy pasta from ffplay.c
    SDL_Rect drect = {0, 0, 640, 480};
    AVPicture pict = { { 0 } };

    SDL_LockYUVOverlay (overlay);
    // sets pointer for pixel buffers (one plane for each colour)
    pict.data [0] = overlay->pixels [0];
    pict.data [1] = overlay->pixels [2];
    pict.data [2] = overlay->pixels [1];

    pict.linesize [0] = overlay->pitches [0];
    pict.linesize [1] = overlay->pitches [2];
    pict.linesize [2] = overlay->pitches [1];

    av_picture_copy (&pict, (AVPicture *) frame, frame->format, overlay->w, overlay->h);
    SDL_UnlockYUVOverlay (overlay);
    SDL_DisplayYUVOverlay (overlay, &drect);
}

#ifdef AUDIO
typedef struct {
    Uint8 *buf;
    int    buf_size;
    int    buf_idx;
    List  *pkg_list;
    AVCodecContext *dec_ctx;
    struct SwrContext *swr_ctx;
    AVFrame *src_frame;
} AudioState;

int audio_decode (AudioState *as)
{
    AVPacket *pkg = NULL;
    int got_frame, ret = 0, seqno;

    if (!as->src_frame) {
        as->src_frame = av_frame_alloc ();
    } else {
        av_frame_unref (as->src_frame);
    }

    for (;;) {
        if (!pkg || pkg->size == 0) {
            List_take (as->pkg_list, (void **) &pkg, &seqno);
            if (!pkg) {
                fprintf (stderr, "[DEBUG] No audio packet left. %i\n", seqno);
                ret = -1;
                goto end;
            }
        }

        ret = avcodec_decode_audio4 (as->dec_ctx, as->src_frame, &got_frame, pkg);
        if (ret < 0) {
            fprintf (stderr, "[ERROR] Failed to decode audio: %s.\n", av_err2str (ret));
            goto end;
        }

        pkg->size -= ret;
        pkg->data += ret;

        if (got_frame) {
            av_freep (&as->buf);
            av_samples_alloc (&as->buf, NULL,
                    as->src_frame->channels,
                    as->src_frame->nb_samples,
                    av_get_packed_sample_fmt (as->src_frame->format), 0
            );
            as->buf_size = av_samples_get_buffer_size (NULL,
                    as->src_frame->channels,
                    as->src_frame->nb_samples,
                    av_get_packed_sample_fmt (as->src_frame->format), 0
            );

            ret = swr_convert (as->swr_ctx, &as->buf, as->buf_size,
                    as->src_frame->data, as->src_frame->nb_samples);
            if (ret < 0) {
                fprintf (stderr, "[ERROR] Failed to resample audio: %s.\n", av_err2str (ret));
            }
            fprintf (stderr, "[DEBUG] Decoded frame with %i bytes.\n", as->buf_size);
            as->buf_idx = 0;

            if (pkg->size > 0) {
                // data left in the packet queue it up for the next call
                List_insert (as->pkg_list, (void *) pkg, 0);
                goto nofree;
            }
            break;
        }
    }

end:
    av_free_packet (pkg);
nofree:
    return ret;
}

void audio_callback (void *userdata, Uint8 *stream, int len)
{
    AudioState *as = (AudioState *) userdata;
    fprintf (stderr, "\n[DEBUG] Entering audio callback.\n\tlen: %i\n\tas->buf_size: %i\n\tas->buf_idx: %i\n",
            len, as->buf_size, as->buf_idx);

    int ret, len1;

    while (len > 0) {
        if (as->buf_idx >= as->buf_size) {
            // as->buf is exhausted
            ret = audio_decode (as);
            if (ret < 0) {
                goto err;
            }
        }

        len1 = FFMIN (len, as->buf_size - as->buf_idx);
        memcpy (stream, as->buf + as->buf_idx, len1);
        as->buf_idx += len1;
        stream += len1;
        len -= len1;
    }

    return;
err:
    fprintf (stderr, "[DEBUG] Outputing silence.\n");
    memset (stream, 0, len);
}
#endif

struct buffer_data {
    uint8_t *ptr;
    size_t size; ///< size left in the buffer
};

static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    struct buffer_data *bd = (struct buffer_data *)opaque;
    buf_size = FFMIN(buf_size, bd->size);
    //printf("ptr:%p size:%zu\n", bd->ptr, bd->size);
    /* copy internal buffer data to buf */
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr  += buf_size;
    bd->size -= buf_size;
    return buf_size;
}

int main(int argc, char *argv[])
{
    AVFormatContext *fmt_ctx = NULL;
    AVIOContext *avio_ctx = NULL;
    uint8_t *buffer = NULL, *avio_ctx_buffer = NULL;
    size_t buffer_size, avio_ctx_buffer_size = 4096;
    char *input_filename = NULL;
    int ret = 0;
    struct buffer_data bd = { 0 };

    int video_stream_idx = -1, audio_stream_idx = -1;
    AVStream *video_stream = NULL, *audio_stream = NULL;
    AVCodec *video_dec = NULL, *audio_dec = NULL;
    AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx = NULL;
    AVPacket pkg;
    AVFrame *frame = NULL;
    int got_frame;

    av_init_packet (&pkg);
    pkg.data = NULL;
    pkg.size = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: %s input_file\n"
                "API example program to show how to read from a custom buffer "
                "accessed through AVIOContext.\n", argv[0]);
        return 1;
    }

    input_filename = argv[1];
    /* register codecs and formats and other lavf/lavc components*/
    av_register_all();
    /* slurp file content into buffer */
    ret = av_file_map(input_filename, &buffer, &buffer_size, 0, NULL);
    if (ret < 0)
        goto end;
    /* fill opaque structure used by the AVIOContext read callback */
    bd.ptr  = buffer;
    bd.size = buffer_size;
    if (!(fmt_ctx = avformat_alloc_context())) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    avio_ctx_buffer = av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
                                  0, &bd, &read_packet, NULL, NULL);
    if (!avio_ctx) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    fmt_ctx->pb = avio_ctx;
    ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open input\n");
        goto end;
    }
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream information\n");
        goto end;
    }
    av_dump_format(fmt_ctx, 0, input_filename, 0);

    // my code from here on

    video_stream_idx = av_find_best_stream (fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream_idx < 0) {
        fprintf (stderr, "[ERROR] Didn't find a video stream.\n");
        goto end;
    }

    video_stream  = fmt_ctx->streams [video_stream_idx];
    video_dec_ctx = video_stream->codec;
    video_dec     = avcodec_find_decoder (video_dec_ctx->codec_id);
    if (video_dec == NULL) {
        fprintf (stderr, "[ERROR] Couldn't find a codec for chosen video stream.\n");
        goto end;
    }

    if (avcodec_open2 (video_dec_ctx, video_dec, NULL) < 0) {
        fprintf (stderr, "[ERROR] Couldn't init video codec.\n");
        goto end;
    }
    fprintf (stderr, "[DEBUG] Opened a %s video stream.\n", video_dec->long_name);

#ifdef AUDIO
    audio_stream_idx = av_find_best_stream (fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_stream_idx <0) {
        fprintf (stderr, "[ERROR] Didn't find a audio stream.\n");
        goto end;
    }

    audio_stream  = fmt_ctx->streams [audio_stream_idx];
    audio_dec_ctx = audio_stream->codec;
    audio_dec     = avcodec_find_decoder (audio_dec_ctx->codec_id);
    if (audio_dec == NULL) {
        fprintf (stderr, "[ERROR] Couldn't find a codec for chosen audio stream.\n");
        goto end;
    }

    if (avcodec_open2 (audio_dec_ctx, audio_dec, NULL) < 0) {
        fprintf (stderr, "[ERROR] Couldn't init audio codec.\n");
        goto end;
    }

    fprintf (stderr, "[DEBUG] Opened a %s audio stream.\n", audio_dec->long_name);
#endif

    SDL_Overlay *overlay = NULL;
    SDL_Surface *surface = NULL;
    SDL_Init (SDL_INIT_VIDEO
#ifdef AUDIO
            | SDL_INIT_AUDIO
#endif
            );
    surface = SDL_SetVideoMode (640, 480, 0, 0);
    overlay = SDL_CreateYUVOverlay (video_dec_ctx->width, video_dec_ctx->height, SDL_YV12_OVERLAY, surface);

    List *video_list = List_new ();
#ifdef AUDIO
    List *audio_list = List_new ();
    if (!video_list || !audio_list) {
        fprintf (stderr, "[ERROR] Can't alloc frame lists.\n");
        goto end;
    }
#else
    if (!video_list) {
        fprintf (stderr, "[ERROR] Can't alloc frame list.\n");
        goto end;
    }
#endif

#ifdef AUDIO
    AudioState as = { 0 };
    as.swr_ctx   = swr_alloc ();
    if (!as.swr_ctx) {
        fprintf (stderr, "[ERROR] Failed to allocate resampling context.\n");
        goto end;
    }
    as.dec_ctx   = audio_dec_ctx;
    as.pkg_list  = audio_list;

    av_opt_set_int (as.swr_ctx, "in_channel_layout", audio_dec_ctx->channel_layout, 0);
    av_opt_set_int (as.swr_ctx, "in_sample_rate", audio_dec_ctx->sample_rate, 0);
    av_opt_set_sample_fmt (as.swr_ctx, "in_sample_fmt", audio_dec_ctx->sample_fmt, 0);

    av_opt_set_int (as.swr_ctx, "out_channel_layout", audio_dec_ctx->channel_layout, 0);
    av_opt_set_int (as.swr_ctx, "out_sample_rate", audio_dec_ctx->sample_rate, 0);
    av_opt_set_sample_fmt (as.swr_ctx, "out_sample_fmt",
            av_get_packed_sample_fmt (audio_dec_ctx->sample_fmt), 0);

    ret = swr_init (as.swr_ctx);
    if (ret < 0) {
        fprintf (stderr, "[ERROR] Failed to init resampling context.\n");
    }

    SDL_AudioSpec audio_spec = { 0 };
    SDL_AudioSpec audio_got  = { 0 };
    audio_spec.freq     = audio_dec_ctx->sample_rate;
    audio_spec.format   = AUDIO_S16SYS;
    audio_spec.channels = audio_dec_ctx->channels;
    audio_spec.silence  = 0;
    audio_spec.samples  = 1024;
    audio_spec.callback = audio_callback;
    audio_spec.userdata = &as;

    if (SDL_OpenAudio (&audio_spec, &audio_got) > 0) {
        fprintf (stderr, "[ERROR] Can't open SDL Audio: %s", SDL_GetError ());
        goto end;
    }

    fprintf (stderr, "[DEBUG] channels: %i (%i wanted)\n", audio_got.channels, audio_spec.channels);
    fprintf (stderr, "[DEBUG] freq: %i (%i wanted)\n", audio_got.freq, audio_spec.freq);
    fprintf (stderr, "[DEBUG] samples: %i (%i wanted)\n", audio_got.samples, audio_spec.samples);
    fprintf (stderr, "[DEBUG] format: %i (%i wanted)\n", audio_got.format, audio_spec.format);
    SDL_PauseAudio (0);
#endif

    int64_t last_update = 0;
    int aframes = 0,
        vframes = 0,
        done = 0;
    frame = av_frame_alloc ();
    while (av_read_frame (fmt_ctx, &pkg) == 0 && !done) {

        if (pkg.stream_index == video_stream_idx) {

            if ( (ret = avcodec_decode_video2 (video_dec_ctx, frame, &got_frame, &pkg)) < 0) {
                fprintf (stderr, "[ERROR] Failed to decode video frame: %s", av_err2str (ret));
                goto end;
            }

            if (got_frame) {
                List_append (video_list, (void *) frame, vframes++);
                fprintf (stderr, "v");
                frame = av_frame_alloc ();
                got_frame = 0;
            }

        }
#ifdef AUDIO
        else if (pkg.stream_index == audio_stream_idx) {

            AVPacket *cpkg = calloc (1, sizeof (AVPacket));
            if (cpkg) {
                av_copy_packet (cpkg, &pkg);
                List_append (audio_list, (void *) cpkg, aframes++);
                fprintf (stderr, "a");
            }
        }
#endif

        av_free_packet (&pkg);

        if ( (av_gettime () - last_update) >= 1000000. / 24 ) {
            AVFrame *dframe = NULL;
            int seqno = -1;

            List_take (video_list, (void **) &dframe, &seqno);

            if (dframe != NULL) {
                fprintf (stderr, "\n[DEBUG] Painting frame no. %i.\n", seqno);
                paint_frame (overlay, dframe);
                av_frame_free (&dframe);

                last_update = av_gettime ();
            }
        }

        SDL_Event event;
        while (SDL_PollEvent (&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    done = 1;
                    break;
            }
        }
    }

end:

    List_free (video_list, free_frame);
#ifdef AUDIO
    List_free (audio_list, (void (*) (void *))av_free_packet);
#endif

    av_free_packet (&pkg);
    av_frame_free (&frame);
    avcodec_close (video_dec_ctx);
    avformat_close_input(&fmt_ctx);
    /* note: the internal buffer could have changed, and be != avio_ctx_buffer */
    if (avio_ctx) {
        av_freep(&avio_ctx->buffer);
        av_freep(&avio_ctx);
    }
    av_file_unmap(buffer, buffer_size);
    if (ret < 0) {
        fprintf(stderr, "[ERROR] %s\n", av_err2str(ret));
        return 1;
    }

    SDL_FreeYUVOverlay (overlay);
    SDL_FreeSurface (surface);
    SDL_Quit ();
    return 0;
}
