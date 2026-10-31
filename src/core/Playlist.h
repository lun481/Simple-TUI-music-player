#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct Track {
    std::filesystem::path path;
    std::string displayName;
    std::string extension;
};

class Playlist {
public:
    void clear();

    // Adds a file, or scans a directory. Returns true when at least one
    // supported audio file was added. `error` is only set for real errors
    // (missing path, unreadable directory, unsupported single file).
    bool addPath(const std::filesystem::path& path, bool recursive, std::string& error);

    bool empty() const { return tracks_.empty(); }
    int size() const { return static_cast<int>(tracks_.size()); }
    const std::vector<Track>& tracks() const { return tracks_; }

    const Track* at(int index) const;
    int current() const { return current_; }
    void setCurrent(int index);

    int nextIndex() const;
    int prevIndex() const;

private:
    bool addFile(const std::filesystem::path& path);
    void sortTracks();

    std::vector<Track> tracks_;
    int current_ = -1;
};
