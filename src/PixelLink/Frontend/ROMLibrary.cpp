#include <PixelLink/Frontend/ROMLibrary.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <set>
#include <stdexcept>

namespace fs = std::filesystem;
namespace PixelLink::Frontend {
namespace {
bool IsROM(const fs::path& path) {
    auto extension = PathUTF8(path.extension());
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return extension == ".gb" || extension == ".gbc" || extension == ".rom" || extension == ".bin";
}

bool SameContents(const fs::path& a, const fs::path& b) {
    if (fs::file_size(a) != fs::file_size(b)) return false;
    std::ifstream first(a, std::ios::binary), second(b, std::ios::binary);
    if (!first || !second) throw std::runtime_error("Cannot read ROM for comparison");
    std::array<char, 8192> left{}, right{};
    while (first) {
        first.read(left.data(), left.size());
        second.read(right.data(), right.size());
        const auto count = first.gcount();
        if (count != second.gcount() || !std::equal(left.begin(), left.begin() + count, right.begin())) return false;
    }
    if (first.bad() || second.bad()) throw std::runtime_error("ROM comparison failed");
    return true;
}
}

std::string PathUTF8(const fs::path& path) {
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

fs::path UTF8Path(const std::string& value) {
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(value.data()), value.size()));
}

ROMLibrary::ROMLibrary(fs::path root, std::vector<fs::path> additionalRoots)
    : root_(fs::absolute(std::move(root))), additionalRoots_(std::move(additionalRoots)) {}

const std::vector<ROMEntry>& ROMLibrary::Entries() const noexcept { return entries_; }
const fs::path& ROMLibrary::Root() const noexcept { return root_; }
const std::vector<std::string>& ROMLibrary::Warnings() const noexcept { return warnings_; }

void ROMLibrary::Refresh() {
    entries_.clear();
    warnings_.clear();
    std::vector<fs::path> roots{root_};
    roots.insert(roots.end(), additionalRoots_.begin(), additionalRoots_.end());
    std::set<fs::path> visited;
    for (const auto& root : roots) {
        std::error_code ec;
        if (!fs::exists(root, ec)) {
            if (ec) warnings_.push_back(PathUTF8(root) + ": " + ec.message());
            continue;
        }
        fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
        if (ec) warnings_.push_back(PathUTF8(root) + ": " + ec.message());
        while (it != end) {
            const auto file = it->path();
            std::error_code fileError;
            if (it->is_regular_file(fileError) && IsROM(file)) {
                const auto canonical = fs::weakly_canonical(file, fileError);
                if (!fileError && visited.insert(canonical).second) {
                    const auto size = fs::file_size(file, fileError);
                    if (!fileError) entries_.push_back({canonical, PathUTF8(file.stem()),
                        PathUTF8(file.lexically_relative(root)), size});
                }
            }
            if (fileError) warnings_.push_back(PathUTF8(file) + ": " + fileError.message());
            it.increment(ec);
            if (ec) {
                warnings_.push_back(PathUTF8(root) + ": " + ec.message());
                ec.clear();
            }
        }
    }
    std::sort(entries_.begin(), entries_.end(), [](const auto& a, const auto& b) {
        return a.relativePath < b.relativePath || (a.relativePath == b.relativePath && a.path < b.path);
    });
}

void ROMLibrary::ValidateROM(const fs::path& source) {
    if (!IsROM(source)) throw std::runtime_error("Please select a .gb, .gbc, .rom or .bin ROM");
    std::ifstream stream(source, std::ios::binary);
    std::array<unsigned char, 0x150> header{};
    if (!stream.read(reinterpret_cast<char*>(header.data()), header.size())) {
        throw std::runtime_error("ROM is unreadable or its header is incomplete");
    }
    if (header[0x148] > 8 || header[0x149] > 5) throw std::runtime_error("Invalid Game Boy ROM header");
    const auto expected = std::uintmax_t{32768} << header[0x148];
    if (fs::file_size(source) < expected) throw std::runtime_error("ROM is truncated");
    switch (header[0x147]) {
    case 0x00: case 0x01: case 0x02: case 0x03:
    case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13:
    case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: break;
    default: throw std::runtime_error("This cartridge controller is not supported yet");
    }
    if (header[0x143] == 0xC0) throw std::runtime_error("This game requires Game Boy Color hardware");
}

fs::path ROMLibrary::Import(const fs::path& source) {
    ValidateROM(source);
    fs::create_directories(root_);
    auto destination = root_ / source.filename();
    for (unsigned suffix = 1;; ++suffix) {
        if (!fs::exists(destination)) {
            auto save = destination; save.replace_extension(".sav");
            auto rtc = destination; rtc.replace_extension(".rtc");
            if (!fs::exists(save) && !fs::exists(rtc)) break;
        } else if (SameContents(source, destination)) {
            Refresh();
            return fs::weakly_canonical(destination);
        }
        destination = root_ / UTF8Path(PathUTF8(source.stem()) + " (" + std::to_string(suffix) + ")" + PathUTF8(source.extension()));
    }
    // copy_file defaults to failing on a concurrent name collision.
    std::vector<fs::path> created;
    try {
        fs::copy_file(source, destination);
        created.push_back(destination);
        // Preserve existing battery saves when importing a user's game.
        for (const auto* extension : {".sav", ".rtc"}) {
            auto from = source; from.replace_extension(extension);
            auto to = destination; to.replace_extension(extension);
            if (fs::is_regular_file(from)) {
                fs::copy_file(from, to);
                created.push_back(to);
            }
        }
    } catch (...) {
        for (const auto& file : created) {
            std::error_code ignored;
            fs::remove(file, ignored);
        }
        throw;
    }
    Refresh();
    return fs::weakly_canonical(destination);
}
} // namespace PixelLink::Frontend
