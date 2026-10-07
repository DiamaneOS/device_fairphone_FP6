// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

#include <gtest/gtest.h>

#include <unistd.h>

#include <string>
#include <vector>

#include <android-base/file.h>
#include <openssl/sha.h>

#include "FirmwareRelease.h"

namespace fwrelease {
namespace {

Digest sha256(const std::string& data, size_t bytes) {
    Digest digest;
    SHA256(reinterpret_cast<const uint8_t*>(data.data()), bytes, digest.data());
    return digest;
}

std::string hex(const Digest& digest) {
    static const char kHex[] = "0123456789abcdef";
    std::string text;
    for (const uint8_t byte : digest) {
        text += kHex[byte >> 4];
        text += kHex[byte & 15];
    }
    return text;
}

// Deterministic test data that does not repeat within a chunk (a
// Lehmer generator; no wrap-around, which the overflow sanitizer would trap).
std::string pattern(size_t bytes, uint64_t seed) {
    std::string data(bytes, '\0');
    for (auto& c : data) {
        seed = seed * 48271 % 2147483647;
        c = static_cast<char>(seed >> 23);
    }
    return data;
}

std::string line(const std::string& name, const std::string& release, const std::string& image) {
    return "image " + name + " " + release + " " + std::to_string(image.size()) + " " +
           hex(sha256(image, image.size())) + "\n";
}

// A partition at a release: the image, then whatever follows it.
struct Partition {
    android::base::TemporaryFile file;
    explicit Partition(const std::string& contents) {
        EXPECT_TRUE(android::base::WriteStringToFd(contents, file.fd));
        EXPECT_EQ(0, lseek(file.fd, 0, SEEK_SET));
    }
};

std::set<std::string> match(const Table& table, const std::string& name, const std::string& contents) {
    Partition partition(contents);
    std::set<uint64_t> lengths;
    for (const auto& image : table.partitions.at(name)) lengths.insert(image.bytes);
    std::vector<uint8_t> buffer(64 * 1024);
    const auto digests = prefixDigests(partition.file.fd, lengths, buffer.data(), buffer.size());
    EXPECT_TRUE(digests.has_value());
    return digests ? matchingReleases(table.partitions.at(name), *digests) : std::set<std::string>{};
}

const std::string kTzOld = pattern(5000, 1);
const std::string kTzNew = pattern(5004, 2);  // a newer image may be longer
const std::string kAblOld = pattern(300000, 3);
const std::string kAblNew = pattern(300000, 4);
const std::string kTail = std::string(200000, '\0') + "partition tail";

std::string twoReleases() {
    return line("abl", "16.111.0", kAblNew) + line("tz", "16.111.0", kTzNew) +
           line("abl", "16.100.0", kAblOld) + line("tz", "16.100.0", kTzOld);
}

TEST(FirmwareReleaseTest, ParsesTheTable) {
    const auto table = parseTable(twoReleases());
    ASSERT_TRUE(table.has_value());
    EXPECT_EQ((std::set<std::string>{"16.100.0", "16.111.0"}), table->releases);
    ASSERT_EQ(2u, table->partitions.size());
    const auto& tz = table->partitions.at("tz");
    ASSERT_EQ(2u, tz.size());
    EXPECT_EQ("16.111.0", tz[0].release);
    EXPECT_EQ(5004u, tz[0].bytes);
    EXPECT_EQ(sha256(kTzNew, kTzNew.size()), tz[0].sha256);
}

TEST(FirmwareReleaseTest, RejectsMalformedTables) {
    const std::string digest(64, 'a');
    const std::string good = "image tz 16.111.0 5 " + digest + "\n";
    ASSERT_TRUE(parseTable(good));
    for (const std::string& text : std::vector<std::string>{
                 "",
                 "\n",
                 "image tz 16.111.0 5 " + digest,                // no newline
                 good + "\n",                                    // empty line
                 "image tz 16.111.0 5 " + digest + "\r\n",       // CRLF
                 "image  tz 16.111.0 5 " + digest + "\n",        // empty field
                 "image tz 16.111.0 5 " + digest + " x\n",       // extra field
                 "img tz 16.111.0 5 " + digest + "\n",           // keyword
                 "image TZ 16.111.0 5 " + digest + "\n",         // upper case
                 "image ../tz 16.111.0 5 " + digest + "\n",      // path
                 "image tz/a 16.111.0 5 " + digest + "\n",
                 "image " + std::string(33, 'a') + " 16.111.0 5 " + digest + "\n",
                 "image tz 16.111 5 " + digest + "\n",           // release
                 "image tz 16.111.0.1 5 " + digest + "\n",
                 "image tz 16.111.x 5 " + digest + "\n",
                 "image tz 16.111.123456 5 " + digest + "\n",
                 "image tz 16..0 5 " + digest + "\n",
                 "image tz 16.111.0 0 " + digest + "\n",         // bytes
                 "image tz 16.111.0 05 " + digest + "\n",
                 "image tz 16.111.0 +5 " + digest + "\n",
                 "image tz 16.111.0 -5 " + digest + "\n",
                 "image tz 16.111.0 536870913 " + digest + "\n", // over 512 MiB
                 "image tz 16.111.0 99999999999 " + digest + "\n",
                 "image tz 16.111.0 5 " + std::string(64, 'A') + "\n",  // digest
                 "image tz 16.111.0 5 " + std::string(63, 'a') + "\n",
                 "image tz 16.111.0 5 " + std::string(64, 'g') + "\n",
                 good + good,                                    // repeated release
                 // A partition without an image of every release.
                 good + "image tz 16.100.0 5 " + digest + "\nimage abl 16.100.0 5 " + digest + "\n",
         }) {
        EXPECT_FALSE(parseTable(text)) << text;
    }
    std::string large;
    while (large.size() <= kMaxTableBytes) large += good;
    EXPECT_FALSE(parseTable(large));
}

TEST(FirmwareReleaseTest, ComparesReleasesByNumber) {
    EXPECT_TRUE(newerRelease("16.111.0", "16.100.0"));
    EXPECT_TRUE(newerRelease("16.100.0", "16.99.0"));
    EXPECT_TRUE(newerRelease("17.0.0", "16.999.9"));
    EXPECT_FALSE(newerRelease("16.100.0", "16.100.0"));
    EXPECT_FALSE(newerRelease("15.178.0", "16.100.0"));
}

TEST(FirmwareReleaseTest, HashesEachPrefixInOnePass) {
    const std::string data = pattern(3 * 1024 * 1024 + 123, 5);
    const std::set<uint64_t> lengths = {1, 4096, 1024 * 1024, 1024 * 1024 + 1, data.size(),
                                        data.size() + 1, 8 * 1024 * 1024};
    for (const size_t bufferSize : {size_t{4096}, size_t{1024 * 1024}}) {
        Partition partition(data);
        std::vector<uint8_t> buffer(bufferSize);
        const auto digests = prefixDigests(partition.file.fd, lengths, buffer.data(), buffer.size());
        ASSERT_TRUE(digests.has_value());
        // Lengths past the end have no digest.
        EXPECT_EQ(5u, digests->size());
        for (const uint64_t length : lengths) {
            if (length > data.size()) {
                EXPECT_EQ(0u, digests->count(length)) << length;
            } else {
                EXPECT_EQ(sha256(data, length), digests->at(length)) << length;
            }
        }
    }
}

TEST(FirmwareReleaseTest, ReadErrorHasNoDigests) {
    uint8_t buffer[4096];
    EXPECT_FALSE(prefixDigests(-1, {1}, buffer, sizeof(buffer)));
}

TEST(FirmwareReleaseTest, ReportsTheReleaseEveryPartitionMatches) {
    const auto table = parseTable(twoReleases());
    ASSERT_TRUE(table.has_value());
    std::map<std::string, std::set<std::string>> matches = {
            {"abl", match(*table, "abl", kAblNew + kTail)},
            {"tz", match(*table, "tz", kTzNew + kTail)},
    };
    EXPECT_EQ((std::set<std::string>{"16.111.0"}), matches["tz"]);
    EXPECT_EQ("16.111.0", result(*table, matches));
    matches = {{"abl", match(*table, "abl", kAblOld + kTail)},
               {"tz", match(*table, "tz", kTzOld + kTail)}};
    EXPECT_EQ("16.100.0", result(*table, matches));
}

TEST(FirmwareReleaseTest, ReportsMixedReleases) {
    const auto table = parseTable(twoReleases());
    ASSERT_TRUE(table.has_value());
    const std::map<std::string, std::set<std::string>> matches = {
            {"abl", match(*table, "abl", kAblNew + kTail)},
            {"tz", match(*table, "tz", kTzOld + kTail)},
    };
    EXPECT_EQ(kMixed, result(*table, matches));
}

TEST(FirmwareReleaseTest, ReportsUnknownWhenAPartitionMatchesNothing) {
    const auto table = parseTable(twoReleases());
    ASSERT_TRUE(table.has_value());
    std::string changed = kTzNew;
    changed[100] ^= 1;
    std::map<std::string, std::set<std::string>> matches = {
            {"abl", match(*table, "abl", kAblNew + kTail)},
            {"tz", match(*table, "tz", changed + kTail)},
    };
    EXPECT_TRUE(matches["tz"].empty());
    EXPECT_EQ(kUnknown, result(*table, matches));
    // A partition shorter than every image of it.
    matches["tz"] = match(*table, "tz", kTzNew.substr(0, 4000));
    EXPECT_EQ(kUnknown, result(*table, matches));
    // A partition that was not read.
    matches.erase("tz");
    EXPECT_EQ(kUnknown, result(*table, matches));
}

TEST(FirmwareReleaseTest, ByteIdenticalReleasesReportTheNewest) {
    // 16.100.0 and 16.111.0 ship the same tz; abl tells them apart.
    const std::string text = line("abl", "16.111.0", kAblNew) + line("tz", "16.111.0", kTzOld) +
                             line("abl", "16.100.0", kAblOld) + line("tz", "16.100.0", kTzOld);
    auto table = parseTable(text);
    ASSERT_TRUE(table.has_value());
    std::map<std::string, std::set<std::string>> matches = {
            {"abl", match(*table, "abl", kAblOld + kTail)},
            {"tz", match(*table, "tz", kTzOld + kTail)},
    };
    EXPECT_EQ((std::set<std::string>{"16.100.0", "16.111.0"}), matches["tz"]);
    EXPECT_EQ("16.100.0", result(*table, matches));
    // Every image identical: the newest release.
    table = parseTable(line("tz", "16.99.0", kTzOld) + line("tz", "16.100.0", kTzOld));
    ASSERT_TRUE(table.has_value());
    matches = {{"tz", match(*table, "tz", kTzOld)}};
    EXPECT_EQ("16.100.0", result(*table, matches));
}

}  // namespace
}  // namespace fwrelease
