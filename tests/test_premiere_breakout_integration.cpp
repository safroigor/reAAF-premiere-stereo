/*
 * Optional integration test for a real Premiere AAF. Set
 * REAAF_PREMIERE_TEST_AAF to enable it; CI skips it when no private fixture is
 * available.
 */

#include "AafImporter.h"
#include "IRppSink.h"
#include "LogBuffer.h"

#include <algorithm>
#include <array>
#include <catch2/catch_all.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct CapturingSink final : IRppSink {
    std::vector<std::string> lines;
    void writeLine(const char *line) override { lines.emplace_back(line ? line : ""); }
};

uint16_t readU16(std::istream &in) {
    std::array<unsigned char, 2> bytes{};
    in.read(reinterpret_cast<char *>(bytes.data()), 2);
    return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
}

uint32_t readU32(std::istream &in) {
    std::array<unsigned char, 4> bytes{};
    in.read(reinterpret_cast<char *>(bytes.data()), 4);
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

struct TempFixture {
    std::filesystem::path dir = std::filesystem::temp_directory_path() /
                                ("reaaf-premiere-" + std::to_string(std::rand()));
    std::filesystem::path aaf = dir / "premiere-breakout.aaf";

    explicit TempFixture(const std::filesystem::path &source) {
        std::filesystem::create_directories(dir);
        std::filesystem::copy_file(source, aaf);
    }

    ~TempFixture() {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
};

bool containsLine(const CapturingSink &sink, const std::string &text) {
    return std::any_of(sink.lines.begin(), sink.lines.end(), [&](const std::string &line) {
        return line.find(text) != std::string::npos;
    });
}

} // namespace

TEST_CASE("real Premiere embedded breakout AAF produces stereo REAPER tracks") {
    const char *fixturePath = std::getenv("REAAF_PREMIERE_TEST_AAF");
    if ( !fixturePath || !*fixturePath ) {
        SUCCEED("REAAF_PREMIERE_TEST_AAF is not set; optional private-fixture test skipped");
        return;
    }

    TempFixture fixture(std::filesystem::u8path(fixturePath));
    CapturingSink sink;
    LogBuffer log(LogEntry::DEBUG);
    REQUIRE(AafImporter(&sink, fixture.aaf.u8string().c_str(), log).run() == 0);

    if ( const char *dumpPath = std::getenv("REAAF_PREMIERE_TEST_RPP_OUTPUT");
         dumpPath && *dumpPath ) {
        std::ofstream dump(std::filesystem::u8path(dumpPath), std::ios::binary | std::ios::trunc);
        for ( const auto &line : sink.lines )
            dump << line << "\n";
    }

    const auto trackCount = std::count_if(sink.lines.begin(), sink.lines.end(),
                                          [](const std::string &line) {
                                              return line.find("<TRACK") != std::string::npos;
                                          });
    CHECK(trackCount == 5); // four audio tracks plus reAAF's video track
    CHECK(containsLine(sink, "NAME \"VIDEO\""));
    CHECK(containsLine(sink, "NAME \"VO1\""));
    CHECK(containsLine(sink, "NAME \"VO2\""));
    CHECK(containsLine(sink, "NAME \"GP\""));
    CHECK(containsLine(sink, "NAME \"MX1\""));

    const auto mediaDir = fixture.dir / "premiere-breakout-media";
    std::vector<std::filesystem::path> stereoFiles;
    for ( const auto &entry : std::filesystem::directory_iterator(mediaDir) ) {
        if ( entry.is_regular_file() && entry.path().filename().string().find("__stereo_") !=
                                           std::string::npos )
            stereoFiles.push_back(entry.path());
    }
    REQUIRE(stereoFiles.size() == 3);
    for ( const auto &path : stereoFiles ) {
        std::ifstream in(path, std::ios::binary);
        REQUIRE(in);
        in.seekg(22);
        CHECK(readU16(in) == 2);
        CHECK(readU32(in) == 48000);
        in.seekg(34);
        CHECK(readU16(in) == 24);
    }
}
