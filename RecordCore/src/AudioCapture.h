#pragma once

#include <atomic>
#include <functional>
#include <thread>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;

class AudioCaptureInfo;

class AudioCapture {
public:
    int startCapture();
    int stopCapture();

    void setFrameCb(std::function<void(AVFrame*, const AudioCaptureInfo&)> cb) {
        m_frameCb = cb;
    }

private:
    int initCapture();
    void deinit();

    void audioCaptureThreadProc();

private:
    std::atomic_bool                                       m_isRunning  = false;
    int                                                    m_aIndex     = -1;  // ������Ƶ������
    AVFormatContext*                                       m_aFmtCtx    = nullptr;
    AVCodecContext*                                        m_aDecodeCtx = nullptr;
    std::thread                                            m_captureThread;
    std::function<void(AVFrame*, const AudioCaptureInfo&)> m_frameCb;
};
