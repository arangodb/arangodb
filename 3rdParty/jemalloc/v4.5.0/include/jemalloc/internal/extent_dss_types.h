#ifndef JEMALLOC_INTERNAL_EXTENT_DSS_TYPES_H
#define JEMALLOC_INTERNAL_EXTENT_DSS_TYPES_H

typedef enum {
	dss_prec_disabled  = 0,
	dss_prec_primary   = 1,
	dss_prec_secondary = 2,

	dss_prec_limit     = 3
} dss_prec_t;
#define DSS_PREC_DEFAULT	dss_prec_secondary
#define DSS_DEFAULT		"secondary"

#endif /* JEMALLOC_INTERNAL_EXTENT_DSS_TYPES_H */
