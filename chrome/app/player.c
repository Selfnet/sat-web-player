#include "boiler.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/c/ppb_var_array.h"
#include "ppapi/c/ppb_var_array_buffer.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"

#include <pthread.h>
#include <string.h>

#include "list.h"
#include "libavutil/avutil.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"

#define min(x,y) (x) < (y) ? (x) : (y)
#define checkav(ret, msg, ...) if ((ret) < 0) {av_log (NULL, AV_LOG_ERROR, "%s:%i " msg ": %s\n", \
            __FILE__,__LINE__, ##__VA_ARGS__, av_err2str (ret)); goto error;}

const PPB_Audio          *G_PPB_AUDIO            = NULL;
const PPB_AudioConfig    *G_PPB_AUDIO_CONFIG     = NULL;
const PPB_VarArrayBuffer *G_PPB_VAR_ARRAY_BUFFER = NULL;
const PPB_VarArray       *G_PPB_VAR_ARRAY        = NULL;

typedef struct {
    struct PP_Var var;
    void *buf;
    uint32_t len;
    uint32_t idx;
} Packet;

typedef struct {
    PP_Instance instance;
    List *packets;
} RefillData;

typedef struct {
    PP_Instance instance;
    AVFormatContext *fmt_ctx;
    AVStream *aud_in;
    AVStream *vid_in;
} DecoderArgs;

typedef struct {
    uint8_t *buf;
    int      buf_size;
    int      buf_idx;
    List    *pkt_list;
    AVFrame *src_frame;
    AVCodecContext *dec_ctx;
    struct SwrContext *swr_ctx;
} AudioState;

// global vars
// incoming stream packets
List *packets = NULL;
// audio frames
List *aud_frames = NULL;
// video frames
List *vid_frames = NULL;
// thread id of decoding thread
pthread_t decoder_thread = {0};
// instance id
PP_Instance instance = {0};

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
            List_take (as->pkt_list, (void **) &pkt);
            if (!pkt) {
                debug ("No audio packet left.");
                ret = -1;
                goto error;
            }
        }

        ret = avcodec_decode_audio4 (as->dec_ctx, as->src_frame, &got_frame, pkt);
        check (ret, "Failed to decode audio");

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
                    (const uint8_t **) as->src_frame->data, as->src_frame->nb_samples);
            check (ret, "Failed to resample audio");

            as->buf_idx = 0;

            if (pkt->size > 0) {
                // data left in the packet queue it up for the next call
                List_insert (as->pkt_list, (void *) pkt);
                goto nofree;
            }
            break;
        }
    }

error:
    av_free_packet (pkt);
nofree:
    return ret;
}

void
audio_callback (void *buf, uint32_t len, PP_TimeDelta latency, void *data)
{
    AudioState *as = (AudioState *) data;

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
        memcpy (buf, as->buf + as->buf_idx, len1);
        as->buf_idx += len1;
        buf += len1;
        len -= len1;
    }

    return;
err:
    debug ("Outputting silence.");
    memset (buf, 0, len);
}

int
refill (void *data, uint8_t *buf, int buf_size)
{
    //List *packets        = ((RefillData *) data)->packets;
    //PP_Instance instance = ((RefillData *) data)->instance;
    debug ("Refilling AVIOContext.");

    int ret = pthread_mutex_lock (packets->lock);
    check (ret == 0, "Failed to lock packet list lock while trying to refill AVIOContext.");

    Packet *pkt = NULL;
    int left, len;
    while (buf_size > 0) {
        if (pkt == NULL) List_take (packets, (void *) pkt);
        check (pkt != NULL, "Packet list is emtpy, cannot refill AVIOContext.")

        left = pkt->len - pkt->idx;
        len = min (left, buf_size);
        buf_size -= len;
        pkt->idx += len;

        memcpy (buf, (uint8_t *) pkt->buf, len);

        if (pkt->idx == pkt->len) {
            free (pkt);
            pkt = NULL;
        }
    }
    if (pkt != NULL) List_insert (packets, (void *) pkt);

    pthread_mutex_unlock (packets->lock);
    return 0;

error:
    pthread_mutex_unlock (packets->lock);
    return 1;
}

/* Allocate input and output format contexts together with their respective
 * streams.
 */
int
allocate_io (AVFormatContext **ifmtx,
             AVStream  **aud_in_stream,
             AVStream  **vid_in_stream)
{
    int vid_index = 0, aud_index = 0, ret = 0;
    size_t avio_ctx_buffer_size = 4096;
    uint8_t *avio_ctx_buffer = NULL;
    AVFormatContext *fmt_ctx = NULL;
    AVIOContext *avio_ctx = NULL;
    AVCodec *aud_dec_codec = NULL,
            *vid_dec_codec = NULL;


    fmt_ctx = avformat_alloc_context ();
    check (fmt_ctx != NULL, "Failed to allocate AVIOFormatContext.");

    avio_ctx_buffer = av_malloc (avio_ctx_buffer_size);
    check (avio_ctx_buffer != NULL, "Failed to allocate buffer for AVIOContext.");

    avio_ctx = avio_alloc_context (avio_ctx_buffer, avio_ctx_buffer_size, 1, NULL,
                                   refill, NULL, NULL);
    check (avio_ctx != NULL, "Failed to allocate AVIOContext.");


    fmt_ctx->pb = avio_ctx;
    ret = avformat_open_input (&fmt_ctx, NULL, NULL, NULL);
    check (ret == 0, "Failed to open AVFormatContext.");

    ret = avformat_find_stream_info (*ifmtx, 0);
    checkav (ret, "Failed to retrieve stream info");

    av_dump_format (*ifmtx, 0, NULL, 0);


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

    return 0;

error:
    if (*aud_in_stream) avcodec_close ((*aud_in_stream)->codec);
    if (*vid_in_stream) avcodec_close ((*vid_in_stream)->codec);
    if (*ifmtx) avformat_close_input (ifmtx);

    return ret;
}

/* Initialize AudioState for use in audio_callback
 */
int
cfg_audio (AudioState *as, AVCodecContext *dec_ctx)
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
    check (ret, "Failed to initialize audio resampling context");

    return 0;

error:
    return ret;
}

void *
VideoThread (void *data)
{
    return NULL;
}

void *
DecoderThread (void *data)
{
    int ret = 0;
    DecoderArgs *args = (DecoderArgs *) data;
    AVPacket pkt, *cpkt;

    while (1) {
        av_init_packet (&pkt);
        ret = av_read_frame (args->fmt_ctx, &pkt);
        checkav (ret, "Failed to read next frame from input");

        if (pkt.stream_index == args->vid_in->index) {
            cpkt = calloc (1, sizeof (AVPacket));
            check (cpkt != NULL, "Failed to allocate new video frame.");
            av_copy_packet (cpkt, &pkt);

            List_append_locked (vid_frames, cpkt);
        } else
        if (pkt.stream_index == args->aud_in->index) {
            cpkt = calloc (1, sizeof (AVPacket));
            check (cpkt != NULL, "Failed to allocate new video frame.");
            av_copy_packet (cpkt, &pkt);

            List_append_locked (aud_frames, cpkt);
        }

        av_free_packet (&pkt);
    }

error:
    debug ("Decoder Thread exiting.");
    return NULL;
}

PP_Bool
DidCreate (PP_Instance inst, uint32_t argc, const char** argn, const char** argv)
{
    check (packets == NULL, "Cannot have multiple instances due to laz… important technical limitations.");

    instance = inst;

    AudioState *audio_state = NULL;
    DecoderArgs *dec_args = NULL;

    int ret = 0;

    av_register_all ();

    packets = List_new ();
    vid_frames = List_new ();
    check (packets != NULL && vid_frames != NULL && aud_frames != NULL,
           "Failed to allocate frame and packet lists.");

    dec_args = calloc (1, sizeof (DecoderArgs));
    check (dec_args != NULL, "Failed to allocate decoder arguments.");

    dec_args->instance = instance;

    ret = allocate_io (&(dec_args->fmt_ctx), &(dec_args->aud_in), &(dec_args->vid_in));
    check (ret < 0, "Failed to initialize decoder arguments.");

    audio_state = calloc (1, sizeof (AudioState));
    check (audio_state != NULL, "Failed to allocate audio state.");

    audio_state->pkt_list = aud_frames;
    ret = cfg_audio (audio_state, dec_args->aud_in->codec);
    check (ret == 0, "Failed to initialize audio state.");
    aud_frames = audio_state->pkt_list;

    uint32_t frame_count = G_PPB_AUDIO_CONFIG->RecommendSampleFrameCount (
            instance, PP_AUDIOSAMPLERATE_48000, 8192);
    PP_Resource audio_config = G_PPB_AUDIO_CONFIG->CreateStereo16Bit (
            instance, PP_AUDIOSAMPLERATE_48000, frame_count);
    check (G_PPB_AUDIO_CONFIG->IsAudioConfig (audio_config) == PP_TRUE,
            "Failed to create audio config resource.");

    PP_Resource audio_resource = G_PPB_AUDIO->Create (instance, audio_config,
            audio_callback, audio_state);
    check (G_PPB_AUDIO->IsAudio (audio_resource) == PP_TRUE,
            "Failed to create audio ressource.");
    G_PPB_AUDIO->StartPlayback (audio_resource);

    pthread_create (&decoder_thread, NULL, DecoderThread, dec_args);
    //pthread_create (&video_thread);
    debug ("Started new instance: %i.", instance);
    return PP_TRUE;

error:
    if (packets) List_free (packets, NULL);
    if (vid_frames) List_free (vid_frames, NULL);
    if (aud_frames) List_free (aud_frames, NULL);
    //if (rfdata) free (rfdata);
    if (dec_args) {
        if (dec_args->fmt_ctx) avformat_close_input (&(dec_args->fmt_ctx));
        free (dec_args);
    }
    return PP_FALSE;
}

void
HandleMessage (PP_Instance instance, struct PP_Var msg_array)
{
    uint32_t i;
    Packet *pkt = NULL;
    struct PP_Var var;
    if (msg_array.type != PP_VARTYPE_ARRAY) {
        log_info ("JS client sent something that is not an array, ignoring.");
        log_var  (msg_array);
        return;
    }

    uint32_t len = G_PPB_VAR_ARRAY->GetLength (msg_array);

    pthread_mutex_lock (packets->lock);
    for (i = 0; i < len; i++) {
        var = G_PPB_VAR_ARRAY->Get (msg_array, i);

        if (var.type != PP_VARTYPE_ARRAY_BUFFER) {
            log_info ("JS client put something not an array buffer in the array, ignoring.");
            continue;
        }

        pkt = calloc (1, sizeof (Packet));
        check (pkt != NULL, "Failed to allocate a Packet struct.");

        pkt->var = var;
        pkt->buf = G_PPB_VAR_ARRAY_BUFFER->Map (pkt->var);
        // for now we just ignore the RTP header
        pkt->idx = 8;
        List_append (packets, pkt);
    }

error:
    pthread_mutex_unlock (packets->lock);
    return;
}

const void *
PPP_GetInterface (const char *name)
{
    if (strcmp (name, PPP_INSTANCE_INTERFACE) == 0) {
        static PPP_Instance ii = {
            &DidCreate, &DidDestroy, &DidChangeView, &DidChangeFocus, &HandleDocumentLoad
        };
        return &ii;
    } else
    if (strcmp (name, PPP_MESSAGING_INTERFACE) == 0) {
        static PPP_Messaging mi = {
            &HandleMessage
        };
        return &mi;
    } else
    return NULL;
}

int32_t
PPP_InitializeModule (PP_Module mod, PPB_GetInterface gbi)
{
    get_browser_interface = gbi;

    fetch_interface (VAR);
    fetch_interface (CONSOLE);
    fetch_interface (VAR_ARRAY);
    fetch_interface (VAR_ARRAY_BUFFER);
    fetch_interface (AUDIO);
    fetch_interface (AUDIO_CONFIG);

    return PP_OK;
}
