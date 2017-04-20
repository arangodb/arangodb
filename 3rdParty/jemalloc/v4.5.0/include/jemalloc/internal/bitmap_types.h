#ifndef JEMALLOC_INTERNAL_BITMAP_TYPES_H
#define JEMALLOC_INTERNAL_BITMAP_TYPES_H

/* Maximum bitmap bit count is 2^LG_BITMAP_MAXBITS. */
#define LG_BITMAP_MAXBITS	LG_SLAB_MAXREGS
#define BITMAP_MAXBITS		(ZU(1) << LG_BITMAP_MAXBITS)

typedef struct bitmap_level_s bitmap_level_t;
typedef struct bitmap_info_s bitmap_info_t;
typedef unsigned long bitmap_t;
#define LG_SIZEOF_BITMAP	LG_SIZEOF_LONG

/* Number of bits per group. */
#define LG_BITMAP_GROUP_NBITS		(LG_SIZEOF_BITMAP + 3)
#define BITMAP_GROUP_NBITS		(1U << LG_BITMAP_GROUP_NBITS)
#define BITMAP_GROUP_NBITS_MASK		(BITMAP_GROUP_NBITS-1)

/*
 * Do some analysis on how big the bitmap is before we use a tree.  For a brute
 * force linear search, if we would have to call ffs_lu() more than 2^3 times,
 * use a tree instead.
 */
#if LG_BITMAP_MAXBITS - LG_BITMAP_GROUP_NBITS > 3
#  define BITMAP_USE_TREE
#endif

/* Number of groups required to store a given number of bits. */
#define BITMAP_BITS2GROUPS(nbits)					\
    (((nbits) + BITMAP_GROUP_NBITS_MASK) >> LG_BITMAP_GROUP_NBITS)

/*
 * Number of groups required at a particular level for a given number of bits.
 */
#define BITMAP_GROUPS_L0(nbits)						\
    BITMAP_BITS2GROUPS(nbits)
#define BITMAP_GROUPS_L1(nbits)						\
    BITMAP_BITS2GROUPS(BITMAP_BITS2GROUPS(nbits))
#define BITMAP_GROUPS_L2(nbits)						\
    BITMAP_BITS2GROUPS(BITMAP_BITS2GROUPS(BITMAP_BITS2GROUPS((nbits))))
#define BITMAP_GROUPS_L3(nbits)						\
    BITMAP_BITS2GROUPS(BITMAP_BITS2GROUPS(BITMAP_BITS2GROUPS(		\
	BITMAP_BITS2GROUPS((nbits)))))
#define BITMAP_GROUPS_L4(nbits)						\
    BITMAP_BITS2GROUPS(BITMAP_BITS2GROUPS(BITMAP_BITS2GROUPS(		\
	BITMAP_BITS2GROUPS(BITMAP_BITS2GROUPS((nbits))))))

/*
 * Assuming the number of levels, number of groups required for a given number
 * of bits.
 */
#define BITMAP_GROUPS_1_LEVEL(nbits)					\
    BITMAP_GROUPS_L0(nbits)
#define BITMAP_GROUPS_2_LEVEL(nbits)					\
    (BITMAP_GROUPS_1_LEVEL(nbits) + BITMAP_GROUPS_L1(nbits))
#define BITMAP_GROUPS_3_LEVEL(nbits)					\
    (BITMAP_GROUPS_2_LEVEL(nbits) + BITMAP_GROUPS_L2(nbits))
#define BITMAP_GROUPS_4_LEVEL(nbits)					\
    (BITMAP_GROUPS_3_LEVEL(nbits) + BITMAP_GROUPS_L3(nbits))
#define BITMAP_GROUPS_5_LEVEL(nbits)					\
    (BITMAP_GROUPS_4_LEVEL(nbits) + BITMAP_GROUPS_L4(nbits))

/*
 * Maximum number of groups required to support LG_BITMAP_MAXBITS.
 */
#ifdef BITMAP_USE_TREE

#if LG_BITMAP_MAXBITS <= LG_BITMAP_GROUP_NBITS
#  define BITMAP_GROUPS_MAX	BITMAP_GROUPS_1_LEVEL(BITMAP_MAXBITS)
#elif LG_BITMAP_MAXBITS <= LG_BITMAP_GROUP_NBITS * 2
#  define BITMAP_GROUPS_MAX	BITMAP_GROUPS_2_LEVEL(BITMAP_MAXBITS)
#elif LG_BITMAP_MAXBITS <= LG_BITMAP_GROUP_NBITS * 3
#  define BITMAP_GROUPS_MAX	BITMAP_GROUPS_3_LEVEL(BITMAP_MAXBITS)
#elif LG_BITMAP_MAXBITS <= LG_BITMAP_GROUP_NBITS * 4
#  define BITMAP_GROUPS_MAX	BITMAP_GROUPS_4_LEVEL(BITMAP_MAXBITS)
#elif LG_BITMAP_MAXBITS <= LG_BITMAP_GROUP_NBITS * 5
#  define BITMAP_GROUPS_MAX	BITMAP_GROUPS_5_LEVEL(BITMAP_MAXBITS)
#else
#  error "Unsupported bitmap size"
#endif

/*
 * Maximum number of levels possible.  This could be statically computed based
 * on LG_BITMAP_MAXBITS:
 *
 * #define BITMAP_MAX_LEVELS \
 *     (LG_BITMAP_MAXBITS / LG_SIZEOF_BITMAP) \
 *     + !!(LG_BITMAP_MAXBITS % LG_SIZEOF_BITMAP)
 *
 * However, that would not allow the generic BITMAP_INFO_INITIALIZER() macro, so
 * instead hardcode BITMAP_MAX_LEVELS to the largest number supported by the
 * various cascading macros.  The only additional cost this incurs is some
 * unused trailing entries in bitmap_info_t structures; the bitmaps themselves
 * are not impacted.
 */
#define BITMAP_MAX_LEVELS	5

#define BITMAP_INFO_INITIALIZER(nbits) {				\
	/* nbits. */							\
	nbits,								\
	/* nlevels. */							\
	(BITMAP_GROUPS_L0(nbits) > BITMAP_GROUPS_L1(nbits)) +		\
	    (BITMAP_GROUPS_L1(nbits) > BITMAP_GROUPS_L2(nbits)) +	\
	    (BITMAP_GROUPS_L2(nbits) > BITMAP_GROUPS_L3(nbits)) +	\
	    (BITMAP_GROUPS_L3(nbits) > BITMAP_GROUPS_L4(nbits)) + 1,	\
	/* levels. */							\
	{								\
		{0},							\
		{BITMAP_GROUPS_L0(nbits)},				\
		{BITMAP_GROUPS_L1(nbits) + BITMAP_GROUPS_L0(nbits)},	\
		{BITMAP_GROUPS_L2(nbits) + BITMAP_GROUPS_L1(nbits) +	\
		    BITMAP_GROUPS_L0(nbits)},				\
		{BITMAP_GROUPS_L3(nbits) + BITMAP_GROUPS_L2(nbits) +	\
		    BITMAP_GROUPS_L1(nbits) + BITMAP_GROUPS_L0(nbits)},	\
		{BITMAP_GROUPS_L4(nbits) + BITMAP_GROUPS_L3(nbits) +	\
		     BITMAP_GROUPS_L2(nbits) + BITMAP_GROUPS_L1(nbits)	\
		     + BITMAP_GROUPS_L0(nbits)}				\
	}								\
}

#else /* BITMAP_USE_TREE */

#define BITMAP_GROUPS_MAX	BITMAP_BITS2GROUPS(BITMAP_MAXBITS)

#define BITMAP_INFO_INITIALIZER(nbits) {				\
	/* nbits. */							\
	nbits,								\
	/* ngroups. */							\
	BITMAP_BITS2GROUPS(nbits)					\
}

#endif /* BITMAP_USE_TREE */

#endif /* JEMALLOC_INTERNAL_BITMAP_TYPES_H */
