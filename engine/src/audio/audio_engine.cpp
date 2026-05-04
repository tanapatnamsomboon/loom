#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "loom/audio/audio_engine.h"
#include "loom/core/log.h"
#include "loom/scene/components.h"

namespace Loom {

    static ma_engine s_Engine;

    void AudioEngine::Init() {
        ma_result result = ma_engine_init(NULL, &s_Engine);
        if (result != MA_SUCCESS)
            LOOM_CORE_ERROR("AudioEngine: failed to initialize (error {})", (int)result);
        else
            LOOM_CORE_INFO("AudioEngine: initialized");
    }

    void AudioEngine::Shutdown() {
        ma_engine_uninit(&s_Engine);
    }

    void AudioEngine::PlaySource(AudioSourceComponent& src, const std::string& full_path) {
        StopSource(src);

        auto* sound = new ma_sound();
        ma_result result = ma_sound_init_from_file(
            &s_Engine, full_path.c_str(),
            MA_SOUND_FLAG_NO_SPATIALIZATION,
            NULL, NULL, sound);

        if (result != MA_SUCCESS) {
            LOOM_CORE_ERROR("AudioEngine: failed to load '{}' (error {})", full_path, (int)result);
            delete sound;
            return;
        }

        ma_sound_set_looping(sound, src.Loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(sound, src.Volume);
        ma_sound_set_pitch(sound, src.Pitch);
        ma_sound_set_pan(sound, src.Pan);
        ma_sound_start(sound);
        src.RuntimeSound = sound;
    }

    void AudioEngine::StopSource(AudioSourceComponent& src) {
        if (!src.RuntimeSound)
            return;
        auto* sound = static_cast<ma_sound*>(src.RuntimeSound);
        ma_sound_stop(sound);
        ma_sound_uninit(sound);
        delete sound;
        src.RuntimeSound = nullptr;
    }

    void AudioEngine::SetVolume(AudioSourceComponent& src, float volume) {
        src.Volume = volume;
        if (src.RuntimeSound)
            ma_sound_set_volume(static_cast<ma_sound*>(src.RuntimeSound), volume);
    }

    void AudioEngine::SetPitch(AudioSourceComponent& src, float pitch) {
        src.Pitch = pitch;
        if (src.RuntimeSound)
            ma_sound_set_pitch(static_cast<ma_sound*>(src.RuntimeSound), pitch);
    }

    bool AudioEngine::IsPlaying(AudioSourceComponent& src) {
        if (!src.RuntimeSound)
            return false;
        return ma_sound_is_playing(static_cast<ma_sound*>(src.RuntimeSound)) == MA_TRUE;
    }

} // namespace Loom
