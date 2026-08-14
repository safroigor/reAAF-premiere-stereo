/*
 * Copyright (C) 2026 reAAF contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "StereoRecombiner.h"

#include "LogBuffer.h"
#include "helpers.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <unordered_map>

namespace {

constexpr double kEpsilon = 1e-8;

bool nearlyEqual(const double a, const double b) { return std::abs(a - b) <= kEpsilon; }

enum class Side { Left, Right };

struct SidedName {
    std::string base;
    Side side;
};

std::optional<SidedName> splitSidedName(const std::string &name) {
    if ( name.size() < 3 )
        return std::nullopt;
    const auto separator = name[name.size() - 2];
    auto suffix = name.back();
    if ( suffix >= 'a' && suffix <= 'z' )
        suffix = static_cast<char>(suffix - 'a' + 'A');
    if ( (separator != '_' && separator != '.') || (suffix != 'L' && suffix != 'R') )
        return std::nullopt;
    return SidedName{name.substr(0, name.size() - 2),
                     suffix == 'L' ? Side::Left : Side::Right};
}

bool envelopesEqual(const std::optional<EnvelopeData> &a,
                    const std::optional<EnvelopeData> &b) {
    if ( a.has_value() != b.has_value() )
        return false;
    if ( !a )
        return true;
    if ( a->tag != b->tag || a->arm != b->arm || a->points.size() != b->points.size() )
        return false;
    for ( size_t i = 0; i < a->points.size(); ++i ) {
        if ( !nearlyEqual(a->points[i].t, b->points[i].t) ||
             !nearlyEqual(a->points[i].value, b->points[i].value) )
            return false;
    }
    return true;
}

bool clipsMatch(const ClipData &left, const ClipData &right) {
    const auto leftName = splitSidedName(left.name);
    const auto rightName = splitSidedName(right.name);
    if ( !leftName || !rightName || leftName->side != Side::Left ||
         rightName->side != Side::Right || leftName->base != rightName->base )
        return false;
    return nearlyEqual(left.pos, right.pos) && nearlyEqual(left.len, right.len) &&
           nearlyEqual(left.srcOffset, right.srcOffset) && nearlyEqual(left.gain, right.gain) &&
           left.mute == right.mute && nearlyEqual(left.fadeInLen, right.fadeInLen) &&
           left.fadeInShape == right.fadeInShape && nearlyEqual(left.fadeOutLen, right.fadeOutLen) &&
           left.fadeOutShape == right.fadeOutShape && envelopesEqual(left.automation, right.automation) &&
           left.source.type == "WAVE" && right.source.type == "WAVE" &&
           !left.source.filePath.empty() && !right.source.filePath.empty() &&
           left.source.filePath != right.source.filePath;
}

bool tracksMatch(const AudioTrackData &left, const AudioTrackData &right) {
    const auto leftName = splitSidedName(left.name);
    const auto rightName = splitSidedName(right.name);
    if ( !leftName || !rightName || leftName->side != Side::Left ||
         rightName->side != Side::Right || leftName->base != rightName->base )
        return false;
    // Premiere's breakout tracks are hard-panned. Requiring this prevents a
    // coincidental _L/_R naming convention from silently changing the mix.
    if ( left.pan > -0.99 || right.pan < 0.99 )
        return false;
    if ( !nearlyEqual(left.vol, right.vol) || left.mute != right.mute || left.solo != right.solo ||
         !envelopesEqual(left.gainEnv, right.gainEnv) || left.clips.size() != right.clips.size() ||
         left.clips.empty() )
        return false;
    for ( size_t i = 0; i < left.clips.size(); ++i )
        if ( !clipsMatch(left.clips[i], right.clips[i]) )
            return false;
    return true;
}

uint16_t readU16(const std::vector<char> &v, const size_t at) {
    return static_cast<uint16_t>(static_cast<unsigned char>(v[at])) |
           static_cast<uint16_t>(static_cast<unsigned char>(v[at + 1]) << 8);
}

uint32_t readU32(const char *v) {
    return static_cast<uint32_t>(static_cast<unsigned char>(v[0])) |
           (static_cast<uint32_t>(static_cast<unsigned char>(v[1])) << 8) |
           (static_cast<uint32_t>(static_cast<unsigned char>(v[2])) << 16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(v[3])) << 24);
}

uint32_t readU32(const std::vector<char> &v, const size_t at) { return readU32(v.data() + at); }

void putU16(std::vector<char> &v, const size_t at, const uint16_t n) {
    v[at] = static_cast<char>(n & 0xff);
    v[at + 1] = static_cast<char>((n >> 8) & 0xff);
}

void putU32(std::vector<char> &v, const size_t at, const uint32_t n) {
    v[at] = static_cast<char>(n & 0xff);
    v[at + 1] = static_cast<char>((n >> 8) & 0xff);
    v[at + 2] = static_cast<char>((n >> 16) & 0xff);
    v[at + 3] = static_cast<char>((n >> 24) & 0xff);
}

void writeU32(std::ostream &out, const uint32_t n) {
    const std::array<char, 4> b{static_cast<char>(n & 0xff), static_cast<char>((n >> 8) & 0xff),
                                static_cast<char>((n >> 16) & 0xff),
                                static_cast<char>((n >> 24) & 0xff)};
    out.write(b.data(), static_cast<std::streamsize>(b.size()));
}

struct WaveChunk {
    std::array<char, 4> id{};
    uint32_t size = 0;
    uint64_t dataOffset = 0;
};

struct WaveInfo {
    std::filesystem::path path;
    std::vector<WaveChunk> chunks;
    std::vector<char> format;
    uint64_t dataOffset = 0;
    uint32_t dataSize = 0;
    uint16_t formatTag = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint32_t byteRate = 0;
    uint16_t blockAlign = 0;
    uint16_t bitsPerSample = 0;
};

bool idIs(const std::array<char, 4> &id, const char *s) {
    return std::equal(id.begin(), id.end(), s);
}

bool readWaveInfo(const std::string &path, WaveInfo &info, std::string &error) {
    info.path = std::filesystem::u8path(path);
    std::ifstream in(info.path, std::ios::binary);
    std::array<char, 12> header{};
    if ( !in.read(header.data(), static_cast<std::streamsize>(header.size())) ||
         std::string(header.data(), 4) != "RIFF" || std::string(header.data() + 8, 4) != "WAVE" ) {
        error = "not a RIFF/WAVE file";
        return false;
    }

    bool foundFormat = false;
    bool foundData = false;
    while ( in ) {
        WaveChunk chunk;
        std::array<char, 8> chunkHeader{};
        if ( !in.read(chunkHeader.data(), static_cast<std::streamsize>(chunkHeader.size())) )
            break;
        std::copy_n(chunkHeader.data(), 4, chunk.id.data());
        chunk.size = readU32(chunkHeader.data() + 4);
        chunk.dataOffset = static_cast<uint64_t>(in.tellg());
        info.chunks.push_back(chunk);

        if ( idIs(chunk.id, "fmt ") ) {
            if ( foundFormat || chunk.size < 16 || chunk.size > 4096 ) {
                error = "unsupported fmt chunk";
                return false;
            }
            info.format.resize(chunk.size);
            if ( !in.read(info.format.data(), chunk.size) ) {
                error = "truncated fmt chunk";
                return false;
            }
            foundFormat = true;
        } else {
            if ( idIs(chunk.id, "data") ) {
                if ( foundData ) {
                    error = "multiple data chunks are unsupported";
                    return false;
                }
                foundData = true;
                info.dataOffset = chunk.dataOffset;
                info.dataSize = chunk.size;
            }
            in.seekg(static_cast<std::streamoff>(chunk.size), std::ios::cur);
        }
        if ( chunk.size & 1u )
            in.seekg(1, std::ios::cur);
    }
    if ( !foundFormat || !foundData ) {
        error = "missing fmt or data chunk";
        return false;
    }

    info.formatTag = readU16(info.format, 0);
    info.channels = readU16(info.format, 2);
    info.sampleRate = readU32(info.format, 4);
    info.byteRate = readU32(info.format, 8);
    info.blockAlign = readU16(info.format, 12);
    info.bitsPerSample = readU16(info.format, 14);
    if ( (info.formatTag != 1 && info.formatTag != 3 && info.formatTag != 0xfffe) ||
         info.channels != 1 || info.blockAlign == 0 || info.dataSize % info.blockAlign != 0 ) {
        error = "source must be a supported mono PCM/float WAVE";
        return false;
    }
    if ( info.formatTag == 0xfffe && info.format.size() < 40 ) {
        error = "truncated extensible WAVE format";
        return false;
    }
    return true;
}

bool compatibleWaves(const WaveInfo &left, const WaveInfo &right, std::string &error) {
    if ( left.formatTag != right.formatTag || left.sampleRate != right.sampleRate ||
         left.bitsPerSample != right.bitsPerSample || left.blockAlign != right.blockAlign ||
         left.dataSize != right.dataSize || left.format != right.format ) {
        error = "left and right WAVE formats or sample counts differ";
        return false;
    }
    if ( left.dataSize > std::numeric_limits<uint32_t>::max() / 2u ) {
        error = "stereo result would exceed classic RIFF size limits";
        return false;
    }
    return true;
}

bool copyBytes(std::ifstream &in, std::ofstream &out, uint64_t count) {
    std::array<char, 64 * 1024> buffer{};
    while ( count > 0 ) {
        const auto amount = static_cast<std::streamsize>(std::min<uint64_t>(count, buffer.size()));
        if ( !in.read(buffer.data(), amount) )
            return false;
        out.write(buffer.data(), amount);
        if ( !out )
            return false;
        count -= static_cast<uint64_t>(amount);
    }
    return true;
}

bool interleaveWave(const std::string &leftPath, const std::string &rightPath,
                    const std::filesystem::path &outputPath, std::string &error) {
    WaveInfo left, right;
    if ( !readWaveInfo(leftPath, left, error) || !readWaveInfo(rightPath, right, error) ||
         !compatibleWaves(left, right, error) )
        return false;

    const uint64_t stereoDataSize = static_cast<uint64_t>(left.dataSize) * 2u;
    uint64_t riffSize = 4;
    for ( const auto &chunk : left.chunks ) {
        const uint64_t size = idIs(chunk.id, "data") ? stereoDataSize : chunk.size;
        riffSize += 8 + size + (size & 1u);
    }
    if ( riffSize > std::numeric_limits<uint32_t>::max() ) {
        error = "stereo result would exceed classic RIFF size limits";
        return false;
    }

    auto stereoFormat = left.format;
    putU16(stereoFormat, 2, 2);
    putU16(stereoFormat, 12, static_cast<uint16_t>(left.blockAlign * 2));
    putU32(stereoFormat, 8, left.byteRate * 2);
    if ( left.formatTag == 0xfffe )
        putU32(stereoFormat, 20, 3); // front-left | front-right speaker mask

    const auto temporaryPath = outputPath.string() + ".tmp";
    std::ifstream leftIn(left.path, std::ios::binary);
    std::ifstream rightIn(right.path, std::ios::binary);
    std::ofstream out(temporaryPath, std::ios::binary | std::ios::trunc);
    if ( !leftIn || !rightIn || !out ) {
        error = "could not open WAVE input or output";
        return false;
    }
    out.write("RIFF", 4);
    writeU32(out, static_cast<uint32_t>(riffSize));
    out.write("WAVE", 4);

    for ( const auto &chunk : left.chunks ) {
        out.write(chunk.id.data(), 4);
        if ( idIs(chunk.id, "fmt ") ) {
            writeU32(out, static_cast<uint32_t>(stereoFormat.size()));
            out.write(stereoFormat.data(), static_cast<std::streamsize>(stereoFormat.size()));
            if ( stereoFormat.size() & 1u )
                out.put('\0');
        } else if ( idIs(chunk.id, "data") ) {
            writeU32(out, static_cast<uint32_t>(stereoDataSize));
            leftIn.seekg(static_cast<std::streamoff>(left.dataOffset));
            rightIn.seekg(static_cast<std::streamoff>(right.dataOffset));
            const uint64_t frameCount = left.dataSize / left.blockAlign;
            constexpr uint64_t kFramesPerBlock = 8192;
            std::vector<char> leftBlock(static_cast<size_t>(kFramesPerBlock * left.blockAlign));
            std::vector<char> rightBlock(static_cast<size_t>(kFramesPerBlock * right.blockAlign));
            std::vector<char> stereoBlock(
                static_cast<size_t>(kFramesPerBlock * left.blockAlign * 2u));
            for ( uint64_t firstFrame = 0; firstFrame < frameCount; firstFrame += kFramesPerBlock ) {
                const auto frames = std::min(kFramesPerBlock, frameCount - firstFrame);
                const auto monoBytes = static_cast<std::streamsize>(frames * left.blockAlign);
                if ( !leftIn.read(leftBlock.data(), monoBytes) ||
                     !rightIn.read(rightBlock.data(), monoBytes) ) {
                    error = "truncated WAVE sample data";
                    out.close();
                    std::filesystem::remove(std::filesystem::u8path(temporaryPath));
                    return false;
                }
                for ( uint64_t frame = 0; frame < frames; ++frame ) {
                    const auto monoOffset = static_cast<size_t>(frame * left.blockAlign);
                    const auto stereoOffset = monoOffset * 2u;
                    std::copy_n(leftBlock.data() + monoOffset, left.blockAlign,
                                stereoBlock.data() + stereoOffset);
                    std::copy_n(rightBlock.data() + monoOffset, right.blockAlign,
                                stereoBlock.data() + stereoOffset + left.blockAlign);
                }
                out.write(stereoBlock.data(), static_cast<std::streamsize>(monoBytes * 2));
            }
        } else {
            writeU32(out, chunk.size);
            leftIn.seekg(static_cast<std::streamoff>(chunk.dataOffset));
            if ( !copyBytes(leftIn, out, chunk.size) ) {
                error = "failed to copy WAVE metadata";
                out.close();
                std::filesystem::remove(std::filesystem::u8path(temporaryPath));
                return false;
            }
            if ( chunk.size & 1u )
                out.put('\0');
        }
    }
    out.close();
    if ( !out ) {
        error = "failed to finish stereo WAVE";
        std::filesystem::remove(std::filesystem::u8path(temporaryPath));
        return false;
    }
    std::error_code ec;
    std::filesystem::remove(outputPath, ec);
    ec.clear();
    std::filesystem::rename(std::filesystem::u8path(temporaryPath), outputPath, ec);
    if ( ec ) {
        error = "failed to install stereo WAVE: " + ec.message();
        std::filesystem::remove(std::filesystem::u8path(temporaryPath));
        return false;
    }
    return true;
}

uint64_t fnv1a(const std::string &value) {
    uint64_t hash = 14695981039346656037ull;
    for ( const auto c : value ) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string safeBaseName(const std::string &name) {
    std::string result;
    for ( const unsigned char c : name ) {
        if ( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
             (c >= '0' && c <= '9') || c == '-' )
            result.push_back(static_cast<char>(c));
        else if ( !result.empty() && result.back() != '_' )
            result.push_back('_');
    }
    while ( !result.empty() && result.back() == '_' )
        result.pop_back();
    return result.empty() ? "stereo" : result;
}

std::string pairKey(const std::string &left, const std::string &right) {
    return left + '\n' + right;
}

} // namespace

void recombinePremiereBreakoutStereo(std::vector<AudioTrackData> &tracks,
                                     const std::string &outputDir, LogBuffer &logBuffer) {
    std::vector<AudioTrackData> result;
    result.reserve(tracks.size());
    std::unordered_map<std::string, std::string> generatedSources;

    for ( size_t i = 0; i < tracks.size(); ) {
        const AudioTrackData *left = nullptr;
        const AudioTrackData *right = nullptr;
        if ( i + 1 < tracks.size() ) {
            const auto first = splitSidedName(tracks[i].name);
            const auto second = splitSidedName(tracks[i + 1].name);
            if ( first && second && first->base == second->base && first->side != second->side ) {
                left = first->side == Side::Left ? &tracks[i] : &tracks[i + 1];
                right = first->side == Side::Right ? &tracks[i] : &tracks[i + 1];
            }
        }

        if ( !left || !right || !tracksMatch(*left, *right) ) {
            result.push_back(std::move(tracks[i]));
            ++i;
            continue;
        }

        if ( !ensure_dir(outputDir) ) {
            logBuffer.logf(LogEntry::WARN,
                           "Could not create media directory; keeping '%s' and '%s' as mono",
                           left->name.c_str(), right->name.c_str());
            result.push_back(std::move(tracks[i]));
            ++i;
            continue;
        }

        AudioTrackData merged = *left;
        merged.name = splitSidedName(left->name)->base;
        merged.pan = 0.0;
        merged.panEnv.reset();
        merged.nchan = 2;
        bool success = true;

        for ( size_t clipIndex = 0; clipIndex < merged.clips.size(); ++clipIndex ) {
            auto &mergedClip = merged.clips[clipIndex];
            const auto &rightClip = right->clips[clipIndex];
            const auto key = pairKey(mergedClip.source.filePath, rightClip.source.filePath);
            auto generated = generatedSources.find(key);
            if ( generated == generatedSources.end() ) {
                std::ostringstream hash;
                hash << std::hex << std::setw(16) << std::setfill('0') << fnv1a(key);
                const auto filename = safeBaseName(merged.name) + "__stereo_" + hash.str() + ".wav";
                const auto outputPath = std::filesystem::u8path(outputDir) / filename;
                std::string error;
                if ( !interleaveWave(mergedClip.source.filePath, rightClip.source.filePath,
                                     outputPath, error) ) {
                    logBuffer.logf(LogEntry::WARN,
                                   "Could not recombine '%s' and '%s': %s; keeping mono tracks",
                                   left->name.c_str(), right->name.c_str(), error.c_str());
                    success = false;
                    break;
                }
                generated = generatedSources.emplace(key, outputPath.u8string()).first;
            }
            mergedClip.name = splitSidedName(mergedClip.name)->base;
            mergedClip.source = {"WAVE", generated->second};
        }

        if ( success ) {
            logBuffer.logf(LogEntry::INFO, "Recombined Premiere breakout tracks '%s' + '%s' as stereo",
                           left->name.c_str(), right->name.c_str());
            result.push_back(std::move(merged));
            i += 2;
        } else {
            result.push_back(std::move(tracks[i]));
            ++i;
        }
    }
    tracks = std::move(result);
}
