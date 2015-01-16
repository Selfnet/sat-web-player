#include <stdlib.h>
#include <libavutil/avconfig.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>
#include <SDL.h>
#include "list.h"

#define checkav(ret, msg, ...) if ((ret) < 0) {av_log (NULL, AV_LOG_ERROR, "%s:%i " msg ": %s\n", \
            __FILE__,__LINE__, ##__VA_ARGS__, av_err2str (ret)); goto err;}
#define check(ret, msg, ...) if (!(ret)) {fprintf (stderr, "%s:%i " msg "\n", \
            __FILE__,__LINE__, ##__VA_ARGS__); goto err;}

typedef struct {
    Uint8 *buf;
    int    buf_size;
    int    buf_idx;
    List  *pkt_list;
    AVCodecContext *dec_ctx;
    struct SwrContext *swr_ctx;
    AVFrame *src_frame;
} AudioState;

int
audio_decode (AudioState *as)
{
    AVPacket *pkt = NULL;
    int got_frame, ret = 0, seqno;

    if (!as->src_frame) {
        as->src_frame = av_frame_alloc ();
    } else {
        av_frame_unref (as->src_frame);
    }

    for (;;) {
        if (!pkt || pkt->size == 0) {
            List_take (as->pkt_list, (void **) &pkt, &seqno);
            if (!pkt) {
                fprintf (stderr, "[DEBUG] No audio packet left. %i\n", seqno);
                ret = -1;
                goto err;
            }
        }

        ret = avcodec_decode_audio4 (as->dec_ctx, as->src_frame, &got_frame, pkt);
        checkav (ret, "[ERROR] Failed to decode audio");

        pkt->size -= ret;
        pkt->data += ret;

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
            checkav (ret, "[ERROR] Failed to resample audio");

            //fprintf (stderr, "[DEBUG] Decoded frame with %i bytes.\n", as->buf_size);
            as->buf_idx = 0;

            if (pkt->size > 0) {
                // data left in the packet queue it up for the next call
                List_insert (as->pkt_list, (void *) pkt, 0);
                goto nofree;
            }
            break;
        }
    }

err:
    av_free_packet (pkt);
nofree:
    return ret;
}

void
audio_callback (void *userdata, Uint8 *stream, int len)
{
    AudioState *as = (AudioState *) userdata;
    //fprintf (stderr, "\n[DEBUG] Entering audio callback.\n\tlen: %i\n\tas->buf_size: %i\n\tas->buf_idx: %i\n",
            //len, as->buf_size, as->buf_idx);

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

/* Allocate input and output format contexts together with their respective
 * streams
 */
int
allocate_io (char *filename,
             AVFormatContext **ifmtx,
             AVFormatContext **ofmtx,
             AVStream  **aud_in_stream,
             AVStream  **vid_in_stream,
             AVStream **vid_out_stream)
{
    int vid_index = 0, aud_index = 0, ret = 0;

    AVCodec *aud_dec_codec = NULL,
            *vid_dec_codec = NULL, *enc_codec  = NULL;

    ret = avformat_open_input (ifmtx, filename, 0, 0);
    checkav (ret, "Failed to open input format context");
    ret = avformat_find_stream_info (*ifmtx, 0);
    checkav (ret, "Failed to retrieve stream info");

    av_dump_format (*ifmtx, 0, filename, 0);


    vid_index = av_find_best_stream (*ifmtx, AVMEDIA_TYPE_VIDEO, -1, -1, &vid_dec_codec, 0);
    checkav (vid_index, "Failed to select video stream");

    *vid_in_stream = (*ifmtx)->streams [vid_index];
    ret = avcodec_open2 ((*vid_in_stream)->codec, vid_dec_codec, NULL);
    checkav (ret, "Failed to open decoding context for stream #%i", vid_index);


    aud_index = av_find_best_stream (*ifmtx, AVMEDIA_TYPE_AUDIO, -1, -1, &aud_dec_codec, 0);
    checkav (aud_index, "Failed to select audio stream");

    *aud_in_stream = (*ifmtx)->streams [aud_index];
    (*aud_in_stream)->codec->channel_layout = 0;
    ret = avcodec_open2 ((*aud_in_stream)->codec, aud_dec_codec, NULL);
    checkav (ret, "Failed to open decoding context for stream #%i", aud_index);


    ret = avformat_alloc_output_context2 (ofmtx, NULL, "SDL", NULL);
    checkav (ret, "Failed to open output format context");

    enc_codec  = avcodec_find_encoder_by_name ("rawvideo");
    *vid_out_stream = avformat_new_stream (*ofmtx, enc_codec);

    (*vid_out_stream)->codec->width   = (*vid_in_stream)->codec->width;
    (*vid_out_stream)->codec->height  = (*vid_in_stream)->codec->height;
    (*vid_out_stream)->codec->pix_fmt = (*vid_in_stream)->codec->pix_fmt;

    ret = avcodec_open2 ((*vid_out_stream)->codec, enc_codec, NULL);
    checkav (ret, "Failed to open encoding context for rawvideo stream");

    av_dump_format (*ofmtx, 0, NULL, 1);

    return 0;

err:
    if (*vid_in_stream)  avcodec_close ((*vid_in_stream)->codec);
    if (*vid_out_stream) avcodec_close ((*vid_out_stream)->codec);
    if (*ifmtx) avformat_close_input (ifmtx);
    if (*ofmtx) avformat_free_context (*ofmtx);

    return ret;
}

/* Initialize SDL_AudioSpec and AudioState for use in audio_callback
 */
int
cfg_audio (SDL_AudioSpec *spec, AudioState *as, AVCodecContext *dec_ctx)
{
    int ret;

    as->pkt_list = List_new ();
    check (as->pkt_list, "Failed to allocate audio packet list.");

    as->dec_ctx = dec_ctx;
    as->swr_ctx = swr_alloc ();
    check (as->swr_ctx, "Failed to allocate audio resampling context.");

    av_opt_set_int (as->swr_ctx, "in_channel_layout", dec_ctx->channel_layout, 0);
    //av_opt_set_int (as->swr_ctx, "in_channel_count", dec_ctx->sample_rate, 0);
    av_opt_set_int (as->swr_ctx, "in_sample_rate", dec_ctx->sample_rate, 0);
    av_opt_set_sample_fmt (as->swr_ctx, "in_sample_fmt", dec_ctx->sample_fmt, 0);

    av_opt_set_int (as->swr_ctx, "out_channel_layout", dec_ctx->channel_layout, 0);
    //av_opt_set_int (as->swr_ctx, "out_channel_count", dec_ctx->sample_rate, 0);
    av_opt_set_int (as->swr_ctx, "out_sample_rate", dec_ctx->sample_rate, 0);
    av_opt_set_sample_fmt (as->swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    ret = swr_init (as->swr_ctx);
    checkav (ret, "Failed to initialize audio resampling context");

    spec->freq     = dec_ctx->sample_rate;
    spec->format   = AUDIO_S16SYS;
    spec->silence  = 0;
    spec->samples  = 1024;
    spec->channels = dec_ctx->channels;
    spec->callback = audio_callback;
    spec->userdata = (void *) as;

    return 0;

err:
    return ret;
}

/* Transcode a packet from dec_ctx->codec to enc_ctx->codec.
 * Assumes to_pkt to be initialized already, but will free to_pkt->data after
 * transcoding.
 * Might not be able to return a new packet right away and expects to be called
 * again with a new from_pkt. In that case *got_pkt will be set to 0 and
 * otherwise to 1.
 * Returns *got_pkt on success and ffmpeg's error code otherwise, however it
 * does log errors on its own.
 */
int
transcode_pkt (AVStream *in_stream,
               AVStream *out_stream,
               AVPacket *from_pkt,
               AVPacket *to_pkt,
               int *got_pkt)
{
    int out_idx = out_stream->index;

    AVCodecContext *dec_ctx =  in_stream->codec,
                   *enc_ctx = out_stream->codec;

    AVFrame *frame;
    int ret;

    frame = av_frame_alloc ();
    ret = avcodec_decode_video2 (dec_ctx, frame, got_pkt, from_pkt);
    if (ret < 0) {
        av_log (NULL, AV_LOG_ERROR, "Failed to decode frame: %s\n", av_err2str (ret));
        av_frame_free (&frame);
        return ret;
    }
    if (!*got_pkt) {
        av_frame_free (&frame);
        return 0;
    }

    ret = avcodec_encode_video2 (enc_ctx, to_pkt, frame, got_pkt);
    av_frame_free (&frame);
    if (ret < 0) {
        av_log (NULL, AV_LOG_ERROR, "Failed to encode frame: %s\n", av_err2str (ret));
        if (to_pkt->data) av_freep (to_pkt->data);
        return ret;
    }

    to_pkt->pts = av_rescale_q_rnd(from_pkt->pts, in_stream->time_base, out_stream->time_base,
                AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    to_pkt->dts = av_rescale_q_rnd(from_pkt->dts, in_stream->time_base, out_stream->time_base,
                AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    to_pkt->duration = av_rescale_q(from_pkt->duration, in_stream->time_base, out_stream->time_base);
    to_pkt->pos = -1;
    to_pkt->stream_index = out_idx;

    return 1;
}

// wrapper around av_frame_free, so we can use it with List_free
inline void
free_frame (void * frame)
{
    av_frame_free ( (AVFrame **) &frame );
}

int
main (int argc, char **argv)
{
    int ret = 0, got_pkt = 0, aframes = 0;
    char *filename;

    AVFormatContext *ifmtx = NULL, *ofmtx = NULL;
    AVStream *vid_in_stream = NULL, *vid_out_stream = NULL,
             *aud_in_stream = NULL;

    AVPacket pkt, npkt;

    AudioState as = { 0 };
    SDL_AudioSpec spec_wanted = { 0 }, spec_got = { 0 };

    List *audio_pkt_list = NULL;

    if (argc < 2) {
        fprintf (stderr, "USAGE: %s videofile\n", argv [0]);
        return -1;
    }

    av_register_all ();
    avdevice_register_all ();

    filename = argv [1];
    ret = allocate_io (filename, &ifmtx, &ofmtx, &aud_in_stream, &vid_in_stream, &vid_out_stream);
    if (ret < 0) goto err;

    ret = avformat_write_header(ofmtx, NULL);
    checkav (ret, "Failed to init SDL output device");

    if (SDL_WasInit (SDL_INIT_AUDIO) != 0) {
        ret = SDL_InitSubSystem (SDL_INIT_AUDIO);
        check (ret, "Failed to init SDL audio.");
    }

    ret = cfg_audio (&spec_wanted, &as, aud_in_stream->codec);
    check (ret == 0, "Failed to configure audio.");

    audio_pkt_list = as.pkt_list;

    ret = SDL_OpenAudio (&spec_wanted, &spec_got);
    SDL_PauseAudio (0);

    while (1) {
        av_init_packet (& pkt);
        ret = av_read_frame (ifmtx, &pkt);
        checkav (ret, "Failed to read next frame from input");

        if (pkt.stream_index == vid_in_stream->index) {
            npkt.size = 0;
            npkt.data = NULL;
            av_init_packet (&npkt);
            ret = transcode_pkt (vid_in_stream, vid_out_stream,
                                &pkt, &npkt, &got_pkt);
            if (ret <= 0) {
                av_free_packet (&npkt);
                if (ret == 0) continue;
                if (ret != 0) break;
            }

            ret = av_interleaved_write_frame (ofmtx, &npkt);
            checkav (ret, "Failed to write video frame to output");
            av_free_packet (&npkt);
        } else
        if (pkt.stream_index == aud_in_stream->index) {
            AVPacket *cpkt = calloc (1, sizeof (AVPacket));
            check (cpkt, "Failed to copy audio packet.");
            av_copy_packet (cpkt, &pkt);

            SDL_LockAudio ();
            List_append (audio_pkt_list, (void *) cpkt, aframes++);
            SDL_UnlockAudio ();
        }


        av_free_packet (&pkt);
    }

err:
    if (ifmtx) avformat_close_input (&ifmtx);
    if (ofmtx) {
        av_write_trailer (ofmtx);
        avformat_free_context (ofmtx);
    }
    if (audio_pkt_list) List_free (audio_pkt_list, free_frame);

    return ret;
}
