#include <windows.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "audio/AudioEngine.h"
#include "core/Playlist.h"
#include "ui/PlayerApp.h"
#include "util/Format.h"
#include "util/PathUtil.h"

namespace fs = std::filesystem;

namespace {

void printUsage() {
    std::cout
        << "Terminal Music Player 1.0.0\n\n"
        << "Usage:\n"
        << "  music-player [path] [options]\n\n"
        << "Arguments:\n"
        << "  path                 Audio file or directory (default: ./music)\n\n"
        << "Options:\n"
        << "  -r, --recursive      Scan subdirectories\n"
        << "  -l, --list           List tracks and exit\n"
        << "      --probe          Load the first track, print its duration, exit\n"
        << "      --selftest       Exercise play/pause/seek/stop and exit\n"
        << "  -h, --help           Show this help\n\n"
        << "Controls:\n"
        << "  Space                Play / pause\n"
        << "  Enter                Play selected track\n"
        << "  Up / Down            Move selection\n"
        << "  N / P                Next / previous track\n"
        << "  S                    Stop\n"
        << "  Left / Right         Seek -5 / +5 seconds\n"
        << "  + / -                Volume up / down\n"
        << "  M                    Mute / unmute\n"
        << "  R                    Rescan playlist\n"
        << "  Q / Esc              Quit\n";
}

void printTracks(const Playlist& playlist) {
    int index = 1;
    for (const Track& track : playlist.tracks()) {
        std::cout << "[" << index++ << "] " << track.displayName << "  ("
                  << pathToUtf8(track.path) << ")\n";
    }
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    SetConsoleTitleW(L"Terminal Music Player");

    fs::path root = fs::current_path() / "music";
    bool recursive = false;
    bool listOnly = false;
    bool probe = false;
    bool selfTest = false;

    for (int i = 1; i < argc; ++i) {
        const std::wstring argument = argv[i];

        if (argument == L"-h" || argument == L"--help") {
            printUsage();
            return 0;
        }
        if (argument == L"-r" || argument == L"--recursive") {
            recursive = true;
            continue;
        }
        if (argument == L"-l" || argument == L"--list") {
            listOnly = true;
            continue;
        }
        if (argument == L"--probe") {
            probe = true;
            continue;
        }
        if (argument == L"--selftest") {
            selfTest = true;
            continue;
        }
        if (!argument.empty() && argument[0] == L'-') {
            std::wcerr << L"Unknown option: " << argument << L"\n";
            printUsage();
            return 1;
        }
        root = fs::path(argument);
    }

    std::error_code ec;
    root = fs::absolute(root, ec);
    if (ec) {
        std::cerr << "Cannot resolve path: " << ec.message() << "\n";
        return 1;
    }

    if (listOnly || probe || selfTest) {
        Playlist playlist;
        std::string scanError;
        playlist.addPath(root, recursive, scanError);

        if (!scanError.empty()) {
            std::cerr << scanError << "\n";
        }
        if (playlist.empty()) {
            std::cout << "No supported audio files found in " << pathToUtf8(root)
                      << "\n";
            return 1;
        }

        if (listOnly) {
            printTracks(playlist);
            return 0;
        }

        AudioEngine audio;
        std::string error;
        if (!audio.initialize(error)) {
            std::cerr << "Audio initialization failed: " << error << "\n";
            return 1;
        }

        const Track* first = playlist.at(0);
        if (first == nullptr || !audio.load(first->path, error)) {
            std::cerr << "Load failed: " << error << "\n";
            return 1;
        }

        std::cout << "Probe OK\n"
                  << "  file:     " << pathToUtf8(first->path) << "\n"
                  << "  duration: " << formatTime(audio.duration()) << "\n";

        if (selfTest) {
            using namespace std::chrono_literals;

            if (!audio.play()) {
                std::cerr << "Play failed: " << audio.lastError() << "\n";
                return 1;
            }
            std::this_thread::sleep_for(700ms);
            std::cout << "  playing:  " << (audio.isPlaying() ? "yes" : "no")
                      << "  position: " << formatTime(audio.position()) << "\n";

            audio.pause();
            std::this_thread::sleep_for(100ms);
            std::cout << "  paused:   " << (!audio.isPlaying() ? "yes" : "no")
                      << "\n";

            audio.seekTo(1.0);
            if (!audio.play()) {
                std::cerr << "Resume failed: " << audio.lastError() << "\n";
                return 1;
            }
            std::this_thread::sleep_for(300ms);
            std::cout << "  resumed:  position: " << formatTime(audio.position())
                      << "\n";

            audio.stop();
            std::cout << "  stopped:  position: " << formatTime(audio.position())
                      << "\n";
        }

        audio.shutdown();
        return 0;
    }

    PlayerApp app(root, recursive);
    return app.run();
}
