#include "audio/AudioSystem.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kPi = 3.1415926535f;

float clampSample(float value)
{
    return std::clamp(value, -0.95f, 0.95f);
}
}

AudioSystem::~AudioSystem()
{
    shutdown();
}

bool AudioSystem::initialize()
{
    SDL_AudioSpec desired{};
    desired.freq = 48000;
    desired.format = AUDIO_F32SYS;
    desired.channels = 1;
    desired.samples = 1024;
    desired.callback = &AudioSystem::audioCallback;
    desired.userdata = this;

    device_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtainedSpec_, 0);
    if (device_ == 0)
    {
        return false;
    }

    SDL_PauseAudioDevice(device_, 0);
    return true;
}

void AudioSystem::shutdown()
{
    if (device_ != 0)
    {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
    toneEvents_.clear();
}

void AudioSystem::setEnabled(bool enabled)
{
    enabled_ = enabled;
}

void AudioSystem::playKeyStroke()
{
    queueTone(1240.0f, 0.10f, 0.018f);
    queueTone(780.0f, 0.06f, 0.024f);
}

void AudioSystem::playUiConfirm()
{
    queueTone(660.0f, 0.12f, 0.07f);
    queueTone(990.0f, 0.08f, 0.05f);
}

void AudioSystem::playUiError()
{
    queueTone(220.0f, 0.14f, 0.10f);
    queueTone(165.0f, 0.10f, 0.14f);
}

void AudioSystem::playUiMove()
{
    queueTone(440.0f, 0.05f, 0.03f);
}

void AudioSystem::audioCallback(void* userdata, Uint8* stream, int len)
{
    auto* audio = static_cast<AudioSystem*>(userdata);
    const int sampleCount = len / static_cast<int>(sizeof(float));
    audio->mix(reinterpret_cast<float*>(stream), sampleCount);
}

void AudioSystem::mix(float* stream, int sampleCount)
{
    if (obtainedSpec_.freq <= 0)
    {
        std::fill(stream, stream + sampleCount, 0.0f);
        return;
    }

    const float sampleRate = static_cast<float>(obtainedSpec_.freq);
    for (int i = 0; i < sampleCount; ++i)
    {
        float sample = 0.0f;
        if (enabled_)
        {
            for (ToneEvent& tone : toneEvents_)
            {
                if (tone.elapsed >= tone.duration)
                {
                    continue;
                }
                const float progress = tone.elapsed / std::max(tone.duration, 0.0001f);
                const float envelope = (1.0f - progress) * (1.0f - progress);
                tone.phase += 2.0f * kPi * tone.frequency / sampleRate;
                sample += std::sin(tone.phase) * tone.amplitude * envelope;
                tone.elapsed += 1.0f / sampleRate;
            }
        }
        stream[i] = clampSample(sample);
    }

    toneEvents_.erase(std::remove_if(toneEvents_.begin(), toneEvents_.end(), [](const ToneEvent& tone) {
        return tone.elapsed >= tone.duration;
    }), toneEvents_.end());
}

void AudioSystem::queueTone(float frequency, float amplitude, float duration)
{
    if (device_ == 0)
    {
        return;
    }

    SDL_LockAudioDevice(device_);
    toneEvents_.push_back({ frequency, amplitude, duration, 0.0f, 0.0f });
    SDL_UnlockAudioDevice(device_);
}
