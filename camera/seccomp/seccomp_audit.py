#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project
"""Turn a service's seccomp audit records into policy additions.

For the camera provider (default) or, with --service bluetooth, the Bluetooth
HCI service (bluetooth/seccomp). Collect the kernel records after exercising
the camera or Bluetooth, for example:

    adb logcat -b all -d > logcat.txt
    adb shell su 0 dmesg > dmesg.txt      # or: adb root; adb shell dmesg

then run

    python3 seccomp_audit.py logcat.txt dmesg.txt
    python3 seccomp_audit.py --write logcat.txt dmesg.txt   # append to the policy
    python3 seccomp_audit.py --service bluetooth logcat.txt dmesg.txt

Log-only builds record every call outside the policy as an audit record of
type 1326 with code 0x7ffc0000 (SECCOMP_RET_LOG). The record names the system
call number only, not its arguments: a call that already has a rule means its
arguments did not match (find them with strace -f -e on the service before
widening the rule). Trap builds leave a tombstone instead ("seccomp prevented
call to disallowed arm64 system call N"), which is read too; the Bluetooth
service also logs trapped calls and calls its policy fails with an errno
(code 0x5nnnn) as audit records.
The kernel and logd rate-limit audit records: repeat the run after adding
frequent calls, until no new records appear.
"""
import argparse
import collections
import datetime
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
# Services with a seccomp filter: policy, executable and SELinux domain.
SERVICES = {
    'camera': (HERE / 'camera-provider.arm64.policy',
               '/vendor/bin/hw/vendor.qti.camera.provider-service_64', 'u:r:hal_camera_default:s0'),
    'bluetooth': (HERE.parent.parent / 'bluetooth/seccomp/bluetooth-hci.arm64.policy',
                  '/vendor/bin/hw/android.hardware.bluetooth@1.1-service.fp6', 'u:r:hal_bluetooth_default:s0'),
}
AUDIT_ARCH_AARCH64 = 'c00000b7'
ACTIONS = {'0x7ffc0000': 'logged and allowed (RET_LOG)', '0x80000000': 'process killed (RET_KILL_PROCESS)',
           '0x0': 'thread killed (RET_KILL_THREAD)', '0x30000': 'trapped (RET_TRAP)'}

# arm64 system call numbers (Linux 6.1 include/uapi/asm-generic/unistd.h).
SYSCALLS = dict((int(n), name) for n, name in (item.split(':') for item in """
    0:io_setup 1:io_destroy 2:io_submit 3:io_cancel 4:io_getevents 5:setxattr
    6:lsetxattr 7:fsetxattr 8:getxattr 9:lgetxattr 10:fgetxattr 11:listxattr
    12:llistxattr 13:flistxattr 14:removexattr 15:lremovexattr 16:fremovexattr
    17:getcwd 18:lookup_dcookie 19:eventfd2 20:epoll_create1 21:epoll_ctl
    22:epoll_pwait 23:dup 24:dup3 25:fcntl 26:inotify_init1 27:inotify_add_watch
    28:inotify_rm_watch 29:ioctl 30:ioprio_set 31:ioprio_get 32:flock 33:mknodat
    34:mkdirat 35:unlinkat 36:symlinkat 37:linkat 38:renameat 39:umount2
    40:mount 41:pivot_root 42:nfsservctl 43:statfs 44:fstatfs 45:truncate
    46:ftruncate 47:fallocate 48:faccessat 49:chdir 50:fchdir 51:chroot
    52:fchmod 53:fchmodat 54:fchownat 55:fchown 56:openat 57:close 58:vhangup
    59:pipe2 60:quotactl 61:getdents64 62:lseek 63:read 64:write 65:readv
    66:writev 67:pread64 68:pwrite64 69:preadv 70:pwritev 71:sendfile
    72:pselect6 73:ppoll 74:signalfd4 75:vmsplice 76:splice 77:tee 78:readlinkat
    79:newfstatat 80:fstat 81:sync 82:fsync 83:fdatasync 84:sync_file_range
    85:timerfd_create 86:timerfd_settime 87:timerfd_gettime 88:utimensat 89:acct
    90:capget 91:capset 92:personality 93:exit 94:exit_group 95:waitid
    96:set_tid_address 97:unshare 98:futex 99:set_robust_list
    100:get_robust_list 101:nanosleep 102:getitimer 103:setitimer 104:kexec_load
    105:init_module 106:delete_module 107:timer_create 108:timer_gettime
    109:timer_getoverrun 110:timer_settime 111:timer_delete 112:clock_settime
    113:clock_gettime 114:clock_getres 115:clock_nanosleep 116:syslog 117:ptrace
    118:sched_setparam 119:sched_setscheduler 120:sched_getscheduler
    121:sched_getparam 122:sched_setaffinity 123:sched_getaffinity
    124:sched_yield 125:sched_get_priority_max 126:sched_get_priority_min
    127:sched_rr_get_interval 128:restart_syscall 129:kill 130:tkill 131:tgkill
    132:sigaltstack 133:rt_sigsuspend 134:rt_sigaction 135:rt_sigprocmask
    136:rt_sigpending 137:rt_sigtimedwait 138:rt_sigqueueinfo 139:rt_sigreturn
    140:setpriority 141:getpriority 142:reboot 143:setregid 144:setgid
    145:setreuid 146:setuid 147:setresuid 148:getresuid 149:setresgid
    150:getresgid 151:setfsuid 152:setfsgid 153:times 154:setpgid 155:getpgid
    156:getsid 157:setsid 158:getgroups 159:setgroups 160:uname 161:sethostname
    162:setdomainname 163:getrlimit 164:setrlimit 165:getrusage 166:umask
    167:prctl 168:getcpu 169:gettimeofday 170:settimeofday 171:adjtimex
    172:getpid 173:getppid 174:getuid 175:geteuid 176:getgid 177:getegid
    178:gettid 179:sysinfo 180:mq_open 181:mq_unlink 182:mq_timedsend
    183:mq_timedreceive 184:mq_notify 185:mq_getsetattr 186:msgget 187:msgctl
    188:msgrcv 189:msgsnd 190:semget 191:semctl 192:semtimedop 193:semop
    194:shmget 195:shmctl 196:shmat 197:shmdt 198:socket 199:socketpair 200:bind
    201:listen 202:accept 203:connect 204:getsockname 205:getpeername 206:sendto
    207:recvfrom 208:setsockopt 209:getsockopt 210:shutdown 211:sendmsg
    212:recvmsg 213:readahead 214:brk 215:munmap 216:mremap 217:add_key
    218:request_key 219:keyctl 220:clone 221:execve 222:mmap 223:fadvise64
    224:swapon 225:swapoff 226:mprotect 227:msync 228:mlock 229:munlock
    230:mlockall 231:munlockall 232:mincore 233:madvise 234:remap_file_pages
    235:mbind 236:get_mempolicy 237:set_mempolicy 238:migrate_pages
    239:move_pages 240:rt_tgsigqueueinfo 241:perf_event_open 242:accept4
    243:recvmmsg 244:arch_specific_syscall 260:wait4 261:prlimit64
    262:fanotify_init 263:fanotify_mark 266:clock_adjtime 267:syncfs 268:setns
    269:sendmmsg 270:process_vm_readv 271:process_vm_writev 272:kcmp
    273:finit_module 274:sched_setattr 275:sched_getattr 276:renameat2
    277:seccomp 278:getrandom 279:memfd_create 280:bpf 281:execveat
    282:userfaultfd 283:membarrier 284:mlock2 285:copy_file_range 286:preadv2
    287:pwritev2 288:pkey_mprotect 289:pkey_alloc 290:pkey_free 291:statx
    292:io_pgetevents 293:rseq 294:kexec_file_load 424:pidfd_send_signal
    425:io_uring_setup 426:io_uring_enter 427:io_uring_register 428:open_tree
    429:move_mount 430:fsopen 431:fsconfig 432:fsmount 433:fspick 434:pidfd_open
    435:clone3 436:close_range 437:openat2 438:pidfd_getfd 439:faccessat2
    440:process_madvise 441:epoll_pwait2 442:mount_setattr 443:quotactl_fd
    444:landlock_create_ruleset 445:landlock_add_rule 446:landlock_restrict_self
    447:memfd_secret 448:process_mrelease 449:futex_waitv
    450:set_mempolicy_home_node
""".split()))

RECORD = re.compile(r'type=1326 audit\(([^)]*)\):(.*)')
FIELD = re.compile(r'(\w+)=("[^"]*"|\S+)')
TOMBSTONE_PROCESS = re.compile(r'>>> (\S+) <<<')
TOMBSTONE_CAUSE = re.compile(r'seccomp prevented call to disallowed arm64 system call (\d+)')
LOST = re.compile(r'rate limit exceeded|audit_lost|backlog limit exceeded|kauditd hold queue overflow')


def decode(value):
    """Audit prints untrusted strings quoted, or as hex when they contain spaces."""
    if value.startswith('"'):
        return value.strip('"')
    try:
        return bytes.fromhex(value).decode('utf-8', 'replace')
    except ValueError:
        return value


def policy_names(path):
    names = set()
    for line in path.read_text().splitlines():
        line = line.split('#', 1)[0].strip()
        if line and not line.startswith('@'):
            names.add(line.split(':', 1)[0].strip())
    return names


def action(code):
    if code in ACTIONS:
        return ACTIONS[code]
    if re.fullmatch(r'0x5[0-9a-f]{4}', code):
        return f'failed with errno {int(code, 16) & 0xffff} (RET_ERRNO)'
    return code


def read_records(paths, executable, domain):
    records, lost, seen = [], 0, set()
    for path in paths:
        process = None
        for line in Path(path).read_text(errors='replace').splitlines():
            if LOST.search(line):
                lost += 1
            match = TOMBSTONE_PROCESS.search(line)
            if match:
                process = match.group(1)
            match = TOMBSTONE_CAUSE.search(line)
            if match and process == executable:
                records.append({'syscall': int(match.group(1)), 'code': 'tombstone', 'comm': ''})
                continue
            match = RECORD.search(line)
            if not match:
                continue
            fields = {k: v for k, v in FIELD.findall(match.group(2))}
            exe, subj = decode(fields.get('exe', '""')), fields.get('subj', '')
            if exe != executable and subj != domain:
                continue
            # logcat and dmesg carry the same record; its serial is unique per boot.
            key = (match.group(1).rpartition(':')[2], fields.get('syscall'))
            if key in seen:
                continue
            seen.add(key)
            if fields.get('arch') != AUDIT_ARCH_AARCH64:
                print('warning: non-arm64 record:', line.strip(), file=sys.stderr)
            records.append({'syscall': int(fields['syscall']), 'code': fields.get('code', '?'),
                            'comm': decode(fields.get('comm', '""'))})
    return records, lost


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('logs', nargs='+', help='logcat and dmesg output files')
    parser.add_argument('--service', choices=sorted(SERVICES), default='camera')
    parser.add_argument('--policy', type=Path, help="default: the service's policy in this repository")
    parser.add_argument('--write', action='store_true', help='append the missing calls to the policy')
    args = parser.parse_args()
    policy, executable, domain = SERVICES[args.service]
    args.policy = args.policy or policy
    listed = policy_names(args.policy)
    records, lost = read_records(args.logs, executable, domain)
    print(f'{len(records)} {args.service} seccomp records in {len(args.logs)} files')
    for code, count in collections.Counter(r['code'] for r in records).most_common():
        print(f'  {count:6}  {action(code)}')
    if lost:
        print(f'warning: {lost} lines report lost or rate-limited audit records; repeat the run')
    counts = collections.Counter(r['syscall'] for r in records)
    threads = collections.defaultdict(set)
    for r in records:
        threads[r['syscall']].add(r['comm'])
    missing, arguments = [], []
    for nr, count in counts.most_common():
        name = SYSCALLS.get(nr, f'unknown_{nr}')
        (arguments if name in listed else missing).append((name, count, sorted(threads[nr])[:3]))
    if missing:
        print('\nnot in the policy (add after review):')
        for name, count, comms in missing:
            print(f'  {name:24} {count:6}  {", ".join(c for c in comms if c)}')
    if arguments:
        print('\nlisted, but the arguments did not match the rule (check with strace first):')
        for name, count, comms in arguments:
            print(f'  {name:24} {count:6}  {", ".join(c for c in comms if c)}')
    additions = [name for name, _, _ in missing if not name.startswith('unknown_')]
    if args.write and additions:
        block = (f'\n# Added from {len(records)} audit records, {datetime.date.today().isoformat()}.\n'
                 + ''.join(f'{name}: 1\n' for name in additions))
        with args.policy.open('a') as policy:
            policy.write(block)
        print(f'\nappended {len(additions)} lines to {args.policy}')
    elif additions:
        print('\n' + ''.join(f'{name}: 1\n' for name in additions), end='')
    return 0


if __name__ == '__main__':
    sys.exit(main())
