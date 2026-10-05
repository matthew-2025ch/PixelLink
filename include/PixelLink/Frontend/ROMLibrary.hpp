#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace PixelLink::Frontend {

struct ROMEntry {
    std::filesystem::path path;
    std::string name;
    std::string relativePath;
    std::uintmax_t size = 0;
};

// Paths and UI strings cross SDL boundaries in UTF-8, including on Windows.
[[nodiscard]] std::string PathUTF8(const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path UTF8Path(const std::string& value);

class ROMLibrary {
public:
    explicit ROMLibrary(std::filesystem::path root,
                        std::vector<std::filesystem::path> additionalRoots = {});
    void Refresh();
    [[nodiscard]] const std::vector<ROMEntry>& Entries() const noexcept;
    [[nodiscard]] const std::filesystem::path& Root() const noexcept;
    [[nodiscard]] const std::vector<std::string>& Warnings() const noexcept;
    // Copies into root; never overwrites a different ROM or an existing save.
    // Returns an existing entry for an identical file. No cartridge is run.
    [[nodiscard]] std::filesystem::path Import(const std::filesystem::path& source);
    static void ValidateROM(const std::filesystem::path& source);
private:
    std::filesystem::path root_;
    std::vector<std::filesystem::path> additionalRoots_;
    std::vector<ROMEntry> entries_;
    std::vector<std::string> warnings_;
};

} // namespace PixelLink::Frontend
