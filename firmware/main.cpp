// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// fwrelease reports which Fairphone firmware release the booted slot runs, for
// Settings (About phone > Android version). The phone cannot say: every
// Qualcomm version string is the same across releases. So it hashes each A/B
// firmware partition of the slot, up to the image length, and compares the
// hashes with the table in the vendor image (written by the tools from their
// firmware inventory, covered by dm-verity). It sets
// ro.vendor.diamaneos.firmware_release to the release every partition
// matches, "mixed" if no release matches them all, or "unknown", logs one line
// and exits. It runs once per boot, after boot completes (fwrelease.rc), and
// reads about 270 MB with O_DIRECT, so the page cache is untouched.

#define LOG_TAG "fwrelease"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <string>

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/unique_fd.h>

#include "FirmwareRelease.h"

using android::base::unique_fd;

namespace {

constexpr char kTablePath[] = "/vendor/etc/diamaneos/firmware-releases.txt";
constexpr char kPartitionDir[] = "/dev/block/by-name/";
constexpr char kProperty[] = "ro.vendor.diamaneos.firmware_release";

// O_DIRECT needs buffer, size and offset aligned to the logical block size
// (4096 on the FP6's UFS).
constexpr size_t kChunkBytes = 1024 * 1024;
alignas(4096) uint8_t gBuffer[kChunkBytes];

// Reads the table with open and read only.
std::optional<std::string> readTable() {
    unique_fd fd(TEMP_FAILURE_RETRY(open(kTablePath, O_RDONLY | O_CLOEXEC | O_NOFOLLOW)));
    if (fd < 0) return std::nullopt;
    std::string text;
    char chunk[4096];
    for (;;) {
        const ssize_t n = TEMP_FAILURE_RETRY(read(fd, chunk, sizeof(chunk)));
        if (n < 0) return std::nullopt;
        if (n == 0) return text;
        text.append(chunk, static_cast<size_t>(n));
        if (text.size() > fwrelease::kMaxTableBytes) return std::nullopt;
    }
}

std::string join(const std::set<std::string>& values) {
    std::string text;
    for (const auto& value : values) text += (text.empty() ? "" : ",") + value;
    return text;
}

// Sets the property (once per boot: it is read-only after that) and logs the
// result line.
int report(const std::string& value, const std::string& detail) {
    if (!android::base::SetProperty(kProperty, value)) {
        LOG(ERROR) << "Cannot set " << kProperty << " to " << value;
        return 1;
    }
    LOG(INFO) << "Firmware release: " << value << (detail.empty() ? "" : " (" + detail + ")");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    // init passes ${ro.boot.slot_suffix}, so the service reads no property.
    if (argc != 2 || (strcmp(argv[1], "_a") != 0 && strcmp(argv[1], "_b") != 0)) {
        return report(fwrelease::kUnknown, "no slot suffix");
    }
    const std::string slot = argv[1];
    const auto text = readTable();
    const auto table = text ? fwrelease::parseTable(*text) : std::nullopt;
    if (!table) return report(fwrelease::kUnknown, "no valid table");
    std::map<std::string, std::set<std::string>> matches;
    std::string releases;
    for (const auto& [name, images] : table->partitions) {
        const std::string partition = name + slot;
        std::set<uint64_t> lengths;
        for (const auto& image : images) lengths.insert(image.bytes);
        const std::string path = kPartitionDir + partition;
        unique_fd fd(TEMP_FAILURE_RETRY(open(path.c_str(), O_RDONLY | O_DIRECT | O_CLOEXEC)));
        std::optional<std::map<uint64_t, fwrelease::Digest>> digests;
        if (fd >= 0) digests = fwrelease::prefixDigests(fd, lengths, gBuffer, sizeof(gBuffer));
        if (!digests) {
            const int error = errno;
            return report(fwrelease::kUnknown, "cannot read " + partition + ": " + strerror(error));
        }
        auto found = fwrelease::matchingReleases(images, *digests);
        if (found.empty()) return report(fwrelease::kUnknown, partition + " matches no release");
        releases += (releases.empty() ? "" : " ") + partition + "=" + join(found);
        matches[name] = std::move(found);
    }
    const std::string value = fwrelease::result(*table, matches);
    // For a mixed slot, which partition holds which release.
    return report(value, value == fwrelease::kMixed ? releases : "");
}
