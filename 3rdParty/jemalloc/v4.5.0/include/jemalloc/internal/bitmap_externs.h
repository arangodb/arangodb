#ifndef JEMALLOC_INTERNAL_BITMAP_EXTERNS_H
#define JEMALLOC_INTERNAL_BITMAP_EXTERNS_H

void	bitmap_info_init(bitmap_info_t *binfo, size_t nbits);
void	bitmap_init(bitmap_t *bitmap, const bitmap_info_t *binfo);
size_t	bitmap_size(const bitmap_info_t *binfo);

#endif /* JEMALLOC_INTERNAL_BITMAP_EXTERNS_H */
