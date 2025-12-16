
/*
 * Copyright [2025] [codeboxqc]
 *
 * This file is part of  cheetah .
 *
 * cheetah is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>

static AVFormatContext* ifmt_ctx;
static AVFormatContext* ofmt_ctx;
static FILE* debug_log = NULL;

// Progress callback
typedef void (*ProgressCallback)(int64_t current_frame, int64_t total_frames,
    double fps, int64_t elapsed_ms, int64_t file_size);
static ProgressCallback g_progress_callback = NULL;
static int64_t g_total_frames = 0;
static int64_t g_current_frame = 0;
static int64_t g_start_time = 0;
static const char* g_output_filename = NULL;

void set_progress_callback(ProgressCallback callback) {
    g_progress_callback = callback;
}

/////////////////////////////////////////
typedef enum {
    ENCODE_H265_ARCHIVE = 1,   // H.265 Archive (CRF 20-22)
    ENCODE_H264_HIGH = 2,       // H.264 High Quality (CRF 18-20)
    ENCODE_H264_BALANCED = 3,   // H.264 Balanced (CRF 22-24)
    ENCODE_H264_SMALL = 4       // H.264 Small Size (CRF 27-30)
} EncodingOption;

// Global variable to store selected encoding option
static EncodingOption g_encoding_option = ENCODE_H264_HIGH;

// Function to set encoding option (call this before transcode_main)
void set_encoding_option(int option) {
    if (option >= 1 && option <= 4) {
        g_encoding_option = (EncodingOption)option;
    }
}
//////////////////////////////////////////////////


static const char* g_output_format = "mp4";  // default

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
} StreamContext;
static StreamContext* stream_ctx;

static void log_debug(const char* msg)
{
    if (debug_log) {
        fprintf(debug_log, "%s\n", msg);
        fflush(debug_log);
    }
}


static void configure_nvenc_quality(AVCodecContext* enc_ctx, EncodingOption option);
static void configure_amf_quality(AVCodecContext* enc_ctx, EncodingOption option);
static void configure_qsv_quality(AVCodecContext* enc_ctx, EncodingOption option);
static void configure_software_quality(AVCodecContext* enc_ctx, EncodingOption option);


static int open_input_file(const char* filename)
{
    int ret;
    unsigned int i;

    log_debug("Opening input file...");

    ifmt_ctx = NULL;
    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }



    // Calculate total frames for progress tracking
    g_total_frames = 0;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream* stream = ifmt_ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (stream->nb_frames > 0) {
                g_total_frames = stream->nb_frames;
            }
            else if (stream->duration > 0 && stream->avg_frame_rate.num > 0) {
                // Estimate from duration and framerate
                g_total_frames = stream->duration * stream->avg_frame_rate.num /
                    (stream->avg_frame_rate.den * stream->time_base.den);
            }
            break; // Use first video stream
        }
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "Total frames estimated: %lld", g_total_frames);
    log_debug(msg);


    stream_ctx = av_calloc(ifmt_ctx->nb_streams, sizeof(*stream_ctx));
    if (!stream_ctx)
        return AVERROR(ENOMEM);

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream* stream = ifmt_ctx->streams[i];
        const AVCodec* dec = avcodec_find_decoder(stream->codecpar->codec_id);
        AVCodecContext* codec_ctx;

        if (!dec) {
            av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u\n", i);
            return AVERROR_DECODER_NOT_FOUND;
        }

        codec_ctx = avcodec_alloc_context3(dec);
        if (!codec_ctx) {
            av_log(NULL, AV_LOG_ERROR, "Failed to allocate decoder context for stream #%u\n", i);
            return AVERROR(ENOMEM);
        }

        ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to copy decoder parameters for stream #%u\n", i);
            return ret;
        }

        codec_ctx->pkt_timebase = stream->time_base;

        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO ||
            codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {

            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
                codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);

            ret = avcodec_open2(codec_ctx, dec, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }

        stream_ctx[i].dec_ctx = codec_ctx;
        stream_ctx[i].dec_frame = av_frame_alloc();
        if (!stream_ctx[i].dec_frame)
            return AVERROR(ENOMEM);
    }

    av_dump_format(ifmt_ctx, 0, filename, 0);
    log_debug("Input file opened successfully");
    return 0;
}




//////////////////////////////////////////////// 
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////

// =============================================================================
// SECTION 2: Main Video Encoder Configuration Function
// This function does 3 things:
//   1. Finds the right encoder (H.264 or H.265, GPU or software)
//   2. Sets basic parameters (width, height, framerate, etc)
//   3. Applies quality settings based on the encoding option
// =============================================================================

static int configure_video_encoder(AVCodecContext* enc_ctx, AVCodecContext* dec_ctx,
    AVStream* in_stream, const AVCodec** encoder_out)
{
    const AVCodec* encoder = NULL;
    char msg[256];

    log_debug("configure_video_encoder: Starting...");

    // Find encoder based on profile
    if (g_encoding_option == ENCODE_H265_ARCHIVE) {
        const char* h265_encoders[] = { "hevc_nvenc", "hevc_amf", "hevc_qsv", "libx265", NULL };
        for (int j = 0; h265_encoders[j]; j++) {
            encoder = avcodec_find_encoder_by_name(h265_encoders[j]);
            if (encoder) {
                snprintf(msg, sizeof(msg), "Found H.265 encoder: %s", encoder->name);
                log_debug(msg);
                break;
            }
        }
    }
    else {
        const char* h264_encoders[] = { "h264_nvenc", "h264_amf", "h264_qsv", "libx264", NULL };
        for (int j = 0; h264_encoders[j]; j++) {
            encoder = avcodec_find_encoder_by_name(h264_encoders[j]);
            if (encoder) {
                snprintf(msg, sizeof(msg), "Found H.264 encoder: %s", encoder->name);
                log_debug(msg);
                break;
            }
        }
    }

    if (!encoder) {
        log_debug("ERROR - No suitable encoder found!");
        return AVERROR_ENCODER_NOT_FOUND;
    }

    *encoder_out = encoder;

    // Basic parameters
    enc_ctx->height = dec_ctx->height;
    enc_ctx->width = dec_ctx->width;
    enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;

    // Time base and framerate
    AVRational guessed_fr = av_guess_frame_rate(ifmt_ctx, in_stream, NULL);
    if (dec_ctx->framerate.num > 0 && dec_ctx->framerate.den > 0) {
        enc_ctx->framerate = dec_ctx->framerate;
        enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
    }
    else if (guessed_fr.num > 0 && guessed_fr.den > 0) {
        enc_ctx->framerate = guessed_fr;
        enc_ctx->time_base = av_inv_q(guessed_fr);
    }
    else {
        enc_ctx->time_base = in_stream->time_base;
        enc_ctx->framerate = av_inv_q(enc_ctx->time_base);
    }

    // Pixel format
    if (strstr(encoder->name, "nvenc")) {
        enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    }
    else if (strstr(encoder->name, "qsv")) {
        enc_ctx->pix_fmt = AV_PIX_FMT_NV12;
    }
    else if (strstr(encoder->name, "amf")) {
        enc_ctx->pix_fmt = AV_PIX_FMT_NV12;
    }
    else {
        enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    }

    // FIXED: Call quality configuration functions with correct signature (only 2 params)
    if (strstr(encoder->name, "nvenc")) {
        configure_nvenc_quality(enc_ctx, g_encoding_option);
    }
    else if (strstr(encoder->name, "amf")) {
        configure_amf_quality(enc_ctx, g_encoding_option);
    }
    else if (strstr(encoder->name, "qsv")) {
        configure_qsv_quality(enc_ctx, g_encoding_option);
    }
    else {
        configure_software_quality(enc_ctx, g_encoding_option);
    }

    log_debug("configure_video_encoder: Complete");
    return 0;
}





// NVIDIA encoder quality configuration
static void configure_nvenc_quality(AVCodecContext* enc_ctx, EncodingOption option)
{
    // CRITICAL: Set rc mode to CQ (constant quality)
    av_opt_set(enc_ctx->priv_data, "rc", "vbr", 0);

    switch (option) {
    case ENCODE_H265_ARCHIVE:
        av_opt_set(enc_ctx->priv_data, "preset", "p7", 0);
        av_opt_set(enc_ctx->priv_data, "profile", "main", 0);
        av_opt_set_int(enc_ctx->priv_data, "cq", 20, 0);  // CQ 20
        av_opt_set_int(enc_ctx->priv_data, "spatial-aq", 1, 0);
        av_opt_set_int(enc_ctx->priv_data, "temporal-aq", 1, 0);
        break;

    case ENCODE_H264_HIGH:
        av_opt_set(enc_ctx->priv_data, "preset", "p7", 0);
        av_opt_set(enc_ctx->priv_data, "profile", "high", 0);
        av_opt_set_int(enc_ctx->priv_data, "cq", 19, 0);  // CQ 19
        av_opt_set_int(enc_ctx->priv_data, "spatial-aq", 1, 0);
        av_opt_set(enc_ctx->priv_data, "level", "4.1", 0);
        break;

    case ENCODE_H264_BALANCED:
        av_opt_set(enc_ctx->priv_data, "preset", "p4", 0);
        av_opt_set(enc_ctx->priv_data, "profile", "high", 0);
        av_opt_set_int(enc_ctx->priv_data, "cq", 23, 0);  // CQ 23
        break;

    case ENCODE_H264_SMALL:
        av_opt_set(enc_ctx->priv_data, "preset", "p1", 0);
        av_opt_set(enc_ctx->priv_data, "profile", "main", 0);
        av_opt_set_int(enc_ctx->priv_data, "cq", 28, 0);  // CQ 28
        break;
    }
}



// AMD encoder quality configuration
static void configure_amf_quality(AVCodecContext* enc_ctx, EncodingOption option)
{
    av_opt_set(enc_ctx->priv_data, "rc", "cqp", 0);  // Constant QP mode

    switch (option) {
    case ENCODE_H265_ARCHIVE:
        av_opt_set(enc_ctx->priv_data, "quality", "quality", 0);
        av_opt_set_int(enc_ctx->priv_data, "qp_i", 21, 0);
        av_opt_set_int(enc_ctx->priv_data, "qp_p", 22, 0);
        break;

    case ENCODE_H264_HIGH:
        av_opt_set(enc_ctx->priv_data, "quality", "quality", 0);
        av_opt_set(enc_ctx->priv_data, "profile", "high", 0);
        av_opt_set_int(enc_ctx->priv_data, "qp_i", 19, 0);
        av_opt_set_int(enc_ctx->priv_data, "qp_p", 20, 0);
        break;

    case ENCODE_H264_BALANCED:
        av_opt_set(enc_ctx->priv_data, "quality", "balanced", 0);
        av_opt_set(enc_ctx->priv_data, "profile", "high", 0);
        av_opt_set_int(enc_ctx->priv_data, "qp_i", 23, 0);
        av_opt_set_int(enc_ctx->priv_data, "qp_p", 24, 0);
        break;

    case ENCODE_H264_SMALL:
        av_opt_set(enc_ctx->priv_data, "quality", "speed", 0);
        av_opt_set(enc_ctx->priv_data, "profile", "main", 0);
        av_opt_set_int(enc_ctx->priv_data, "qp_i", 28, 0);
        av_opt_set_int(enc_ctx->priv_data, "qp_p", 30, 0);
        break;
    }
}




// Intel QSV encoder quality configuration
static void configure_qsv_quality(AVCodecContext* enc_ctx, EncodingOption option)
{
    switch (option) {
    case ENCODE_H265_ARCHIVE:
        av_opt_set(enc_ctx->priv_data, "preset", "veryslow", 0);
        av_opt_set_int(enc_ctx->priv_data, "global_quality", 20, 0);
        av_opt_set(enc_ctx->priv_data, "look_ahead", "1", 0);
        break;

    case ENCODE_H264_HIGH:
        av_opt_set(enc_ctx->priv_data, "preset", "veryslow", 0);
        av_opt_set(enc_ctx->priv_data, "profile", "high", 0);
        av_opt_set_int(enc_ctx->priv_data, "global_quality", 19, 0);
        av_opt_set(enc_ctx->priv_data, "look_ahead", "1", 0);
        break;

    case ENCODE_H264_BALANCED:
        av_opt_set(enc_ctx->priv_data, "preset", "medium", 0);
        av_opt_set(enc_ctx->priv_data, "profile", "high", 0);
        av_opt_set_int(enc_ctx->priv_data, "global_quality", 23, 0);
        break;

    case ENCODE_H264_SMALL:
        av_opt_set(enc_ctx->priv_data, "preset", "fast", 0);
        av_opt_set(enc_ctx->priv_data, "profile", "main", 0);
        av_opt_set_int(enc_ctx->priv_data, "global_quality", 28, 0);
        break;
    }
}




// Software encoder (libx264/libx265) quality configuration
static void configure_software_quality(AVCodecContext* enc_ctx, EncodingOption option)
{
    switch (option) {
    case ENCODE_H265_ARCHIVE:
        av_opt_set(enc_ctx->priv_data, "preset", "slow", 0);
        av_opt_set(enc_ctx->priv_data, "crf", "21", 0);  // CRF 21
        av_opt_set(enc_ctx->priv_data, "profile", "main", 0);
        // Optional: enable 10-bit if supported
        // av_opt_set(enc_ctx->priv_data, "x265-params", "profile=main10", 0);
        break;

    case ENCODE_H264_HIGH:
        av_opt_set(enc_ctx->priv_data, "preset", "slow", 0);
        av_opt_set(enc_ctx->priv_data, "crf", "19", 0);  // CRF 19
        av_opt_set(enc_ctx->priv_data, "profile", "high", 0);
        av_opt_set(enc_ctx->priv_data, "level", "4.1", 0);
        av_opt_set(enc_ctx->priv_data, "tune", "animation", 0);
        break;

    case ENCODE_H264_BALANCED:
        av_opt_set(enc_ctx->priv_data, "preset", "medium", 0);
        av_opt_set(enc_ctx->priv_data, "crf", "23", 0);  // CRF 23
        av_opt_set(enc_ctx->priv_data, "profile", "high", 0);
        break;

    case ENCODE_H264_SMALL:
        av_opt_set(enc_ctx->priv_data, "preset", "fast", 0);
        av_opt_set(enc_ctx->priv_data, "crf", "28", 0);  // CRF 28
        av_opt_set(enc_ctx->priv_data, "profile", "main", 0);
        break;
    }
}
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////










 

static int open_output_file(const char* filename)
{
    AVStream* out_stream;
    AVStream* in_stream;
    AVCodecContext* dec_ctx;
    AVCodecContext* enc_ctx = NULL;
    const AVCodec* encoder = NULL;
    int ret;
    unsigned int i;

    log_debug("Opening output file...");

    ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
    if (!ofmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }




    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
            return AVERROR_UNKNOWN;
        }

        in_stream = ifmt_ctx->streams[i];
        dec_ctx = stream_ctx[i].dec_ctx;

        if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            enc_ctx = avcodec_alloc_context3(NULL);
            if (!enc_ctx) {
                return AVERROR(ENOMEM);
            }

            ret = configure_video_encoder(enc_ctx, dec_ctx, in_stream, &encoder);
            if (ret < 0) {
                avcodec_free_context(&enc_ctx);
                return ret;
            }

            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            ret = avcodec_open2(enc_ctx, encoder, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder: %s\n", av_err2str(ret));
                avcodec_free_context(&enc_ctx);
                return ret;
            }

            ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
            if (ret < 0) {
                avcodec_free_context(&enc_ctx);
                return ret;
            }

            out_stream->time_base = enc_ctx->time_base;
            stream_ctx[i].enc_ctx = enc_ctx;
        }
        else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
            if (!encoder) {
                av_log(NULL, AV_LOG_ERROR, "AAC encoder not found\n");
                return AVERROR_ENCODER_NOT_FOUND;
            }

            enc_ctx = avcodec_alloc_context3(encoder);
            if (!enc_ctx)
                return AVERROR(ENOMEM);

            enc_ctx->sample_rate = dec_ctx->sample_rate;
            ret = av_channel_layout_copy(&enc_ctx->ch_layout, &dec_ctx->ch_layout);
            if (ret < 0) {
                avcodec_free_context(&enc_ctx);
                return ret;
            }

            enc_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
            enc_ctx->bit_rate = 192000;  // 192kbps AAC
            enc_ctx->time_base = (AVRational){ 1, enc_ctx->sample_rate };

            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            ret = avcodec_open2(enc_ctx, encoder, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot open audio encoder\n");
                avcodec_free_context(&enc_ctx);
                return ret;
            }

            ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
            if (ret < 0) {
                avcodec_free_context(&enc_ctx);
                return ret;
            }

            out_stream->time_base = enc_ctx->time_base;
            stream_ctx[i].enc_ctx = enc_ctx;
        }
        else {
            ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
            if (ret < 0)
                return ret;
            out_stream->time_base = in_stream->time_base;
        }
    }

    av_dump_format(ofmt_ctx, 0, filename, 1);

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'\n", filename);
            return ret;
        }
    }

    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error writing header\n");
        return ret;
    }

    log_debug("Output file opened successfully");
    return 0;
}





static int init_filter(FilteringContext* fctx, AVCodecContext* dec_ctx,
    AVCodecContext* enc_ctx, const char* filter_spec)
{
    char args[512];
    char msg[256];
    int ret = 0;
    const AVFilter* buffersrc = NULL;
    const AVFilter* buffersink = NULL;
    AVFilterContext* buffersrc_ctx = NULL;
    AVFilterContext* buffersink_ctx = NULL;
    AVFilterInOut* outputs = avfilter_inout_alloc();
    AVFilterInOut* inputs = avfilter_inout_alloc();
    AVFilterGraph* filter_graph = avfilter_graph_alloc();

    log_debug("  init_filter: Entry");

    if (!outputs || !inputs || !filter_graph) {
        log_debug("  ERROR: Failed to allocate filter graph structures");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        log_debug("  init_filter: Setting up VIDEO filter");

        buffersrc = avfilter_get_by_name("buffer");
        buffersink = avfilter_get_by_name("buffersink");
        if (!buffersrc || !buffersink) {
            log_debug("  ERROR: Video filter source or sink not found");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        // Validate and fix timebase
        if (dec_ctx->pkt_timebase.den == 0) {
            log_debug("  WARNING: Invalid pkt_timebase, using default");
            dec_ctx->pkt_timebase = (AVRational){ 1, 25 };
        }
        if (dec_ctx->sample_aspect_ratio.den == 0) {
            log_debug("  WARNING: Invalid sample_aspect_ratio, using default");
            dec_ctx->sample_aspect_ratio = (AVRational){ 1, 1 };
        }

        snprintf(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
            dec_ctx->pkt_timebase.num, dec_ctx->pkt_timebase.den,
            dec_ctx->sample_aspect_ratio.num,
            dec_ctx->sample_aspect_ratio.den);

        snprintf(msg, sizeof(msg), "  Video filter args: %s", args);
        log_debug(msg);

        log_debug("  Creating buffer source...");
        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
            args, NULL, filter_graph);
        if (ret < 0) {
            snprintf(msg, sizeof(msg), "  ERROR: Cannot create buffer source (ret=%d)", ret);
            log_debug(msg);
            goto end;
        }

        log_debug("  Allocating buffer sink...");
        buffersink_ctx = avfilter_graph_alloc_filter(filter_graph, buffersink, "out");
        if (!buffersink_ctx) {
            log_debug("  ERROR: Cannot allocate buffer sink");
            ret = AVERROR(ENOMEM);
            goto end;
        }

        log_debug("  Setting pixel format...");
        ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
            (uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt),
            AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            snprintf(msg, sizeof(msg), "  ERROR: Cannot set pixel format (ret=%d)", ret);
            log_debug(msg);
            goto end;
        }

        log_debug("  Initializing buffer sink...");
        ret = avfilter_init_dict(buffersink_ctx, NULL);
        if (ret < 0) {
            snprintf(msg, sizeof(msg), "  ERROR: Cannot initialize buffer sink (ret=%d)", ret);
            log_debug(msg);
            goto end;
        }

    }
    else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        log_debug("  init_filter: Setting up AUDIO filter");

        char buf[64];
        buffersrc = avfilter_get_by_name("abuffer");
        buffersink = avfilter_get_by_name("abuffersink");
        if (!buffersrc || !buffersink) {
            log_debug("  ERROR: Audio filter source or sink not found");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        if (dec_ctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
            av_channel_layout_default(&dec_ctx->ch_layout, dec_ctx->ch_layout.nb_channels);
        av_channel_layout_describe(&dec_ctx->ch_layout, buf, sizeof(buf));

        snprintf(args, sizeof(args),
            "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%s",
            dec_ctx->pkt_timebase.num, dec_ctx->pkt_timebase.den, dec_ctx->sample_rate,
            av_get_sample_fmt_name(dec_ctx->sample_fmt), buf);

        snprintf(msg, sizeof(msg), "  Audio filter args: %s", args);
        log_debug(msg);

        log_debug("  Creating audio buffer source...");
        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
            args, NULL, filter_graph);
        if (ret < 0) {
            snprintf(msg, sizeof(msg), "  ERROR: Cannot create audio buffer source (ret=%d)", ret);
            log_debug(msg);
            goto end;
        }

        log_debug("  Allocating audio buffer sink...");
        buffersink_ctx = avfilter_graph_alloc_filter(filter_graph, buffersink, "out");
        if (!buffersink_ctx) {
            log_debug("  ERROR: Cannot allocate audio buffer sink");
            ret = AVERROR(ENOMEM);
            goto end;
        }

        log_debug("  Setting audio parameters...");
        ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
            (uint8_t*)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
            AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            snprintf(msg, sizeof(msg), "  ERROR: Cannot set sample format (ret=%d)", ret);
            log_debug(msg);
            goto end;
        }

        av_channel_layout_describe(&enc_ctx->ch_layout, buf, sizeof(buf));
        ret = av_opt_set(buffersink_ctx, "ch_layouts", buf, AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            snprintf(msg, sizeof(msg), "  ERROR: Cannot set channel layout (ret=%d)", ret);
            log_debug(msg);
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
            (uint8_t*)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
            AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            snprintf(msg, sizeof(msg), "  ERROR: Cannot set sample rate (ret=%d)", ret);
            log_debug(msg);
            goto end;
        }

        log_debug("  Initializing audio buffer sink...");
        ret = avfilter_init_dict(buffersink_ctx, NULL);
        if (ret < 0) {
            snprintf(msg, sizeof(msg), "  ERROR: Cannot initialize audio buffer sink (ret=%d)", ret);
            log_debug(msg);
            goto end;
        }
    }

    log_debug("  Setting up filter endpoints...");

    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if (!outputs->name || !inputs->name) {
        log_debug("  ERROR: Failed to duplicate endpoint names");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    snprintf(msg, sizeof(msg), "  Parsing filter graph with spec: %s", filter_spec);
    log_debug(msg);

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec,
        &inputs, &outputs, NULL)) < 0) {
        snprintf(msg, sizeof(msg), "  ERROR: Filter graph parse failed (ret=%d)", ret);
        log_debug(msg);
        goto end;
    }

    log_debug("  Configuring filter graph...");
    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0) {
        snprintf(msg, sizeof(msg), "  ERROR: Filter graph config failed (ret=%d)", ret);
        log_debug(msg);
        goto end;
    }

    log_debug("  Storing filter context...");
    fctx->buffersrc_ctx = buffersrc_ctx;
    fctx->buffersink_ctx = buffersink_ctx;
    fctx->filter_graph = filter_graph;

    log_debug("  init_filter: Success");

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return ret;
}

static int init_filters(void)
{
    const char* filter_spec;
    unsigned int i;
    int ret;
    char msg[256];

    log_debug("Initializing filters...");

    filter_ctx = av_malloc_array(ifmt_ctx->nb_streams, sizeof(*filter_ctx));
    if (!filter_ctx) {
        log_debug("ERROR: Failed to allocate filter context array");
        return AVERROR(ENOMEM);
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        snprintf(msg, sizeof(msg), "Processing stream %u...", i);
        log_debug(msg);

        filter_ctx[i].buffersrc_ctx = NULL;
        filter_ctx[i].buffersink_ctx = NULL;
        filter_ctx[i].filter_graph = NULL;
        filter_ctx[i].enc_pkt = NULL;
        filter_ctx[i].filtered_frame = NULL;

        if (!stream_ctx[i].enc_ctx) {
            snprintf(msg, sizeof(msg), "Stream %u: No encoder context, skipping", i);
            log_debug(msg);
            continue;
        }

        if (!(ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ||
            ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)) {
            snprintf(msg, sizeof(msg), "Stream %u: Not audio/video, skipping", i);
            log_debug(msg);
            continue;
        }

        filter_spec = (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) ?
            "null" : "anull";

        snprintf(msg, sizeof(msg), "Stream %u: Calling init_filter with spec '%s'", i, filter_spec);
        log_debug(msg);

        // CRITICAL: Validate codec contexts before passing to init_filter
        if (!stream_ctx[i].dec_ctx) {
            snprintf(msg, sizeof(msg), "ERROR: Stream %u has NULL decoder context", i);
            log_debug(msg);
            return AVERROR(EINVAL);
        }

        if (!stream_ctx[i].dec_ctx->codec) {
            snprintf(msg, sizeof(msg), "ERROR: Stream %u decoder context has NULL codec", i);
            log_debug(msg);
            return AVERROR(EINVAL);
        }

        if (!stream_ctx[i].enc_ctx->codec) {
            snprintf(msg, sizeof(msg), "ERROR: Stream %u encoder context has NULL codec", i);
            log_debug(msg);
            return AVERROR(EINVAL);
        }

        ret = init_filter(&filter_ctx[i], stream_ctx[i].dec_ctx,
            stream_ctx[i].enc_ctx, filter_spec);
        if (ret) {
            snprintf(msg, sizeof(msg), "ERROR: init_filter failed for stream %u with code %d", i, ret);
            log_debug(msg);
            return ret;
        }

        snprintf(msg, sizeof(msg), "Stream %u: Filter initialized, allocating packet/frame", i);
        log_debug(msg);

        filter_ctx[i].enc_pkt = av_packet_alloc();
        if (!filter_ctx[i].enc_pkt) {
            log_debug("ERROR: Failed to allocate enc_pkt");
            return AVERROR(ENOMEM);
        }

        filter_ctx[i].filtered_frame = av_frame_alloc();
        if (!filter_ctx[i].filtered_frame) {
            log_debug("ERROR: Failed to allocate filtered_frame");
            return AVERROR(ENOMEM);
        }

        snprintf(msg, sizeof(msg), "Stream %u: Complete", i);
        log_debug(msg);
    }

    log_debug("Filters initialized successfully");
    return 0;
}

static int encode_write_frame(unsigned int stream_index, int flush)
{
    StreamContext* stream = &stream_ctx[stream_index];
    FilteringContext* filter = &filter_ctx[stream_index];
    AVFrame* filt_frame = flush ? NULL : filter->filtered_frame;
    AVPacket* enc_pkt = filter->enc_pkt;
    int ret;

    if (!stream->enc_ctx || !enc_pkt)
        return AVERROR(EINVAL);

    av_packet_unref(enc_pkt);

    if (filt_frame && filt_frame->pts != AV_NOPTS_VALUE)
        filt_frame->pts = av_rescale_q(filt_frame->pts, filt_frame->time_base,
            stream->enc_ctx->time_base);

    ret = avcodec_send_frame(stream->enc_ctx, filt_frame);
    if (ret < 0)
        return ret;

    while (ret >= 0) {
        ret = avcodec_receive_packet(stream->enc_ctx, enc_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;

        enc_pkt->stream_index = stream_index;
        av_packet_rescale_ts(enc_pkt,
            stream->enc_ctx->time_base,
            ofmt_ctx->streams[stream_index]->time_base);

        ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
    }

    return ret;
}

static int filter_encode_write_frame(AVFrame* frame, unsigned int stream_index)
{
    FilteringContext* filter = &filter_ctx[stream_index];
    int ret;

    if (!filter->buffersrc_ctx || !filter->buffersink_ctx || !filter->filtered_frame)
        return AVERROR(EINVAL);

    ret = av_buffersrc_add_frame_flags(filter->buffersrc_ctx, frame, 0);
    if (ret < 0)
        return ret;

    while (1) {
        ret = av_buffersink_get_frame(filter->buffersink_ctx, filter->filtered_frame);
        if (ret < 0) {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                ret = 0;
            break;
        }

        filter->filtered_frame->time_base = av_buffersink_get_time_base(filter->buffersink_ctx);
        filter->filtered_frame->pict_type = AV_PICTURE_TYPE_NONE;

        ret = encode_write_frame(stream_index, 0);
        av_frame_unref(filter->filtered_frame);
        if (ret < 0)
            break;
    }

    return ret;
}

static int flush_encoder(unsigned int stream_index)
{
    if (!stream_ctx[stream_index].enc_ctx)
        return 0;

    if (!(stream_ctx[stream_index].enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY))
        return 0;

    return encode_write_frame(stream_index, 1);
}

int transcode_main(int argc, char** argv)
{
    int ret;
    AVPacket* packet = NULL;
    unsigned int stream_index;
    unsigned int i;

   

    // Open debug log (Windows secure version)
#ifdef _WIN32
    fopen_s(&debug_log, "cheetah.log", "w");
#else
    debug_log = fopen("cheetah.log", "w");
#endif

    log_debug("=== Cheetah Transcode Started ===");

    char msg[256];
    const char* option_names[] = {
        "Unknown",
        "H.265 High Quality",
        "H.264 High Quality",
        "H.264 Medium Size",
        "H.264 Small Size"
    };

    snprintf(msg, sizeof(msg), "Encoding option: %s", option_names[g_encoding_option]);
    log_debug(msg);

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
#endif
    avformat_network_init();

    if (argc != 3) {
        log_debug("ERROR: Invalid arguments");
        if (debug_log) fclose(debug_log);
        return 1;
    }

    if ((ret = open_input_file(argv[1])) < 0)
        goto end;

    if ((ret = open_output_file(argv[2])) < 0)
        goto end;

    if ((ret = init_filters()) < 0)
        goto end;

    if (!(packet = av_packet_alloc()))
        goto end;

    log_debug("Starting main transcode loop...");


    g_current_frame = 0;
    g_start_time = get_time_ms();
    g_output_filename = argv[2];

    // Initial progress update
    update_progress();



    while (1) {
        if ((ret = av_read_frame(ifmt_ctx, packet)) < 0)
            break;

        stream_index = packet->stream_index;

        if (stream_index >= ifmt_ctx->nb_streams) {
            av_packet_unref(packet);
            continue;
        }

        if (filter_ctx[stream_index].filter_graph) {
            StreamContext* stream = &stream_ctx[stream_index];

            if (!stream->dec_ctx) {
                av_packet_unref(packet);
                continue;
            }

            ret = avcodec_send_packet(stream->dec_ctx, packet);
            if (ret < 0) {
                av_packet_unref(packet);
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(stream->dec_ctx, stream->dec_frame);
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                    break;
                else if (ret < 0)
                    goto end;

                stream->dec_frame->pts = stream->dec_frame->best_effort_timestamp;

                // UPDATE PROGRESS HERE (only for video frames)
                if (stream->dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                    g_current_frame++;
                    if (g_current_frame % 10 == 0) {  // Update every 10 frames
                        update_progress();
                    }
                }

                ret = filter_encode_write_frame(stream->dec_frame, stream_index);
                if (ret < 0)
                    goto end;
            }
        }
        else {
            av_packet_rescale_ts(packet,
                ifmt_ctx->streams[stream_index]->time_base,
                ofmt_ctx->streams[stream_index]->time_base);

            ret = av_interleaved_write_frame(ofmt_ctx, packet);
            if (ret < 0)
                goto end;
        }
        av_packet_unref(packet);
    }

    log_debug("Flushing encoders...");

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        if (!filter_ctx[i].filter_graph)
            continue;

        StreamContext* stream = &stream_ctx[i];
        if (!stream->dec_ctx || !stream->dec_frame)
            continue;

        ret = avcodec_send_packet(stream->dec_ctx, NULL);
        if (ret < 0)
            goto end;

        while (ret >= 0) {
            ret = avcodec_receive_frame(stream->dec_ctx, stream->dec_frame);
            if (ret == AVERROR_EOF)
                break;
            else if (ret < 0)
                goto end;

            stream->dec_frame->pts = stream->dec_frame->best_effort_timestamp;
            ret = filter_encode_write_frame(stream->dec_frame, i);
            if (ret < 0)
                goto end;
        }

        ret = filter_encode_write_frame(NULL, i);
        if (ret < 0)
            goto end;

        ret = flush_encoder(i);
        if (ret < 0)
            goto end;
    }

    log_debug("Writing trailer...");
    av_write_trailer(ofmt_ctx);
    log_debug("Transcode completed successfully");

    update_progress();

end:
    log_debug("Cleanup starting...");

    av_packet_free(&packet);

    if (stream_ctx) {
        for (i = 0; i < ifmt_ctx->nb_streams; i++) {
            if (stream_ctx[i].dec_ctx)
                avcodec_free_context(&stream_ctx[i].dec_ctx);
            if (stream_ctx[i].enc_ctx)
                avcodec_free_context(&stream_ctx[i].enc_ctx);
            if (stream_ctx[i].dec_frame)
                av_frame_free(&stream_ctx[i].dec_frame);
        }
        av_free(stream_ctx);
    }

    if (filter_ctx) {
        for (i = 0; i < ifmt_ctx->nb_streams; i++) {
            if (filter_ctx[i].filter_graph)
                avfilter_graph_free(&filter_ctx[i].filter_graph);
            if (filter_ctx[i].enc_pkt)
                av_packet_free(&filter_ctx[i].enc_pkt);
            if (filter_ctx[i].filtered_frame)
                av_frame_free(&filter_ctx[i].filtered_frame);
        }
        av_free(filter_ctx);
    }

    if (ifmt_ctx)
        avformat_close_input(&ifmt_ctx);

    if (ofmt_ctx) {
        if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&ofmt_ctx->pb);
        avformat_free_context(ofmt_ctx);
    }

    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));

    log_debug("=== Cleanup complete ===");
    if (debug_log)
        fclose(debug_log);

    return ret ? 1 : 0;
}