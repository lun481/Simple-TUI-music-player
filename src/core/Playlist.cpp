#include "core/Playlist.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <system_error>

#include "util/PathUtil.h"

namespace {

constexpr std::array<std::string_view, 5> kSupportedExtensions = {
    ".mp3", ".flac", ".wav", ".ogg", ".oga"
};

bool isSupportedExtension(const std::string& extension) {
    return std::find(kSupportedExtensions.begin(), kSupportedExtensions.end(),
                     extension) != kSupportedExtensions.end();
}

}  // namespace

void Playlist::clear() {
    tracks_.clear();
    current_ = -1;
}

bool Playlist::addPath(const std::filesystem::path& path, bool recursive,
                       std::string& error) {
    error.clear();

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        error = "Path does not exist: " + pathToUtf8(path);
        return false;
    }

    if (std::filesystem::is_regular_file(path, ec)) {
        if (!addFile(path)) {
            error = "Unsupported audio format: " + pathToUtf8(path);
            return false;
        }
        sortTracks();
        return !tracks_.empty();
    }

    if (!std::filesystem::is_directory(path, ec)) {
        error = "Not a file or directory: " + pathToUtf8(path);
        return false;
    }

    auto addEntry = [this](const std::filesystem::directory_entry& entry) {
        std::error_code entryError;
        if (entry.is_regular_file(entryError)) {
            addFile(entry.path());
        }
    };

    if (recursive) {
        std::filesystem::recursive_directory_iterator it(
            path, std::filesystem::directory_options::skip_permission_denied, ec);
        const std::filesystem::recursive_directory_iterator end;
        while (!ec && it != end) {
            addEntry(*it);
            it.increment(ec);
        }
    } else {
        std::filesystem::directory_iterator it(
            path, std::filesystem::directory_options::skip_permission_denied, ec);
        const std::filesystem::directory_iterator end;
        while (!ec && it != end) {
            addEntry(*it);
            it.increment(ec);
        }
    }

    if (ec) {
        error = "Scan warning: " + ec.message();
    }

    sortTracks();
    return !tracks_.empty();
}

bool Playlist::addFile(const std::filesystem::path& path) {
    const std::string extension = toLowerAscii(pathToUtf8(path.extension()));
    if (!isSupportedExtension(extension)) {
        return false;
    }

    Track track;
    track.path = path;
    track.extension = extension;
    track.displayName = pathToUtf8(path.stem());
    if (track.displayName.empty()) {
        track.displayName = pathToUtf8(path.filename());
    }

    tracks_.push_back(std::move(track));
    return true;
}

void Playlist::sortTracks() {
    std::stable_sort(tracks_.begin(), tracks_.end(),
                     [](const Track& lhs, const Track& rhs) {
                         const std::string left = toLowerAscii(lhs.displayName);
                         const std::string right = toLowerAscii(rhs.displayName);
                         if (left != right) {
                             return left < right;
                         }
                         return lhs.path < rhs.path;
                     });
}

const Track* Playlist::at(int index) const {
    if (index < 0 || index >= size()) {
        return nullptr;
    }
    return &tracks_[static_cast<std::size_t>(index)];
}

void Playlist::setCurrent(int index) {
    current_ = (index >= 0 && index < size()) ? index : -1;
}

int Playlist::nextIndex() const {
    if (tracks_.empty()) {
        return -1;
    }
    if (current_ < 0) {
        return 0;
    }
    return (current_ + 1) % size();
}

int Playlist::prevIndex() const {
    if (tracks_.empty()) {
        return -1;
    }
    if (current_ <= 0) {
        return size() - 1;
    }
    return current_ - 1;
}
