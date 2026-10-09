// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// Sensors multi-HAL sub-HAL with two sensors: a single tap and a double tap on
// the touch controller (ESWIN EPH861x) while the screen is off or shows the
// always-on display. SystemUI's doze service listens to them
// (config_dozeTapSensorType for Tap to check phone,
// config_dozeDoubleTapSensorType for Display > Tap to wake) and wakes the
// screen after a proximity check.
//
// While a sensor is armed, its gesture is on in the controller (gesture_wakeup
// 1 for the tap, 2 for the double tap). The touch driver reports gestures
// through wake_gesture: reading it returns the gestures seen since the last
// read (1 single tap, 2 double tap, as bits), and the driver wakes poll()
// readers. Both sensors are one-shot: after its gesture a sensor disarms itself
// and turns the gesture off in the controller until SystemUI asks again.
//
// One thread does all node I/O; activate() only records what is wanted.

#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <utils/SystemClock.h>

#include "V2_1/SubHal.h"

namespace {

using ::android::sp;
using ::android::base::unique_fd;
using ::android::hardware::hidl_handle;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::sensors::V1_0::OperationMode;
using ::android::hardware::sensors::V1_0::RateLevel;
using ::android::hardware::sensors::V1_0::Result;
using ::android::hardware::sensors::V1_0::SensorFlagBits;
using ::android::hardware::sensors::V1_0::SharedMemInfo;
using ::android::hardware::sensors::V2_1::Event;
using ::android::hardware::sensors::V2_1::SensorInfo;
using ::android::hardware::sensors::V2_1::SensorType;
using ::android::hardware::sensors::V2_1::implementation::IHalProxyCallback;
using ::android::hardware::sensors::V2_1::implementation::ISensorsSubHal;

constexpr char kGesturePath[] = "/sys/bus/spi/devices/spi0.0/gesture_wakeup";
constexpr char kReportPath[] = "/sys/bus/spi/devices/spi0.0/wake_gesture";

// Only SystemUI's doze service needs the sensors. Apps without this permission
// neither see them nor can arm the controller's gestures through them.
constexpr char kPermission[] = "android.permission.DEVICE_POWER";
// Values of an event: no touch position (SystemUI ignores negative ones).
constexpr size_t kEventValues = 16;

struct TapSensor {
    int32_t handle;
    const char* name;
    const char* typeString;
    int32_t typeOffset;  // from SensorType::DEVICE_PRIVATE_BASE
    const char* on;      // gesture_wakeup values
    const char* off;
    int reportBit;       // wake_gesture bit
};

constexpr std::array<TapSensor, 2> kSensors = {{
        {1, "Single tap", "de.diamaneos.sensor.single_tap", 1, "1", "-1", 1},
        {2, "Double tap", "de.diamaneos.sensor.double_tap", 2, "2", "-2", 2},
}};

SensorType typeOf(const TapSensor& sensor) {
    return static_cast<SensorType>(static_cast<int32_t>(SensorType::DEVICE_PRIVATE_BASE) +
                                   sensor.typeOffset);
}

// Index into kSensors, or -1.
int indexOf(int32_t handle) {
    for (size_t i = 0; i < kSensors.size(); i++) {
        if (kSensors[i].handle == handle) return i;
    }
    return -1;
}

bool writeNode(const char* path, const char* value) {
    unique_fd fd(TEMP_FAILURE_RETRY(open(path, O_WRONLY | O_CLOEXEC)));
    if (fd < 0) {
        PLOG(ERROR) << "Cannot open " << path;
        return false;
    }
    const ssize_t len = strlen(value);
    if (TEMP_FAILURE_RETRY(write(fd, value, len)) != len) {
        PLOG(ERROR) << "Cannot write " << value << " to " << path;
        return false;
    }
    return true;
}

// Reads wake_gesture from the start. Reading also consumes the reported
// gestures and re-arms poll().
bool readReport(int fd, int* gestures) {
    char buf[8] = {};
    if (lseek(fd, 0, SEEK_SET) < 0) return false;
    const ssize_t n = TEMP_FAILURE_RETRY(read(fd, buf, sizeof(buf) - 1));
    if (n <= 0) return false;
    *gestures = atoi(buf);
    return true;
}

class TapSubHal : public ISensorsSubHal {
  public:
    using ISensorsSubHal::initialize;

    TapSubHal() {
        for (const TapSensor& sensor : kSensors) {
            SensorInfo info;
            info.sensorHandle = sensor.handle;
            info.name = sensor.name;
            info.vendor = "DiamaneOS";
            info.version = 1;
            info.type = typeOf(sensor);
            info.typeAsString = sensor.typeString;
            info.maxRange = 1.0f;
            info.resolution = 1.0f;
            info.power = 0.0f;
            // One-shot sensors have no rate and no FIFO.
            info.minDelay = -1;
            info.maxDelay = 0;
            info.fifoReservedEventCount = 0;
            info.fifoMaxEventCount = 0;
            info.requiredPermission = kPermission;
            info.flags = static_cast<uint32_t>(SensorFlagBits::ONE_SHOT_MODE) |
                         static_cast<uint32_t>(SensorFlagBits::WAKE_UP);
            mSensorInfos.push_back(info);
        }
    }

    ~TapSubHal() {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mStop = true;
        }
        wakeThread();
        if (mThread.joinable()) mThread.join();
    }

    // ISensorsSubHal

    const std::string getName() override { return "FP6-Taps"; }

    Return<Result> initialize(const sp<IHalProxyCallback>& halProxyCallback) override {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            // Called again when the sensor service restarts: start disarmed.
            mCallback = halProxyCallback;
            mWantArmed.fill(false);
            mMode = OperationMode::NORMAL;
            if (!mThread.joinable()) {
                mWakeFd.reset(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK));
                if (mWakeFd < 0) {
                    PLOG(ERROR) << "Cannot create the wake eventfd";
                    return Result::NO_MEMORY;
                }
                mThread = std::thread(&TapSubHal::run, this);
            }
        }
        wakeThread();
        return Result::OK;
    }

    Return<void> debug(const hidl_handle& fd, const hidl_vec<hidl_string>& /* args */) override {
        if (fd.getNativeHandle() == nullptr || fd->numFds < 1) return Void();
        std::lock_guard<std::mutex> lock(mMutex);
        for (size_t i = 0; i < kSensors.size(); i++) {
            dprintf(fd->data[0], "%s: armed %d, reported %u\n", kSensors[i].typeString,
                    mWantArmed[i], mReported[i]);
        }
        return Void();
    }

    // ISensors 2.1

    Return<void> getSensorsList_2_1(getSensorsList_2_1_cb _hidl_cb) override {
        _hidl_cb(hidl_vec<SensorInfo>(mSensorInfos));
        return Void();
    }

    Return<Result> injectSensorData_2_1(const Event& /* event */) override {
        return Result::INVALID_OPERATION;
    }

    // ISensors 2.0

    Return<Result> setOperationMode(OperationMode mode) override {
        std::lock_guard<std::mutex> lock(mMutex);
        mMode = mode;
        return Result::OK;
    }

    Return<Result> activate(int32_t sensorHandle, bool enabled) override {
        const int index = indexOf(sensorHandle);
        if (index < 0) return Result::BAD_VALUE;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mWantArmed[index] = enabled;
        }
        wakeThread();
        return Result::OK;
    }

    Return<Result> batch(int32_t sensorHandle, int64_t /* samplingPeriodNs */,
                         int64_t /* maxReportLatencyNs */) override {
        return indexOf(sensorHandle) >= 0 ? Result::OK : Result::BAD_VALUE;
    }

    // One-shot sensors have nothing to flush.
    Return<Result> flush(int32_t /* sensorHandle */) override { return Result::BAD_VALUE; }

    Return<void> registerDirectChannel(
            const SharedMemInfo& /* mem */,
            ::android::hardware::sensors::V2_0::ISensors::registerDirectChannel_cb _hidl_cb)
            override {
        _hidl_cb(Result::INVALID_OPERATION, -1 /* channelHandle */);
        return Void();
    }

    Return<Result> unregisterDirectChannel(int32_t /* channelHandle */) override {
        return Result::INVALID_OPERATION;
    }

    Return<void> configDirectReport(
            int32_t /* sensorHandle */, int32_t /* channelHandle */, RateLevel /* rate */,
            ::android::hardware::sensors::V2_0::ISensors::configDirectReport_cb _hidl_cb)
            override {
        _hidl_cb(Result::INVALID_OPERATION, 0 /* reportToken */);
        return Void();
    }

  private:
    using Flags = std::array<bool, kSensors.size()>;

    void wakeThread() {
        mCondition.notify_all();
        if (mWakeFd >= 0) {
            const uint64_t one = 1;
            (void)TEMP_FAILURE_RETRY(write(mWakeFd, &one, sizeof(one)));
        }
    }

    // Arms or disarms one gesture in the controller. Arming the first one opens
    // the report and drops gestures reported before.
    bool apply(size_t index, bool arm, bool anyArmed) {
        if (!arm) return writeNode(kGesturePath, kSensors[index].off);
        if (mReportFd < 0) {
            mReportFd.reset(TEMP_FAILURE_RETRY(open(kReportPath, O_RDONLY | O_CLOEXEC)));
            if (mReportFd < 0) {
                PLOG(ERROR) << "Cannot open " << kReportPath;
                return false;
            }
        }
        int gestures = 0;
        if (!anyArmed && !readReport(mReportFd, &gestures)) {
            PLOG(ERROR) << "Cannot read " << kReportPath;
            return false;
        }
        return writeNode(kGesturePath, kSensors[index].on);
    }

    void report(int gestures) {
        sp<IHalProxyCallback> callback;
        std::vector<Event> events;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mMode != OperationMode::NORMAL) return;
            for (size_t i = 0; i < kSensors.size(); i++) {
                if (!(gestures & kSensors[i].reportBit) || !mWantArmed[i]) continue;
                // One-shot: disarm before the event goes out.
                mWantArmed[i] = false;
                mReported[i]++;

                Event event;
                event.timestamp = ::android::elapsedRealtimeNano();
                event.sensorHandle = kSensors[i].handle;
                event.sensorType = typeOf(kSensors[i]);
                float* values = event.u.data.data();
                std::fill(values, values + kEventValues, 0.0f);
                values[0] = -1.0f;  // no x
                values[1] = -1.0f;  // no y
                events.push_back(event);
            }
            callback = mCallback;
        }
        if (events.empty() || callback == nullptr) return;
        callback->postEvents(events, callback->createScopedWakelock(true));
    }

    void run() {
        Flags armed{};
        for (;;) {
            Flags want;
            {
                std::unique_lock<std::mutex> lock(mMutex);
                mCondition.wait(lock, [&] {
                    return mStop || mWantArmed != armed ||
                           std::any_of(armed.begin(), armed.end(), [](bool a) { return a; });
                });
                if (mStop) break;
                want = mWantArmed;
            }

            if (want != armed) {
                for (size_t i = 0; i < kSensors.size(); i++) {
                    if (want[i] == armed[i]) continue;
                    const bool anyArmed =
                            std::any_of(armed.begin(), armed.end(), [](bool a) { return a; });
                    if (apply(i, want[i], anyArmed)) {
                        armed[i] = want[i];
                    } else if (want[i]) {
                        // No touch node: give up until SystemUI asks again.
                        std::lock_guard<std::mutex> lock(mMutex);
                        mWantArmed[i] = false;
                    }
                }
                continue;
            }

            pollfd fds[] = {
                    {mWakeFd.get(), POLLIN, 0},
                    {mReportFd.get(), POLLPRI | POLLERR, 0},
            };
            if (TEMP_FAILURE_RETRY(poll(fds, 2, -1)) < 0) {
                PLOG(ERROR) << "poll";
                continue;
            }
            if (fds[0].revents & POLLIN) {
                uint64_t count;
                (void)TEMP_FAILURE_RETRY(read(mWakeFd, &count, sizeof(count)));
            }
            if (fds[1].revents & (POLLPRI | POLLERR)) {
                int gestures = 0;
                if (readReport(mReportFd, &gestures) && gestures != 0) report(gestures);
            }
        }
        for (size_t i = 0; i < kSensors.size(); i++) {
            if (armed[i]) writeNode(kGesturePath, kSensors[i].off);
        }
    }

    std::vector<SensorInfo> mSensorInfos;

    std::mutex mMutex;
    std::condition_variable mCondition;
    sp<IHalProxyCallback> mCallback;              // guarded by mMutex
    Flags mWantArmed{};                           // guarded by mMutex
    std::array<unsigned, kSensors.size()> mReported{};  // guarded by mMutex
    bool mStop = false;                           // guarded by mMutex
    OperationMode mMode = OperationMode::NORMAL;  // guarded by mMutex

    std::thread mThread;
    unique_fd mWakeFd;
    unique_fd mReportFd;  // used by the thread only
};

}  // namespace

::android::hardware::sensors::V2_1::implementation::ISensorsSubHal* sensorsHalGetSubHal_2_1(
        uint32_t* version) {
    static TapSubHal subHal;
    *version = SUB_HAL_2_1_VERSION;
    return &subHal;
}
