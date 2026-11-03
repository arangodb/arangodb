#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/jemalloc_internal_includes.h"

#include "jemalloc/internal/malloc_io.h"
#include "jemalloc/internal/prof_sys.h"

#if defined(__linux__) && defined(JEMALLOC_HAVE_GETTID)

#	include <errno.h>
#	include <fcntl.h>
#	include <stdio.h>
#	include <stdlib.h> // strtoul
#	include <string.h>
#	include <unistd.h>

/*
 * Converts a string representing a hexadecimal number to an unsigned long long
 * integer. Functionally equivalent to strtoull() (for base 16) but faster for
 * that case.
 *
 * @param nptr Pointer to the string to be converted.
 * @param endptr Pointer to a pointer to character, which will be set to the
 * character in `nptr` where parsing stopped. Can be NULL.
 * @return The converted unsigned long long integer value.
 */
static inline unsigned long long int
strtoull_hex(const char *nptr, char **endptr) {
	unsigned long long int val = 0;
	int                    ii = 0;
	for (; ii < 16; ++ii) {
		char c = nptr[ii];
		if (c >= '0' && c <= '9') {
			val = (val << 4) + (c - '0');
		} else if (c >= 'a' && c <= 'f') {
			val = (val << 4) + (c - 'a' + 10);
		} else {
			break;
		}
	}
	if (endptr) {
		*endptr = (char *)(nptr + ii);
	}
	return val;
}

static int
prof_mapping_containing_addr(uintptr_t addr, const char *maps_path,
    uintptr_t *mm_start, uintptr_t *mm_end) {
	int ret = ENOENT; /* not found */
	*mm_start = *mm_end = 0;

	/*
     * Each line of /proc/<pid>/maps is:
     * <start>-<end> <perms> <offset> <dev> <inode> <pathname>
     *
     * The fields we care about are always within the first 34 characters so
     * as long as `buf` contains the start of a mapping line it can always be
     * parsed.
     */
	static const int kMappingFieldsWidth = 34;

	int     fd = -1;
	char    buf[4096];
	ssize_t remaining = 0; /* actual number of bytes read to buf */
	char   *line = NULL;

	while (1) {
		if (fd < 0) {
			/* case 0: initial open of maps file */
			fd = malloc_open(maps_path, O_RDONLY);
			if (fd < 0) {
				return errno;
			}

			remaining = malloc_read_fd(fd, buf, sizeof(buf));
			if (remaining < 0) {
				ret = errno;
				break;
			} else if (remaining == 0) {
				break;
			}
			line = buf;
		} else if (line == NULL) {
			/* case 1: no newline found in buf */
			remaining = malloc_read_fd(fd, buf, sizeof(buf));
			if (remaining < 0) {
				ret = errno;
				break;
			} else if (remaining == 0) {
				break;
			}
			line = memchr(buf, '\n', remaining);
			if (line != NULL) {
				line++; /* advance to character after newline */
				remaining -= (line - buf);
			}
		} else if (line != NULL && remaining < kMappingFieldsWidth) {
			/*
             * case 2: found newline but insufficient characters remaining in
             * buf
             */
			memcpy(buf, line,
			    remaining); /* copy remaining characters to start of buf */
			line = buf;

			ssize_t count = malloc_read_fd(
			    fd, buf + remaining, sizeof(buf) - remaining);
			if (count < 0) {
				ret = errno;
				break;
			} else if (count == 0) {
				break;
			}

			remaining +=
			    count; /* actual number of bytes read to buf */
		} else {
			/* case 3: found newline and sufficient characters to parse */

			/* parse <start>-<end> */
			char     *tmp = line;
			uintptr_t start_addr = (uintptr_t)strtoull_hex(
			    tmp, &tmp);
			if (addr >= start_addr) {
				tmp++; /* advance to character after '-' */
				uintptr_t end_addr = (uintptr_t)strtoull_hex(
				    tmp, NULL);
				if (addr < end_addr) {
					*mm_start = start_addr;
					*mm_end = end_addr;
					ret = 0;
					break;
				}
			}

			/* Advance to character after next newline in the current buf. */
			char *prev_line = line;
			line = memchr(line, '\n', remaining);
			if (line != NULL) {
				line++; /* advance to character after newline */
				remaining -= (line - prev_line);
			}
		}
	}

	malloc_close(fd);
	return ret;
}

int
prof_thread_stack_range(uintptr_t fp, uintptr_t *low, uintptr_t *high) {
	/*
     * NOTE: Prior to kernel 4.5 an entry for every thread stack was included in
     * /proc/<pid>/maps as [STACK:<tid>]. Starting with kernel 4.5 only the main
     * thread stack remains as the [stack] mapping. For other thread stacks the
     * mapping is still visible in /proc/<pid>/task/<tid>/maps (though not
     * labeled as [STACK:tid]).
     * https://lists.ubuntu.com/archives/kernel-team/2016-March/074681.html
    */
	char maps_path[64]; // "/proc/<pid>/task/<tid>/maps"
	malloc_snprintf(maps_path, sizeof(maps_path), "/proc/%d/task/%d/maps",
	    getpid(), gettid());
	return prof_mapping_containing_addr(fp, maps_path, low, high);
}

#else

int
prof_thread_stack_range(
    UNUSED uintptr_t addr, uintptr_t *stack_start, uintptr_t *stack_end) {
	*stack_start = *stack_end = 0;
	return ENOENT;
}

#endif // __linux__
