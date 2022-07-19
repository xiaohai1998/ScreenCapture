#include "VideoEncoder.h"

#include <QDebug>

int VideoEncoder::initH264(int width, int height, int fps)
{
    int ret = -1;

    m_vEncodeCtx = avcodec_alloc_context3(NULL);
    if (nullptr == m_vEncodeCtx) {
        qDebug() << "avcodec_alloc_context3 failed";
        return -1;
    }
    m_vEncodeCtx->width = width;
    m_vEncodeCtx->height = height;
    m_vEncodeCtx->time_base.num = 1;
    m_vEncodeCtx->time_base.den = fps;
    m_vEncodeCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    m_vEncodeCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    m_vEncodeCtx->codec_id = AV_CODEC_ID_H264;
    m_vEncodeCtx->bit_rate = 800 * 1000;
    m_vEncodeCtx->rc_max_rate = 800 * 1000;
    m_vEncodeCtx->rc_buffer_size = 500 * 1000;
    //设置图像组层的大小, gop_size越大，文件越小 
    m_vEncodeCtx->gop_size = 30;
    m_vEncodeCtx->max_b_frames = 0;
    //设置h264中相关的参数,不设置avcodec_open2会失败
    m_vEncodeCtx->qmin = 10;	//2
    m_vEncodeCtx->qmax = 31;	//31
    m_vEncodeCtx->max_qdiff = 4;
    m_vEncodeCtx->me_range = 16;	//0	
    m_vEncodeCtx->max_qdiff = 4;	//3	
    m_vEncodeCtx->qcompress = 0.6;	//0.5
    m_vEncodeCtx->codec_tag = 0;
    //正确设置sps/pps
    m_vEncodeCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    //查找视频编码器
    AVCodec* encoder;
    encoder = avcodec_find_encoder(m_vEncodeCtx->codec_id);
    if (!encoder) {
        qDebug() << "Can not find the encoder, id: " << m_vEncodeCtx->codec_id;
        return -1;
    }
    //打开视频编码器
    ret = avcodec_open2(m_vEncodeCtx, encoder, nullptr);
    if (ret < 0) {
        qDebug() << "Can not open encoder id: " << encoder->id << "error code: " << ret;
        return -1;
    }
    return 0;
}

void VideoEncoder::deinit()
{
    if (m_vEncodeCtx) {
        avcodec_free_context(&m_vEncodeCtx);
        m_vEncodeCtx = nullptr;
    }
}

int VideoEncoder::encode(AVFrame* frame, int stream_index, int64_t pts, int64_t time_base, std::vector<AVPacket*>& packets)
{
    if (!m_vEncodeCtx) return -1;

    int ret = 0;

    pts = av_rescale_q(pts, AVRational{ 1, (int)time_base }, m_vEncodeCtx->time_base);
    frame->pts = pts;
    ret = avcodec_send_frame(m_vEncodeCtx, frame);

    if (ret != 0) {
        char errbuf[1024] = { 0 };
        av_strerror(ret, errbuf, sizeof(errbuf) - 1);
        qDebug() << "video avcodec_send_frame failed:" << errbuf;
        return -1;
    }
    //ret = avcodec_receive_packet(m_vEncodeCtx, &pkt);
    //if (ret != 0)
    //{
    //    qDebug() << "video avcodec_receive_packet failed, ret: " << ret;
    //    av_packet_unref(&pkt);
    //    continue;
    //}

    while (1) {
        AVPacket* packet = av_packet_alloc();
        ret = avcodec_receive_packet(m_vEncodeCtx, packet);
        packet->stream_index = stream_index;
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            ret = 0;
            av_packet_free(&packet);
            break;
        }
        else if (ret < 0) {
            char errbuf[1024] = { 0 };
            av_strerror(ret, errbuf, sizeof(errbuf) - 1);
            qDebug() << "avcodec_receive_packet failed:" << errbuf;
            av_packet_free(&packet);
            ret = -1;
        }
        //printf("h264 pts:%lld\n", packet->pts);
        packets.push_back(packet);
    }

    return ret;
}
