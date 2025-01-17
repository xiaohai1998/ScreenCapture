#pragma once
#include "FFmpegHeader.h"
#include "RecordConfig.h"

#include <condition_variable>

#define MAX_AV_PLANES 8

class AudioFrameQueue {
public:
    int      initBuf(AVCodecContext* encodeCtx);
    void     deinit();
    int      writeFrame(AVFrame* frame, const AudioCaptureInfo& info);
    AVFrame* readFrame();

private:
    bool                    m_isInit   = false;
    uint64_t                m_channelLayout;
    int                     m_channelNum;
    AVSampleFormat          m_format;
    int                     m_sampleRate;
    AVAudioFifo*            m_aFifoBuf = nullptr;
    std::mutex              m_mtxABuf;
    std::condition_variable m_cvABufNotFull;
    std::condition_variable m_cvABufNotEmpty;
    AVFrame*                m_aOutFrame    = nullptr;
    SwrContext*             m_swrCtx       = nullptr;
    AudioCaptureInfo        m_audioCapInfo{};  // 解码获取到的音频信息
    int64_t                 m_resampleBufSize = 0;
    uint8_t*                m_resampleBuf[MAX_AV_PLANES];
};
