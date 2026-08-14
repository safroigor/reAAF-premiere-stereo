/*
 * Copyright (C) 2026 reAAF contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "AafData.h"
#include "LogBuffer.h"
#include "StereoRecombiner.h"

#include <array>
#include <catch2/catch_all.hpp>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

void writeU16(std::ostream &out, const uint16_t value) {
    const std::array<char, 2> bytes{static_cast<char>(value & 0xff),
                                    static_cast<char>((value >> 8) & 0xff)};
    out.write(bytes.data(), 2);
}

void writeU32(std::ostream &out, const uint32_t value) {
    const std::array<char, 4> bytes{static_cast<char>(value & 0xff),
                                    static_cast<char>((value >> 8) & 0xff),
                                    static_cast<char>((value >> 16) & 0xff),
                                    static_cast<char>((value >> 24) & 0xff)};
    out.write(bytes.data(), 4);
}

void writeMono16Wave(const std::filesystem::path &path, const std::vector<int16_t> &samples) {
    std::ofstream out(path, std::ios::binary);
    const auto dataSize = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    out.write("RIFF", 4);
    writeU32(out, 36 + dataSize);
    out.write("WAVEfmt ", 8);
    writeU32(out, 16);
    writeU16(out, 1);
    writeU16(out, 1);
    writeU32(out, 48000);
    writeU32(out, 96000);
    writeU16(out, 2);
    writeU16(out, 16);
    out.write("data", 4);
    writeU32(out, dataSize);
    for ( const auto sample : samples )
        writeU16(out, static_cast<uint16_t>(sample));
}

void writeMono16WaveNarrow(const std::string &path, const std::vector<int16_t> &samples) {
    std::ofstream out(path, std::ios::binary);
    const auto dataSize = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    out.write("RIFF", 4);
    writeU32(out, 36 + dataSize);
    out.write("WAVEfmt ", 8);
    writeU32(out, 16);
    writeU16(out, 1);
    writeU16(out, 1);
    writeU32(out, 48000);
    writeU32(out, 96000);
    writeU16(out, 2);
    writeU16(out, 16);
    out.write("data", 4);
    writeU32(out, dataSize);
    for ( const auto sample : samples )
        writeU16(out, static_cast<uint16_t>(sample));
}

void writeMono24Wave(const std::filesystem::path &path, const std::vector<char> &sampleBytes) {
    REQUIRE(sampleBytes.size() % 3 == 0);
    std::ofstream out(path, std::ios::binary);
    const auto dataSize = static_cast<uint32_t>(sampleBytes.size());
    out.write("RIFF", 4);
    writeU32(out, 36 + dataSize + (dataSize & 1u));
    out.write("WAVEfmt ", 8);
    writeU32(out, 16);
    writeU16(out, 1);
    writeU16(out, 1);
    writeU32(out, 48000);
    writeU32(out, 144000);
    writeU16(out, 3);
    writeU16(out, 24);
    out.write("data", 4);
    writeU32(out, dataSize);
    out.write(sampleBytes.data(), static_cast<std::streamsize>(sampleBytes.size()));
    if ( dataSize & 1u )
        out.put('\0');
}

uint32_t readU32(std::istream &in) {
    std::array<unsigned char, 4> bytes{};
    in.read(reinterpret_cast<char *>(bytes.data()), 4);
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

std::vector<int16_t> readWaveSamples(const std::filesystem::path &path, uint16_t &channels) {
    std::ifstream in(path, std::ios::binary);
    in.seekg(22);
    std::array<unsigned char, 2> channelBytes{};
    in.read(reinterpret_cast<char *>(channelBytes.data()), 2);
    channels = static_cast<uint16_t>(channelBytes[0] | (channelBytes[1] << 8));
    in.seekg(40);
    const auto dataSize = readU32(in);
    std::vector<int16_t> samples(dataSize / 2);
    in.read(reinterpret_cast<char *>(samples.data()), dataSize);
    return samples;
}

std::vector<char> readWaveData(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    in.seekg(40);
    const auto dataSize = readU32(in);
    std::vector<char> data(dataSize);
    in.read(data.data(), dataSize);
    return data;
}

AudioTrackData makeTrack(const std::string &name, const double pan, const std::string &source) {
    AudioTrackData track;
    track.name = name;
    track.pan = pan;
    ClipData clip;
    clip.name = "Source.wav" + name.substr(name.size() - 2);
    clip.pos = 1.0;
    clip.len = 2.0;
    clip.srcOffset = 0.25;
    clip.fadeInLen = 0.1;
    clip.fadeOutLen = 0.2;
    clip.source = {"WAVE", source};
    track.clips.push_back(clip);
    return track;
}

struct TempDir {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
                                 ("reaaf-stereo-test-" + std::to_string(std::rand()));
    TempDir() { std::filesystem::create_directories(path); }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

} // namespace

TEST_CASE("Premiere breakout pair is recombined without changing sample bytes") {
    TempDir temp;
    const auto leftPath = temp.path / "source_L.wav";
    const auto rightPath = temp.path / "source_R.wav";
    writeMono16Wave(leftPath, {1, -2, 300, -400});
    writeMono16Wave(rightPath, {10, -20, 3000, -4000});

    std::vector<AudioTrackData> tracks{
        makeTrack("MX_L", -1.0, leftPath.string()), makeTrack("MX_R", 1.0, rightPath.string())};
    LogBuffer log;
    recombinePremiereBreakoutStereo(tracks, temp.path.string(), log);

    REQUIRE(tracks.size() == 1);
    CHECK(tracks[0].name == "MX");
    CHECK(tracks[0].pan == 0.0);
    CHECK(tracks[0].clips[0].name == "Source.wav");
    uint16_t channels = 0;
    const auto samples = readWaveSamples(tracks[0].clips[0].source.filePath, channels);
    CHECK(channels == 2);
    CHECK(samples == std::vector<int16_t>{1, 10, -2, -20, 300, 3000, -400, -4000});
}

TEST_CASE("non-identical edits stay as separate mono tracks") {
    TempDir temp;
    const auto leftPath = temp.path / "source_L.wav";
    const auto rightPath = temp.path / "source_R.wav";
    writeMono16Wave(leftPath, {1, 2});
    writeMono16Wave(rightPath, {3, 4});
    auto left = makeTrack("FX_L", -1.0, leftPath.string());
    auto right = makeTrack("FX_R", 1.0, rightPath.string());
    right.clips[0].srcOffset += 0.5;
    std::vector<AudioTrackData> tracks{left, right};
    LogBuffer log;

    recombinePremiereBreakoutStereo(tracks, temp.path.string(), log);

    REQUIRE(tracks.size() == 2);
    CHECK(tracks[0].name == "FX_L");
    CHECK(tracks[1].name == "FX_R");
}

TEST_CASE("24-bit Premiere-style sample bytes are interleaved losslessly") {
    TempDir temp;
    const auto leftPath = temp.path / "source_L.wav";
    const auto rightPath = temp.path / "source_R.wav";
    writeMono24Wave(leftPath, {char(0x01), char(0x02), char(0x03), char(0xa1), char(0xa2),
                               char(0xa3)});
    writeMono24Wave(rightPath, {char(0x11), char(0x12), char(0x13), char(0xb1), char(0xb2),
                                char(0xb3)});
    std::vector<AudioTrackData> tracks{
        makeTrack("GP_L", -1.0, leftPath.string()), makeTrack("GP_R", 1.0, rightPath.string())};
    LogBuffer log;

    recombinePremiereBreakoutStereo(tracks, temp.path.string(), log);

    REQUIRE(tracks.size() == 1);
    CHECK(readWaveData(tracks[0].clips[0].source.filePath) ==
          std::vector<char>{char(0x01), char(0x02), char(0x03), char(0x11), char(0x12),
                            char(0x13), char(0xa1), char(0xa2), char(0xa3), char(0xb1),
                            char(0xb2), char(0xb3)});
}

TEST_CASE("center-panned L/R names are not guessed to be breakout stereo") {
    TempDir temp;
    const auto leftPath = temp.path / "source_L.wav";
    const auto rightPath = temp.path / "source_R.wav";
    writeMono16Wave(leftPath, {1});
    writeMono16Wave(rightPath, {2});
    std::vector<AudioTrackData> tracks{
        makeTrack("DX_L", 0.0, leftPath.string()), makeTrack("DX_R", 0.0, rightPath.string())};
    LogBuffer log;

    recombinePremiereBreakoutStereo(tracks, temp.path.string(), log);

    CHECK(tracks.size() == 2);
}

TEST_CASE("non-ASCII files created through the narrow path API are recombined") {
    TempDir temp;
    const auto leftPath = temp.path.string() + "/Landstr\xc3\xb6m_L.wav";
    const auto rightPath = temp.path.string() + "/Landstr\xc3\xb6m_R.wav";
    writeMono16WaveNarrow(leftPath, {1, 2});
    writeMono16WaveNarrow(rightPath, {10, 20});
    std::vector<AudioTrackData> tracks{
        makeTrack("MX_L", -1.0, leftPath), makeTrack("MX_R", 1.0, rightPath)};
    LogBuffer log;

    recombinePremiereBreakoutStereo(tracks, temp.path.string(), log);

    REQUIRE(tracks.size() == 1);
    uint16_t channels = 0;
    CHECK(readWaveSamples(tracks[0].clips[0].source.filePath, channels) ==
          std::vector<int16_t>{1, 10, 2, 20});
    CHECK(channels == 2);
}

TEST_CASE("shared mono clips inside a breakout pair become dual mono stereo") {
    TempDir temp;
    const auto monoPath = temp.path / "shared_mono.wav";
    writeMono16Wave(monoPath, {7, -8});
    auto left = makeTrack("FX_L", -1.0, monoPath.string());
    auto right = makeTrack("FX_R", 1.0, monoPath.string());
    left.clips[0].name = "shared_mono.wav";
    right.clips[0].name = "shared_mono.wav";
    std::vector<AudioTrackData> tracks{left, right};
    LogBuffer log;

    recombinePremiereBreakoutStereo(tracks, temp.path.string(), log);

    REQUIRE(tracks.size() == 1);
    uint16_t channels = 0;
    CHECK(readWaveSamples(tracks[0].clips[0].source.filePath, channels) ==
          std::vector<int16_t>{7, 7, -8, -8});
    CHECK(channels == 2);
}
