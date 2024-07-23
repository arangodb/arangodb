#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/jemalloc_internal_includes.h"

#include "jemalloc/internal/buf_writer.h"
#include "jemalloc/internal/ctl.h"
#include "jemalloc/internal/prof_data.h"
#include "jemalloc/internal/prof_sys.h"

#ifdef JEMALLOC_PROF_LIBUNWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

#ifdef JEMALLOC_PROF_LIBGCC
/*
 * We have a circular dependency -- jemalloc_internal.h tells us if we should
 * use libgcc's unwinding functionality, but after we've included that, we've
 * already hooked _Unwind_Backtrace.  We'll temporarily disable hooking.
 */
#undef _Unwind_Backtrace
#include <unwind.h>
#define _Unwind_Backtrace JEMALLOC_TEST_HOOK(_Unwind_Backtrace, test_hooks_libc_hook)
#endif

/******************************************************************************/

malloc_mutex_t prof_dump_filename_mtx;

static uint64_t prof_dump_seq;
static uint64_t prof_dump_iseq;
static uint64_t prof_dump_mseq;
static uint64_t prof_dump_useq;

static char *prof_prefix = NULL;

/* The fallback allocator profiling functionality will use. */
base_t *prof_base;

void
bt_init(prof_bt_t *bt, void **vec) {
	cassert(config_prof);

	bt->vec = vec;
	bt->len = 0;
}

#ifdef JEMALLOC_PROF_LIBUNWIND
static void
prof_backtrace_impl(void **vec, unsigned *len, unsigned max_len) {
	int nframes;

	cassert(config_prof);
	assert(*len == 0);
	assert(vec != NULL);
	assert(max_len <= PROF_BT_MAX_LIMIT);

	nframes = unw_backtrace(vec, max_len);
	if (nframes <= 0) {
		return;
	}
	*len = nframes;
}
#elif (defined(JEMALLOC_PROF_LIBGCC))
static _Unwind_Reason_Code
prof_unwind_init_callback(struct _Unwind_Context *context, void *arg) {
	cassert(config_prof);

	return _URC_NO_REASON;
}

static _Unwind_Reason_Code
prof_unwind_callback(struct _Unwind_Context *context, void *arg) {
	prof_unwind_data_t *data = (prof_unwind_data_t *)arg;
	void *ip;

	cassert(config_prof);

	ip = (void *)_Unwind_GetIP(context);
	if (ip == NULL) {
		return _URC_END_OF_STACK;
	}
	data->vec[*data->len] = ip;
	(*data->len)++;
	if (*data->len == data->max) {
		return _URC_END_OF_STACK;
	}

	return _URC_NO_REASON;
}

static void
prof_backtrace_impl(void **vec, unsigned *len, unsigned max_len) {
	prof_unwind_data_t data = {vec, len, max_len};

	cassert(config_prof);
	assert(vec != NULL);
	assert(max_len <= PROF_BT_MAX_LIMIT);

	_Unwind_Backtrace(prof_unwind_callback, &data);
}
#elif (defined(JEMALLOC_PROF_GCC))
JEMALLOC_DIAGNOSTIC_PUSH
JEMALLOC_DIAGNOSTIC_IGNORE_FRAME_ADDRESS
static void
prof_backtrace_impl(void **vec, unsigned *len, unsigned max_len) {
/* The input arg must be a constant for __builtin_return_address. */
#define BT_FRAME(i)							\
	if ((i) < max_len) {						\
		void *p;						\
		if (__builtin_frame_address(i) == 0) {			\
			return;						\
		}							\
		p = __builtin_return_address(i);			\
		if (p == NULL) {					\
			return;						\
		}							\
		vec[(i)] = p;						\
		*len = (i) + 1;						\
	} else {							\
		return;							\
	}

	cassert(config_prof);
	assert(vec != NULL);
	assert(max_len <= PROF_BT_MAX_LIMIT);

	BT_FRAME(0)
	BT_FRAME(1)
	BT_FRAME(2)
	BT_FRAME(3)
	BT_FRAME(4)
	BT_FRAME(5)
	BT_FRAME(6)
	BT_FRAME(7)
	BT_FRAME(8)
	BT_FRAME(9)

	BT_FRAME(10)
	BT_FRAME(11)
	BT_FRAME(12)
	BT_FRAME(13)
	BT_FRAME(14)
	BT_FRAME(15)
	BT_FRAME(16)
	BT_FRAME(17)
	BT_FRAME(18)
	BT_FRAME(19)

	BT_FRAME(20)
	BT_FRAME(21)
	BT_FRAME(22)
	BT_FRAME(23)
	BT_FRAME(24)
	BT_FRAME(25)
	BT_FRAME(26)
	BT_FRAME(27)
	BT_FRAME(28)
	BT_FRAME(29)

	BT_FRAME(30)
	BT_FRAME(31)
	BT_FRAME(32)
	BT_FRAME(33)
	BT_FRAME(34)
	BT_FRAME(35)
	BT_FRAME(36)
	BT_FRAME(37)
	BT_FRAME(38)
	BT_FRAME(39)

	BT_FRAME(40)
	BT_FRAME(41)
	BT_FRAME(42)
	BT_FRAME(43)
	BT_FRAME(44)
	BT_FRAME(45)
	BT_FRAME(46)
	BT_FRAME(47)
	BT_FRAME(48)
	BT_FRAME(49)

	BT_FRAME(50)
	BT_FRAME(51)
	BT_FRAME(52)
	BT_FRAME(53)
	BT_FRAME(54)
	BT_FRAME(55)
	BT_FRAME(56)
	BT_FRAME(57)
	BT_FRAME(58)
	BT_FRAME(59)

	BT_FRAME(60)
	BT_FRAME(61)
	BT_FRAME(62)
	BT_FRAME(63)
	BT_FRAME(64)
	BT_FRAME(65)
	BT_FRAME(66)
	BT_FRAME(67)
	BT_FRAME(68)
	BT_FRAME(69)

	BT_FRAME(70)
	BT_FRAME(71)
	BT_FRAME(72)
	BT_FRAME(73)
	BT_FRAME(74)
	BT_FRAME(75)
	BT_FRAME(76)
	BT_FRAME(77)
	BT_FRAME(78)
	BT_FRAME(79)

	BT_FRAME(80)
	BT_FRAME(81)
	BT_FRAME(82)
	BT_FRAME(83)
	BT_FRAME(84)
	BT_FRAME(85)
	BT_FRAME(86)
	BT_FRAME(87)
	BT_FRAME(88)
	BT_FRAME(89)

	BT_FRAME(90)
	BT_FRAME(91)
	BT_FRAME(92)
	BT_FRAME(93)
	BT_FRAME(94)
	BT_FRAME(95)
	BT_FRAME(96)
	BT_FRAME(97)
	BT_FRAME(98)
	BT_FRAME(99)

	BT_FRAME(100)
	BT_FRAME(101)
	BT_FRAME(102)
	BT_FRAME(103)
	BT_FRAME(104)
	BT_FRAME(105)
	BT_FRAME(106)
	BT_FRAME(107)
	BT_FRAME(108)
	BT_FRAME(109)

	BT_FRAME(110)
	BT_FRAME(111)
	BT_FRAME(112)
	BT_FRAME(113)
	BT_FRAME(114)
	BT_FRAME(115)
	BT_FRAME(116)
	BT_FRAME(117)
	BT_FRAME(118)
	BT_FRAME(119)

	BT_FRAME(120)
	BT_FRAME(121)
	BT_FRAME(122)
	BT_FRAME(123)
	BT_FRAME(124)
	BT_FRAME(125)
	BT_FRAME(126)
	BT_FRAME(127)
	BT_FRAME(128)
	BT_FRAME(129)

	BT_FRAME(130)
	BT_FRAME(131)
	BT_FRAME(132)
	BT_FRAME(133)
	BT_FRAME(134)
	BT_FRAME(135)
	BT_FRAME(136)
	BT_FRAME(137)
	BT_FRAME(138)
	BT_FRAME(139)

	BT_FRAME(140)
	BT_FRAME(141)
	BT_FRAME(142)
	BT_FRAME(143)
	BT_FRAME(144)
	BT_FRAME(145)
	BT_FRAME(146)
	BT_FRAME(147)
	BT_FRAME(148)
	BT_FRAME(149)

	BT_FRAME(150)
	BT_FRAME(151)
	BT_FRAME(152)
	BT_FRAME(153)
	BT_FRAME(154)
	BT_FRAME(155)
	BT_FRAME(156)
	BT_FRAME(157)
	BT_FRAME(158)
	BT_FRAME(159)

	BT_FRAME(160)
	BT_FRAME(161)
	BT_FRAME(162)
	BT_FRAME(163)
	BT_FRAME(164)
	BT_FRAME(165)
	BT_FRAME(166)
	BT_FRAME(167)
	BT_FRAME(168)
	BT_FRAME(169)

	BT_FRAME(170)
	BT_FRAME(171)
	BT_FRAME(172)
	BT_FRAME(173)
	BT_FRAME(174)
	BT_FRAME(175)
	BT_FRAME(176)
	BT_FRAME(177)
	BT_FRAME(178)
	BT_FRAME(179)

	BT_FRAME(180)
	BT_FRAME(181)
	BT_FRAME(182)
	BT_FRAME(183)
	BT_FRAME(184)
	BT_FRAME(185)
	BT_FRAME(186)
	BT_FRAME(187)
	BT_FRAME(188)
	BT_FRAME(189)

	BT_FRAME(190)
	BT_FRAME(191)
	BT_FRAME(192)
	BT_FRAME(193)
	BT_FRAME(194)
	BT_FRAME(195)
	BT_FRAME(196)
	BT_FRAME(197)
	BT_FRAME(198)
	BT_FRAME(199)

	BT_FRAME(200)
	BT_FRAME(201)
	BT_FRAME(202)
	BT_FRAME(203)
	BT_FRAME(204)
	BT_FRAME(205)
	BT_FRAME(206)
	BT_FRAME(207)
	BT_FRAME(208)
	BT_FRAME(209)

	BT_FRAME(210)
	BT_FRAME(211)
	BT_FRAME(212)
	BT_FRAME(213)
	BT_FRAME(214)
	BT_FRAME(215)
	BT_FRAME(216)
	BT_FRAME(217)
	BT_FRAME(218)
	BT_FRAME(219)

	BT_FRAME(220)
	BT_FRAME(221)
	BT_FRAME(222)
	BT_FRAME(223)
	BT_FRAME(224)
	BT_FRAME(225)
	BT_FRAME(226)
	BT_FRAME(227)
	BT_FRAME(228)
	BT_FRAME(229)

	BT_FRAME(230)
	BT_FRAME(231)
	BT_FRAME(232)
	BT_FRAME(233)
	BT_FRAME(234)
	BT_FRAME(235)
	BT_FRAME(236)
	BT_FRAME(237)
	BT_FRAME(238)
	BT_FRAME(239)

	BT_FRAME(240)
	BT_FRAME(241)
	BT_FRAME(242)
	BT_FRAME(243)
	BT_FRAME(244)
	BT_FRAME(245)
	BT_FRAME(246)
	BT_FRAME(247)
	BT_FRAME(248)
	BT_FRAME(249)

	BT_FRAME(250)
	BT_FRAME(251)
	BT_FRAME(252)
	BT_FRAME(253)
	BT_FRAME(254)
	BT_FRAME(255)
#undef BT_FRAME
JEMALLOC_DIAGNOSTIC_POP
}
#else
static void
prof_backtrace_impl(void **vec, unsigned *len, unsigned max_len) {
	cassert(config_prof);
	not_reached();
}
#endif

void
prof_backtrace(tsd_t *tsd, prof_bt_t *bt) {
	cassert(config_prof);
	prof_backtrace_hook_t prof_backtrace_hook = prof_backtrace_hook_get();
	assert(prof_backtrace_hook != NULL);

	pre_reentrancy(tsd, NULL);
	prof_backtrace_hook(bt->vec, &bt->len, opt_prof_bt_max);
	post_reentrancy(tsd);
}

void
prof_hooks_init(void) {
	prof_backtrace_hook_set(&prof_backtrace_impl);
	prof_dump_hook_set(NULL);
	prof_sample_hook_set(NULL);
	prof_sample_free_hook_set(NULL);
}

void
prof_unwind_init(void) {
#ifdef JEMALLOC_PROF_LIBGCC
	/*
	 * Cause the backtracing machinery to allocate its internal
	 * state before enabling profiling.
	 */
	_Unwind_Backtrace(prof_unwind_init_callback, NULL);
#endif
}

static int
prof_sys_thread_name_read_impl(char *buf, size_t limit) {
#if defined(JEMALLOC_HAVE_PTHREAD_GETNAME_NP)
	return pthread_getname_np(pthread_self(), buf, limit);
#elif defined(JEMALLOC_HAVE_PTHREAD_GET_NAME_NP)
	pthread_get_name_np(pthread_self(), buf, limit);
	return 0;
#else
	return ENOSYS;
#endif
}
prof_sys_thread_name_read_t *JET_MUTABLE prof_sys_thread_name_read =
    prof_sys_thread_name_read_impl;

void
prof_sys_thread_name_fetch(tsd_t *tsd) {
	prof_tdata_t *tdata = prof_tdata_get(tsd, true);
	if (tdata == NULL) {
		return;
	}

	if (prof_sys_thread_name_read(tdata->thread_name,
	    PROF_THREAD_NAME_MAX_LEN) != 0) {
		prof_thread_name_clear(tdata);
	}

	tdata->thread_name[PROF_THREAD_NAME_MAX_LEN - 1] = '\0';
}

int
prof_getpid(void) {
#ifdef _WIN32
	return GetCurrentProcessId();
#else
	return getpid();
#endif
}

/*
 * This buffer is rather large for stack allocation, so use a single buffer for
 * all profile dumps; protected by prof_dump_mtx.
 */
static char prof_dump_buf[PROF_DUMP_BUFSIZE];

typedef struct prof_dump_arg_s prof_dump_arg_t;
struct prof_dump_arg_s {
	/*
	 * Whether error should be handled locally: if true, then we print out
	 * error message as well as abort (if opt_abort is true) when an error
	 * occurred, and we also report the error back to the caller in the end;
	 * if false, then we only report the error back to the caller in the
	 * end.
	 */
	const bool handle_error_locally;
	/*
	 * Whether there has been an error in the dumping process, which could
	 * have happened either in file opening or in file writing.  When an
	 * error has already occurred, we will stop further writing to the file.
	 */
	bool error;
	/* File descriptor of the dump file. */
	int prof_dump_fd;
};

static void
prof_dump_check_possible_error(prof_dump_arg_t *arg, bool err_cond,
    const char *format, ...) {
	assert(!arg->error);
	if (!err_cond) {
		return;
	}

	arg->error = true;
	if (!arg->handle_error_locally) {
		return;
	}

	va_list ap;
	char buf[PROF_PRINTF_BUFSIZE];
	va_start(ap, format);
	malloc_vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);
	malloc_write(buf);

	if (opt_abort) {
		abort();
	}
}

static int
prof_dump_open_file_impl(const char *filename, int mode) {
	return creat(filename, mode);
}
prof_dump_open_file_t *JET_MUTABLE prof_dump_open_file =
    prof_dump_open_file_impl;

static void
prof_dump_open(prof_dump_arg_t *arg, const char *filename) {
	arg->prof_dump_fd = prof_dump_open_file(filename, 0644);
	prof_dump_check_possible_error(arg, arg->prof_dump_fd == -1,
	    "<jemalloc>: failed to open \"%s\"\n", filename);
}

prof_dump_write_file_t *JET_MUTABLE prof_dump_write_file = malloc_write_fd;

static void
prof_dump_flush(void *opaque, const char *s) {
	cassert(config_prof);
	prof_dump_arg_t *arg = (prof_dump_arg_t *)opaque;
	if (!arg->error) {
		ssize_t err = prof_dump_write_file(arg->prof_dump_fd, s,
		    strlen(s));
		prof_dump_check_possible_error(arg, err == -1,
		    "<jemalloc>: failed to write during heap profile flush\n");
	}
}

static void
prof_dump_close(prof_dump_arg_t *arg) {
	if (arg->prof_dump_fd != -1) {
		close(arg->prof_dump_fd);
	}
}

#ifndef _WIN32
JEMALLOC_FORMAT_PRINTF(1, 2)
static int
prof_open_maps_internal(const char *format, ...) {
	int mfd;
	va_list ap;
	char filename[PATH_MAX + 1];

	va_start(ap, format);
	malloc_vsnprintf(filename, sizeof(filename), format, ap);
	va_end(ap);

#if defined(O_CLOEXEC)
	mfd = open(filename, O_RDONLY | O_CLOEXEC);
#else
	mfd = open(filename, O_RDONLY);
	if (mfd != -1) {
		fcntl(mfd, F_SETFD, fcntl(mfd, F_GETFD) | FD_CLOEXEC);
	}
#endif

	return mfd;
}
#endif

static int
prof_dump_open_maps_impl(void) {
	int mfd;

	cassert(config_prof);
#if defined(__FreeBSD__) || defined(__DragonFly__)
	mfd = prof_open_maps_internal("/proc/curproc/map");
#elif defined(_WIN32)
	mfd = -1; // Not implemented
#else
	int pid = prof_getpid();

	mfd = prof_open_maps_internal("/proc/%d/task/%d/maps", pid, pid);
	if (mfd == -1) {
		mfd = prof_open_maps_internal("/proc/%d/maps", pid);
	}
#endif
	return mfd;
}
prof_dump_open_maps_t *JET_MUTABLE prof_dump_open_maps =
    prof_dump_open_maps_impl;

static ssize_t
prof_dump_read_maps_cb(void *read_cbopaque, void *buf, size_t limit) {
	int mfd = *(int *)read_cbopaque;
	assert(mfd != -1);
	return malloc_read_fd(mfd, buf, limit);
}

static void
prof_dump_maps(buf_writer_t *buf_writer) {
	int mfd = prof_dump_open_maps();
	if (mfd == -1) {
		return;
	}

	buf_writer_cb(buf_writer, "\nMAPPED_LIBRARIES:\n");
	buf_writer_pipe(buf_writer, prof_dump_read_maps_cb, &mfd);
	close(mfd);
}

static bool
prof_dump(tsd_t *tsd, bool propagate_err, const char *filename,
    bool leakcheck) {
	cassert(config_prof);
	assert(tsd_reentrancy_level_get(tsd) == 0);

	prof_tdata_t * tdata = prof_tdata_get(tsd, true);
	if (tdata == NULL) {
		return true;
	}

	prof_dump_arg_t arg = {/* handle_error_locally */ !propagate_err,
	    /* error */ false, /* prof_dump_fd */ -1};

	pre_reentrancy(tsd, NULL);
	malloc_mutex_lock(tsd_tsdn(tsd), &prof_dump_mtx);

	prof_dump_open(&arg, filename);
	buf_writer_t buf_writer;
	bool err = buf_writer_init(tsd_tsdn(tsd), &buf_writer, prof_dump_flush,
	    &arg, prof_dump_buf, PROF_DUMP_BUFSIZE);
	assert(!err);
	prof_dump_impl(tsd, buf_writer_cb, &buf_writer, tdata, leakcheck);
	prof_dump_maps(&buf_writer);
	buf_writer_terminate(tsd_tsdn(tsd), &buf_writer);
	prof_dump_close(&arg);

	prof_dump_hook_t dump_hook = prof_dump_hook_get();
	if (dump_hook != NULL) {
		dump_hook(filename);
	}
	malloc_mutex_unlock(tsd_tsdn(tsd), &prof_dump_mtx);
	post_reentrancy(tsd);

	return arg.error;
}

/*
 * If profiling is off, then PROF_DUMP_FILENAME_LEN is 1, so we'll end up
 * calling strncpy with a size of 0, which triggers a -Wstringop-truncation
 * warning (strncpy can never actually be called in this case, since we bail out
 * much earlier when config_prof is false).  This function works around the
 * warning to let us leave the warning on.
 */
static inline void
prof_strncpy(char *UNUSED dest, const char *UNUSED src, size_t UNUSED size) {
	cassert(config_prof);
#ifdef JEMALLOC_PROF
	strncpy(dest, src, size);
#endif
}

static const char *
prof_prefix_get(tsdn_t* tsdn) {
	malloc_mutex_assert_owner(tsdn, &prof_dump_filename_mtx);

	return prof_prefix == NULL ? opt_prof_prefix : prof_prefix;
}

static bool
prof_prefix_is_empty(tsdn_t *tsdn) {
	malloc_mutex_lock(tsdn, &prof_dump_filename_mtx);
	bool ret = (prof_prefix_get(tsdn)[0] == '\0');
	malloc_mutex_unlock(tsdn, &prof_dump_filename_mtx);
	return ret;
}

#define DUMP_FILENAME_BUFSIZE (PATH_MAX + 1)
#define VSEQ_INVALID UINT64_C(0xffffffffffffffff)
static void
prof_dump_filename(tsd_t *tsd, char *filename, char v, uint64_t vseq) {
	cassert(config_prof);

	assert(tsd_reentrancy_level_get(tsd) == 0);
	const char *prefix = prof_prefix_get(tsd_tsdn(tsd));

	if (vseq != VSEQ_INVALID) {
	        /* "<prefix>.<pid>.<seq>.v<vseq>.heap" */
		malloc_snprintf(filename, DUMP_FILENAME_BUFSIZE,
		    "%s.%d.%"FMTu64".%c%"FMTu64".heap", prefix, prof_getpid(),
		    prof_dump_seq, v, vseq);
	} else {
	        /* "<prefix>.<pid>.<seq>.<v>.heap" */
		malloc_snprintf(filename, DUMP_FILENAME_BUFSIZE,
		    "%s.%d.%"FMTu64".%c.heap", prefix, prof_getpid(),
		    prof_dump_seq, v);
	}
	prof_dump_seq++;
}

void
prof_get_default_filename(tsdn_t *tsdn, char *filename, uint64_t ind) {
	malloc_mutex_lock(tsdn, &prof_dump_filename_mtx);
	malloc_snprintf(filename, PROF_DUMP_FILENAME_LEN,
	    "%s.%d.%"FMTu64".json", prof_prefix_get(tsdn), prof_getpid(), ind);
	malloc_mutex_unlock(tsdn, &prof_dump_filename_mtx);
}

void
prof_fdump_impl(tsd_t *tsd) {
	char filename[DUMP_FILENAME_BUFSIZE];

	assert(!prof_prefix_is_empty(tsd_tsdn(tsd)));
	malloc_mutex_lock(tsd_tsdn(tsd), &prof_dump_filename_mtx);
	prof_dump_filename(tsd, filename, 'f', VSEQ_INVALID);
	malloc_mutex_unlock(tsd_tsdn(tsd), &prof_dump_filename_mtx);
	prof_dump(tsd, false, filename, opt_prof_leak);
}

bool
prof_prefix_set(tsdn_t *tsdn, const char *prefix) {
	cassert(config_prof);
	ctl_mtx_assert_held(tsdn);
	if (prefix == NULL) {
		return true;
	}
	malloc_mutex_lock(tsdn, &prof_dump_filename_mtx);
	if (prof_prefix == NULL) {
		malloc_mutex_unlock(tsdn, &prof_dump_filename_mtx);
		/* Everything is still guarded by ctl_mtx. */
		char *buffer = base_alloc(tsdn, prof_base,
		    PROF_DUMP_FILENAME_LEN, QUANTUM);
		if (buffer == NULL) {
			return true;
		}
		malloc_mutex_lock(tsdn, &prof_dump_filename_mtx);
		prof_prefix = buffer;
	}
	assert(prof_prefix != NULL);

	prof_strncpy(prof_prefix, prefix, PROF_DUMP_FILENAME_LEN - 1);
	prof_prefix[PROF_DUMP_FILENAME_LEN - 1] = '\0';
	malloc_mutex_unlock(tsdn, &prof_dump_filename_mtx);

	return false;
}

void
prof_idump_impl(tsd_t *tsd) {
	malloc_mutex_lock(tsd_tsdn(tsd), &prof_dump_filename_mtx);
	if (prof_prefix_get(tsd_tsdn(tsd))[0] == '\0') {
		malloc_mutex_unlock(tsd_tsdn(tsd), &prof_dump_filename_mtx);
		return;
	}
	char filename[PATH_MAX + 1];
	prof_dump_filename(tsd, filename, 'i', prof_dump_iseq);
	prof_dump_iseq++;
	malloc_mutex_unlock(tsd_tsdn(tsd), &prof_dump_filename_mtx);
	prof_dump(tsd, false, filename, false);
}

bool
prof_mdump_impl(tsd_t *tsd, const char *filename) {
	char filename_buf[DUMP_FILENAME_BUFSIZE];
	if (filename == NULL) {
		/* No filename specified, so automatically generate one. */
		malloc_mutex_lock(tsd_tsdn(tsd), &prof_dump_filename_mtx);
		if (prof_prefix_get(tsd_tsdn(tsd))[0] == '\0') {
			malloc_mutex_unlock(tsd_tsdn(tsd), &prof_dump_filename_mtx);
			return true;
		}
		prof_dump_filename(tsd, filename_buf, 'm', prof_dump_mseq);
		prof_dump_mseq++;
		malloc_mutex_unlock(tsd_tsdn(tsd), &prof_dump_filename_mtx);
		filename = filename_buf;
	}
	return prof_dump(tsd, true, filename, false);
}

void
prof_gdump_impl(tsd_t *tsd) {
	tsdn_t *tsdn = tsd_tsdn(tsd);
	malloc_mutex_lock(tsdn, &prof_dump_filename_mtx);
	if (prof_prefix_get(tsdn)[0] == '\0') {
		malloc_mutex_unlock(tsdn, &prof_dump_filename_mtx);
		return;
	}
	char filename[DUMP_FILENAME_BUFSIZE];
	prof_dump_filename(tsd, filename, 'u', prof_dump_useq);
	prof_dump_useq++;
	malloc_mutex_unlock(tsdn, &prof_dump_filename_mtx);
	prof_dump(tsd, false, filename, false);
}
