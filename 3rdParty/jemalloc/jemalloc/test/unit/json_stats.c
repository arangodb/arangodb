#include "test/jemalloc_test.h"

typedef struct {
	char  *buf;
	size_t len;
	size_t capacity;
} stats_buf_t;

static void
stats_buf_init(stats_buf_t *sbuf) {
	/* 1MB buffer should be enough since per-arena stats are omitted. */
	sbuf->capacity = 1 << 20;
	sbuf->buf = mallocx(sbuf->capacity, MALLOCX_TCACHE_NONE);
	assert_ptr_not_null(sbuf->buf, "Failed to allocate stats buffer");
	sbuf->len = 0;
	sbuf->buf[0] = '\0';
}

static void
stats_buf_fini(stats_buf_t *sbuf) {
	dallocx(sbuf->buf, MALLOCX_TCACHE_NONE);
}

static void
stats_buf_write_cb(void *opaque, const char *str) {
	stats_buf_t *sbuf = (stats_buf_t *)opaque;
	size_t       slen = strlen(str);

	if (sbuf->len + slen + 1 > sbuf->capacity) {
		return;
	}
	memcpy(&sbuf->buf[sbuf->len], str, slen + 1);
	sbuf->len += slen;
}

static bool
json_extract_uint64(const char *json, const char *key, uint64_t *result) {
	char   search_key[128];
	size_t key_len;

	key_len = snprintf(search_key, sizeof(search_key), "\"%s\":", key);
	if (key_len >= sizeof(search_key)) {
		return true;
	}

	const char *pos = strstr(json, search_key);
	if (pos == NULL) {
		return true;
	}

	pos += key_len;
	while (*pos == ' ' || *pos == '\t' || *pos == '\n') {
		pos++;
	}

	char    *endptr;
	uint64_t value = strtoull(pos, &endptr, 10);
	if (endptr == pos) {
		return true;
	}

	*result = value;
	return false;
}

static const char *
json_find_section(const char *json, const char *section_name) {
	char   search_pattern[128];
	size_t pattern_len;

	pattern_len = snprintf(
	    search_pattern, sizeof(search_pattern), "\"%s\":", section_name);
	if (pattern_len >= sizeof(search_pattern)) {
		return NULL;
	}

	return strstr(json, search_pattern);
}

static void
verify_mutex_json(const char *mutexes_section, const char *mallctl_prefix,
    const char *mutex_name) {
	char   mallctl_path[128];
	size_t sz;

	const char *mutex_section = json_find_section(
	    mutexes_section, mutex_name);
	expect_ptr_not_null(mutex_section,
	    "Could not find %s mutex section in JSON", mutex_name);

	uint64_t ctl_num_ops, ctl_num_wait, ctl_num_spin_acq;
	uint64_t ctl_num_owner_switch, ctl_total_wait_time, ctl_max_wait_time;
	uint32_t ctl_max_num_thds;

	sz = sizeof(uint64_t);
	snprintf(mallctl_path, sizeof(mallctl_path), "%s.%s.num_ops",
	    mallctl_prefix, mutex_name);
	expect_d_eq(mallctl(mallctl_path, &ctl_num_ops, &sz, NULL, 0), 0,
	    "Unexpected mallctl() failure for %s", mallctl_path);

	snprintf(mallctl_path, sizeof(mallctl_path), "%s.%s.num_wait",
	    mallctl_prefix, mutex_name);
	expect_d_eq(mallctl(mallctl_path, &ctl_num_wait, &sz, NULL, 0), 0,
	    "Unexpected mallctl() failure for %s", mallctl_path);

	snprintf(mallctl_path, sizeof(mallctl_path), "%s.%s.num_spin_acq",
	    mallctl_prefix, mutex_name);
	expect_d_eq(mallctl(mallctl_path, &ctl_num_spin_acq, &sz, NULL, 0), 0,
	    "Unexpected mallctl() failure for %s", mallctl_path);

	snprintf(mallctl_path, sizeof(mallctl_path), "%s.%s.num_owner_switch",
	    mallctl_prefix, mutex_name);
	expect_d_eq(mallctl(mallctl_path, &ctl_num_owner_switch, &sz, NULL, 0),
	    0, "Unexpected mallctl() failure for %s", mallctl_path);

	snprintf(mallctl_path, sizeof(mallctl_path), "%s.%s.total_wait_time",
	    mallctl_prefix, mutex_name);
	expect_d_eq(mallctl(mallctl_path, &ctl_total_wait_time, &sz, NULL, 0),
	    0, "Unexpected mallctl() failure for %s", mallctl_path);

	snprintf(mallctl_path, sizeof(mallctl_path), "%s.%s.max_wait_time",
	    mallctl_prefix, mutex_name);
	expect_d_eq(mallctl(mallctl_path, &ctl_max_wait_time, &sz, NULL, 0), 0,
	    "Unexpected mallctl() failure for %s", mallctl_path);

	sz = sizeof(uint32_t);
	snprintf(mallctl_path, sizeof(mallctl_path), "%s.%s.max_num_thds",
	    mallctl_prefix, mutex_name);
	expect_d_eq(mallctl(mallctl_path, &ctl_max_num_thds, &sz, NULL, 0), 0,
	    "Unexpected mallctl() failure for %s", mallctl_path);

	uint64_t json_num_ops, json_num_wait, json_num_spin_acq;
	uint64_t json_num_owner_switch, json_total_wait_time,
	    json_max_wait_time;
	uint64_t json_max_num_thds;

	expect_false(
	    json_extract_uint64(mutex_section, "num_ops", &json_num_ops),
	    "%s: num_ops not found in JSON", mutex_name);
	expect_false(
	    json_extract_uint64(mutex_section, "num_wait", &json_num_wait),
	    "%s: num_wait not found in JSON", mutex_name);
	expect_false(json_extract_uint64(
	                 mutex_section, "num_spin_acq", &json_num_spin_acq),
	    "%s: num_spin_acq not found in JSON", mutex_name);
	expect_false(json_extract_uint64(mutex_section, "num_owner_switch",
	                 &json_num_owner_switch),
	    "%s: num_owner_switch not found in JSON", mutex_name);
	expect_false(json_extract_uint64(mutex_section, "total_wait_time",
	                 &json_total_wait_time),
	    "%s: total_wait_time not found in JSON", mutex_name);
	expect_false(json_extract_uint64(
	                 mutex_section, "max_wait_time", &json_max_wait_time),
	    "%s: max_wait_time not found in JSON", mutex_name);
	expect_false(json_extract_uint64(
	                 mutex_section, "max_num_thds", &json_max_num_thds),
	    "%s: max_num_thds not found in JSON", mutex_name);

	expect_u64_eq(json_num_ops, ctl_num_ops,
	    "%s: JSON num_ops doesn't match mallctl", mutex_name);
	expect_u64_eq(json_num_wait, ctl_num_wait,
	    "%s: JSON num_wait doesn't match mallctl", mutex_name);
	expect_u64_eq(json_num_spin_acq, ctl_num_spin_acq,
	    "%s: JSON num_spin_acq doesn't match mallctl", mutex_name);
	expect_u64_eq(json_num_owner_switch, ctl_num_owner_switch,
	    "%s: JSON num_owner_switch doesn't match mallctl", mutex_name);
	expect_u64_eq(json_total_wait_time, ctl_total_wait_time,
	    "%s: JSON total_wait_time doesn't match mallctl", mutex_name);
	expect_u64_eq(json_max_wait_time, ctl_max_wait_time,
	    "%s: JSON max_wait_time doesn't match mallctl", mutex_name);
	expect_u32_eq((uint32_t)json_max_num_thds, ctl_max_num_thds,
	    "%s: JSON max_num_thds doesn't match mallctl", mutex_name);
}

static const char  *global_mutex_names[] = {"background_thread",
     "max_per_bg_thd", "ctl", "prof", "prof_thds_data", "prof_dump",
     "prof_recent_alloc", "prof_recent_dump", "prof_stats"};
static const size_t num_global_mutexes = sizeof(global_mutex_names)
    / sizeof(global_mutex_names[0]);

static const char  *arena_mutex_names[] = {"large", "extent_avail",
     "extents_dirty", "extents_muzzy", "extents_retained", "decay_dirty",
     "decay_muzzy", "base", "tcache_list", "hpa_shard", "hpa_shard_grow",
     "hpa_sec"};
static const size_t num_arena_mutexes = sizeof(arena_mutex_names)
    / sizeof(arena_mutex_names[0]);

static const char *
json_find_object_end(const char *object_begin) {
	int depth = 0;
	for (const char *cur = object_begin; *cur != '\0'; cur++) {
		if (*cur == '{') {
			depth++;
		} else if (*cur == '}') {
			depth--;
			if (depth == 0) {
				return cur;
			}
			if (depth < 0) {
				return NULL;
			}
		}
	}
	return NULL;
}

static const char *
json_find_array_end(const char *array_begin) {
	int depth = 0;
	for (const char *cur = array_begin; *cur != '\0'; cur++) {
		if (*cur == '[') {
			depth++;
		} else if (*cur == ']') {
			depth--;
			if (depth == 0) {
				return cur;
			}
			if (depth < 0) {
				return NULL;
			}
		}
	}
	return NULL;
}

static const char *
json_find_previous_hpa_shard_object(
    const char *json, const char *pos, const char **object_end) {
	*object_end = NULL;
	const char *found = NULL;
	const char *cur = json;
	const char *next;

	while ((next = strstr(cur, "\"hpa_shard\":{")) != NULL && next < pos) {
		found = strchr(next, '{');
		cur = next + 1;
	}
	if (found == NULL) {
		return NULL;
	}
	*object_end = json_find_object_end(found);
	return found;
}

static const char *
json_find_named_object(
    const char *json, const char *key, const char **object_end) {
	*object_end = NULL;
	char   search_key[128];
	size_t written = malloc_snprintf(
	    search_key, sizeof(search_key), "\"%s\":{", key);
	if (written >= sizeof(search_key)) {
		return NULL;
	}

	const char *object_begin = strstr(json, search_key);
	if (object_begin == NULL) {
		return NULL;
	}
	object_begin = strchr(object_begin, '{');
	if (object_begin == NULL) {
		return NULL;
	}
	*object_end = json_find_object_end(object_begin);
	return object_begin;
}

static const char *
json_find_named_array(
    const char *json, const char *key, const char **array_end) {
	*array_end = NULL;
	char   search_key[128];
	size_t written = malloc_snprintf(
	    search_key, sizeof(search_key), "\"%s\":[", key);
	if (written >= sizeof(search_key)) {
		return NULL;
	}

	const char *array_begin = strstr(json, search_key);
	if (array_begin == NULL) {
		return NULL;
	}
	array_begin = strchr(array_begin, '[');
	if (array_begin == NULL) {
		return NULL;
	}
	*array_end = json_find_array_end(array_begin);
	return array_begin;
}

TEST_BEGIN(test_json_stats_mutexes) {
	test_skip_if(!config_stats);

	uint64_t epoch;
	expect_d_eq(mallctl("epoch", NULL, NULL, (void *)&epoch, sizeof(epoch)),
	    0, "Unexpected mallctl() failure");

	stats_buf_t sbuf;
	stats_buf_init(&sbuf);
	/* "J" for JSON format, "a" to omit per-arena stats. */
	malloc_stats_print(stats_buf_write_cb, &sbuf, "Ja");

	/* Verify global mutexes under stats.mutexes. */
	const char *global_mutexes_section = json_find_section(
	    sbuf.buf, "mutexes");
	expect_ptr_not_null(global_mutexes_section,
	    "Could not find global mutexes section in JSON output");

	for (size_t i = 0; i < num_global_mutexes; i++) {
		verify_mutex_json(global_mutexes_section, "stats.mutexes",
		    global_mutex_names[i]);
	}

	/* Verify arena mutexes under stats.arenas.merged.mutexes. */
	const char *arenas_section = json_find_section(
	    sbuf.buf, "stats.arenas");
	expect_ptr_not_null(arenas_section,
	    "Could not find stats.arenas section in JSON output");

	const char *merged_section = json_find_section(
	    arenas_section, "merged");
	expect_ptr_not_null(
	    merged_section, "Could not find merged section in JSON output");

	const char *arena_mutexes_section = json_find_section(
	    merged_section, "mutexes");
	expect_ptr_not_null(arena_mutexes_section,
	    "Could not find arena mutexes section in JSON output");

	for (size_t i = 0; i < num_arena_mutexes; i++) {
		/*
		 * MALLCTL_ARENAS_ALL is 4096 representing all arenas in
		 * mallctl queries.
		 */
		verify_mutex_json(arena_mutexes_section,
		    "stats.arenas.4096.mutexes", arena_mutex_names[i]);
	}

	stats_buf_fini(&sbuf);
}
TEST_END

/*
 * Verify that hpa_shard JSON stats contain "ndirty_huge" key in both
 * full_slabs and empty_slabs sections.  A previous bug emitted duplicate
 * "nactive_huge" instead of "ndirty_huge".
 */
TEST_BEGIN(test_hpa_shard_json_ndirty_huge) {
	test_skip_if(!config_stats);
	test_skip_if(!hpa_supported());

	/* Do some allocation to create HPA state. */
	void *p = mallocx(PAGE, MALLOCX_TCACHE_NONE);
	expect_ptr_not_null(p, "Unexpected mallocx failure");

	uint64_t epoch = 1;
	size_t   sz = sizeof(epoch);
	expect_d_eq(mallctl("epoch", NULL, NULL, (void *)&epoch, sz), 0,
	    "Unexpected mallctl() failure");

	stats_buf_t sbuf;
	stats_buf_init(&sbuf);
	/* "J" for JSON, include per-arena HPA stats. */
	malloc_stats_print(stats_buf_write_cb, &sbuf, "J");

	/*
	 * Find "full_slabs" and check it contains "ndirty_huge".
	 */
	const char *full_slabs = strstr(sbuf.buf, "\"full_slabs\"");
	if (full_slabs != NULL) {
		const char *empty_slabs = strstr(full_slabs, "\"empty_slabs\"");
		const char *search_end = empty_slabs != NULL
		    ? empty_slabs
		    : sbuf.buf + sbuf.len;
		/*
		 * Search for "ndirty_huge" between full_slabs and
		 * empty_slabs.
		 */
		const char *ndirty = full_slabs;
		bool        found = false;
		while (ndirty < search_end) {
			ndirty = strstr(ndirty, "\"ndirty_huge\"");
			if (ndirty != NULL && ndirty < search_end) {
				found = true;
				break;
			}
			break;
		}
		expect_true(
		    found, "full_slabs section should contain ndirty_huge key");
	}

	/*
	 * Find "empty_slabs" and check it contains "ndirty_huge".
	 */
	const char *empty_slabs = strstr(sbuf.buf, "\"empty_slabs\"");
	if (empty_slabs != NULL) {
		/* Find the end of the empty_slabs object. */
		const char *nonfull = strstr(empty_slabs, "\"nonfull_slabs\"");
		const char *search_end = nonfull != NULL ? nonfull
		                                         : sbuf.buf + sbuf.len;
		const char *ndirty = strstr(empty_slabs, "\"ndirty_huge\"");
		bool        found = (ndirty != NULL && ndirty < search_end);
		expect_true(found,
		    "empty_slabs section should contain ndirty_huge key");
	}

	stats_buf_fini(&sbuf);
	dallocx(p, MALLOCX_TCACHE_NONE);
}
TEST_END

TEST_BEGIN(test_hpa_shard_json_contains_sec_stats) {
	test_skip_if(!config_stats);
	test_skip_if(!hpa_supported());

	void *p = mallocx(PAGE, MALLOCX_TCACHE_NONE);
	expect_ptr_not_null(p, "Unexpected mallocx failure");

	uint64_t epoch = 1;
	size_t   sz = sizeof(epoch);
	expect_d_eq(mallctl("epoch", NULL, NULL, (void *)&epoch, sz), 0,
	    "Unexpected mallctl() failure");

	stats_buf_t sbuf;
	stats_buf_init(&sbuf);
	malloc_stats_print(stats_buf_write_cb, &sbuf, "J");

	const char *sec_bytes = strstr(sbuf.buf, "\"sec_bytes\"");
	expect_ptr_not_null(sec_bytes, "JSON output should contain sec_bytes");
	const char *hpa_shard_end = NULL;
	const char *hpa_shard = json_find_previous_hpa_shard_object(
	    sbuf.buf, sec_bytes, &hpa_shard_end);
	expect_ptr_not_null(hpa_shard,
	    "sec_bytes should be associated with an hpa_shard JSON object");
	expect_ptr_not_null(hpa_shard_end,
	    "Could not find end of enclosing hpa_shard JSON object");
	expect_true(sec_bytes != NULL && sec_bytes < hpa_shard_end,
	    "sec_bytes should be nested inside hpa_shard JSON object");
	const char *sec_hits = strstr(hpa_shard, "\"sec_hits\"");
	expect_true(sec_hits != NULL && sec_hits < hpa_shard_end,
	    "sec_hits should be nested inside hpa_shard JSON object");
	const char *sec_misses = strstr(hpa_shard, "\"sec_misses\"");
	expect_true(sec_misses != NULL && sec_misses < hpa_shard_end,
	    "sec_misses should be nested inside hpa_shard JSON object");

	stats_buf_fini(&sbuf);
	dallocx(p, MALLOCX_TCACHE_NONE);
}
TEST_END

TEST_BEGIN(test_hpa_shard_json_contains_retained_stats) {
	test_skip_if(!config_stats);
	test_skip_if(!hpa_supported());

	void *p = mallocx(PAGE, MALLOCX_TCACHE_NONE);
	expect_ptr_not_null(p, "Unexpected mallocx failure");

	uint64_t epoch = 1;
	size_t   sz = sizeof(epoch);
	expect_d_eq(mallctl("epoch", NULL, NULL, (void *)&epoch, sz), 0,
	    "Unexpected mallctl() failure");

	stats_buf_t sbuf;
	stats_buf_init(&sbuf);
	malloc_stats_print(stats_buf_write_cb, &sbuf, "J");

	const char *full_slabs_end = NULL;
	const char *full_slabs = json_find_named_object(
	    sbuf.buf, "full_slabs", &full_slabs_end);
	expect_ptr_not_null(
	    full_slabs, "JSON output should contain full_slabs");
	const char *full_retained = strstr(full_slabs, "\"nretained_nonhuge\"");
	expect_true(full_retained != NULL && full_retained < full_slabs_end,
	    "full_slabs should contain nretained_nonhuge");

	const char *empty_slabs_end = NULL;
	const char *empty_slabs = json_find_named_object(
	    sbuf.buf, "empty_slabs", &empty_slabs_end);
	expect_ptr_not_null(
	    empty_slabs, "JSON output should contain empty_slabs");
	const char *empty_retained = strstr(
	    empty_slabs, "\"nretained_nonhuge\"");
	expect_true(empty_retained != NULL && empty_retained < empty_slabs_end,
	    "empty_slabs should contain nretained_nonhuge");

	const char *nonfull_slabs_end = NULL;
	const char *nonfull_slabs = json_find_named_array(
	    sbuf.buf, "nonfull_slabs", &nonfull_slabs_end);
	expect_ptr_not_null(
	    nonfull_slabs, "JSON output should contain nonfull_slabs");
	const char *nonfull_retained = strstr(
	    nonfull_slabs, "\"nretained_nonhuge\"");
	expect_true(
	    nonfull_retained != NULL && nonfull_retained < nonfull_slabs_end,
	    "nonfull_slabs should contain nretained_nonhuge");

	stats_buf_fini(&sbuf);
	dallocx(p, MALLOCX_TCACHE_NONE);
}
TEST_END

int
main(void) {
	return test_no_reentrancy(test_json_stats_mutexes,
	    test_hpa_shard_json_ndirty_huge,
	    test_hpa_shard_json_contains_sec_stats,
	    test_hpa_shard_json_contains_retained_stats);
}
