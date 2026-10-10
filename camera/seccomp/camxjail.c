// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// Seccomp loader for the stock CamX camera provider.
//
// The tools renderer renames the provider's libhardware.so dependency to this
// library (same name length, pinned input and output hashes), and this library
// links libhardware in turn. The dynamic linker runs the constructor below
// after the provider's own dependencies are loaded and before its main(), so
// the filter covers the provider's binder setup, the CamX module load
// (camera.qcom.so and everything it opens) and every camera session. Nothing
// is executed and the SELinux domain does not change (hal_camera_default).
//
// camera-provider.policy (minijail syntax) lists the allowed system calls.
// Unlisted calls, and listed calls whose arguments do not match their rule, are
// - logged and allowed (SECCOMP_RET_LOG, kernel audit record type 1326) when
//   built with CAMXJAIL_LOG_ONLY, to collect the provider's profile;
// - otherwise trapped: SIGSYS, a tombstone naming the call, and the provider
//   dies (init restarts it).
// If the filter cannot be installed the provider aborts: it never runs
// unconfined.

#define LOG_TAG "camxjail"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>

#include <log/log.h>

// Internal libminijail headers, linked statically from the same source tree.
// minijail's public API cannot make RET_LOG the default action on Android
// (it never detects RET_LOG there), so the policy is compiled directly.
#include "syscall_filter.h"
#include "syscall_wrapper.h"

#define POLICY "/vendor/etc/seccomp_policy/camera-provider.policy"

#ifdef CAMXJAIL_LOG_ONLY
#define CAMXJAIL_MODE "log-only"
#define CAMXJAIL_ACTION ACTION_RET_LOG
#else
#define CAMXJAIL_MODE "trap"
#define CAMXJAIL_ACTION ACTION_RET_TRAP
#endif

__attribute__((constructor)) static void camxjail_install(void) {
    // A dump reader that closes its pipe early must not kill the provider:
    // its writes then fail with EPIPE instead of raising SIGPIPE.
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        ALOGW("cannot ignore SIGPIPE: %s", strerror(errno));
    }

    const struct filter_options options = {
            .action = CAMXJAIL_ACTION,
            // RET_LOG needs allow_logging, which also makes the compiler skip
            // unknown system call names instead of failing (log-only builds).
            .allow_logging = CAMXJAIL_ACTION == ACTION_RET_LOG,
            .allow_syscalls_for_logging = 0,
            .allow_duplicate_syscalls = false,
            .include_libc_compatibility_allowlist = false,
    };
    struct sock_fprog prog = {0};

    FILE* policy = fopen(POLICY, "re");
    if (policy == NULL) {
        LOG_ALWAYS_FATAL("cannot open %s: %s", POLICY, strerror(errno));
    }
    int compiled = compile_filter(POLICY, policy, &prog, &options);
    fclose(policy);
    if (compiled != 0) {
        LOG_ALWAYS_FATAL("cannot compile %s", POLICY);
    }

    // The provider has no CAP_SYS_ADMIN, so the kernel requires no_new_privs.
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        LOG_ALWAYS_FATAL("cannot set no_new_privs: %s", strerror(errno));
    }
    // TSYNC applies the filter and no_new_privs to every thread of the process;
    // a positive result names a thread that could not be synchronised.
    int installed = sys_seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC, &prog);
    if (installed != 0) {
        LOG_ALWAYS_FATAL("cannot install the seccomp filter: %s",
                         installed > 0 ? "a thread could not be synchronised" : strerror(errno));
    }
    ALOGI("seccomp filter installed (%s, %u instructions)", CAMXJAIL_MODE, (unsigned)prog.len);
    free(prog.filter);
}
