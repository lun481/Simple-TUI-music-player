#include "audio/AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "util/PathUtil.h"

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

struct AudioEngine::Impl {
    ma_engine engine{};
    ma_sound sound{};
    bool engineReady = false;
    bool soundReady = false;
    float volume = 0.7f;
    double duration = 0.0;
    std::string error;
};

AudioEngine::AudioEngine() : impl_(std::make_unique<Impl>()) {}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::initialize(std::string& error) {
    if (impl_->engineReady) {
        return true;
    }

    ma_engine_config config = ma_engine_config_init();
    const ma_result result = ma_engine_init(&config, &impl_->engine);
    if (result != MA_SUCCESS) {
        impl_->error = "Failed to initialize audio engine: " +
                       std::string(ma_result_description(result));
        error = impl_->error;
        return false;
    }

    impl_->engineReady = true;
    return true;
}

void AudioEngine::shutdown() {
    unload();

    if (impl_ && impl_->engineReady) {
        ma_engine_uninit(&impl_->engine);
        impl_->engineReady = false;
    }
}

bool AudioEngine::load(const std::filesystem::path& file, std::string& error) {
    if (!impl_->engineReady) {
        error = "Audio engine is not initialized.";
        return false;
    }

    unload();

    const std::string utf8Path = pathToUtf8(file);

#if defined(_WIN32)
    // The narrow-character miniaudio entry point does not reliably handle
    // non-ASCII paths on Windows, so use the wide-character variant.
    const std::wstring widePath = file.wstring();
    const ma_result result = ma_sound_init_from_file_w(
        &impl_->engine, widePath.c_str(),
        MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, nullptr,
        &impl_->sound);
#else
    const ma_result result = ma_sound_init_from_file(
        &impl_->engine, utf8Path.c_str(),
        MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, nullptr,
        &impl_->sound);
#endif

    if (result != MA_SUCCESS) {
        impl_->error = "Failed to load '" + utf8Path + "': " +
                       std::string(ma_result_description(result));
        error = impl_->error;
        return false;
    }

    impl_->soundReady = true;
    impl_->duration = 0.0;

    float length = 0.0f;
    if (ma_sound_get_length_in_seconds(&impl_->sound, &length) == MA_SUCCESS) {
        impl_->duration = static_cast<double>(length);
    }

    ma_sound_set_volume(&impl_->sound, impl_->volume);
    return true;
}

void AudioEngine::unload() {
    if (impl_ && impl_->soundReady) {
        ma_sound_stop(&impl_->sound);
        ma_sound_uninit(&impl_->sound);
        impl_->soundReady = false;
    }
    if (impl_) {
        impl_->duration = 0.0;
    }
}

bool AudioEngine::play() {
    if (!impl_->soundReady) {
        return false;
    }

    // Restart a sound that already reached the end.
    if (ma_sound_at_end(&impl_->sound)) {
        ma_sound_seek_to_pcm_frame(&impl_->sound, 0);
    }

    const ma_result result = ma_sound_start(&impl_->sound);
    if (result != MA_SUCCESS) {
        impl_->error = "Failed to start playback: " +
                       std::string(ma_result_description(result));
        return false;
    }
    return true;
}

void AudioEngine::pause() {
    if (impl_->soundReady) {
        ma_sound_stop(&impl_->sound);
    }
}

void AudioEngine::stop() {
    if (impl_->soundReady) {
        ma_sound_stop(&impl_->sound);
        ma_sound_seek_to_pcm_frame(&impl_->sound, 0);
    }
}

bool AudioEngine::isLoaded() const {
    return impl_ && impl_->soundReady;
}

bool AudioEngine::isPlaying() const {
    return impl_ && impl_->soundReady && ma_sound_is_playing(&impl_->sound) != 0;
}

bool AudioEngine::atEnd() const {
    return impl_ && impl_->soundReady && ma_sound_at_end(&impl_->sound) != 0;
}

void AudioEngine::seekTo(double seconds) {
    if (!impl_->soundReady) {
        return;
    }

    if (!std::isfinite(seconds) || seconds < 0.0) {
        seconds = 0.0;
    }
    if (impl_->duration > 0.0 && seconds > impl_->duration) {
        seconds = impl_->duration;
    }

    ma_sound_seek_to_second(&impl_->sound, static_cast<float>(seconds));
}

void AudioEngine::seekBy(double deltaSeconds) {
    seekTo(position() + deltaSeconds);
}

void AudioEngine::setVolume(float volume) {
    impl_->volume = std::clamp(volume, 0.0f, 1.0f);
    if (impl_->soundReady) {
        ma_sound_set_volume(&impl_->sound, impl_->volume);
    }
}

float AudioEngine::volume() const {
    return impl_->volume;
}

double AudioEngine::position() const {
    if (!impl_->soundReady) {
        return 0.0;
    }

    float cursor = 0.0f;
    if (ma_sound_get_cursor_in_seconds(&impl_->sound, &cursor) == MA_SUCCESS) {
        return static_cast<double>(cursor);
    }
    return 0.0;
}

double AudioEngine::duration() const {
    return impl_->duration;
}

const std::string& AudioEngine::lastError() const {
    return impl_->error;
}
