// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// Forwards the stock camera's (CamX) open, close and snapshot perf hints to
// the power HAL as time-limited IPower boosts.
//
// CamX sends Qualcomm perf hint IDs through perf_hint_renew and ends most of
// them early with perf_lock_rel. Their meaning is in Qualcomm's power
// configuration for this SoC (CodeLinaro vendor/qcom-opensource/power,
// config/volcano/powerhint.xml):
//   0x1337 camera open tunings: at camera open (duration 0) and at every
//          configure_streams (5 s)
//   0x1338 camera close tunings: before a flush or close (duration 0)
//   0x1339 camera snapshot tunings: at each capture (10 s)
//   0x1330 camera ZSL preview and 0x1331 camera 30 fps: while streaming;
//          Qualcomm's tunings for them cap the little cores to save power.
//          Not forwarded.
// Open and close both become Boost::CAMERA_LAUNCH and the snapshot
// Boost::CAMERA_SHOT, as in Qualcomm's open camera HAL (QCameraPerf.cpp in
// hardware/qcom/camera); powerhint.json defines both boosts.
//
// Bounds: a request lasts its duration, at most kMaxTimedMs, or, with
// duration 0, until perf_lock_rel and at most kMaxHoldMs. Each setBoost
// carries the remaining time (never 0, which libperfmgr would treat as its
// default), so the power HAL ends every boost on its own timer even if this
// process dies. perf_lock_rel and renewals end what the handle started; when
// the last request for a boost ends, the boost is cancelled (setBoost -1).
//
// CamX threads only update a small request table under a mutex and wake a
// worker thread; the worker alone looks up the power HAL (checkService, no
// waiting), caches the binder and makes the one-way setBoost calls. A slow or
// missing power HAL never stalls the camera, and without it the hints stay
// no-ops.

#include "camera_hints.h"

#include "trace.h"

#include <aidl/android/hardware/power/Boost.h>
#include <aidl/android/hardware/power/IPower.h>
#include <android/binder_manager.h>
#include <pthread.h>
#include <time.h>

#include <climits>
#include <cstdint>
#include <memory>

namespace {

using ::aidl::android::hardware::power::Boost;
using ::aidl::android::hardware::power::IPower;

constexpr char kPowerInstance[] = "android.hardware.power.IPower/default";

constexpr int kHintCameraOpen = 0x1337;
constexpr int kHintCameraClose = 0x1338;
constexpr int kHintCameraSnapshot = 0x1339;

// CamX asks 5 s at configure_streams and 10 s per capture, and releases both
// once the work is done.
constexpr int64_t kMaxTimedMs = 5000;
// Holds bracket open, flush and close, which take milliseconds.
constexpr int64_t kMaxHoldMs = 2000;

// Above every handle the stub's other calls return (1, or the caller's own),
// so perf_lock_rel of those never ends a boost.
constexpr int kFirstHandle = 0x10000;
constexpr int kMaxRequests = 32;

enum BoostIndex { kLaunch, kShot, kBoostCount };
constexpr Boost kBoosts[kBoostCount] = {Boost::CAMERA_LAUNCH, Boost::CAMERA_SHOT};
constexpr const char* kBoostNames[kBoostCount] = {"CAMERA_LAUNCH", "CAMERA_SHOT"};

struct Request {
    int handle;  // 0: free
    int boost;
    int64_t deadline_ns;
};

// A setBoost call for the worker; duration_ms -1 cancels the boost.
struct Action {
    int boost;
    int32_t duration_ms;
};

// Plain pthread objects and arrays: nothing here has a destructor that could
// run at exit while the worker still uses it.
pthread_mutex_t gLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t gWake = PTHREAD_COND_INITIALIZER;
Request gRequests[kMaxRequests];     // guarded by gLock
int64_t gSentDeadline[kBoostCount];  // guarded by gLock; 0: no boost running
bool gDirty;                         // guarded by gLock
int gNextHandle = kFirstHandle;      // guarded by gLock
pthread_once_t gWorkerOnce = PTHREAD_ONCE_INIT;
bool gWorkerRunning;  // set once, inside pthread_once

int64_t now_ns() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * INT64_C(1000000000) + ts.tv_nsec;
}

int boost_for_hint(int hint) {
    switch (hint) {
        case kHintCameraOpen:
        case kHintCameraClose:
            return kLaunch;
        case kHintCameraSnapshot:
            return kShot;
        default:
            return -1;
    }
}

int next_handle_locked() {
    for (;;) {
        int handle = gNextHandle;
        gNextHandle = gNextHandle == INT_MAX ? kFirstHandle : gNextHandle + 1;
        bool used = false;
        for (const Request& r : gRequests) used = used || r.handle == handle;
        if (!used) return handle;
    }
}

void wake_worker_locked() {
    gDirty = true;
    pthread_cond_signal(&gWake);
}

int32_t ms_until(int64_t deadline_ns, int64_t now) {
    int64_t ms = (deadline_ns - now + 999999) / 1000000;
    return ms < 1 ? 1 : static_cast<int32_t>(ms);
}

// Compares the live requests with what the power HAL was told and returns the
// calls that bring it in line. libperfmgr only ever extends a running boost,
// so a shorter remaining time needs a cancel first.
int plan_locked(int64_t now, Action* out) {
    int64_t want[kBoostCount] = {};
    for (Request& r : gRequests) {
        if (r.handle == 0) continue;
        if (r.deadline_ns <= now) {
            r.handle = 0;
            continue;
        }
        if (r.deadline_ns > want[r.boost]) want[r.boost] = r.deadline_ns;
    }
    int count = 0;
    for (int b = 0; b < kBoostCount; ++b) {
        bool running = gSentDeadline[b] > now;
        if (want[b] == 0) {
            if (running) out[count++] = {b, -1};
            gSentDeadline[b] = 0;
            continue;
        }
        if (running && want[b] == gSentDeadline[b]) continue;
        if (running && want[b] < gSentDeadline[b]) out[count++] = {b, -1};
        out[count++] = {b, ms_until(want[b], now)};
        gSentDeadline[b] = want[b];
    }
    return count;
}

// Worker only. Returns false when the power HAL is missing or a call fails.
bool send(std::shared_ptr<IPower>& power, bool& warned, const Action* actions, int count) {
    if (!power) {
        ndk::SpAIBinder binder(AServiceManager_checkService(kPowerInstance));
        if (binder.get() != nullptr) power = IPower::fromBinder(binder);
        if (!power) {
            if (!warned) {
                __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                                    "%s not available; camera hints stay local", kPowerInstance);
                warned = true;
            }
            return false;
        }
    }
    for (int i = 0; i < count; ++i) {
        const Action& a = actions[i];
        ndk::ScopedAStatus status = power->setBoost(kBoosts[a.boost], a.duration_ms);
        if (!status.isOk()) {
            if (!warned) {
                __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "setBoost %s failed: %s",
                                    kBoostNames[a.boost], status.getDescription().c_str());
                warned = true;
            }
            power.reset();
            return false;
        }
        if (a.duration_ms < 0) {
            TRACE("power HAL: end %s", kBoostNames[a.boost]);
        } else {
            TRACE("power HAL: %s for %d ms", kBoostNames[a.boost], a.duration_ms);
        }
    }
    warned = false;
    return true;
}

void* worker_main(void*) {
    pthread_setname_np(pthread_self(), "camera-power");
    // Locals of a loop that never returns: never destroyed.
    std::shared_ptr<IPower> power;
    bool warned = false;
    int failures = 0;
    Action actions[2 * kBoostCount];

    pthread_mutex_lock(&gLock);
    for (;;) {
        while (!gDirty) pthread_cond_wait(&gWake, &gLock);
        gDirty = false;
        int count = plan_locked(now_ns(), actions);
        pthread_mutex_unlock(&gLock);
        bool ok = count == 0 || send(power, warned, actions, count);
        pthread_mutex_lock(&gLock);
        if (ok) {
            failures = 0;
            continue;
        }
        // The power HAL's state is unknown: anything it still runs ends on its
        // own timer. Retry once with a fresh lookup (it may have restarted),
        // then wait for the next hint.
        for (int64_t& deadline : gSentDeadline) deadline = 0;
        if (++failures == 1) gDirty = true;
    }
    return nullptr;
}

void start_worker() {
    pthread_attr_t attr;
    pthread_t thread;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int rc = pthread_create(&thread, &attr, worker_main, nullptr);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                            "no forwarding thread (error %d); camera hints stay local", rc);
        return;
    }
    gWorkerRunning = true;
}

}  // namespace

extern "C" int camera_hint_release(int handle) {
    if (handle < kFirstHandle) return 0;
    int ended = 0;
    pthread_mutex_lock(&gLock);
    for (Request& r : gRequests) {
        if (r.handle == handle) {
            r.handle = 0;
            ended = 1;
            wake_worker_locked();
            break;
        }
    }
    pthread_mutex_unlock(&gLock);
    return ended;
}

extern "C" int camera_hint_acquire(int handle, int hint, int duration_ms) {
    int boost = boost_for_hint(hint);
    if (boost < 0) {
        camera_hint_release(handle);
        return 0;
    }
    pthread_once(&gWorkerOnce, start_worker);
    if (!gWorkerRunning) return 0;

    int64_t now = now_ns();
    int64_t ms = duration_ms > 0 ? (duration_ms < kMaxTimedMs ? duration_ms : kMaxTimedMs)
                                 : kMaxHoldMs;
    int result = 0;
    pthread_mutex_lock(&gLock);
    Request* slot = nullptr;
    Request* free_slot = nullptr;
    for (Request& r : gRequests) {
        if (r.handle != 0 && r.deadline_ns <= now) r.handle = 0;
        if (r.handle != 0 && r.handle == handle) {
            slot = &r;
        } else if (r.handle == 0 && free_slot == nullptr) {
            free_slot = &r;
        }
    }
    if (slot == nullptr && free_slot != nullptr) {
        slot = free_slot;
        slot->handle = next_handle_locked();
    }
    if (slot != nullptr) {
        slot->boost = boost;
        slot->deadline_ns = now + ms * 1000000;
        result = slot->handle;
        wake_worker_locked();
    }
    pthread_mutex_unlock(&gLock);
    return result;
}
