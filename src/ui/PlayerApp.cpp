#include "ui/PlayerApp.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

#include "util/Format.h"
#include "util/PathUtil.h"

namespace {

std::string percentText(float value) {
    return std::to_string(static_cast<int>(std::lround(value * 100.0f))) + "%";
}

}  // namespace

PlayerApp::PlayerApp(std::filesystem::path root, bool recursive)
    : root_(std::move(root)), recursive_(recursive) {}

int PlayerApp::run() {
    std::string error;
    if (!audio_.initialize(error)) {
        std::cerr << "Audio initialization failed: " << error << "\n";
        return 1;
    }

    audio_.setVolume(volume_);
    loadPlaylist();

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    screen_ = &screen;

    std::atomic<bool> refreshRunning{true};
    std::thread refresher([&] {
        using namespace std::chrono_literals;
        while (refreshRunning.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(200ms);
            screen.PostEvent(ftxui::Event::Custom);
        }
    });

    auto renderer = ftxui::Renderer([this] { return render(); });
    auto component = ftxui::CatchEvent(
        renderer, [this](ftxui::Event event) {
            if (event == ftxui::Event::Custom) {
                update();
                return true;
            }
            return handleEvent(event);
        });

    screen.Loop(component);

    refreshRunning.store(false);
    refresher.join();
    screen_ = nullptr;
    audio_.shutdown();
    return 0;
}

void PlayerApp::loadPlaylist() {
    playlist_.clear();
    error_.clear();

    std::string scanError;
    playlist_.addPath(root_, recursive_, scanError);
    if (!scanError.empty()) {
        error_ = scanError;
    }

    selected_ = 0;
    status_ = playlist_.empty() ? "No audio files" : "Ready";
}

void PlayerApp::rescan() {
    std::filesystem::path currentPath;
    if (const Track* currentTrack = playlist_.at(playlist_.current())) {
        currentPath = currentTrack->path;
    }

    playlist_.clear();
    error_.clear();

    std::string scanError;
    playlist_.addPath(root_, recursive_, scanError);
    if (!scanError.empty()) {
        error_ = scanError;
    }

    if (playlist_.empty()) {
        selected_ = 0;
        status_ = "No audio files";
        audio_.unload();
        playIntent_ = false;
        return;
    }

    selected_ = 0;
    bool restored = false;
    for (int i = 0; i < playlist_.size(); ++i) {
        const Track* track = playlist_.at(i);
        if (track != nullptr && track->path == currentPath) {
            selected_ = i;
            playlist_.setCurrent(i);
            restored = true;
            break;
        }
    }

    if (!currentPath.empty() && !restored) {
        audio_.unload();
        playIntent_ = false;
        status_ = "Stopped";
    } else {
        status_ = "Playlist reloaded";
    }
}

void PlayerApp::playIndex(int index) {
    const Track* track = playlist_.at(index);
    if (track == nullptr) {
        return;
    }

    std::string error;
    if (!audio_.load(track->path, error)) {
        error_ = error;
        status_ = "Error";
        playIntent_ = false;
        return;
    }

    error_.clear();
    playlist_.setCurrent(index);
    selected_ = index;
    audio_.setVolume(volume_);
    endHandled_ = false;

    if (audio_.play()) {
        playIntent_ = true;
        status_ = "Playing";
    } else {
        playIntent_ = false;
        status_ = "Error";
        error_ = audio_.lastError();
    }
}

void PlayerApp::next() {
    if (playlist_.empty()) {
        return;
    }

    const int current = playlist_.current();
    const int index = (current < 0) ? selected_ : playlist_.nextIndex();
    playIndex(index);
}

void PlayerApp::prev() {
    if (playlist_.empty()) {
        return;
    }

    // Common player behaviour: restart the current track if we are more than
    // three seconds in, otherwise jump to the previous track.
    if (audio_.isLoaded() && audio_.position() > 3.0) {
        audio_.seekTo(0.0);
        endHandled_ = false;
        return;
    }

    const int current = playlist_.current();
    const int index = (current < 0) ? selected_ : playlist_.prevIndex();
    playIndex(index);
}

void PlayerApp::togglePlay() {
    if (!audio_.isLoaded()) {
        playIndex(selected_);
        return;
    }

    if (audio_.isPlaying()) {
        audio_.pause();
        playIntent_ = false;
        status_ = "Paused";
        return;
    }

    if (audio_.atEnd()) {
        audio_.seekTo(0.0);
        endHandled_ = false;
    }

    if (audio_.play()) {
        playIntent_ = true;
        status_ = "Playing";
    } else {
        playIntent_ = false;
        status_ = "Error";
        error_ = audio_.lastError();
    }
}

void PlayerApp::stop() {
    if (!audio_.isLoaded()) {
        return;
    }

    audio_.stop();
    playIntent_ = false;
    endHandled_ = false;
    status_ = "Stopped";
}

void PlayerApp::seekBy(double seconds) {
    if (!audio_.isLoaded()) {
        return;
    }

    audio_.seekBy(seconds);
    endHandled_ = false;
}

void PlayerApp::changeVolume(float delta) {
    volume_ = std::clamp(volume_ + delta, 0.0f, 1.0f);
    audio_.setVolume(volume_);
    if (volume_ > 0.001f) {
        lastVolume_ = volume_;
    }
}

void PlayerApp::toggleMute() {
    if (volume_ > 0.001f) {
        lastVolume_ = volume_;
        volume_ = 0.0f;
    } else {
        volume_ = (lastVolume_ > 0.001f) ? lastVolume_ : 0.70f;
    }
    audio_.setVolume(volume_);
}

void PlayerApp::update() {
    if (!audio_.isLoaded() || !playIntent_ || endHandled_) {
        return;
    }

    if (audio_.atEnd()) {
        endHandled_ = true;
        playIntent_ = false;
        next();
    }
}

int PlayerApp::visibleRows() const {
    const int height = screen_ != nullptr ? screen_->dimy() : 30;
    return std::max(3, height - 11);
}

ftxui::Element PlayerApp::render() {
    using namespace ftxui;

    const auto& tracks = playlist_.tracks();
    const int current = playlist_.current();
    const bool loaded = audio_.isLoaded();
    const bool playing = audio_.isPlaying();

    const double position = audio_.position();
    const double duration = audio_.duration();
    const float progress =
        duration > 0.0
            ? static_cast<float>(std::clamp(position / duration, 0.0, 1.0))
            : 0.0f;

    const bool isError = (status_ == "Error" || status_ == "No audio files");
    const Color statusColor = playing ? Color::Green
                               : loaded ? Color::Yellow
                               : isError ? Color::Red
                                         : Color::Cyan;

    // ---------------------------------------------------------------- header
    Element header = hbox({
        text(" TERMINAL MUSIC PLAYER ") | bold | color(Color::Cyan),
        filler(),
        text("[" + status_ + "]") | bold | color(statusColor),
        text("   VOL " + percentText(volume_)) | dim,
    });

    // -------------------------------------------------------------- playlist
    const int visible = visibleRows();
    int start = 0;
    if (!tracks.empty()) {
        const int maxStart = std::max(0, static_cast<int>(tracks.size()) - visible);
        start = std::clamp(selected_ - visible / 2, 0, maxStart);
    }

    Elements rows;
    if (tracks.empty()) {
        rows.push_back(text("  No supported audio files.") | dim);
        rows.push_back(text("  Put .mp3/.flac/.wav/.ogg files in:") | dim);
        rows.push_back(text("  " + pathToUtf8(root_)) | dim);
    } else {
        const int end =
            std::min(static_cast<int>(tracks.size()), start + visible);
        for (int i = start; i < end; ++i) {
            const Track& track = tracks[static_cast<std::size_t>(i)];
            const bool isCurrent = (i == current);
            const bool isSelected = (i == selected_);

            const std::string marker = isCurrent ? ">" : " ";
            const std::string number =
                (i + 1 < 10 ? "  " : " ") + std::to_string(i + 1) + " ";

            Element row = hbox({
                text(marker) | color(isCurrent ? Color::Green : Color::GrayDark),
                text(number) | dim,
                text(track.displayName) | flex,
            });

            if (isSelected) {
                row = row | inverted;
            } else if (isCurrent) {
                row = row | color(Color::Green);
            }
            rows.push_back(row);
        }
    }

    Element playlistPane =
        window(text(" Playlist (" + std::to_string(tracks.size()) + ") "),
               vbox(std::move(rows)) | flex) |
        flex;

    // ----------------------------------------------------------- now playing
    Elements info;
    const Track* currentTrack = playlist_.at(current);

    if (loaded && currentTrack != nullptr) {
        info.push_back(text("NOW PLAYING") | bold | color(Color::Cyan));
        info.push_back(text(currentTrack->displayName) | bold);
        info.push_back(text(pathToUtf8(currentTrack->path.filename())) | dim);
        info.push_back(separator());

        info.push_back(hbox({
            text("Status   "),
            text(status_) | bold | color(statusColor),
        }));
        info.push_back(hbox({
            text("Time     "),
            text(formatTime(position) + " / " + formatTime(duration)),
        }));
        info.push_back(hbox({
            text(formatTime(position)) | size(WIDTH, EQUAL, 7),
            gauge(progress) | flex,
            text(formatTime(duration)) | size(WIDTH, EQUAL, 7),
        }));

        info.push_back(separator());
        info.push_back(text("Volume"));
        info.push_back(gauge(volume_) | color(Color::Cyan));
        info.push_back(text(percentText(volume_)) | dim);

        info.push_back(separator());
        info.push_back(text("Format   " + currentTrack->extension) | dim);
        info.push_back(text("Index    " + std::to_string(current + 1) + " / " +
                            std::to_string(tracks.size())) |
                       dim);
    } else {
        info.push_back(text("NOW PLAYING") | bold | color(Color::Cyan));
        info.push_back(text("Nothing loaded") | dim);
        info.push_back(separator());
        info.push_back(text("Select a track and press Enter") | dim);
        info.push_back(text("or press Space to start") | dim);
    }

    if (!error_.empty()) {
        info.push_back(separator());
        info.push_back(paragraph(error_) | color(Color::Red));
    }

    Element nowPlaying =
        window(text(" Player "), vbox(std::move(info))) |
        size(WIDTH, EQUAL, 42);

    // ---------------------------------------------------------------- footer
    Element footer = vbox({
        hbox({
            text("[Space]") | bold,
            text(" Play/Pause  "),
            text("[S]") | bold,
            text(" Stop  "),
            text("[N]") | bold,
            text(" Next  "),
            text("[P]") | bold,
            text(" Prev"),
        }),
        hbox({
            text("[Up]/[Down]") | bold,
            text(" Select  "),
            text("[Enter]") | bold,
            text(" Play  "),
            text("[Left]/[Right]") | bold,
            text(" Seek 5s"),
        }),
        hbox({
            text("[+]/[-]") | bold,
            text(" Volume  "),
            text("[M]") | bold,
            text(" Mute  "),
            text("[R]") | bold,
            text(" Rescan  "),
            text("[Q]/[Esc]") | bold,
            text(" Quit"),
        }),
    }) | dim;

    return vbox({
               header,
               separator(),
               hbox({
                   playlistPane,
                   nowPlaying,
               }) | flex,
               separator(),
               footer,
           }) |
           border;
}

bool PlayerApp::handleEvent(const ftxui::Event& event) {
    using namespace ftxui;

    if (event == Event::Escape) {
        if (screen_ != nullptr) {
            screen_->Exit();
        }
        return true;
    }

    if (event == Event::ArrowUp) {
        if (!playlist_.empty()) {
            selected_ = (selected_ - 1 + playlist_.size()) % playlist_.size();
        }
        return true;
    }

    if (event == Event::ArrowDown) {
        if (!playlist_.empty()) {
            selected_ = (selected_ + 1) % playlist_.size();
        }
        return true;
    }

    if (event == Event::ArrowLeft) {
        seekBy(-5.0);
        return true;
    }

    if (event == Event::ArrowRight) {
        seekBy(5.0);
        return true;
    }

    if (event == Event::Return) {
        playIndex(selected_);
        return true;
    }

    if (!event.is_character()) {
        return false;
    }

    const std::string character = event.character();

    if (character == "q" || character == "Q") {
        if (screen_ != nullptr) {
            screen_->Exit();
        }
        return true;
    }
    if (character == " ") {
        togglePlay();
        return true;
    }
    if (character == "s" || character == "S") {
        stop();
        return true;
    }
    if (character == "n" || character == "N") {
        next();
        return true;
    }
    if (character == "p" || character == "P") {
        prev();
        return true;
    }
    if (character == "+" || character == "=") {
        changeVolume(0.05f);
        return true;
    }
    if (character == "-" || character == "_") {
        changeVolume(-0.05f);
        return true;
    }
    if (character == "m" || character == "M") {
        toggleMute();
        return true;
    }
    if (character == "r" || character == "R") {
        rescan();
        return true;
    }

    return false;
}
