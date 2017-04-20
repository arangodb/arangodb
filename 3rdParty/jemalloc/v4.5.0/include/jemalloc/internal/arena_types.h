#ifndef JEMALLOC_INTERNAL_ARENA_TYPES_H
#define JEMALLOC_INTERNAL_ARENA_TYPES_H

#define LARGE_MINCLASS		(ZU(1) << LG_LARGE_MINCLASS)

/* Maximum number of regions in one slab. */
#define LG_SLAB_MAXREGS		(LG_PAGE - LG_TINY_MIN)
#define SLAB_MAXREGS		(1U << LG_SLAB_MAXREGS)

/* Default decay time in seconds. */
#define DECAY_TIME_DEFAULT	10
/* Number of event ticks between time checks. */
#define DECAY_NTICKS_PER_UPDATE	1000

typedef struct arena_slab_data_s arena_slab_data_t;
typedef struct arena_bin_info_s arena_bin_info_t;
typedef struct arena_decay_s arena_decay_t;
typedef struct arena_bin_s arena_bin_t;
typedef struct arena_s arena_t;
typedef struct arena_tdata_s arena_tdata_t;

#endif /* JEMALLOC_INTERNAL_ARENA_TYPES_H */
