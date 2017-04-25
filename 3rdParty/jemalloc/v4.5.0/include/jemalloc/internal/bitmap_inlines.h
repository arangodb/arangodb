#ifndef JEMALLOC_INTERNAL_BITMAP_INLINES_H
#define JEMALLOC_INTERNAL_BITMAP_INLINES_H

#ifndef JEMALLOC_ENABLE_INLINE
bool	bitmap_full(bitmap_t *bitmap, const bitmap_info_t *binfo);
bool	bitmap_get(bitmap_t *bitmap, const bitmap_info_t *binfo, size_t bit);
void	bitmap_set(bitmap_t *bitmap, const bitmap_info_t *binfo, size_t bit);
size_t	bitmap_sfu(bitmap_t *bitmap, const bitmap_info_t *binfo);
void	bitmap_unset(bitmap_t *bitmap, const bitmap_info_t *binfo, size_t bit);
#endif

#if (defined(JEMALLOC_ENABLE_INLINE) || defined(JEMALLOC_BITMAP_C_))
JEMALLOC_INLINE bool
bitmap_full(bitmap_t *bitmap, const bitmap_info_t *binfo) {
#ifdef BITMAP_USE_TREE
	size_t rgoff = binfo->levels[binfo->nlevels].group_offset - 1;
	bitmap_t rg = bitmap[rgoff];
	/* The bitmap is full iff the root group is 0. */
	return (rg == 0);
#else
	size_t i;

	for (i = 0; i < binfo->ngroups; i++) {
		if (bitmap[i] != 0) {
			return false;
		}
	}
	return true;
#endif
}

JEMALLOC_INLINE bool
bitmap_get(bitmap_t *bitmap, const bitmap_info_t *binfo, size_t bit) {
	size_t goff;
	bitmap_t g;

	assert(bit < binfo->nbits);
	goff = bit >> LG_BITMAP_GROUP_NBITS;
	g = bitmap[goff];
	return !(g & (ZU(1) << (bit & BITMAP_GROUP_NBITS_MASK)));
}

JEMALLOC_INLINE void
bitmap_set(bitmap_t *bitmap, const bitmap_info_t *binfo, size_t bit) {
	size_t goff;
	bitmap_t *gp;
	bitmap_t g;

	assert(bit < binfo->nbits);
	assert(!bitmap_get(bitmap, binfo, bit));
	goff = bit >> LG_BITMAP_GROUP_NBITS;
	gp = &bitmap[goff];
	g = *gp;
	assert(g & (ZU(1) << (bit & BITMAP_GROUP_NBITS_MASK)));
	g ^= ZU(1) << (bit & BITMAP_GROUP_NBITS_MASK);
	*gp = g;
	assert(bitmap_get(bitmap, binfo, bit));
#ifdef BITMAP_USE_TREE
	/* Propagate group state transitions up the tree. */
	if (g == 0) {
		unsigned i;
		for (i = 1; i < binfo->nlevels; i++) {
			bit = goff;
			goff = bit >> LG_BITMAP_GROUP_NBITS;
			gp = &bitmap[binfo->levels[i].group_offset + goff];
			g = *gp;
			assert(g & (ZU(1) << (bit & BITMAP_GROUP_NBITS_MASK)));
			g ^= ZU(1) << (bit & BITMAP_GROUP_NBITS_MASK);
			*gp = g;
			if (g != 0) {
				break;
			}
		}
	}
#endif
}

/* sfu: set first unset. */
JEMALLOC_INLINE size_t
bitmap_sfu(bitmap_t *bitmap, const bitmap_info_t *binfo) {
	size_t bit;
	bitmap_t g;
	unsigned i;

	assert(!bitmap_full(bitmap, binfo));

#ifdef BITMAP_USE_TREE
	i = binfo->nlevels - 1;
	g = bitmap[binfo->levels[i].group_offset];
	bit = ffs_lu(g) - 1;
	while (i > 0) {
		i--;
		g = bitmap[binfo->levels[i].group_offset + bit];
		bit = (bit << LG_BITMAP_GROUP_NBITS) + (ffs_lu(g) - 1);
	}
#else
	i = 0;
	g = bitmap[0];
	while ((bit = ffs_lu(g)) == 0) {
		i++;
		g = bitmap[i];
	}
	bit = (i << LG_BITMAP_GROUP_NBITS) + (bit - 1);
#endif
	bitmap_set(bitmap, binfo, bit);
	return bit;
}

JEMALLOC_INLINE void
bitmap_unset(bitmap_t *bitmap, const bitmap_info_t *binfo, size_t bit) {
	size_t goff;
	bitmap_t *gp;
	bitmap_t g;
	UNUSED bool propagate;

	assert(bit < binfo->nbits);
	assert(bitmap_get(bitmap, binfo, bit));
	goff = bit >> LG_BITMAP_GROUP_NBITS;
	gp = &bitmap[goff];
	g = *gp;
	propagate = (g == 0);
	assert((g & (ZU(1) << (bit & BITMAP_GROUP_NBITS_MASK))) == 0);
	g ^= ZU(1) << (bit & BITMAP_GROUP_NBITS_MASK);
	*gp = g;
	assert(!bitmap_get(bitmap, binfo, bit));
#ifdef BITMAP_USE_TREE
	/* Propagate group state transitions up the tree. */
	if (propagate) {
		unsigned i;
		for (i = 1; i < binfo->nlevels; i++) {
			bit = goff;
			goff = bit >> LG_BITMAP_GROUP_NBITS;
			gp = &bitmap[binfo->levels[i].group_offset + goff];
			g = *gp;
			propagate = (g == 0);
			assert((g & (ZU(1) << (bit & BITMAP_GROUP_NBITS_MASK)))
			    == 0);
			g ^= ZU(1) << (bit & BITMAP_GROUP_NBITS_MASK);
			*gp = g;
			if (!propagate) {
				break;
			}
		}
	}
#endif /* BITMAP_USE_TREE */
}

#endif

#endif /* JEMALLOC_INTERNAL_BITMAP_INLINES_H */
