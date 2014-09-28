#include <libavutil/avconfig.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>

#define if_err_avlog(ret, msg, ...) if (ret < 0) {av_log (NULL, AV_LOG_ERROR, "%s:%i " msg ": %s\n", \
            __FILE__,__LINE__, ##__VA_ARGS__, av_err2str (ret)); goto err;}

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
    int (*dec_func) (AVCodecContext *, AVFrame *, int *, const AVPacket *) =
            (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) ? avcodec_decode_video2 : avcodec_decode_audio4;
    int (*enc_func) (AVCodecContext *, AVPacket *, const AVFrame *, int *) =
            (enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO) ? avcodec_encode_video2 : avcodec_encode_audio2;

    AVFrame *frame;
    int ret;

    frame = av_frame_alloc ();
    ret = dec_func (dec_ctx, frame, got_pkt, from_pkt);
    if (ret < 0) {
        av_log (NULL, AV_LOG_ERROR, "Failed to decode frame: %s\n", av_err2str (ret));
        av_frame_free (&frame);
        return ret;
    }
    if (!got_pkt) {
        av_frame_free (&frame);
        return 0;
    }

    // save stream index here, because enc_func reserves to overwrite all
    // attributes in to_pkt
    ret = enc_func (enc_ctx, to_pkt, frame, got_pkt);
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

int
main (int argc, char **argv)
{
    int ret, i, got_pkt, idx;
    char *filename;

    AVFormatContext *ifmtx = NULL, *ofmtx = NULL;

    int in_indices[2];//, out_indices[2];
    AVStream *in_streams[2] = {0}, *out_streams[2] = {0};
    AVCodec  *dec_codecs[2] = {0}, *enc_codecs[2] = {0};
    AVCodecContext *dec_ctxs[2] = {0}, *enc_ctxs[2] = {0};

    AVPacket pkt, npkt;

    if (argc < 2) {
        fprintf (stderr, "USAGE: %s videofile\n", argv [0]);
        return -1;
    }

    av_register_all ();
    avdevice_register_all ();

    filename = argv [1];
    ret = avformat_open_input (&ifmtx, filename, 0, 0);
    if_err_avlog (ret, "Failed to open input format context");
    ret = avformat_find_stream_info (ifmtx, 0);
    if_err_avlog (ret, "Failed to retrieve stream info");

    av_dump_format (ifmtx, 0, filename, 0);

    ret = avformat_alloc_output_context2 (&ofmtx, NULL, "SDL", NULL);
    if_err_avlog (ret, "Failed to open output format context");

    in_indices [0] = av_find_best_stream (ifmtx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec_codecs [0], 0);
    if_err_avlog (in_indices [0], "Failed to select video stream");
    in_indices [1] = av_find_best_stream (ifmtx, AVMEDIA_TYPE_AUDIO, -1, -1, &dec_codecs [1], 0);
    if_err_avlog (in_indices [1], "Failed to select audio stream");
    for (i = 0; i < 2; i++) {
        idx = in_indices [i];
        in_streams [i] = ifmtx->streams [idx];
        dec_ctxs   [i] = in_streams [i]->codec;
        ret = avcodec_open2 (dec_ctxs [i], dec_codecs [i], NULL);
        if_err_avlog (ret, "Failed to open decoding context for stream #%i", i);
    }

    enc_codecs [0] = avcodec_find_encoder_by_name ("rawvideo");
    enc_codecs [1] = avcodec_find_encoder (ofmtx->oformat->audio_codec);
    for (i = 0; i < 2; i++) {
        out_streams [i] = avformat_new_stream (ofmtx, enc_codecs [i]);
        enc_ctxs    [i] = out_streams [i]->codec;
        //out_indices [i] = out_streams [i]->index;

        if (i == 0) {
            enc_ctxs [0]->width   = 640;
            enc_ctxs [0]->height  = 320;
            enc_ctxs [0]->pix_fmt = dec_ctxs [0]->pix_fmt;
        } else {
            // Initialization for audio context goes here
        }

        ret = avcodec_open2 (enc_ctxs [i], enc_codecs [i], NULL);
        if_err_avlog (ret, "Failed to open encoding context for stream #%i", i);
    }

    av_dump_format (ofmtx, 0, NULL, 0);

    ret = avformat_write_header(ofmtx, NULL);
    if_err_avlog (ret, "Failed to init SDL");

    av_init_packet (& pkt);
    av_init_packet (&npkt);
    while (1) {
        ret = av_read_frame (ifmtx, &pkt);
        if_err_avlog (ret, "Failed to read next frame from input");

        for (i = 0; i < 2; i++) {
            if (pkt.stream_index == in_indices [i]) break;
        }

        if (i == 2) {
            av_free_packet (&pkt);
            continue;
        }

        npkt.size = 0;
        npkt.data = NULL;
        av_init_packet (&npkt);
        ret = transcode_pkt (in_streams [i], out_streams [i],
                             &pkt, &npkt, &got_pkt);

        av_interleaved_write_frame (ofmtx, &npkt);
        av_free_packet (& pkt);
        av_free_packet (&npkt);
    }

err:

    avformat_close_input (&ifmtx);
    avformat_free_context (ofmtx);

    return ret;
}
