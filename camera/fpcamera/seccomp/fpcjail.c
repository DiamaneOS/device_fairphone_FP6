// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The DiamaneOS Project

// Seccomp loader for the stock TCL camera algorithm service of the Fairphone
// Camera app (/vendor/bin/hw/cameraalgoservice).
//
// uicam (compat/tcl-camera-v34), which the tools renderer puts in place of the
// service's libui.so dependency, links this library. The dynamic linker runs
// the constructor below before the constructors of the libraries that link
// uicam (libtctcameraalgo.so) and before the service's main(), so the filter
// covers the service's binder setup, every algorithm plugin it opens and every
// capture. Only the service loads uicam: it binds all of the TCL stack's
// GraphicBuffer imports, so the service cannot run without this filter. Nothing
// is executed and the SELinux domain does not change (fpcamera_algoservice).
//
// fpcamera-algoservice.policy (minijail syntax) lists the allowed system calls.
// Unlisted calls, and listed calls whose arguments do not match their rule, are
// - logged and allowed (SECCOMP_RET_LOG, kernel audit record type 1326) when
//   built with FPCJAIL_LOG_ONLY, to collect the service's profile.
// - otherwise trapped: SIGSYS, a tombstone naming the call, and the service
//   dies (the app reconnects and init starts it again).
// If the filter cannot be installed the service aborts: it never runs
// unconfined.

#define LOG_TAG "fpcjail"

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

#define POLICY "/vendor/etc/seccomp_policy/fpcamera-algoservice.policy"

#ifdef FPCJAIL_LOG_ONLY
#define FPCJAIL_MODE "log-only"
#define FPCJAIL_ACTION ACTION_RET_LOG
#else
#define FPCJAIL_MODE "trap"
#define FPCJAIL_ACTION ACTION_RET_TRAP
#endif

__attribute__((constructor)) static void fpcjail_install(void) {
    // A dump reader that closes its pipe early must not kill the service:
    // its writes then fail with EPIPE instead of raising SIGPIPE.
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        ALOGW("cannot ignore SIGPIPE: %s", strerror(errno));
    }

    const struct filter_options options = {
            .action = FPCJAIL_ACTION,
            // RET_LOG needs allow_logging, which also makes the compiler skip
            // unknown system call names instead of failing (log-only builds).
            .allow_logging = FPCJAIL_ACTION == ACTION_RET_LOG,
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

    // The service has no CAP_SYS_ADMIN, so the kernel requires no_new_privs.
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        LOG_ALWAYS_FATAL("cannot set no_new_privs: %s", strerror(errno));
    }
    // TSYNC applies the filter and no_new_privs to every thread of the process.
    // A positive result names a thread that could not be synchronised.
    int installed = sys_seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC, &prog);
    if (installed != 0) {
        LOG_ALWAYS_FATAL("cannot install the seccomp filter: %s",
                         installed > 0 ? "a thread could not be synchronised" : strerror(errno));
    }
    ALOGI("seccomp filter installed (%s, %u instructions)", FPCJAIL_MODE, (unsigned)prog.len);
    free(prog.filter);
}
