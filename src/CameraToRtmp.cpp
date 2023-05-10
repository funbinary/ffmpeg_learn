#include <iostream>
#include <cinttypes>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}
using namespace std;

static AVFormatContext *OpenDev() {
    int ret = 0;
    char errors[1024] = {
            0,
    };

    // ctx
    AVFormatContext *fmt_ctx = NULL;
    AVDictionary *options = NULL;

    //摄像头的设备文件
    char *devicename = "/dev/video0";

    // register video device
    avdevice_register_all();

    // get format
    auto *iformat = av_find_input_format("video4linux2");

    av_dict_set(&options, "video_size", "1920x1080", 0);
    av_dict_set(&options, "framerate", "30", 0);
    av_dict_set(&options, "pixel_format", "yuyv422", 0);

    // open device
    ret = avformat_open_input(&fmt_ctx, devicename, iformat, &options);
    if (ret < 0) {
        av_strerror(ret, errors, 1024);
        fprintf(stderr, "Failed to open video device, [%d]%s\n", ret, errors);
        return NULL;
    }

    return fmt_ctx;
}

static void openEncoder(int width, int height, AVCodecContext **enc_ctx) {
    int ret = 0;

    auto codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) {
        printf("Codec libx264 not found\n");
        exit(1);
    }

    *enc_ctx = avcodec_alloc_context3(codec);
    if (!enc_ctx) {
        printf("Could not allocate video codec context!\n");
        exit(1);
    }

    // SPS/PPS
    (*enc_ctx)->profile = FF_PROFILE_H264_HIGH_444;
    (*enc_ctx)->level = 50; //表示LEVEL是5.0

    //设置分辫率
    (*enc_ctx)->width = width;
    (*enc_ctx)->height = height;

    // GOP
    (*enc_ctx)->gop_size = 250;
    (*enc_ctx)->keyint_min = 25; // option

    //设置B帧数据
    (*enc_ctx)->max_b_frames = 3; // option
    (*enc_ctx)->has_b_frames = 1; // option

    //参考帧的数量
    (*enc_ctx)->refs = 3; // option

    //设置输入YUV格式
    (*enc_ctx)->pix_fmt = AV_PIX_FMT_YUV420P;

    //设置码率
    (*enc_ctx)->bit_rate = 600000; // 600kbps

    //设置帧率
    (*enc_ctx)->time_base = (AVRational) {1, 30}; //帧与帧之间的间隔是time_base
    (*enc_ctx)->framerate = (AVRational) {30, 1}; //帧率，每秒 30帧

    ret = avcodec_open2((*enc_ctx), codec, NULL);
    if (ret < 0) {
        printf("Could not open codec:\n");
        exit(1);
    }
}

static AVFrame *create_frame(int width, int height) {
    int ret = 0;
    AVFrame *frame = NULL;

    frame = av_frame_alloc();
    if (!frame) {
        printf("Error, No Memory!\n");
        goto __ERROR;
    }

    //设置参数
    frame->width = width;
    frame->height = height;
    frame->format = AV_PIX_FMT_YUV420P;

    // alloc inner memory
    ret = av_frame_get_buffer(frame, 32); //按 32 位对齐
    if (ret < 0) {
        printf("Error, Failed to alloc buffer for frame!\n");
        goto __ERROR;
    }

    return frame;

    __ERROR:
    if (frame) {
        av_frame_free(&frame);
    }

    return NULL;
}

void yuyv422ToYuv420p(AVFrame *frame, AVPacket *pkt) {
    int i = 0;
    int yuv422_length = 1920 * 1080 * 2;
    int y_index = 0;
    // copy all y
    for (i = 0; i < yuv422_length; i += 2) {
        frame->data[0][y_index] = pkt->data[i];
        y_index++;
    }

    // copy u and v
    int line_start = 0;
    int is_u = 1;
    int u_index = 0;
    int v_index = 0;
    // copy u, v per line. skip a line once
    for (i = 0; i < 1920; i += 2) {
        // line i offset
        line_start = i * 1080 * 2;
        for (int j = line_start + 1; j < line_start + 1080 * 2; j += 4) {
            frame->data[1][u_index] = pkt->data[j];
            u_index++;
            frame->data[2][v_index] = pkt->data[j + 2];
            v_index++;
        }
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
    AVDictionary *options = NULL;
    av_dict_set(&options, "video_size", "1920x1080", 0);
    av_dict_set(&options, "framerate", "30", 0);
    av_dict_set(&options, "pixel_format", "yuyv422", 0);
    auto err = avformat_open_input(&ctx, "/dev/video0", ifmt, &options);
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

    AVCodecContext *oEncCtx;
    openEncoder(1920, 1080, &oEncCtx);

    AVFrame *frame = create_frame(1920, 1080);

    // 输出流上下文
    AVFormatContext *oCtx;
    avformat_alloc_output_context2(&oCtx, NULL, "flv", rtmpUrl.c_str());
    if (!oCtx) {
        cout << "创建输出上下文失败" << endl;
        avformat_close_input(&ctx);
        return -1;
    }

    // 添加视频流
    auto ofmt = oCtx->oformat;
    AVStream *inStream, *outStream;
    for (auto i = 0; i < ctx->nb_streams; i++) {
        inStream = ctx->streams[i];
        outStream = avformat_new_stream(oCtx, NULL);
        outStream->codecpar->codec_tag = 0; // 附加标志
        if (!outStream) {
            cout << "分配输出流失败" << endl;
            avformat_close_input(&ctx);
            avformat_free_context(oCtx);
            return -1;
        }
        avcodec_parameters_from_context(outStream->codecpar, oEncCtx);
        outStream->time_base.num = 1;
    }

    // 连接RTMP
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

    cout << __LINE__ << "-----------------" << endl;
    int frameIndex = 0;
    auto start_time = av_gettime();
    auto pkt = av_packet_alloc();
    auto newpkt = av_packet_alloc();
    cout << __LINE__ << "-----------------" << endl;
    while (av_read_frame(ctx, pkt) == 0) {
        av_log(NULL, AV_LOG_INFO, "packet size is %d(%p)\n", pkt->size, pkt->data);
        cout << __LINE__ << "-----------------" << endl;
        yuyv422ToYuv420p(frame, pkt);
        cout << __LINE__ << "-----------------" << endl;
        // H264编码
        frame->pts = frameIndex++;
        cout << __LINE__ << "-----------------" << endl;
        if (oEncCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (avcodec_send_frame(oEncCtx, frame) != 0) {
                cout << "avcodec_send_frame error" << endl;
                break;
            }
        }
        cout << __LINE__ << "-----------------" << endl;
        avcodec_receive_packet(oEncCtx, newpkt);
        if (pkt->size == 0) {
            break;
        }
        cout << __LINE__ << "-----------------" << endl;
        // 3.推流

        auto in_stream = ctx->streams[pkt->stream_index];

        newpkt->pts = av_rescale_q_rnd(pkt->pts, in_stream->time_base, outStream->time_base,
                                       (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        newpkt->dts = av_rescale_q_rnd(pkt->dts, in_stream->time_base, outStream->time_base,
                                       (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        newpkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, outStream->time_base);
        newpkt->pos = -1;
        if (av_interleaved_write_frame(oCtx, newpkt) != 0) {
            cout << "推流失败" << endl;
            break;
        }
        av_packet_unref(newpkt);
        cout << __LINE__ << "-----------------" << endl;
    }
    av_packet_free(&pkt);
    av_packet_free(&newpkt);
    //写文件尾（Write file trailer）
    av_write_trailer(oCtx);


    avformat_close_input(&ctx);
    if (oCtx && !(ofmt->flags & AVFMT_NOFILE)) {
        avio_close(oCtx->pb);
    }
    avformat_free_context(oCtx);
    return 0;

}