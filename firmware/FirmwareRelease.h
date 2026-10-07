// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace fwrelease {

using Digest = std::array<uint8_t, 32>;

// What a partition holds at one release: the image's length and SHA-256.
// Partitions are larger than their images; only the image length is hashed.
struct Image {
    std::string release;  // "16.111.0"
    uint64_t bytes;
    Digest sha256;
};

// The firmware release table, by partition name without slot suffix. Every
// partition has exactly one image per release.
struct Table {
    std::map<std::string, std::vector<Image>> partitions;
    std::set<std::string> releases;
};

// Limits the tools' table generator also enforces (firmware_release.py).
constexpr size_t kMaxTableBytes = 256 * 1024;
constexpr uint64_t kMaxImageBytes = 512ull * 1024 * 1024;
constexpr size_t kMaxPartitions = 64;
constexpr size_t kMaxReleases = 64;

// The reported values besides a release.
constexpr char kMixed[] = "mixed";
constexpr char kUnknown[] = "unknown";

// Parses the table: lines "image <partition> <release> <bytes> <sha256>\n".
// nullopt unless every line is valid, no partition lists a release twice and
// every partition lists the same releases.
std::optional<Table> parseTable(std::string_view text);

// Whether release a is newer than release b (both valid, "N.N.N").
bool newerRelease(const std::string& a, const std::string& b);

// The SHA-256 of the first n bytes read from fd, for each n in lengths, in one
// pass of reads of bufferSize bytes into buffer (aligned for O_DIRECT by the
// caller). A length past the end of the data has no digest. nullopt if a read
// fails.
std::optional<std::map<uint64_t, Digest>> prefixDigests(int fd, const std::set<uint64_t>& lengths,
                                                        uint8_t* buffer, size_t bufferSize);

// The releases whose image the partition holds, from its prefix digests.
std::set<std::string> matchingReleases(const std::vector<Image>& images,
                                       const std::map<uint64_t, Digest>& digests);

// What to report from each partition's matching releases: the newest release
// every partition matches (several can match only if their images are
// byte-identical), kMixed if no release matches them all, kUnknown if a
// partition matches none or is missing.
std::string result(const Table& table, const std::map<std::string, std::set<std::string>>& matches);

}  // namespace fwrelease
