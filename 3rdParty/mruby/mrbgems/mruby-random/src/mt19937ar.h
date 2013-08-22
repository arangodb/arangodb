/*
** mt19937ar.h - MT Random functions
**
** See Copyright Notice in mruby.h
*/

#define N 624

typedef struct {
  unsigned long mt[N];
  int mti;
  union {
    unsigned long gen_int;
    double gen_dbl;
  };
} mt_state;

void mrb_random_init_genrand(mt_state *, unsigned long);
unsigned long mrb_random_genrand_int32(mt_state *);
double mrb_random_genrand_real1(mt_state *t);

/* initializes mt[N] with a seed */
void init_genrand(unsigned long s);

/* initialize by an array with array-length */
/* init_key is the array for initializing keys */
/* key_length is its length */
/* slight change for C++, 2004/2/26 */
void init_by_array(unsigned long init_key[], int key_length);

/* generates a random number on [0,0xffffffff]-interval */
unsigned long genrand_int32(void);

/* generates a random number on [0,0x7fffffff]-interval */
long genrand_int31(void);

/* These real versions are due to Isaku Wada, 2002/01/09 added */
/* generates a random number on [0,1]-real-interval */
double genrand_real1(void);

/* generates a random number on [0,1)-real-interval */
double genrand_real2(void);

/* generates a random number on (0,1)-real-interval */
double genrand_real3(void);

/* generates a random number on [0,1) with 53-bit resolution*/
double genrand_res53(void);
