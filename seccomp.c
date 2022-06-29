/*
 * Copyright (c) 2022 Tobias Stoeckmann <tobias@stoeckmann.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/seccomp.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <seccomp.h>
#include <unistd.h>

#include "config.h"
#include "functions.h"

static int
use_seccomp(void)
{
	return prctl(PR_GET_SECCOMP, 0, 0, 0, 0) != -1 &&
	    prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER,
	    NULL, 0, 0) == -1 && errno == EFAULT;
}

static int
add_common_stage2_rules(scmp_filter_ctx ctx)
{
	/* add pledge stdio and additionally needed system calls */
	return seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(access), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(brk), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_getres), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_gettime), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(close), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(dup), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(dup2), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(dup3), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fchdir), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fcntl), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fcntl64), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstat), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstat64), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstatat64), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fsync), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ftruncate), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(futex), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getdents), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getegid), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(geteuid), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getgid), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getgroups), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getitimer), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpgid), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpgrp), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpid), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getppid), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getresgid), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getresuid), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getrlimit), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getsid), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(gettimeofday), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getuid), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(lseek), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(_llseek), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(madvise), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mmap), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mmap2), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mprotect), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(munmap), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(nanosleep), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(newfstatat), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pipe), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pipe2), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(poll), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ppoll), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(prctl), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(preadv), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pwritev), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(readv), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(recv), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(recvfrom), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(recvmsg), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(restart_syscall), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(select), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sendmsg), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sendto), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setitimer), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(shutdown), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sigaction), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sigprocmask), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sigreturn), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(socketpair), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(statx), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(umask), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(wait4), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(writev), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(uname), 0);
}

void
stage1_sandbox(void)
{
	scmp_filter_ctx ctx;

	if (!use_seccomp()) {
		debug("Linked with libseccomp, but kernel has no support.");
		return;
	}

	ctx = seccomp_init(SCMP_ACT_KILL);
	if (ctx == NULL ||
	    /* pledge: dns */
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(connect), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(sendto), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(socket), 0) ||
	    /* pledge: inet+unix */
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(accept), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(accept4), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(bind), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(listen), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpeername), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getsockname), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getsockopt), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setsockopt), 0) ||
	    /* pledge: rpath */
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(chdir), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(chmod), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(chown), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(faccessat), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fchmodat), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fchmod), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fchown), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fchownat), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getcwd), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(lstat), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(open), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(openat), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(readlinkat), 0) ||
	    /* pledge: proc */
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clone), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(set_robust_list), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(setsid), 0) ||
	    /* seccomp for stage 2 */
	    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(seccomp), 0) ||
	    add_common_stage2_rules(ctx) ||
	    seccomp_load(ctx))
		err(1, "failed to set up stage 1 seccomp");
	seccomp_release(ctx);
}

void
stage2_sandbox(void)
{
	scmp_filter_ctx ctx;

	if (!use_seccomp()) {
		debug("Linked with libseccomp, but kernel has no support.");
		return;
	}

	ctx = seccomp_init(SCMP_ACT_KILL);
	if (ctx == NULL || add_common_stage2_rules(ctx) ||
#if defined (WITH_JPEG) && defined(__linux__) && (defined(__aarch64__) || \
    defined(__arm__) || defined(__mips__) || defined(__powerpc64__) || \
    defined(__powerpc__))
	    /*
	     * libjpeg-turbo opens /proc/cpuinfo on these architectures;
	     * deny the access with error instead of termination.
	     */
	    seccomp_rule_add(ctx, SCMP_ACT_ERRNO(1), SCMP_SYS(open), 0) ||
	    seccomp_rule_add(ctx, SCMP_ACT_ERRNO(1), SCMP_SYS(openat), 0) ||
#endif /* WITH_JPEG and /proc/cpuinfo */
	    seccomp_load(ctx))
		err(1, "failed to set up stage 2 seccomp");
	seccomp_release(ctx);
}

