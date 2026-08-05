#ifndef JEMALLOC_INTERNAL_CONF_H
#define JEMALLOC_INTERNAL_CONF_H

#include "jemalloc/internal/sc.h"

void malloc_conf_init(sc_data_t *sc_data, unsigned bin_shard_sizes[SC_NBINS],
    char readlink_buf[PATH_MAX + 1]);
void malloc_abort_invalid_conf(void);

#ifdef JEMALLOC_JET
extern bool had_conf_error;

bool conf_next(char const **opts_p, char const **k_p, size_t *klen_p,
    char const **v_p, size_t *vlen_p);
void conf_error(
    const char *msg, const char *k, size_t klen, const char *v, size_t vlen);
bool conf_handle_bool(const char *v, size_t vlen, bool *result);
bool conf_handle_signed(const char *v, size_t vlen, intmax_t min, intmax_t max,
    bool check_min, bool check_max, bool clip, intmax_t *result);
bool conf_handle_char_p(const char *v, size_t vlen, char *dest, size_t dest_sz);
#endif

#endif /* JEMALLOC_INTERNAL_CONF_H */
