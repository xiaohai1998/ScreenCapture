#include "VideoCapture.h"
#include "FFmpegHeader.h"
#include "RecordConfig.h"

#include <QString>
#include <QDebug>

#include <string>
//#include <functional>

using namespace std;

//VideoCapture::VideoCapture(const RecordInfo& info)
//{
//}

int VideoCapture::startCapture()
{
    int ret = initCapture();
    if (0 != ret) {
        return -1;
    }
    std::thread t(std::bind(&VideoCapture::captureThreadProc, this));
    m_captureThread.swap(t);
    //m_captureThread.swap(std::thread(std::bind(&VideoCapture::captureThreadProc, this)));
}

int VideoCapture::stopCapture()
{
    return 0;
}

int VideoCapture::initCapture()
{
    int fps = g_record.fps;
    int width = g_record.width;
    int height = g_record.height;

    int ret = -1;
    AVDictionary* options = nullptr;
    AVCodec* decoder = nullptr;
    AVInputFormat* ifmt = av_find_input_format("gdigrab");
    string resolution = QString("%1x%2").arg(width).arg(height).toStdString();

    av_dict_set(&options, "framerate", QString::number(fps).toStdString().c_str(), NULL);
    av_dict_set(&options, "video_size", resolution.c_str(), 0);

    if (avformat_open_input(&m_vFmtCtx, "desktop", ifmt, &options) != 0)
    {
        qDebug() << "Cant not open video input stream";
        return -1;
    }
    if (avformat_find_stream_info(m_vFmtCtx, nullptr) < 0)
    {
        qDebug() << "Couldn't find stream information";
        return -1;
    }
    for (int i = 0; i < m_vFmtCtx->nb_streams; ++i)
    {
        AVStream* stream = m_vFmtCtx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            decoder = avcodec_find_decoder(stream->codecpar->codec_id);
            if (decoder == nullptr)
            {
                qDebug() << "can not find decoder";
                return -1;
            }
            //从视频流中拷贝参数到codecCtx
            m_vDecodeCtx = avcodec_alloc_context3(decoder);
            if ((ret = avcodec_parameters_to_context(m_vDecodeCtx, stream->codecpar)) < 0)
            {
                qDebug() << "Video avcodec_parameters_to_context failed,error code: " << ret;
                return -1;
            }
            //m_vIndex = i;
            break;
        }
    }
    if (avcodec_open2(m_vDecodeCtx, decoder, nullptr) < 0)
    {
        qDebug() << "Could not open codec";
        return -1;
    }

    m_swsCtx = sws_getContext(m_vDecodeCtx->width, m_vDecodeCtx->height, m_vDecodeCtx->pix_fmt,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

    return 0;
}

void VideoCapture::captureThreadProc()
{
    int width = g_record.width;
    int height = g_record.height;

    int ret = -1;
    AVPacket pkt = { 0 };
    av_init_packet(&pkt);
    int y_size = width * height;
    AVFrame* oldFrame = av_frame_alloc();
    AVFrame* newFrame = av_frame_alloc();

    int newFrameBufSize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, width, height, 1);
    uint8_t* newFrameBuf = (uint8_t*)av_malloc(newFrameBufSize);
    av_image_fill_arrays(newFrame->data, newFrame->linesize, newFrameBuf,
        AV_PIX_FMT_YUV420P, width, height, 1);

    while (m_state != RecordState::Stopped)
    {
        if (m_state == RecordState::Paused)
        {
            unique_lock<mutex> lk(m_mtxPause);
            m_cvNotPause.wait(lk, [this] { return m_state != RecordState::Paused; });
        }
        if (av_read_frame(m_vFmtCtx, &pkt) < 0)
        {
            qDebug() << "video av_read_frame < 0";
            continue;
        }

        if (pkt.stream_index != m_vIndex)
        {
            qDebug() << "not a video packet from video input";
            av_packet_unref(&pkt);
        }
        ret = avcodec_send_packet(m_vDecodeCtx, &pkt);
        if (ret != 0)
        {
            qDebug() << "video avcodec_send_packet failed, ret:" << ret;
            av_packet_unref(&pkt);
            continue;
        }
        ret = avcodec_receive_frame(m_vDecodeCtx, oldFrame);
        if (ret != 0)
        {
            qDebug() << "video avcodec_receive_frame failed, ret:" << ret;
            av_packet_unref(&pkt);
            continue;
        }

        // 拿到frame再记录时间戳
        //static bool s_singleton = true;
        //if (s_singleton)
        //{
        //    s_singleton = false;
        //    m_firstTimePoint = chrono::steady_clock::now();
        //    m_timestamp = 0;
        //}
        //else
        //{
        m_timestamp = duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - m_firstTimePoint).count();
        //}

        ++g_vCollectFrameCnt;
        sws_scale(m_swsCtx, (const uint8_t* const*)oldFrame->data, oldFrame->linesize, 0,
            m_vEncodeCtx->height, newFrame->data, newFrame->linesize);

        {
            unique_lock<mutex> lk(m_mtxVBuf);
            m_cvVBufNotFull.wait(lk, [this] { return av_fifo_space(m_vFifoBuf) >= m_vOutFrameItemSize; });
        }

        // 先写入时间戳
        av_fifo_generic_write(m_vFifoBuf, &m_timestamp, sizeof(m_timestamp), NULL);

        av_fifo_generic_write(m_vFifoBuf, newFrame->data[0], y_size, NULL);
        av_fifo_generic_write(m_vFifoBuf, newFrame->data[1], y_size / 4, NULL);
        av_fifo_generic_write(m_vFifoBuf, newFrame->data[2], y_size / 4, NULL);
        m_cvVBufNotEmpty.notify_one();

        av_packet_unref(&pkt);
    }
    FlushVideoDecoder();

    av_free(newFrameBuf);
    av_frame_free(&oldFrame);
    av_frame_free(&newFrame);
    qDebug() << "screen record thread exit";
}
