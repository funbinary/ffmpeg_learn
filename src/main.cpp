#include <iostream>
#include <cinttypes>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavutil/time.h>
}
using namespace std;

//枚举摄像头
void show_dshow_device() {
    AVFormatContext *pFormatCtx = avformat_alloc_context();                    // (pFormatCtx - ★★★)
    AVDictionary *options = NULL;
    av_dict_set(&options, "list_devices", "true", 0);
    auto *iformat = av_find_input_format("v4l2");
    printf("========Device Info=============\n");
    //参数：上下文、文件名、文件格式、选项
    //作用：参数挂载，打印显示系统摄像头信息
    avformat_open_input(&pFormatCtx, "video=dummy", iformat, &options);        // (pFormatCtx - ★★★)
//    printf("==============摄像头枚举结束！==================\n");
    avformat_close_input(&pFormatCtx);                                        // (pFormatCtx - ☆☆☆)
}


//根据枚举摄像头信息，挑选所需摄像头，将名称写入avformat_open_input()函数的文件名参数中
void show_dshow_device_option() {
    AVFormatContext *pFormatCtx = avformat_alloc_context();                    // (pFormatCtx - ★★★)
    AVDictionary *options = NULL;
    av_dict_set(&options, "list_options", "true", 0);
    auto *iformat = av_find_input_format("v4l2");
    printf("========Device Option Info=============\n");
    //参数：上下文、文件名、文件格式、选项
    avformat_open_input(&pFormatCtx, "video=Integrated Camera", iformat, &options);
    cout << "finish" << endl;
    avformat_close_input(&pFormatCtx);                                        // (pFormatCtx - ☆☆☆)
}

bool initCode(const string pixel_type) {

    try {
        // 1.初始化格式转换上下文
        if (pixel_type == "BGRA") {
            m_sc = sws_getCachedContext(m_sc, m_width, m_height, AV_PIX_FMT_BGRA,
                                        m_width, m_height, AV_PIX_FMT_YUV420P,
                                        SWS_BICUBIC, 0, 0, 0);
        } else if (pixel_type == "Gray") {
            m_sc = sws_getCachedContext(m_sc, m_width, m_height, AV_PIX_FMT_GRAY8,
                                        m_width, m_height, AV_PIX_FMT_YUV420P,
                                        SWS_BICUBIC, 0, 0, 0);
        }

        if (!m_sc) {
            // throw exception("Failed to initialize format-transfer context!");
            cout << "Failed to initialize format-transfer context!" << endl;
            return false;
        }

        // 2.初始化输出数据格式，未压缩的数据
        m_frame = av_frame_alloc();
        m_frame->format = AV_PIX_FMT_YUV420P;
        m_frame->width = m_width;
        m_frame->height = m_height;
        m_frame->pts = 0;
        //分配m_frame空间
        m_ret = av_frame_get_buffer(m_frame, 32);
        if (m_ret != 0) {
            printError(m_ret);
            return false;
        }

        // 3.初始化编码器上下文
        // a.找编码器
        m_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!m_codec) {
            // throw exception("Failed to find encoder!");
            cout << "Failed to find encoder!" << endl;
            return false;
        }

        // b.创建编码器上下文
        m_acc = avcodec_alloc_context3(m_codec);
        if (!m_acc) {
            // throw exception("Failed to allocate encoder context!");
            cout << "Failed to allocate encoder context!" << endl;
            return false;
        }

        // c.配置编码器参数
        m_acc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; //全局参数
        m_acc->codec_id = m_codec->id;
        m_acc->thread_count = 8;

        AVDictionary *param = NULL;
        av_dict_set(&param, "preset", "superfast", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);

        m_acc->bit_rate = 50 * 1024 * 8; // 压缩后每帧30kb
        m_acc->width = m_width;
        m_acc->height = m_height;
        m_acc->time_base = {1, m_fps};
        m_acc->framerate = {m_fps, 1};

        //画面组的大小，多少帧一个关键帧
        m_acc->gop_size = 50;
        m_acc->max_b_frames = 0;
        m_acc->pix_fmt = AV_PIX_FMT_YUV420P;

        // d.打开编码器上下文
        m_ret = avcodec_open2(m_acc, m_codec, &param);
        if (m_ret != 0) {
            printError(m_ret);
            return false;
        }

        return true;
    }
    catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        if (m_sc) {
            sws_freeContext(m_sc);
            m_sc = NULL;
        }
        if (m_acc) {
            avio_closep(&m_afc->pb);
            avcodec_free_context(&m_acc);
        }
        return false;
    }
}

int main(int argc, char *argv[]) {

    std::string rtmpUrl = "rtmp://192.168.3.250/live/test";

    avformat_network_init();
    avdevice_register_all();
//    show_dshow_device();		 // 枚举摄像头
//    show_dshow_device_option();	 // 输出USB摄像头信息
    AVFormatContext *ctx = avformat_alloc_context();                    // (pFormatCtx - ★★★)
    auto *ifmt = av_find_input_format("v4l2");
    if (!ifmt) {
        av_log(NULL, AV_LOG_FATAL, "Unkonw input format:v4l2\n");
        return AVERROR(EINVAL);
    }
    auto err = avformat_open_input(&ctx, "/dev/video0", ifmt, NULL);
    if (err < 0) {
        return 0;
    }
    if (avformat_find_stream_info(ctx, NULL) < 0) {
        av_log(NULL, AV_LOG_FATAL, "不能找到流信息\n");
        return AVERROR(EINVAL);
    }
    int videoIndex = -1, width, height;

    for (auto i = 0; i < ctx->nb_streams; i++) {
        if (ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIndex = i;
            width = ctx->streams[i]->codecpar->width;
            height = ctx->streams[i]->codecpar->height;
            break;
        }
    }

    if (videoIndex == -1) {
        cout << "找不到视频流" << endl;
        return -1;
    }
    // 打印输入流信息
    av_dump_format(ctx, videoIndex, "/dev/video0", 0);

    // 输出流上下文
    AVFormatContext *oCtx;
    avformat_alloc_output_context2(&oCtx, NULL, "flv", rtmpUrl.c_str());
    if (!oCtx) {
        cout << "创建输出上下文失败" << endl;
        avformat_close_input(&ctx);
        return -1;
    }
    auto ofmt = oCtx->oformat;
    for (auto i = 0; i < ctx->nb_streams; i++) {
        AVStream *inStream = ctx->streams[i];
        AVStream *outStream = avformat_new_stream(oCtx, NULL);
        if (!outStream) {
            cout << "分配输出流失败" << endl;
            avformat_close_input(&ctx);
            avformat_free_context(oCtx);
            return -1;
        }
        avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
    }
    av_dump_format(oCtx, 0, rtmpUrl.c_str(), 1);
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        auto ret = avio_open(&oCtx->pb, rtmpUrl.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            avformat_close_input(&ctx);
            avformat_free_context(oCtx);
            return -1;
        }
    }
    //Write file header   写文件头
    auto ret = avformat_write_header(oCtx, NULL);
    if (ret < 0) {
        printf("Error occurred when opening output URL\n");
        avformat_close_input(&ctx);
        if (oCtx && !(ofmt->flags & AVFMT_NOFILE)) {
            avio_close(oCtx->pb);
        }
        avformat_free_context(oCtx);
        return -1;
    }
    int frameIndex = 0;
    auto start_time = av_gettime();
    auto pkt = av_packet_alloc();
    while (1) {
        AVStream *in_stream, *out_stream;
        //获取一个AVPacket（Get an AVPacket）
        ret = av_read_frame(ctx, pkt);
        if (ret < 0)
            break;
        //FIX：No PTS (Example: Raw H.264)
        //Simple Write PTS
        if (pkt->pts == AV_NOPTS_VALUE) {
            //Write PTS
            AVRational time_base1 = ctx->streams[videoIndex]->time_base;
            //Duration between 2 frames (us)
            int64_t calc_duration = (double) AV_TIME_BASE / av_q2d(ctx->streams[videoIndex]->r_frame_rate);
            //Parameters
            pkt->pts = (double) (frameIndex * calc_duration) / (double) (av_q2d(time_base1) * AV_TIME_BASE);
            pkt->dts = pkt->pts;
            pkt->duration = (double) calc_duration / (double) (av_q2d(time_base1) * AV_TIME_BASE);
        }
        //Important:Delay
        if (pkt->stream_index == videoIndex) {
            AVRational time_base = ctx->streams[videoIndex]->time_base;
            AVRational time_base_q = {1, AV_TIME_BASE};
            int64_t pts_time = av_rescale_q(pkt->dts, time_base, time_base_q);
            int64_t now_time = av_gettime() - start_time;
            if (pts_time > now_time)
                av_usleep(pts_time - now_time);

        }

        in_stream = ctx->streams[pkt->stream_index];
        out_stream = oCtx->streams[pkt->stream_index];
        /* copy packet */
        //转换PTS/DTS（Convert PTS/DTS）
        pkt->pts = av_rescale_q_rnd(pkt->pts, in_stream->time_base, out_stream->time_base,
                                    (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt->dts = av_rescale_q_rnd(pkt->dts, in_stream->time_base, out_stream->time_base,
                                    (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
        pkt->pos = -1;
        //Print to Screen
        if (pkt->stream_index == videoIndex) {
            printf("Send %8d video frames to output URL\n", frameIndex);
            frameIndex++;
        }
        //ret = av_write_frame(oCtx, &pkt);
        ret = av_interleaved_write_frame(oCtx, pkt);

        if (ret < 0) {
            printf("Error muxing packet\n");
            break;
        }

        av_packet_unref(pkt);

    }
    av_packet_free(&pkt);
    //写文件尾（Write file trailer）
    av_write_trailer(oCtx);


    avformat_close_input(&ctx);
    if (oCtx && !(ofmt->flags & AVFMT_NOFILE)) {
        avio_close(oCtx->pb);
    }
    avformat_free_context(oCtx);
    return 0;

}


//
//int ret = avformat_alloc_output_context2(&m_formatContext, nullptr, "h264", fileName.toStdString().data());
//
//
//AVFormatContext* format_context = nullptr;
//AVStream* video_stream = nullptr;
//AVCodecContext* codec_context = nullptr;
//AVFrame* frame = nullptr;
//AVPacket packet;
//
//// Open the camera
//auto input_format = av_find_input_format("v4l2");
//AVDictionary* options = nullptr;
//av_dict_set(&options, "video_size", "640x480", 0);
//av_dict_set(&options, "framerate", "30", 0);
//av_dict_set(&options, "pixel_format", "yuyv422", 0);
//avformat_open_input(&format_context, "/dev/video0", input_format, &options);
//avformat_find_stream_info(format_context, nullptr);
//
//// Create the output format context
//avformat_alloc_output_context2(&format_context, nullptr, "flv", "rtmp://192.168.3.250/live/test");
//auto output_format = format_context->oformat;
//
//// Create the video stream
//video_stream = avformat_new_stream(format_context, nullptr);
//auto video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
//codec_context = avcodec_alloc_context3(video_codec);
//codec_context->width = 640;
//codec_context->height = 480;
//codec_context->time_base = { 1, 30 };
//codec_context->gop_size = 10;
//codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
//codec_context->framerate = { 30, 1 };
//avcodec_open2(codec_context, video_codec, nullptr);
//avcodec_parameters_from_context(video_stream->codecpar, codec_context);
//video_stream->time_base = codec_context->time_base;
//
//// Allocate the frame
//frame = av_frame_alloc();
//frame->format = codec_context->pix_fmt;
//frame->width = codec_context->width;
//frame->height = codec_context->height;
//av_frame_get_buffer(frame, 32);
//
//// Open the output file
//avio_open(&format_context->pb, "rtmp://192.168.3.250/live/test", AVIO_FLAG_WRITE);
//
//// Write the header
//avformat_write_header(format_context, nullptr);
//
//// Start capturing frames and writing to the RTMP stream
//while (true) {
//AVFrame* input_frame = av_frame_alloc();
//if (av_read_frame(format_context, &packet) < 0) {
//break;
//}
//if (packet.stream_index == video_stream->index) {
//avcodec_send_packet(codec_context, &packet);
//while (avcodec_receive_frame(codec_context, input_frame) == 0) {
//input_frame->pts = av_rescale_q(input_frame->pts, codec_context->time_base, video_stream->time_base);
//avcodec_send_frame(codec_context, input_frame);
//while (avcodec_receive_packet(codec_context, &packet) == 0) {
//av_interleaved_write_frame(format_context, &packet);
//av_packet_unref(&packet);
//}
//}
//}
//av_frame_free(&input_frame);
//}
//
//// Write the trailer
//av_write_trailer(format_context);
//
//// Clean up
//av_frame_free(&frame);
//avcodec_free_context(&codec_context);
//avformat_free_context(format_context);