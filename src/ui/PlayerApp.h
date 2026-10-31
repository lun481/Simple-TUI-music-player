#pragma once

#include <filesystem>
#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "audio/AudioEngine.h"
#include "core/Playlist.h"

class PlayerApp {
public:
    PlayerApp(std::filesystem::path root, bool recursive);

    int run();

private:
    void loadPlaylist();
    void rescan();

    void playIndex(int index);
    void next();
    void prev();
    void togglePlay();
    void stop();
    void seekBy(double seconds);
    void changeVolume(float delta);
    void toggleMute();
    void update();

    int visibleRows() const;

    ftxui::Element render();
    bool handleEvent(const ftxui::Event& event);

    std::filesystem::path root_;
    bool recursive_ = false;

    Playlist playlist_;
    AudioEngine audio_;

    int selected_ = 0;
    bool playIntent_ = false;
    bool endHandled_ = false;

    float volume_ = 0.70f;
    float lastVolume_ = 0.70f;

    std::string status_ = "Ready";
    std::string error_;

    ftxui::ScreenInteractive* screen_ = nullptr;
};
