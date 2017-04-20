#ifndef JEMALLOC_INTERNAL_CKH_EXTERNS_H
#define JEMALLOC_INTERNAL_CKH_EXTERNS_H

bool	ckh_new(tsd_t *tsd, ckh_t *ckh, size_t minitems, ckh_hash_t *hash,
    ckh_keycomp_t *keycomp);
void	ckh_delete(tsd_t *tsd, ckh_t *ckh);
size_t	ckh_count(ckh_t *ckh);
bool	ckh_iter(ckh_t *ckh, size_t *tabind, void **key, void **data);
bool	ckh_insert(tsd_t *tsd, ckh_t *ckh, const void *key, const void *data);
bool	ckh_remove(tsd_t *tsd, ckh_t *ckh, const void *searchkey, void **key,
    void **data);
bool	ckh_search(ckh_t *ckh, const void *searchkey, void **key, void **data);
void	ckh_string_hash(const void *key, size_t r_hash[2]);
bool	ckh_string_keycomp(const void *k1, const void *k2);
void	ckh_pointer_hash(const void *key, size_t r_hash[2]);
bool	ckh_pointer_keycomp(const void *k1, const void *k2);

#endif /* JEMALLOC_INTERNAL_CKH_EXTERNS_H */
