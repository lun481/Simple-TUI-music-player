#pragma once

#include <filesystem>
#include <memory>
#include <string>

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool initialize(std::string& error);
    void shutdown();

    bool load(const std::filesystem::path& file, std::string& error);
    void unload();

    bool play();
    void pause();
    void stop();

    bool isLoaded() const;
    bool isPlaying() const;
    bool atEnd() const;

    void seekTo(double seconds);
    void seekBy(double deltaSeconds);

    void setVolume(float volume);  // 0.0 .. 1.0
    float volume() const;

    double position() const;
    double duration() const;

    const std::string& lastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
