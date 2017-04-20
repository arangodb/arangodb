#ifndef JEMALLOC_INTERNAL_ATOMIC_EXTERNS_H
#define JEMALLOC_INTERNAL_ATOMIC_EXTERNS_H

#if (LG_SIZEOF_PTR == 3 || LG_SIZEOF_INT == 3)
#define atomic_read_u64(p)	atomic_add_u64(p, 0)
#endif
#define atomic_read_u32(p)	atomic_add_u32(p, 0)
#define atomic_read_p(p)	atomic_add_p(p, NULL)
#define atomic_read_zu(p)	atomic_add_zu(p, 0)
#define atomic_read_u(p)	atomic_add_u(p, 0)

#endif /* JEMALLOC_INTERNAL_ATOMIC_EXTERNS_H */
