#include "test/jemalloc_test.h"

/* Additional includes for PA functionality */
#include "jemalloc/internal/pa.h"
#include "jemalloc/internal/tsd.h"
#include "jemalloc/internal/sz.h"
#include "jemalloc/internal/base.h"
#include "jemalloc/internal/ehooks.h"
#include "jemalloc/internal/nstime.h"
#include "jemalloc/internal/hpa.h"
#include "jemalloc/internal/sec.h"
#include "jemalloc/internal/emap.h"
#include "jemalloc/internal/psset.h"

/*
 * PA Microbenchmark (Simplified Version)
 *
 * This tool reads allocation traces and simulates PA behavior
 * for testing and understanding the allocation patterns.
 *
 * Features:
 * 1. Reads CSV input file with format: shard_ind,operation,size_or_alloc_index,is_frequent
 * 2. Simulates allocations/deallocations tracking
 * 3. Provides basic statistics analysis
 * 4. Validates the framework setup
 */

#define MAX_LINE_LENGTH 1024
#define MAX_ALLOCATIONS 10000000
#define MAX_ARENAS 128

typedef enum { PA_ALLOC = 0, PA_DALLOC = 1 } pa_op_t;

typedef struct {
	int      shard_ind;
	pa_op_t  operation;
	size_t   size_or_alloc_index;
	uint64_t nsecs;
	int      is_frequent;
} pa_event_t;

typedef struct {
	edata_t *edata;
	size_t   size;
	int      shard_ind;
	bool     active;
} allocation_record_t;

/* Structure to group per-shard tracking statistics */
typedef struct {
	uint64_t alloc_count;     /* Number of allocations */
	uint64_t dealloc_count;   /* Number of deallocations */
	uint64_t bytes_allocated; /* Current bytes allocated */
} shard_stats_t;

/* Structure to group per-shard PA infrastructure */
typedef struct {
	base_t          *base;        /* Base allocator */
	emap_t           emap;        /* Extent map */
	pa_shard_t       pa_shard;    /* PA shard */
	pa_shard_stats_t shard_stats; /* PA shard statistics */
	malloc_mutex_t   stats_mtx;   /* Statistics mutex */
} shard_infrastructure_t;

static FILE                *g_stats_output = NULL; /* Output file for stats */
static size_t               g_alloc_counter = 0; /* Global allocation counter */
static allocation_record_t *g_alloc_records =
    NULL;                     /* Global allocation tracking */
static bool g_use_sec = true; /* Global flag for SEC vs HPA-only */

/* Refactored arrays using structures */
static shard_stats_t *g_shard_stats = NULL; /* Per-shard tracking statistics */
static shard_infrastructure_t *g_shard_infra =
    NULL;                         /* Per-shard PA infrastructure */
static pa_central_t g_pa_central; /* Global PA central */

/* Override for curtime */
static hpa_hooks_t hpa_hooks_override;
static nstime_t    cur_time_clock;

void
curtime(nstime_t *r_time, bool first_reading) {
	if (first_reading) {
		nstime_init_zero(r_time);
	}
	*r_time = cur_time_clock;
}

static void
set_clock(uint64_t nsecs) {
	nstime_init(&cur_time_clock, nsecs);
}

static void
init_hpa_hooks() {
	hpa_hooks_override = hpa_hooks_default;
	hpa_hooks_override.curtime = curtime;
}

static void cleanup_pa_infrastructure(int num_shards);

static bool
initialize_pa_infrastructure(int num_shards) {
	/*
	 * Note when we call malloc, it resolves to je_malloc, while internal
	 * functions like base_new resolve to jet_malloc.  This is because this
	 * file is compiled with -DJEMALLOC_JET as a test.  This allows us to
	 * completely isolate the PA infrastructure benchmark from the rest of
	 * the jemalloc usage.
	*/
	void *dummy_jet = jet_malloc(16);
	if (dummy_jet == NULL) {
		fprintf(stderr, "Failed to initialize JET jemalloc\n");
		return 1;
	}

	/* Force JET system to be fully initialized */
	if (jet_mallctl("epoch", NULL, NULL, NULL, 0) != 0) {
		fprintf(stderr, "Failed to initialize JET system fully\n");
		jet_free(dummy_jet);
		return 1;
	}
	jet_free(dummy_jet);

	/* Allocate shard tracking statistics */
	g_shard_stats = calloc(num_shards, sizeof(shard_stats_t));
	if (g_shard_stats == NULL) {
		printf("DEBUG: Failed to allocate shard stats\n");
		return true;
	}

	/* Allocate shard infrastructure */
	g_shard_infra = calloc(num_shards, sizeof(shard_infrastructure_t));
	if (g_shard_infra == NULL) {
		printf("DEBUG: Failed to allocate shard infrastructure\n");
		free(g_shard_stats);
		return true;
	}

	/* Initialize one base allocator for PA central */
	base_t *central_base = base_new(tsd_tsdn(tsd_fetch()), 0 /* ind */,
	    (extent_hooks_t *)&ehooks_default_extent_hooks,
	    /* metadata_use_hooks */ true);
	if (central_base == NULL) {
		printf("DEBUG: Failed to create central_base\n");
		free(g_shard_stats);
		free(g_shard_infra);
		return true;
	}

	/* Initialize PA central with HPA enabled */
	init_hpa_hooks();
	if (pa_central_init(&g_pa_central, central_base, true /* hpa */,
	        &hpa_hooks_override)) {
		printf("DEBUG: Failed to initialize PA central\n");
		base_delete(tsd_tsdn(tsd_fetch()), central_base);
		free(g_shard_stats);
		free(g_shard_infra);
		return true;
	}

	for (int i = 0; i < num_shards; i++) {
		/* Create a separate base allocator for each shard */
		g_shard_infra[i].base = base_new(tsd_tsdn(tsd_fetch()),
		    i /* ind */, (extent_hooks_t *)&ehooks_default_extent_hooks,
		    /* metadata_use_hooks */ true);
		if (g_shard_infra[i].base == NULL) {
			printf("DEBUG: Failed to create base %d\n", i);
			/* Clean up partially initialized shards */
			cleanup_pa_infrastructure(num_shards);
			return true;
		}

		/* Initialize emap for this shard */
		if (emap_init(&g_shard_infra[i].emap, g_shard_infra[i].base,
		        /* zeroed */ false)) {
			printf("DEBUG: Failed to initialize emap %d\n", i);
			/* Clean up partially initialized shards */
			cleanup_pa_infrastructure(num_shards);
			return true;
		}

		/* Initialize stats mutex */
		if (malloc_mutex_init(&g_shard_infra[i].stats_mtx,
		        "pa_shard_stats", WITNESS_RANK_OMIT,
		        malloc_mutex_rank_exclusive)) {
			printf(
			    "DEBUG: Failed to initialize stats mutex %d\n", i);
			/* Clean up partially initialized shards */
			cleanup_pa_infrastructure(num_shards);
			return true;
		}

		/* Initialize PA shard */
		nstime_t cur_time;
		nstime_init_zero(&cur_time);

		if (pa_shard_init(tsd_tsdn(tsd_fetch()),
		        &g_shard_infra[i].pa_shard, &g_pa_central,
		        &g_shard_infra[i].emap /* emap */,
		        g_shard_infra[i].base, i /* ind */,
		        &g_shard_infra[i].shard_stats /* stats */,
		        &g_shard_infra[i].stats_mtx /* stats_mtx */,
		        &cur_time /* cur_time */,
		        SIZE_MAX /* oversize_threshold */,
		        -1 /* dirty_decay_ms */, -1 /* muzzy_decay_ms */)) {
			printf("DEBUG: Failed to initialize PA shard %d\n", i);
			/* Clean up partially initialized shards */
			cleanup_pa_infrastructure(num_shards);
			return true;
		}

		/* Enable HPA for this shard with proper configuration */
		hpa_shard_opts_t hpa_opts = HPA_SHARD_OPTS_DEFAULT;
		hpa_opts.deferral_allowed =
		    false; /* No background threads in microbench */

		sec_opts_t sec_opts = SEC_OPTS_DEFAULT;
		if (!g_use_sec) {
			/* Disable SEC by setting nshards to 0 */
			sec_opts.nshards = 0;
		}

		if (pa_shard_enable_hpa(tsd_tsdn(tsd_fetch()),
		        &g_shard_infra[i].pa_shard, &hpa_opts, &sec_opts)) {
			fprintf(
			    stderr, "Failed to enable HPA on shard %d\n", i);
			/* Clean up partially initialized shards */
			cleanup_pa_infrastructure(num_shards);
			return true;
		}
	}

	printf("PA infrastructure configured: HPA=enabled, SEC=%s\n",
	    g_use_sec ? "enabled" : "disabled");

	return false;
}

static void
cleanup_pa_infrastructure(int num_shards) {
	if (g_shard_infra != NULL) {
		for (int i = 0; i < num_shards; i++) {
			pa_shard_destroy(
			    tsd_tsdn(tsd_fetch()), &g_shard_infra[i].pa_shard);
			if (g_shard_infra[i].base != NULL) {
				base_delete(tsd_tsdn(tsd_fetch()),
				    g_shard_infra[i].base);
			}
		}
		free(g_shard_infra);
		g_shard_infra = NULL;
	}

	if (g_shard_stats != NULL) {
		free(g_shard_stats);
		g_shard_stats = NULL;
	}
}

static bool
parse_csv_line(const char *line, pa_event_t *event) {
	/* Expected format: shard_ind,operation,size_or_alloc_index,is_frequent */
	int operation;
	int fields = sscanf(line, "%d,%d,%zu,%lu,%d", &event->shard_ind,
	    &operation, &event->size_or_alloc_index, &event->nsecs,
	    &event->is_frequent);

	if (fields < 4) { /* is_frequent is optional */
		return false;
	}

	if (fields == 4) {
		event->is_frequent = 0; /* Default value */
	}

	if (operation == 0) {
		event->operation = PA_ALLOC;
	} else if (operation == 1) {
		event->operation = PA_DALLOC;
	} else {
		return false;
	}

	return true;
}

static size_t
load_trace_file(const char *filename, pa_event_t **events, int *max_shard_id) {
	FILE *file = fopen(filename, "r");
	if (!file) {
		fprintf(stderr, "Failed to open trace file: %s\n", filename);
		return 0;
	}

	*events = malloc(MAX_ALLOCATIONS * sizeof(pa_event_t));
	if (!*events) {
		fclose(file);
		return 0;
	}

	char   line[MAX_LINE_LENGTH];
	size_t count = 0;
	*max_shard_id = 0;

	/* Skip header line */
	if (fgets(line, sizeof(line), file) == NULL) {
		fclose(file);
		free(*events);
		return 0;
	}

	while (fgets(line, sizeof(line), file) && count < MAX_ALLOCATIONS) {
		if (parse_csv_line(line, &(*events)[count])) {
			if ((*events)[count].shard_ind > *max_shard_id) {
				*max_shard_id = (*events)[count].shard_ind;
			}
			count++;
		}
	}

	fclose(file);
	printf("Loaded %zu events from %s\n", count, filename);
	printf("Maximum shard ID found: %d\n", *max_shard_id);
	return count;
}

static void
collect_hpa_stats(int shard_id, hpa_shard_stats_t *hpa_stats_out) {
	/* Get tsdn for statistics collection */
	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());

	/* Clear the output structure */
	memset(hpa_stats_out, 0, sizeof(hpa_shard_stats_t));

	/* Check if this shard has HPA enabled */
	if (!g_shard_infra[shard_id].pa_shard.ever_used_hpa) {
		return;
	}

	/* Merge HPA statistics from the shard */
	hpa_shard_stats_merge(
	    tsdn, &g_shard_infra[shard_id].pa_shard.hpa_shard, hpa_stats_out);
}

static void
print_shard_stats(int shard_id, size_t operation_count) {
	if (!g_stats_output) {
		return;
	}

	/* Collect HPA statistics */
	hpa_shard_stats_t hpa_stats;
	collect_hpa_stats(shard_id, &hpa_stats);
	psset_stats_t *psset_stats = &hpa_stats.psset_stats;

	/* Total pageslabs */
	size_t total_pageslabs = psset_stats->merged.npageslabs;

	/* Full pageslabs breakdown by hugification */
	size_t full_pageslabs_non_huge =
	    psset_stats->full_slabs[0].npageslabs; /* [0] = non-hugified */
	size_t full_pageslabs_huge =
	    psset_stats->full_slabs[1].npageslabs; /* [1] = hugified */
	size_t full_pageslabs_total = full_pageslabs_non_huge
	    + full_pageslabs_huge;

	/* Empty pageslabs breakdown by hugification */
	size_t empty_pageslabs_non_huge =
	    psset_stats->empty_slabs[0].npageslabs; /* [0] = non-hugified */
	size_t empty_pageslabs_huge =
	    psset_stats->empty_slabs[1].npageslabs; /* [1] = hugified */
	size_t empty_pageslabs_total = empty_pageslabs_non_huge
	    + empty_pageslabs_huge;

	/* Hugified pageslabs (full + empty + partial) */
	size_t hugified_pageslabs = full_pageslabs_huge + empty_pageslabs_huge;
	/* Add hugified partial slabs */
	for (int i = 0; i < PSSET_NPSIZES; i++) {
		hugified_pageslabs +=
		    psset_stats->nonfull_slabs[i][1].npageslabs;
	}

	/* Dirty bytes */
	size_t   dirty_bytes = psset_stats->merged.ndirty * PAGE;
	uint64_t npurge_passes = hpa_stats.nonderived_stats.npurge_passes;
	uint64_t npurges = hpa_stats.nonderived_stats.npurges;

	assert(g_use_sec
	    || psset_stats->merged.nactive * PAGE
	        == g_shard_stats[shard_id].bytes_allocated);
	/* Output enhanced stats with detailed breakdown */
	fprintf(g_stats_output,
	    "%zu,%d,%lu,%lu,%lu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%lu,%lu,%lu"
	    ",%lu,%lu\n",
	    operation_count, shard_id, g_shard_stats[shard_id].alloc_count,
	    g_shard_stats[shard_id].dealloc_count,
	    g_shard_stats[shard_id].bytes_allocated, total_pageslabs,
	    full_pageslabs_total, empty_pageslabs_total, hugified_pageslabs,
	    full_pageslabs_non_huge, full_pageslabs_huge,
	    empty_pageslabs_non_huge, empty_pageslabs_huge, dirty_bytes,
	    hpa_stats.nonderived_stats.nhugifies,
	    hpa_stats.nonderived_stats.nhugify_failures,
	    hpa_stats.nonderived_stats.ndehugifies, npurge_passes, npurges);
	fflush(g_stats_output);
}

static void
simulate_trace(
    int num_shards, pa_event_t *events, size_t count, size_t stats_interval) {
	uint64_t total_allocs = 0, total_deallocs = 0;
	uint64_t total_allocated_bytes = 0;

	printf("Starting simulation with %zu events across %d shards...\n",
	    count, num_shards);

	for (size_t i = 0; i < count; i++) {
		pa_event_t *event = &events[i];

		/* Validate shard index */
		if (event->shard_ind >= num_shards) {
			fprintf(stderr,
			    "Warning: Invalid shard index %d (max %d)\n",
			    event->shard_ind, num_shards - 1);
			continue;
		}

		set_clock(event->nsecs);
		switch (event->operation) {
		case PA_ALLOC: {
			size_t size = event->size_or_alloc_index;

			/* Get tsdn and calculate parameters for PA */
			tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
			szind_t szind = sz_size2index(size);
			bool    slab =
			    event
			        ->is_frequent; /* Use frequent_reuse for slab */
			bool deferred_work_generated = false;

			/* Allocate using PA allocator */
			edata_t *edata = pa_alloc(tsdn,
			    &g_shard_infra[event->shard_ind].pa_shard, size,
			    PAGE /* alignment */, slab, szind, false /* zero */,
			    false /* guarded */, &deferred_work_generated);

			if (edata != NULL) {
				/* Store allocation record */
				g_alloc_records[g_alloc_counter].edata = edata;
				g_alloc_records[g_alloc_counter].size = size;
				g_alloc_records[g_alloc_counter].shard_ind =
				    event->shard_ind;
				g_alloc_records[g_alloc_counter].active = true;
				g_alloc_counter++;

				/* Update shard-specific stats */
				g_shard_stats[event->shard_ind].alloc_count++;
				g_shard_stats[event->shard_ind]
				    .bytes_allocated += size;

				total_allocs++;
				total_allocated_bytes += size;
			}
			break;
		}
		case PA_DALLOC: {
			size_t alloc_index = event->size_or_alloc_index;
			if (alloc_index < g_alloc_counter
			    && g_alloc_records[alloc_index].active
			    && g_alloc_records[alloc_index].shard_ind
			        == event->shard_ind) {
				/* Get tsdn for PA */
				tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
				bool    deferred_work_generated = false;

				/* Deallocate using PA allocator */
				pa_dalloc(tsdn,
				    &g_shard_infra[event->shard_ind].pa_shard,
				    g_alloc_records[alloc_index].edata,
				    &deferred_work_generated);

				/* Update shard-specific stats */
				g_shard_stats[event->shard_ind].dealloc_count++;
				g_shard_stats[event->shard_ind]
				    .bytes_allocated -=
				    g_alloc_records[alloc_index].size;

				g_alloc_records[alloc_index].active = false;
				total_deallocs++;
			}
			break;
		}
		}

		/* Periodic stats output and progress reporting */
		if (stats_interval > 0 && (i + 1) % stats_interval == 0) {
			/* Print stats for all shards */
			for (int j = 0; j < num_shards; j++) {
				print_shard_stats(j, i + 1);
			}
		}
	}

	printf("\nSimulation completed:\n");
	printf("  Total allocations: %lu\n", total_allocs);
	printf("  Total deallocations: %lu\n", total_deallocs);
	printf("  Total allocated: %lu bytes\n", total_allocated_bytes);
	printf("  Active allocations: %lu\n", g_alloc_counter - total_deallocs);

	/* Print final stats for all shards */
	printf("\nFinal shard statistics:\n");
	for (int i = 0; i < num_shards; i++) {
		printf(
		    "  Shard %d: Allocs=%lu, Deallocs=%lu, Active Bytes=%lu\n",
		    i, g_shard_stats[i].alloc_count,
		    g_shard_stats[i].dealloc_count,
		    g_shard_stats[i].bytes_allocated);

		/* Final stats to file */
		print_shard_stats(i, count);
	}
}

static void
cleanup_remaining_allocations(int num_shards) {
	size_t cleaned_up = 0;

	printf("Cleaning up remaining allocations...\n");

	for (size_t i = 0; i < g_alloc_counter; i++) {
		if (g_alloc_records[i].active) {
			int shard_ind = g_alloc_records[i].shard_ind;
			if (shard_ind < num_shards) {
				tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
				bool    deferred_work_generated = false;

				pa_dalloc(tsdn,
				    &g_shard_infra[shard_ind].pa_shard,
				    g_alloc_records[i].edata,
				    &deferred_work_generated);

				g_alloc_records[i].active = false;
				cleaned_up++;
			}
		}
	}

	printf("Cleaned up %zu remaining allocations\n", cleaned_up);
}

static void
print_usage(const char *program) {
	printf("Usage: %s [options] <trace_file.csv>\n", program);
	printf("Options:\n");
	printf("  -h, --help           Show this help message\n");
	printf(
	    "  -o, --output FILE    Output file for statistics (default: stdout)\n");
	printf("  -s, --sec            Use SEC (default)\n");
	printf("  -p, --hpa-only       Use HPA only (no SEC)\n");
	printf(
	    "  -i, --interval N     Stats print interval (default: 100000, 0=disable)\n");
	printf(
	    "\nTrace file format: shard_ind,operation,size_or_alloc_index,is_frequent\n");
	printf("  - operation: 0=alloc, 1=dealloc\n");
	printf("  - is_frequent: optional column\n");
}

int
main(int argc, char *argv[]) {
	const char *trace_file = NULL;
	const char *stats_output_file = NULL;
	size_t      stats_interval = 100000; /* Default stats print interval */
	/* Parse command line arguments */
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0
		    || strcmp(argv[i], "--help") == 0) {
			print_usage(argv[0]);
			return 0;
		} else if (strcmp(argv[i], "-o") == 0
		    || strcmp(argv[i], "--output") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr,
				    "Error: %s requires an argument\n",
				    argv[i]);
				return 1;
			}
			stats_output_file = argv[++i];
		} else if (strcmp(argv[i], "-s") == 0
		    || strcmp(argv[i], "--sec") == 0) {
			g_use_sec = true;
		} else if (strcmp(argv[i], "-p") == 0
		    || strcmp(argv[i], "--hpa-only") == 0) {
			g_use_sec = false;
		} else if (strcmp(argv[i], "-i") == 0
		    || strcmp(argv[i], "--interval") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr,
				    "Error: %s requires an argument\n",
				    argv[i]);
				return 1;
			}
			stats_interval = (size_t)atol(argv[++i]);
		} else if (argv[i][0] != '-') {
			trace_file = argv[i];
		} else {
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			print_usage(argv[0]);
			return 1;
		}
	}

	if (!trace_file) {
		fprintf(stderr, "Error: No trace file specified\n");
		print_usage(argv[0]);
		return 1;
	}

	printf("Trace file: %s\n", trace_file);
	printf("Mode: %s\n", g_use_sec ? "PA with SEC" : "HPA only");

	/* Open stats output file */
	if (stats_output_file) {
		g_stats_output = fopen(stats_output_file, "w");
		if (!g_stats_output) {
			fprintf(stderr,
			    "Failed to open stats output file: %s\n",
			    stats_output_file);
			return 1;
		}
		printf("Stats output: %s\n", stats_output_file);

		/* Write CSV header */
		fprintf(g_stats_output,
		    "operation_count,shard_id,alloc_count,dealloc_count,active_bytes,"
		    "total_pageslabs,full_pageslabs_total,empty_pageslabs_total,hugified_pageslabs,"
		    "full_pageslabs_non_huge,full_pageslabs_huge,"
		    "empty_pageslabs_non_huge,empty_pageslabs_huge,"
		    "dirty_bytes,nhugifies,nhugify_failures,ndehugifies,"
		    "npurge_passes,npurges\n");
	}

	/* Load trace data and determine max number of arenas */
	pa_event_t *events;
	int         max_shard_id;
	size_t      event_count = load_trace_file(
            trace_file, &events, &max_shard_id);
	if (event_count == 0) {
		if (g_stats_output)
			fclose(g_stats_output);
		return 1;
	}

	int num_shards = max_shard_id + 1; /* shard IDs are 0-based */
	if (num_shards > MAX_ARENAS) {
		fprintf(stderr, "Error: Too many arenas required (%d > %d)\n",
		    num_shards, MAX_ARENAS);
		free(events);
		if (g_stats_output)
			fclose(g_stats_output);
		return 1;
	}

	/* Allocate allocation tracking array */
	g_alloc_records = malloc(event_count * sizeof(allocation_record_t));

	if (!g_alloc_records) {
		fprintf(
		    stderr, "Failed to allocate allocation tracking array\n");
		free(events);
		if (g_stats_output) {
			fclose(g_stats_output);
		}
		return 1;
	}

	/* Initialize PA infrastructure */
	if (initialize_pa_infrastructure(num_shards)) {
		fprintf(stderr, "Failed to initialize PA infrastructure\n");
		free(events);
		free(g_alloc_records);
		if (g_stats_output) {
			fclose(g_stats_output);
		}
		return 1;
	}

	/* Run simulation */
	simulate_trace(num_shards, events, event_count, stats_interval);

	/* Clean up remaining allocations */
	cleanup_remaining_allocations(num_shards);

	/* Cleanup PA infrastructure */
	cleanup_pa_infrastructure(num_shards);

	/* Cleanup */
	free(g_alloc_records);
	free(events);

	if (g_stats_output) {
		fclose(g_stats_output);
		printf("Statistics written to: %s\n", stats_output_file);
	}

	return 0;
}
