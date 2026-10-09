// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// Host test of libc2hwjail_avservices's failure paths (Android.bp):
//   m c2hwjail_host_test && out/host/linux-x86/nativetest64/c2hwjail_host_test/c2hwjail_host_test

#include <errno.h>
#include <fcntl.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <android-base/file.h>
#include <gtest/gtest.h>

#include "../c2hwjail.h"
#include "../c2hwjail_store.h"

namespace {

const std::set<std::string> kEncoders = {"c2.qti.avc.encoder", "c2.qti.hevc.encoder",
                                         "c2.qti.hevc.encoder.cq", "c2.qti.hevc.encoder.hdr",
                                         "c2.qti.heic.encoder"};
const std::set<std::string> kDecoders = {"c2.qti.avc.decoder", "c2.qti.hevc.decoder",
                                         "c2.qti.vp9.decoder"};

std::string List(const std::set<std::string>& names) {
    std::string out;
    for (const auto& name : names) out += (out.empty() ? "\"" : ", \"") + name + "\"";
    return out;
}

// A target specification shaped like the stock one (whole-line comments).
std::string Spec(const std::set<std::string>& decoders, const std::set<std::string>& encoders,
                 const std::string& optional = "") {
    return "//Copyright (c) 2024 Qualcomm Technologies, Inc.\n{\n    \"Video\": {\n"
           "        // feature list\n        \"Features\": { \"enc_csc_enable\": false },\n"
           "        \"QC2CodecPlugins\": [ \"libqcodec2_imgtxrfilter.so\" ],\n"
           "        \"codecs-available\": {\n            \"decoders\": [ " + List(decoders) +
           " ],\n            \"encoders\": [ " + List(encoders) + " ]\n        },\n"
           "        //\n        // Put below optional codecs under \"OptionalCodecs\"\n"
           "        \"OptionalCodecs\": [ " + optional + " ]\n    }\n}\n";
}

struct Fake {
    std::map<std::string, std::string> props;
    std::map<std::string, std::string> files;
    int nodeError = 0;
    uid_t uid = 0;
    gid_t gid = 0;
    mode_t mode = S_IFCHR | 0600;

    c2hwjail::Platform platform() {
        c2hwjail::Platform p;
        p.getProperty = [this](const char* name) {
            auto it = props.find(name);
            return it == props.end() ? std::string() : it->second;
        };
        p.readFile = [this](const std::string& path, std::string* content) {
            auto it = files.find(path);
            if (it == files.end()) return false;
            *content = it->second;
            return true;
        };
        p.statPath = [this](const char* path, struct stat* st) {
            if (std::string(path) != "/dev/video32") return ENOENT;
            if (nodeError != 0) return nodeError;
            st->st_uid = uid;
            st->st_gid = gid;
            st->st_mode = mode;
            return 0;
        };
        return p;
    }
};

const char kOffSpec[] = "/vendor/etc/media_volcano_v1/video_system_specs.json";
const char kOnSpec[] = "/vendor/etc/media_volcano_v1_hwdec/video_system_specs.json";

Fake GoodOff() {
    Fake f;
    f.props["vendor.media.target_variant"] = "_volcano_v1";
    f.props["ro.vendor.diamaneos.hw_video_decode"] = "0";
    f.files[kOffSpec] = Spec({}, kEncoders);
    return f;
}

Fake GoodOn() {
    Fake f;
    f.props["vendor.media.target_variant"] = "_volcano_v1_hwdec";
    f.props["ro.vendor.diamaneos.hw_video_decode"] = "1";
    f.files[kOnSpec] = Spec(kDecoders, kEncoders);
    f.gid = 1046;  // AID_MEDIA_CODEC
    f.mode = S_IFCHR | 0660;
    return f;
}

std::set<std::string> Allowed(Fake f, std::string* reason = nullptr) {
    std::string r;
    auto allowed = c2hwjail::AllowedCodecs(f.platform(), &r);
    if (reason) *reason = r;
    return allowed;
}

TEST(AllowedCodecs, ConsistentStatesOfferTheirCodecs) {
    EXPECT_EQ(kEncoders, Allowed(GoodOff()));
    Fake unset = GoodOff();
    unset.props.erase("ro.vendor.diamaneos.hw_video_decode");
    EXPECT_EQ(kEncoders, Allowed(unset));
    std::set<std::string> all = kEncoders;
    all.insert(kDecoders.begin(), kDecoders.end());
    EXPECT_EQ(all, Allowed(GoodOn()));
    // No video driver: the node cannot be opened at all.
    Fake missing = GoodOff();
    missing.nodeError = ENOENT;
    EXPECT_EQ(kEncoders, Allowed(missing));
}

TEST(AllowedCodecs, EveryInconsistencyOffersNothing) {
    std::vector<std::pair<std::string, Fake>> cases;
    Fake f;
    f = GoodOff(); f.props["ro.vendor.diamaneos.hw_video_decode"] = "2"; cases.emplace_back("mode 2", f);
    f = GoodOff(); f.props["ro.vendor.diamaneos.hw_video_decode"] = "true"; cases.emplace_back("mode true", f);
    f = GoodOff(); f.props.erase("vendor.media.target_variant"); cases.emplace_back("no variant", f);
    f = GoodOn(); f.props["vendor.media.target_variant"] = "_volcano_v1"; cases.emplace_back("on, off variant", f);
    f = GoodOff(); f.props["vendor.media.target_variant"] = "_volcano_v1_hwdec"; cases.emplace_back("off, on variant", f);
    f = GoodOff(); f.files.clear(); cases.emplace_back("unreadable spec", f);
    f = GoodOff(); f.files[kOffSpec] = "{ \"Video\": "; cases.emplace_back("truncated spec", f);
    f = GoodOff(); f.files[kOffSpec] = "[]"; cases.emplace_back("not an object", f);
    f = GoodOff(); f.files[kOffSpec] = "{ \"Video\": { } }"; cases.emplace_back("no codec list", f);
    f = GoodOff(); f.files[kOffSpec] = Spec({}, {}); cases.emplace_back("empty lists (library enables all)", f);
    f = GoodOff(); f.files[kOffSpec] = Spec({"c2.qti.avc.decoder"}, kEncoders); cases.emplace_back("off lists a decoder", f);
    f = GoodOff(); f.files[kOffSpec] = Spec({}, kEncoders, "\"c2.qti.dv.decoder\""); cases.emplace_back("optional codec", f);
    f = GoodOn(); f.files[kOnSpec] = Spec({"c2.qti.avc.decoder.secure"}, kEncoders); cases.emplace_back("secure decoder", f);
    f = GoodOff(); f.files[kOffSpec] = Spec({}, {"c2.qti.avc.encoder"}); cases.emplace_back("missing encoders", f);
    f = GoodOff(); f.files[kOffSpec] = std::string(2 << 20, ' '); cases.emplace_back("too large", f);
    f = GoodOff(); f.files[kOffSpec] = "{ \"Video\": { \"codecs-available\": { \"decoders\": 1 } } }"; cases.emplace_back("list not an array", f);
    f = GoodOff(); f.files[kOffSpec] = "{ \"Video\": { \"codecs-available\": { \"encoders\": [ 1 ] } } }"; cases.emplace_back("entry not a name", f);
    f = GoodOff(); f.mode = S_IFCHR | 0660; f.gid = 1046; cases.emplace_back("off, node open to group", f);
    f = GoodOff(); f.uid = 1000; cases.emplace_back("off, node owned by system", f);
    f = GoodOn(); f.mode = S_IFCHR | 0600; f.gid = 0; cases.emplace_back("on, node root-only", f);
    f = GoodOff(); f.mode = S_IFREG | 0600; cases.emplace_back("node not a device", f);
    f = GoodOff(); f.nodeError = EACCES; cases.emplace_back("node unreadable", f);
    for (auto& [name, fake] : cases) {
        std::string reason;
        EXPECT_TRUE(Allowed(fake, &reason).empty()) << name;
        EXPECT_FALSE(reason.empty()) << name;
    }
}

// A store with an allowed encoder, a decoder and a secure encoder.
class FakeStore : public C2ComponentStore {
  public:
    std::vector<std::string> created;
    C2String getName() const override { return "fake"; }
    c2_status_t createComponent(C2String name, std::shared_ptr<C2Component>* const) override {
        created.push_back(name);
        return C2_OK;
    }
    c2_status_t createInterface(C2String name, std::shared_ptr<C2ComponentInterface>* const) override {
        created.push_back(name);
        return C2_OK;
    }
    std::vector<std::shared_ptr<const C2Component::Traits>> listComponents() override {
        std::vector<std::shared_ptr<const C2Component::Traits>> list;
        for (const char* name : {"c2.qti.avc.encoder", "c2.qti.avc.decoder", "c2.qti.avc.encoder.secure"}) {
            auto traits = std::make_shared<C2Component::Traits>();
            traits->name = name;
            if (std::string(name) == "c2.qti.avc.encoder") traits->aliases = {"OMX.qcom.video.encoder.avc"};
            list.push_back(traits);
        }
        return list;
    }
    c2_status_t copyBuffer(std::shared_ptr<C2GraphicBuffer>, std::shared_ptr<C2GraphicBuffer>) override {
        return C2_OK;
    }
    c2_status_t query_sm(const std::vector<C2Param*>&, const std::vector<C2Param::Index>&,
                         std::vector<std::unique_ptr<C2Param>>* const) const override {
        return C2_OK;
    }
    c2_status_t config_sm(const std::vector<C2Param*>&,
                          std::vector<std::unique_ptr<C2SettingResult>>* const) override {
        return C2_OK;
    }
    std::shared_ptr<C2ParamReflector> getParamReflector() const override { return nullptr; }
    c2_status_t querySupportedParams_nb(std::vector<std::shared_ptr<C2ParamDescriptor>>* const) const override {
        return C2_OK;
    }
    c2_status_t querySupportedValues_sm(std::vector<C2FieldSupportedValuesQuery>&) const override {
        return C2_OK;
    }
};

TEST(FilterStore, OffersOnlyAllowedComponentsAndTheirAliases) {
    auto fake = std::make_shared<FakeStore>();
    auto store = c2hwjail::FilterStore(fake, {"c2.qti.avc.encoder", "c2.qti.hevc.encoder"});
    auto listed = store->listComponents();
    ASSERT_EQ(1u, listed.size());
    EXPECT_EQ("c2.qti.avc.encoder", listed[0]->name);
    std::shared_ptr<C2Component> component;
    std::shared_ptr<C2ComponentInterface> interface;
    EXPECT_EQ(C2_OK, store->createComponent("c2.qti.avc.encoder", &component));
    EXPECT_EQ(C2_OK, store->createInterface("OMX.qcom.video.encoder.avc", &interface));
    EXPECT_EQ(C2_NOT_FOUND, store->createComponent("c2.qti.avc.decoder", &component));
    EXPECT_EQ(C2_NOT_FOUND, store->createInterface("c2.qti.avc.encoder.secure", &interface));
    // Allowed but not registered by the Qualcomm store: not offered either.
    EXPECT_EQ(C2_NOT_FOUND, store->createComponent("c2.qti.hevc.encoder", &component));
    EXPECT_EQ((std::vector<std::string>{"c2.qti.avc.encoder", "OMX.qcom.video.encoder.avc"}), fake->created);
    EXPECT_EQ("fake", store->getName());
}

TEST(FilterStore, NothingAllowedIsAnEmptyStore) {
    auto fake = std::make_shared<FakeStore>();
    for (auto store : {c2hwjail::FilterStore(fake, {}), c2hwjail::FilterStore(nullptr, kEncoders)}) {
        std::shared_ptr<C2Component> component;
        EXPECT_TRUE(store->listComponents().empty());
        EXPECT_EQ(C2_NOT_FOUND, store->createComponent("c2.qti.avc.encoder", &component));
    }
    EXPECT_TRUE(fake->created.empty());
}

TEST(TrapsToErrno, RewritesOnlyTrapResults) {
    struct sock_filter insns[] = {
            BPF_STMT(BPF_LD | BPF_W | BPF_ABS, 0),
            BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
            BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRAP),
            BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL),
            BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRAP),
    };
    struct sock_fprog prog = {sizeof(insns) / sizeof(insns[0]), insns};
    EXPECT_EQ(2u, c2hwjail::TrapsToErrno(&prog));
    EXPECT_EQ(SECCOMP_RET_ALLOW, insns[1].k);
    EXPECT_EQ(SECCOMP_RET_ERRNO | EPERM, insns[2].k);
    EXPECT_EQ(SECCOMP_RET_KILL, insns[3].k);
    EXPECT_EQ(SECCOMP_RET_ERRNO | EPERM, insns[4].k);
}

// In a child process: install the policy, then try calls with raw system
// calls and report each result (0 allowed, else the errno) through a pipe.
TEST(InstallFilter, DisallowedCallsFailWithEperm) {
    const std::string policy = android::base::GetExecutableDirectory() + "/codec2-service.arm64.policy";
    int fds[2];
    ASSERT_EQ(0, pipe(fds));
    pid_t pid = fork();
    ASSERT_NE(-1, pid);
    if (pid == 0) {
        close(fds[0]);
        std::string error;
        int results[8];
        results[0] = c2hwjail::InstallFilter(policy.c_str(), false, &error) ? 0 : 1;
        auto call = [](long r) { return r >= 0 ? 0 : errno; };
        results[1] = call(syscall(SYS_getpid));
        long unix_socket = syscall(SYS_socket, AF_UNIX, SOCK_DGRAM, 0);
        results[2] = call(unix_socket);
        results[3] = call(syscall(SYS_socket, AF_INET, SOCK_STREAM, 0));
        long forked = syscall(SYS_clone, SIGCHLD, 0, 0, 0, 0);  // fork
        if (forked == 0) syscall(SYS_exit_group, 0);
        results[4] = call(forked);
        results[5] = call(syscall(SYS_prctl, PR_SET_DUMPABLE, 1, 0, 0, 0));
        results[6] = call(syscall(SYS_mmap, 0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        const char* argv[] = {"/bin/true", nullptr};
        results[7] = call(syscall(SYS_execve, "/bin/true", argv, nullptr));
        syscall(SYS_write, fds[1], results, sizeof(results));
        syscall(SYS_exit_group, 0);
    }
    close(fds[1]);
    int results[8] = {};
    ASSERT_EQ(static_cast<ssize_t>(sizeof(results)), read(fds[0], results, sizeof(results)));
    int status = 0;
    ASSERT_EQ(pid, waitpid(pid, &status, 0));
    EXPECT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0) << "child died: " << status;
    EXPECT_EQ(0, results[0]) << "filter not installed";
    EXPECT_EQ(0, results[1]) << "getpid";
    EXPECT_EQ(0, results[2]) << "AF_UNIX socket";
    EXPECT_EQ(EPERM, results[3]) << "AF_INET socket";
    EXPECT_EQ(EPERM, results[4]) << "fork";
    EXPECT_EQ(EPERM, results[5]) << "PR_SET_DUMPABLE";
    EXPECT_EQ(EPERM, results[6]) << "writable and executable mapping";
    EXPECT_EQ(EPERM, results[7]) << "execve";
}

TEST(InstallFilter, MissingPolicyIsAResultNotAnAbort) {
    std::string error;
    EXPECT_FALSE(c2hwjail::InstallFilter("/nonexistent/codec2-service.policy", false, &error));
    EXPECT_FALSE(error.empty());
}

}  // namespace
