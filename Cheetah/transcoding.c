/*
*******************  This software uses FFmpeg licensed under the GPL.******************************
* 
* 
* 
 * Copyright [2025] [codeboxqc]
 *
 * This file is part of  cheetah .
 *
 * cheetah is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or any later version.
 *
 * cheetah is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with [Program Name].  If not, see <https://www.gnu.org/licenses/>.
 */



 /*
 *******************  This software uses FFmpeg licensed under the GPL.******************************
  * Copyright [2025] [codeboxqc]
  *
  * This file is part of  cheetah .
  *
  * cheetah is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or any later version.
  *
  * cheetah is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with [Program Name].  If not, see <https://www.gnu.org/licenses/>.
  */
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/error.h>

  // =============================================================================
  // GLOBALS & STRUCTURES
  // =============================================================================

static AVFormatContext* ifmt_ctx;
static AVFormatContext* ofmt_ctx;
static FILE* debug_log = NULL;

typedef void (*ProgressCallback)(int64_t current_frame, int64_t total_frames,
    double fps, int64_t elapsed_ms, int64_t file_size);
static ProgressCallback g_progress_callback = NULL;

static int64_t g_total_frames = 0;
static int64_t g_current_frame = 0;
static int64_t g_start_time = 0;
static const char* g_output_filename = NULL;
static const char* g_output_format = "mp4";

typedef enum {
    ENCODE_H265_ARCHIVE = 1,
    ENCODE_H264_HIGH = 2,
    ENCODE_H264_BALANCED = 3,
    ENCODE_H264_SMALL = 4
} EncodingOption;

static EncodingOption g_encoding_option = ENCODE_H264_HIGH;

typedef struct FilteringContext {
    AVFilterContext* buffersink_ctx;
    AVFilterContext* buffersrc_ctx;
    AVFilterGraph* filter_graph;
    AVPacket* enc_pkt;
    AVFrame* filtered_frame;
} FilteringContext;
static FilteringContext* filter_ctx;

typedef struct StreamContext {
    AVCodecContext* dec_ctx;
    AVCodecContext* enc_ctx;
    AVFrame* dec_frame;
    int out_stream_index;
} StreamContext;
static StreamContext* stream_ctx;

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

void set_progress_callback(ProgressCallback callback) {
    g_progress_callback = callback;
}

void set_encoding_option(int option) {
    if (option >= 1 && option <= 4) {
        g_encoding_option = (EncodingOption)option;
    }
}

void set_output_format(int format) {
    switch (format) {
    case 1: g_output_format = "mp4"; break;
    case 2: g_output_format = "matroska"; break;
    case 3: g_output_format = "webm"; break;
    case 4: g_output_format = "mov"; break;
    default: g_output_format = "mp4"; break;
    }
}

static int64_t get_time_ms() {
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart * 1000) / frequency.QuadPart;
}

static int64_t get_file_size(const char* filename) {
    HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return 0;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(hFile, &size)) {
        CloseHandle(hFile);
        return 0;
    }
    CloseHandle(hFile);
    return size.QuadPart;
}

static void update_progress() {
    if (!g_progress_callback) return;
    int64_t elapsed = get_time_ms() - g_start_time;
    double fps = (elapsed > 0) ? (g_current_frame * 1000.0 / elapsed) : 0.0;
    int64_t file_size = g_output_filename ? get_file_size(g_output_filename) : 0;
    g_progress_callback(g_current_frame, g_total_frames, fps, elapsed, file_size);
}

static void log_debug(const char* fmt, ...) {
    if (debug_log) {
        va_list args;
        va_start(args, fmt);
        vfprintf(debug_log, fmt, args);
        fprintf(debug_log, "\n");
        fflush(debug_log);
        va_end(args);
    }
}

static void log_av_error(int err, const char* context) {
    char errbuf[256];
    av_strerror(err, errbuf, sizeof(errbuf));
    log_debug("ERROR [%s]: %s (%d)", context, errbuf, err);
}

// =============================================================================
// QUALITY CONFIGURATION
// =============================================================================

static void configure_vp9_quality(AVCodecContext* enc_ctx, EncodingOption option) {
    av_opt_set(enc_ctx->priv_data, "deadline", "good", 0);
    av_opt_set_int(enc_ctx->priv_data, "cpu-used", 3, 0);
    switch (option) {
    case ENCODE_H265_ARCHIVE: av_opt_set_int(enc_ctx->priv_data, "crf", 20, 0); break;
    case ENCODE_H264_HIGH:    av_opt_set_int(enc_ctx->priv_data, "crf", 28, 0); break;
    case ENCODE_H264_BALANCED:av_opt_set_int(enc_ctx->priv_data, "crf", 34, 0); break;
    case ENCODE_H264_SMALL:   av_opt_set_int(enc_ctx->priv_data, "crf", 45, 0); break;
    }
    av_opt_set(enc_ctx->priv_data, "b", "0", 0);
}

static void configure_nvenc_quality(AVCodecContext* enc_ctx, EncodingOption option) {
    av_opt_set(enc_ctx->priv_data, "rc", "vbr", 0);
    switch (option) {
    case ENCODE_H265_ARCHIVE:
        av_opt_set(enc_ctx->priv_data, "preset", "p7", 0);
        av_opt_set_int(enc_ctx->priv_data, "cq", 20, 0);
        break;
    case ENCODE_H264_HIGH:
        av_opt_set(enc_ctx->priv_data, "preset", "p7", 0);
        av_opt_set_int(enc_ctx->priv_data, "cq", 19, 0);
        break;
    case ENCODE_H264_BALANCED:
        av_opt_set(enc_ctx->priv_data, "preset", "p4", 0);
        av_opt_set_int(enc_ctx->priv_data, "cq", 23, 0);
        break;
    case ENCODE_H264_SMALL:
        av_opt_set(enc_ctx->priv_data, "preset", "p1", 0);
        av_opt_set_int(enc_ctx->priv_data, "cq", 28, 0);
        break;
    }
}

static void configure_amf_quality(AVCodecContext* enc_ctx, EncodingOption option) {
    av_opt_set(enc_ctx->priv_data, "rc", "cqp", 0);
    switch (option) {
    case ENCODE_H265_ARCHIVE:
        av_opt_set(enc_ctx->priv_data, "quality", "quality", 0);
        av_opt_set_int(enc_ctx->priv_data, "qp_i", 21, 0);
        break;
    case ENCODE_H264_HIGH:
        av_opt_set(enc_ctx->priv_data, "quality", "quality", 0);
        av_opt_set_int(enc_ctx->priv_data, "qp_i", 19, 0);
        break;
    case ENCODE_H264_BALANCED:
        av_opt_set(enc_ctx->priv_data, "quality", "balanced", 0);
        av_opt_set_int(enc_ctx->priv_data, "qp_i", 23, 0);
        break;
    case ENCODE_H264_SMALL:
        av_opt_set(enc_ctx->priv_data, "quality", "speed", 0);
        av_opt_set_int(enc_ctx->priv_data, "qp_i", 28, 0);
        break;
    }
}

static void configure_qsv_quality(AVCodecContext* enc_ctx, EncodingOption option) {

    av_opt_set(enc_ctx->priv_data, "look_ahead", "1", 0);

    switch (option) {
    case ENCODE_H265_ARCHIVE:
        av_opt_set(enc_ctx->priv_data, "preset", "veryslow", 0);
        av_opt_set_int(enc_ctx->priv_data, "global_quality", 22, 0);//20
        break;
    case ENCODE_H264_HIGH:
        av_opt_set(enc_ctx->priv_data, "preset", "veryslow", 0);
        av_opt_set_int(enc_ctx->priv_data, "global_quality", 18, 0);//19
        break;
    case ENCODE_H264_BALANCED:
        av_opt_set(enc_ctx->priv_data, "preset", "medium", 0);
        av_opt_set_int(enc_ctx->priv_data, "global_quality", 26, 0);//23
        break;
    case ENCODE_H264_SMALL:
        av_opt_set(enc_ctx->priv_data, "preset", "fast", 0);
        av_opt_set_int(enc_ctx->priv_data, "global_quality", 33, 0); //28
        break;
    }
}

static void configure_software_quality(AVCodecContext* enc_ctx, EncodingOption option) {
    switch (option) {
    case ENCODE_H265_ARCHIVE:
        av_opt_set(enc_ctx->priv_data, "preset", "slow", 0);
        av_opt_set(enc_ctx->priv_data, "crf", "21", 0);
        break;
    case ENCODE_H264_HIGH:
        av_opt_set(enc_ctx->priv_data, "preset", "slow", 0);
        av_opt_set(enc_ctx->priv_data, "crf", "19", 0);
        break;
    case ENCODE_H264_BALANCED:
        av_opt_set(enc_ctx->priv_data, "preset", "medium", 0);
        av_opt_set(enc_ctx->priv_data, "crf", "23", 0);
        break;
    case ENCODE_H264_SMALL:
        av_opt_set(enc_ctx->priv_data, "preset", "fast", 0);
        av_opt_set(enc_ctx->priv_data, "crf", "28", 0);
        break;
    }
}

static int configure_video_encoder(AVCodecContext* enc_ctx, AVCodecContext* dec_ctx,
    AVStream* in_stream, const AVCodec** encoder_out)
{
    const AVCodec* encoder = NULL;
    char msg[256];

    // Select Encoder
    if (strcmp(g_output_format, "webm") == 0) {
        encoder = avcodec_find_encoder(AV_CODEC_ID_VP9);
        if (!encoder) encoder = avcodec_find_encoder(AV_CODEC_ID_VP8);
        if (!encoder) encoder = avcodec_find_encoder_by_name("libvpx-vp9");

        if (encoder) {
            snprintf(msg, sizeof(msg), "Found WebM encoder: %s", encoder->name);
            log_debug(msg);
        }
        else {
            log_debug("CRITICAL: libvpx (VP8/VP9) not found!");
            return AVERROR_ENCODER_NOT_FOUND;
        }
    }
    else if (g_encoding_option == ENCODE_H265_ARCHIVE) {
        const char* h265_encoders[] = { "hevc_nvenc", "hevc_amf", "hevc_qsv", "libx265", NULL };
        for (int j = 0; h265_encoders[j]; j++) {
            encoder = avcodec_find_encoder_by_name(h265_encoders[j]);
            if (encoder) break;
        }
    }
    else {
        const char* h264_encoders[] = { "h264_nvenc", "h264_amf", "h264_qsv", "libx264", NULL };
        for (int j = 0; h264_encoders[j]; j++) {
            encoder = avcodec_find_encoder_by_name(h264_encoders[j]);
            if (encoder) break;
        }
    }

    if (!encoder) return AVERROR_ENCODER_NOT_FOUND;
    *encoder_out = encoder;

    // Basic Parameters
    enc_ctx->height = dec_ctx->height;
    enc_ctx->width = dec_ctx->width;
    enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;

    AVRational guessed_fr = av_guess_frame_rate(ifmt_ctx, in_stream, NULL);
    if (dec_ctx->framerate.num > 0) {
        enc_ctx->framerate = dec_ctx->framerate;
        enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
    }
    else if (guessed_fr.num > 0) {
        enc_ctx->framerate = guessed_fr;
        enc_ctx->time_base = av_inv_q(guessed_fr);
    }
    else {
        enc_ctx->time_base = in_stream->time_base;
        enc_ctx->framerate = av_inv_q(enc_ctx->time_base);
    }

    // Config based on Encoder Type
    if (strstr(encoder->name, "libvpx") || encoder->id == AV_CODEC_ID_VP9) {
        enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        configure_vp9_quality(enc_ctx, g_encoding_option);
    }
    else if (strstr(encoder->name, "nvenc")) {
        enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        configure_nvenc_quality(enc_ctx, g_encoding_option);
    }
    else if (strstr(encoder->name, "qsv")) {
        enc_ctx->pix_fmt = AV_PIX_FMT_NV12;
        configure_qsv_quality(enc_ctx, g_encoding_option);
    }
    else if (strstr(encoder->name, "amf")) {
        enc_ctx->pix_fmt = AV_PIX_FMT_NV12;
        configure_amf_quality(enc_ctx, g_encoding_option);
    }
    else {
        enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        configure_software_quality(enc_ctx, g_encoding_option);
    }

    return 0;
}

// =============================================================================
// MAIN LOGIC
// =============================================================================

static int open_input_file(const char* filename)
{
    int ret;
    unsigned int i;

    log_debug("Opening input: %s", filename);

    ifmt_ctx = NULL;
    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
        log_av_error(ret, "avformat_open_input");
        return ret;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        log_av_error(ret, "avformat_find_stream_info");
        return ret;
    }

    g_total_frames = 0;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            g_total_frames = ifmt_ctx->streams[i]->nb_frames;
            if (g_total_frames <= 0 && ifmt_ctx->streams[i]->duration > 0) {
                // Estimate frames if nb_frames missing
                g_total_frames = ifmt_ctx->streams[i]->duration *
                    av_q2d(ifmt_ctx->streams[i]->avg_frame_rate) / AV_TIME_BASE;
            }
            break;
        }
    }
    log_debug("Frames: %lld", g_total_frames);

    stream_ctx = av_calloc(ifmt_ctx->nb_streams, sizeof(*stream_ctx));
    if (!stream_ctx) return AVERROR(ENOMEM);

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        stream_ctx[i].out_stream_index = -1;
        const AVCodec* dec = avcodec_find_decoder(ifmt_ctx->streams[i]->codecpar->codec_id);
        if (!dec) continue;

        stream_ctx[i].dec_ctx = avcodec_alloc_context3(dec);
        avcodec_parameters_to_context(stream_ctx[i].dec_ctx, ifmt_ctx->streams[i]->codecpar);
        stream_ctx[i].dec_ctx->pkt_timebase = ifmt_ctx->streams[i]->time_base;
        avcodec_open2(stream_ctx[i].dec_ctx, dec, NULL);
        stream_ctx[i].dec_frame = av_frame_alloc();
    }

    av_dump_format(ifmt_ctx, 0, filename, 0);
    return 0;
}

static int open_output_file(const char* filename)
{
    int ret;
    unsigned int i;
    int video_found = 0;

    log_debug("Opening output: %s", filename);

    avformat_alloc_output_context2(&ofmt_ctx, NULL, g_output_format, filename);
    if (!ofmt_ctx) return AVERROR_UNKNOWN;

    // WebM doesn't use global headers, others might
    if (strcmp(g_output_format, "webm") != 0 && (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER))
        ofmt_ctx->flags |= AVFMT_GLOBALHEADER;

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        if (!stream_ctx[i].dec_ctx) continue;

        AVCodecContext* dec_ctx = stream_ctx[i].dec_ctx;
        enum AVMediaType type = dec_ctx->codec_type;

        if (type != AVMEDIA_TYPE_VIDEO && type != AVMEDIA_TYPE_AUDIO) continue;

        // WebM Logic: Only 1 video stream allowed
        if (type == AVMEDIA_TYPE_VIDEO) {
            if (video_found) {
                log_debug("Skipping secondary video stream");
                continue;
            }
            video_found = 1;
        }

        AVStream* out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) return AVERROR_UNKNOWN;
        stream_ctx[i].out_stream_index = out_stream->index;

        AVCodecContext* enc_ctx = avcodec_alloc_context3(NULL);
        const AVCodec* encoder = NULL;

        if (type == AVMEDIA_TYPE_VIDEO) {
            configure_video_encoder(enc_ctx, dec_ctx, ifmt_ctx->streams[i], &encoder);
        }
        else {
            // Audio selection
            if (strcmp(g_output_format, "webm") == 0) {
                encoder = avcodec_find_encoder(AV_CODEC_ID_OPUS);
                if (!encoder) encoder = avcodec_find_encoder(AV_CODEC_ID_VORBIS);
            }
            else {
                encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
            }

            if (!encoder) {
                log_debug("Audio encoder not found, skipping");
                continue;
            }

            av_channel_layout_copy(&enc_ctx->ch_layout, &dec_ctx->ch_layout);
            enc_ctx->sample_rate = dec_ctx->sample_rate;
            enc_ctx->sample_fmt = encoder->sample_fmts ? encoder->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
            enc_ctx->time_base = (AVRational){ 1, enc_ctx->sample_rate };

            if (encoder->id == AV_CODEC_ID_OPUS) {
                // FORCE 48kHz for Opus to prevent crashes
                enc_ctx->sample_rate = 48000;
                enc_ctx->time_base = (AVRational){ 1, 48000 };
                enc_ctx->bit_rate = 128000;
            }
            else {
                enc_ctx->bit_rate = 192000;
            }
        }

        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        ret = avcodec_open2(enc_ctx, encoder, NULL);
        if (ret < 0) {
            avcodec_free_context(&enc_ctx);
            log_av_error(ret, "Encoder open");
            continue;
        }

        avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
        out_stream->time_base = enc_ctx->time_base;
        stream_ctx[i].enc_ctx = enc_ctx;
    }

    av_dump_format(ofmt_ctx, 0, filename, 1);

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if ((ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE)) < 0) {
            log_av_error(ret, "File open");
            return ret;
        }
    }

    return avformat_write_header(ofmt_ctx, NULL);
}

static int init_filter(FilteringContext* fctx, AVCodecContext* dec_ctx,
    AVCodecContext* enc_ctx, const char* filter_spec)
{
    int ret;
    AVFilterInOut* outputs = avfilter_inout_alloc();
    AVFilterInOut* inputs = avfilter_inout_alloc();
    AVFilterGraph* graph = avfilter_graph_alloc();
    char args[512];

    const AVFilter* buffer_src = avfilter_get_by_name(dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO ? "buffer" : "abuffer");
    const AVFilter* buffer_sink = avfilter_get_by_name(dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO ? "buffersink" : "abuffersink");

    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
            dec_ctx->pkt_timebase.num, dec_ctx->pkt_timebase.den,
            dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);
    }
    else {
        char layout[64];
        av_channel_layout_describe(&dec_ctx->ch_layout, layout, sizeof(layout));
        snprintf(args, sizeof(args), "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
            dec_ctx->pkt_timebase.num, dec_ctx->pkt_timebase.den, dec_ctx->sample_rate,
            av_get_sample_fmt_name(dec_ctx->sample_fmt), layout);
    }

    ret = avfilter_graph_create_filter(&fctx->buffersrc_ctx, buffer_src, "in", args, NULL, graph);
    if (ret < 0) goto end;

    ret = avfilter_graph_create_filter(&fctx->buffersink_ctx, buffer_sink, "out", NULL, NULL, graph);
    if (ret < 0) goto end;

    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        av_opt_set_bin(fctx->buffersink_ctx, "pix_fmts", (uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt), AV_OPT_SEARCH_CHILDREN);
    }
    else {
        av_opt_set_bin(fctx->buffersink_ctx, "sample_fmts", (uint8_t*)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt), AV_OPT_SEARCH_CHILDREN);
        av_opt_set_bin(fctx->buffersink_ctx, "sample_rates", (uint8_t*)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate), AV_OPT_SEARCH_CHILDREN);
    }

    outputs->name = av_strdup("in");
    outputs->filter_ctx = fctx->buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = fctx->buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if ((ret = avfilter_graph_parse_ptr(graph, filter_spec, &inputs, &outputs, NULL)) < 0) goto end;
    if ((ret = avfilter_graph_config(graph, NULL)) < 0) goto end;

    fctx->filter_graph = graph;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return ret;
}

static int init_filters(void)
{
    char filter_spec[512];
    filter_ctx = av_malloc_array(ifmt_ctx->nb_streams, sizeof(*filter_ctx));

    for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++) {
        filter_ctx[i].filter_graph = NULL;
        if (stream_ctx[i].out_stream_index < 0 || !stream_ctx[i].enc_ctx) continue;

        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            snprintf(filter_spec, sizeof(filter_spec), "format=yuv420p");
        }
        else {
            // CRITICAL for Opus: Ensure 48kHz and correct frame sizing
            int frame_size = stream_ctx[i].enc_ctx->frame_size;
            if (frame_size > 0)
                snprintf(filter_spec, sizeof(filter_spec), "aresample=48000,asetnsamples=n=%d", frame_size);
            else
                snprintf(filter_spec, sizeof(filter_spec), "aresample=48000");
        }

        if (init_filter(&filter_ctx[i], stream_ctx[i].dec_ctx, stream_ctx[i].enc_ctx, filter_spec) < 0)
            return -1;

        filter_ctx[i].enc_pkt = av_packet_alloc();
        filter_ctx[i].filtered_frame = av_frame_alloc();
    }
    return 0;
}

static int encode_write(unsigned int index, AVFrame* frame)
{
    int ret;
    AVPacket* pkt = filter_ctx[index].enc_pkt;

    // Send frame to encoder
    av_packet_unref(pkt);
    if (frame) frame->pts = av_rescale_q(frame->pts, frame->time_base, stream_ctx[index].enc_ctx->time_base);

    ret = avcodec_send_frame(stream_ctx[index].enc_ctx, frame);
    if (ret < 0 && ret != AVERROR_EOF) return ret;

    // Receive packets
    while (ret >= 0) {
        ret = avcodec_receive_packet(stream_ctx[index].enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) return ret;

        pkt->stream_index = stream_ctx[index].out_stream_index;
        av_packet_rescale_ts(pkt, stream_ctx[index].enc_ctx->time_base, ofmt_ctx->streams[pkt->stream_index]->time_base);

        ret = av_interleaved_write_frame(ofmt_ctx, pkt);
        if (ret < 0) return ret;
    }
    return 0;
}

static int filter_encode(unsigned int index, AVFrame* frame)
{
    int ret;
    if (!filter_ctx[index].filter_graph) return 0;

    if (av_buffersrc_add_frame_flags(filter_ctx[index].buffersrc_ctx, frame, 0) < 0) return -1;

    while (1) {
        ret = av_buffersink_get_frame(filter_ctx[index].buffersink_ctx, filter_ctx[index].filtered_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) return ret;

        filter_ctx[index].filtered_frame->time_base = av_buffersink_get_time_base(filter_ctx[index].buffersink_ctx);
        if (encode_write(index, filter_ctx[index].filtered_frame) < 0) break;

        av_frame_unref(filter_ctx[index].filtered_frame);
    }
    return 0;
}

int transcode_main(int argc, char** argv)
{
    int ret;
    AVPacket* packet = NULL;

#ifdef _WIN32
    fopen_s(&debug_log, "cheetah.log", "w");
#else
    debug_log = fopen("cheetah.log", "w");
#endif

    log_debug("=== Cheetah Transcode Started ===");
    avformat_network_init();

    if (argc != 3) {
        log_debug("Usage: %s <input> <output>", argv[0]);
        goto end;
    }

    if (open_input_file(argv[1]) < 0 || open_output_file(argv[2]) < 0 || init_filters() < 0)
        goto end;

    packet = av_packet_alloc();
    g_current_frame = 0;
    g_start_time = get_time_ms();
    g_output_filename = argv[2];
    update_progress();

    // Main Loop
    while (av_read_frame(ifmt_ctx, packet) >= 0) {
        int idx = packet->stream_index;

        if (idx < (int)ifmt_ctx->nb_streams && stream_ctx[idx].out_stream_index >= 0) {
            if (stream_ctx[idx].dec_ctx) {
                avcodec_send_packet(stream_ctx[idx].dec_ctx, packet);
                while (avcodec_receive_frame(stream_ctx[idx].dec_ctx, stream_ctx[idx].dec_frame) >= 0) {
                    stream_ctx[idx].dec_frame->pts = stream_ctx[idx].dec_frame->best_effort_timestamp;

                    if (stream_ctx[idx].dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                        g_current_frame++;
                        if (g_current_frame % 10 == 0) update_progress();
                    }

                    filter_encode(idx, stream_ctx[idx].dec_frame);
                }
            }
        }
        av_packet_unref(packet);
    }

    // Flush
    log_debug("Flushing encoders...");
    for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++) {
        if (stream_ctx[i].out_stream_index < 0 || !stream_ctx[i].dec_ctx) continue;

        // Flush Decoder -> Filter
        avcodec_send_packet(stream_ctx[i].dec_ctx, NULL);
        while (avcodec_receive_frame(stream_ctx[i].dec_ctx, stream_ctx[i].dec_frame) >= 0) {
            filter_encode(i, stream_ctx[i].dec_frame);
        }

        // Flush Filter -> Encoder
        filter_encode(i, NULL);

        // Flush Encoder
        encode_write(i, NULL);
    }

    av_write_trailer(ofmt_ctx);
    update_progress();
    log_debug("Done.");
    ret = 0;

end:
    if (packet) av_packet_free(&packet);
    if (stream_ctx) {
        for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++) {
            avcodec_free_context(&stream_ctx[i].dec_ctx);
            avcodec_free_context(&stream_ctx[i].enc_ctx);
            av_frame_free(&stream_ctx[i].dec_frame);
        }
        av_free(stream_ctx);
    }
    if (filter_ctx) {
        for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++) {
            avfilter_graph_free(&filter_ctx[i].filter_graph);
            av_packet_free(&filter_ctx[i].enc_pkt);
            av_frame_free(&filter_ctx[i].filtered_frame);
        }
        av_free(filter_ctx);
    }
    if (ifmt_ctx) avformat_close_input(&ifmt_ctx);
    if (ofmt_ctx) {
        if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&ofmt_ctx->pb);
        avformat_free_context(ofmt_ctx);
    }
    if (debug_log) fclose(debug_log);

    return ret;
}