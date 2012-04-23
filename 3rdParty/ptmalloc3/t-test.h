/*
 * $Id: t-test.h,v 1.1 2004/11/04 14:32:21 wg Exp $
 * by Wolfram Gloger 1996.
 * Common data structures and functions for testing malloc performance.
 */

/* Testing level */
#ifndef TEST
#define TEST 0
#endif

/* For large allocation sizes, the time required by copying in
   realloc() can dwarf all other execution times.  Avoid this with a
   size threshold. */
#ifndef REALLOC_MAX
#define REALLOC_MAX	2000
#endif

struct bin {
	unsigned char *ptr;
	unsigned long size;
};

#if TEST > 0

static void
mem_init(unsigned char *ptr, unsigned long size)
{
	unsigned long i, j;

	if(size == 0) return;
	for(i=0; i<size; i+=2047) {
		j = (unsigned long)ptr ^ i;
		ptr[i] = ((j ^ (j>>8)) & 0xFF);
	}
	j = (unsigned long)ptr ^ (size-1);
	ptr[size-1] = ((j ^ (j>>8)) & 0xFF);
}

static int
mem_check(unsigned char *ptr, unsigned long size)
{
	unsigned long i, j;

	if(size == 0) return 0;
	for(i=0; i<size; i+=2047) {
		j = (unsigned long)ptr ^ i;
		if(ptr[i] != ((j ^ (j>>8)) & 0xFF)) return 1;
	}
	j = (unsigned long)ptr ^ (size-1);
	if(ptr[size-1] != ((j ^ (j>>8)) & 0xFF)) return 2;
	return 0;
}

static int
zero_check(unsigned* ptr, unsigned long size)
{
	unsigned char* ptr2;

	while(size >= sizeof(*ptr)) {
		if(*ptr++ != 0)
			return -1;
		size -= sizeof(*ptr);
	}
	ptr2 = (unsigned char*)ptr;
	while(size > 0) {
		if(*ptr2++ != 0)
			return -1;
		--size;
	}
	return 0;
}

#endif /* TEST > 0 */

/* Allocate a bin with malloc(), realloc() or memalign().  r must be a
   random number >= 1024. */

static void
bin_alloc(struct bin *m, unsigned long size, int r)
{
#if TEST > 0
	if(mem_check(m->ptr, m->size)) {
		printf("memory corrupt!\n");
		exit(1);
	}
#endif
	r %= 1024;
	/*printf("%d ", r);*/
	if(r < 4) { /* memalign */
		if(m->size > 0) free(m->ptr);
		m->ptr = (unsigned char *)memalign(sizeof(int) << r, size);
	} else if(r < 20) { /* calloc */
		if(m->size > 0) free(m->ptr);
		m->ptr = (unsigned char *)calloc(size, 1);
#if TEST > 0
		if(zero_check((unsigned*)m->ptr, size)) {
			long i;
			for(i=0; i<size; i++)
				if(m->ptr[i] != 0)
					break;
			printf("calloc'ed memory non-zero (ptr=%p, i=%ld)!\n", m->ptr, i);
			exit(1);
		}
#endif
	} else if(r < 100 && m->size < REALLOC_MAX) { /* realloc */
		if(m->size == 0) m->ptr = NULL;
		m->ptr = realloc(m->ptr, size);
	} else { /* plain malloc */
		if(m->size > 0) free(m->ptr);
		m->ptr = (unsigned char *)malloc(size);
	}
	if(!m->ptr) {
		printf("out of memory (r=%d, size=%ld)!\n", r, (long)size);
		exit(1);
	}
	m->size = size;
#if TEST > 0
	mem_init(m->ptr, m->size);
#endif
}

/* Free a bin. */

static void
bin_free(struct bin *m)
{
	if(m->size == 0) return;
#if TEST > 0
	if(mem_check(m->ptr, m->size)) {
		printf("memory corrupt!\n");
		exit(1);
	}
#endif
	free(m->ptr);
	m->size = 0;
}

/*
 * Local variables:
 * tab-width: 4
 * End:
 */
