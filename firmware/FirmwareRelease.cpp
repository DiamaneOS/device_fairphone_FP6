// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#include "FirmwareRelease.h"

#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <iterator>
#include <tuple>
#include <utility>

#include <openssl/sha.h>

namespace fwrelease {

namespace {

std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    for (;;) {
        const size_t end = text.find(separator);
        parts.push_back(text.substr(0, end));
        if (end == std::string_view::npos) return parts;
        text.remove_prefix(end + 1);
    }
}

bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

// [a-z0-9_-]{1,32}: a partition name, so it cannot leave /dev/block/by-name.
bool validName(std::string_view name) {
    return !name.empty() && name.size() <= 32 &&
           std::all_of(name.begin(), name.end(), [](char c) {
               return (c >= 'a' && c <= 'z') || isDigit(c) || c == '_' || c == '-';
           });
}

// N.N.N with 1 to 5 digits each.
std::optional<std::tuple<int, int, int>> parseRelease(std::string_view release) {
    const auto parts = split(release, '.');
    if (parts.size() != 3) return std::nullopt;
    int numbers[3];
    for (size_t i = 0; i < 3; ++i) {
        if (parts[i].empty() || parts[i].size() > 5 ||
            !std::all_of(parts[i].begin(), parts[i].end(), isDigit)) {
            return std::nullopt;
        }
        numbers[i] = 0;
        for (const char c : parts[i]) numbers[i] = numbers[i] * 10 + (c - '0');
    }
    return std::make_tuple(numbers[0], numbers[1], numbers[2]);
}

// A decimal from 1 to kMaxImageBytes without sign or leading zero.
std::optional<uint64_t> parseBytes(std::string_view text) {
    if (text.empty() || text.size() > 10 || text[0] == '0' ||
        !std::all_of(text.begin(), text.end(), isDigit)) {
        return std::nullopt;
    }
    uint64_t value = 0;
    for (const char c : text) value = value * 10 + static_cast<uint64_t>(c - '0');
    if (value > kMaxImageBytes) return std::nullopt;
    return value;
}

// 64 lowercase hex digits.
std::optional<Digest> parseDigest(std::string_view text) {
    if (text.size() != 64) return std::nullopt;
    Digest digest;
    for (size_t i = 0; i < digest.size(); ++i) {
        uint8_t byte = 0;
        for (const char c : text.substr(2 * i, 2)) {
            byte <<= 4;
            if (isDigit(c)) {
                byte |= static_cast<uint8_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                byte |= static_cast<uint8_t>(c - 'a' + 10);
            } else {
                return std::nullopt;
            }
        }
        digest[i] = byte;
    }
    return digest;
}

}  // namespace

std::optional<Table> parseTable(std::string_view text) {
    if (text.empty() || text.size() > kMaxTableBytes || text.back() != '\n') return std::nullopt;
    text.remove_suffix(1);
    Table table;
    for (const std::string_view line : split(text, '\n')) {
        const auto fields = split(line, ' ');
        if (fields.size() != 5 || fields[0] != "image" || !validName(fields[1]) ||
            !parseRelease(fields[2])) {
            return std::nullopt;
        }
        const auto bytes = parseBytes(fields[3]);
        const auto digest = parseDigest(fields[4]);
        if (!bytes || !digest) return std::nullopt;
        auto& images = table.partitions[std::string(fields[1])];
        if (std::any_of(images.begin(), images.end(),
                        [&](const Image& image) { return image.release == fields[2]; })) {
            return std::nullopt;
        }
        images.push_back({std::string(fields[2]), *bytes, *digest});
        table.releases.insert(std::string(fields[2]));
    }
    if (table.partitions.size() > kMaxPartitions || table.releases.size() > kMaxReleases) {
        return std::nullopt;
    }
    // Each partition has an image of every release, so no release can match
    // because a partition is not compared.
    for (const auto& [name, images] : table.partitions) {
        if (images.size() != table.releases.size()) return std::nullopt;
    }
    return table;
}

bool newerRelease(const std::string& a, const std::string& b) {
    return parseRelease(a) > parseRelease(b);
}

std::optional<std::map<uint64_t, Digest>> prefixDigests(int fd, const std::set<uint64_t>& lengths,
                                                        uint8_t* buffer, size_t bufferSize) {
    std::map<uint64_t, Digest> digests;
    SHA256_CTX context;
    SHA256_Init(&context);
    uint64_t hashed = 0;
    auto next = lengths.begin();
    while (next != lengths.end()) {
        ssize_t n;
        do {
            n = read(fd, buffer, bufferSize);
        } while (n < 0 && errno == EINTR);
        if (n < 0) return std::nullopt;
        // The end of the data: the longer images do not fit.
        if (n == 0) break;
        const uint64_t end = hashed + static_cast<uint64_t>(n);
        size_t used = 0;
        // Finish a copy of the running hash at each length this read reaches.
        while (next != lengths.end() && *next <= end) {
            const size_t take = static_cast<size_t>(*next - hashed);
            SHA256_Update(&context, buffer + used, take);
            used += take;
            hashed = *next;
            SHA256_CTX prefix = context;
            Digest digest;
            SHA256_Final(digest.data(), &prefix);
            digests[*next] = digest;
            ++next;
        }
        if (next == lengths.end()) break;
        SHA256_Update(&context, buffer + used, static_cast<size_t>(n) - used);
        hashed = end;
    }
    return digests;
}

std::set<std::string> matchingReleases(const std::vector<Image>& images,
                                       const std::map<uint64_t, Digest>& digests) {
    std::set<std::string> releases;
    for (const Image& image : images) {
        const auto digest = digests.find(image.bytes);
        if (digest != digests.end() && digest->second == image.sha256) releases.insert(image.release);
    }
    return releases;
}

std::string result(const Table& table, const std::map<std::string, std::set<std::string>>& matches) {
    std::optional<std::set<std::string>> common;
    for (const auto& [name, images] : table.partitions) {
        const auto match = matches.find(name);
        if (match == matches.end() || match->second.empty()) return kUnknown;
        if (!common) {
            common = match->second;
            continue;
        }
        std::set<std::string> both;
        std::set_intersection(common->begin(), common->end(), match->second.begin(),
                              match->second.end(), std::inserter(both, both.end()));
        common = std::move(both);
    }
    if (!common) return kUnknown;
    if (common->empty()) return kMixed;
    return *std::max_element(common->begin(), common->end(),
                             [](const std::string& a, const std::string& b) {
                                 return newerRelease(b, a);
                             });
}

}  // namespace fwrelease
