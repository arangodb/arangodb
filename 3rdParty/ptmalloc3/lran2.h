/* lran2.h
 * by Wolfram Gloger 1996.
 *
 * A small, portable pseudo-random number generator.
 */

#ifndef _LRAN2_H
#define _LRAN2_H

#define LRAN2_MAX 714025l /* constants for portable */
#define IA	  1366l	  /* random number generator */
#define IC	  150889l /* (see e.g. `Numerical Recipes') */

struct lran2_st {
    long x, y, v[97];
};

static void
lran2_init(struct lran2_st* d, long seed)
{
    long x;
    int j;

    x = (IC - seed) % LRAN2_MAX;
    if(x < 0) x = -x;
    for(j=0; j<97; j++) {
	x = (IA*x + IC) % LRAN2_MAX;
	d->v[j] = x;
    }
    d->x = (IA*x + IC) % LRAN2_MAX;
    d->y = d->x;
}

#ifdef __GNUC__
__inline__
#endif
static long
lran2(struct lran2_st* d)
{
    int j = (d->y % 97);

    d->y = d->v[j];
    d->x = (IA*d->x + IC) % LRAN2_MAX;
    d->v[j] = d->x;
    return d->y;
}

#undef IA
#undef IC

#endif
