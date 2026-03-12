#pragma once

#include <SDL2/SDL.h>

#include <cstdint>
#include <vector>

class AudioSystem
{
public:
    ~AudioSystem();

    bool initialize();
    void shutdown();

    void setEnabled(bool enabled);
    bool enabled() const { return enabled_; }

    void playKeyStroke();
    void playUiConfirm();
    void playUiError();
    void playUiMove();

private:
    struct ToneEvent
    {
        float frequency = 440.0f;
        float amplitude = 0.0f;
        float duration = 0.0f;
        float phase = 0.0f;
        float elapsed = 0.0f;
    };

    static void audioCallback(void* userdata, Uint8* stream, int len);
    void mix(float* stream, int sampleCount);
    void queueTone(float frequency, float amplitude, float duration);

    SDL_AudioDeviceID device_ = 0;
    SDL_AudioSpec obtainedSpec_{};
    bool enabled_ = true;
    float humPhase_ = 0.0f;
    float wobblePhase_ = 0.0f;
    std::uint32_t randomState_ = 0xA51C9E37u;
    std::vector<ToneEvent> toneEvents_;
};
