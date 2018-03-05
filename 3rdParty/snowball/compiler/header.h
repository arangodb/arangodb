#include <stdio.h>

typedef unsigned char byte;
typedef unsigned short symbol;

#define true 1
#define false 0

#define MALLOC check_malloc
#define FREE check_free

#define NEW(type, p) struct type * p = (struct type *) MALLOC(sizeof(struct type))
#define NEWVEC(type, p, n) struct type * p = (struct type *) MALLOC(sizeof(struct type) * n)

#define STARTSIZE   10
#define SIZE(p)     ((int *)(p))[-1]
#define CAPACITY(p) ((int *)(p))[-2]

extern symbol * create_b(int n);
extern void report_b(FILE * out, const symbol * p);
extern void lose_b(symbol * p);
extern symbol * increase_capacity(symbol * p, int n);
extern symbol * move_to_b(symbol * p, int n, const symbol * q);
extern symbol * add_to_b(symbol * p, int n, const symbol * q);
extern symbol * copy_b(const symbol * p);
extern char * b_to_s(const symbol * p);
extern symbol * add_s_to_b(symbol * p, const char * s);

#define MOVE_TO_B(B, LIT) \
    move_to_b(B, sizeof(LIT) / sizeof(LIT[0]), LIT)

struct str; /* defined in space.c */

extern struct str * str_new(void);
extern void str_delete(struct str * str);
extern void str_append(struct str * str, struct str * add);
extern void str_append_ch(struct str * str, char add);
extern void str_append_b(struct str * str, symbol * q);
extern void str_append_string(struct str * str, const char * s);
extern void str_append_int(struct str * str, int i);
extern void str_clear(struct str * str);
extern void str_assign(struct str * str, char * s);
extern struct str * str_copy(struct str * old);
extern symbol * str_data(struct str * str);
extern int str_len(struct str * str);
extern int str_back(struct str *str);
extern int get_utf8(const symbol * p, int * slot);
extern int put_utf8(int ch, symbol * p);
extern void output_str(FILE * outfile, struct str * str);

typedef enum { ENC_SINGLEBYTE, ENC_UTF8, ENC_WIDECHARS } enc;

struct m_pair {

    struct m_pair * next;
    symbol * name;
    symbol * value;

};

/* struct input must be a prefix of struct tokeniser. */
struct input {

    struct input * next;
    symbol * p;
    int c;
    char * file;
    int line_number;

};

struct include {

    struct include * next;
    symbol * b;

};

enum token_codes {

#include "syswords2.h"

    c_mathassign,
    c_name,
    c_number,
    c_literalstring,
    c_neg,
    c_call,
    c_grouping,
    c_booltest,

    NUM_TOKEN_CODES
};

/* struct input must be a prefix of struct tokeniser. */
struct tokeniser {

    struct input * next;
    symbol * p;
    int c;
    char * file;
    int line_number;
    symbol * b;
    symbol * b2;
    int number;
    int m_start;
    int m_end;
    struct m_pair * m_pairs;
    int get_depth;
    int error_count;
    int token;
    int previous_token;
    byte token_held;
    enc encoding;

    int omission;
    struct include * includes;

    char token_disabled[NUM_TOKEN_CODES];
};

extern symbol * get_input(symbol * p, char ** p_file);
extern struct tokeniser * create_tokeniser(symbol * b, char * file);
extern int read_token(struct tokeniser * t);
extern const char * name_of_token(int code);
extern void disable_token(struct tokeniser * t, int code);
extern void close_tokeniser(struct tokeniser * t);

extern int space_count;
extern void * check_malloc(int n);
extern void check_free(void * p);

struct node;

struct name {

    struct name * next;
    symbol * b;
    int type;                   /* t_string etc */
    int mode;                   /*    )_  for routines, externals */
    struct node * definition;   /*    )                           */
    int count;                  /* 0, 1, 2 for each type */
    struct grouping * grouping; /* for grouping names */
    byte referenced;
    byte used_in_among;         /* Function used in among? */
    struct node * used;         /* First use, or NULL if not used */
    struct name * local_to;     /* Local to one routine/external */
    int declaration_line_number;/* Line number of declaration */

};

struct literalstring {

    struct literalstring * next;
    symbol * b;

};

struct amongvec {

    symbol * b;      /* the string giving the case */
    int size;        /* - and its size */
    struct node * p; /* the corresponding node for this string */
    int i;           /* the amongvec index of the longest substring of b */
    int result;      /* the numeric result for the case */
    struct name * function;

};

struct among {

    struct among * next;
    struct amongvec * b;      /* pointer to the amongvec */
    int number;               /* amongs are numbered 0, 1, 2 ... */
    int literalstring_count;  /* in this among */
    int command_count;        /* in this among */
    int function_count;       /* in this among */
    struct node * starter;    /* i.e. among( (starter) 'string' ... ) */
    struct node * substring;  /* i.e. substring ... among ( ... ) */
};

struct grouping {

    struct grouping * next;
    int number;               /* groupings are numbered 0, 1, 2 ... */
    symbol * b;               /* the characters of this group */
    int largest_ch;           /* character with max code */
    int smallest_ch;          /* character with min code */
    struct name * name;       /* so g->name->grouping == g */
    int line_number;
};

struct node {

    struct node * next;
    struct node * left;
    struct node * aux;     /* used in setlimit */
    struct among * among;  /* used in among */
    struct node * right;
    int type;
    int mode;
    struct node * AE;
    struct name * name;
    symbol * literalstring;
    int number;
    int line_number;
    int amongvar_needed;   /* used in routine definitions */
};

enum name_types {

    t_size = 6,

    t_string = 0, t_boolean = 1, t_integer = 2, t_routine = 3, t_external = 4,
    t_grouping = 5

/*  If this list is extended, adjust wvn in generator.c  */
};

/*  In name_count[i] below, remember that
    type   is
    ----+----
      0 |  string
      1 |  boolean
      2 |  integer
      3 |  routine
      4 |  external
      5 |  grouping
*/

struct analyser {

    struct tokeniser * tokeniser;
    struct node * nodes;
    struct name * names;
    struct literalstring * literalstrings;
    int mode;
    byte modifyable;          /* false inside reverse(...) */
    struct node * program;
    struct node * program_end;
    int name_count[t_size];   /* name_count[i] counts the number of names of type i */
    struct among * amongs;
    struct among * amongs_end;
    int among_count;
    int amongvar_needed;      /* used in reading routine definitions */
    struct grouping * groupings;
    struct grouping * groupings_end;
    struct node * substring;  /* pending 'substring' in current routine definition */
    enc encoding;
    byte int_limits_used;     /* are maxint or minint used? */
};

enum analyser_modes {

    m_forward = 0, m_backward /*, m_integer */

};

extern void print_program(struct analyser * a);
extern struct analyser * create_analyser(struct tokeniser * t);
extern void close_analyser(struct analyser * a);

extern void read_program(struct analyser * a);

struct generator {

    struct analyser * analyser;
    struct options * options;
    int unreachable;           /* 0 if code can be reached, 1 if current code
                                * is unreachable. */
    int var_number;            /* Number of next variable to use. */
    struct str * outbuf;       /* temporary str to store output */
    struct str * declarations; /* str storing variable declarations */
    int next_label;
#ifndef DISABLE_PYTHON
    int max_label;
#endif
    int margin;

    /* if > 0, keep_count to restore in case of a failure;
     * if < 0, the negated keep_count for the limit to restore in case of
     * failure. */
    int failure_keep_count;
#if !defined(DISABLE_JAVA) && !defined(DISABLE_JSX) && !defined(DISABLE_PYTHON)
    struct str * failure_str;  /* This is used by some generators instead of failure_keep_count */
#endif

    int label_used;     /* Keep track of whether the failure label is used. */
    int failure_label;
    int debug_count;
    int copy_from_count; /* count of calls to copy_from() */

    const char * S[10];  /* strings */
    symbol * B[10];      /* blocks */
    int I[10];           /* integers */
    struct name * V[5];  /* variables */
    symbol * L[5];       /* literals, used in formatted write */

    int line_count;      /* counts number of lines output */
    int line_labelled;   /* in ISO C, will need extra ';' if it is a block end */
    int literalstring_count;
    int keep_count;      /* used to number keep/restore pairs to avoid compiler warnings
                            about shadowed variables */
};

struct options {

    /* for the command line: */

    const char * output_file;
    const char * name;
    FILE * output_src;
    FILE * output_h;
    byte syntax_tree;
    enc encoding;
    enum { LANG_JAVA, LANG_C, LANG_CPLUSPLUS, LANG_PYTHON, LANG_JSX, LANG_RUST, LANG_GO } make_lang;
    const char * externals_prefix;
    const char * variables_prefix;
    const char * runtime_path;
    const char * parent_class_name;
    const char * package;
    const char * go_package;
    const char * go_snowball_runtime;
    const char * string_class;
    const char * among_class;
    struct include * includes;
    struct include * includes_end;
};

/* Generator functions common to several backends. */

extern struct generator * create_generator(struct analyser * a, struct options * o);
extern void close_generator(struct generator * g);

extern void write_char(struct generator * g, int ch);
extern void write_newline(struct generator * g);
extern void write_string(struct generator * g, const char * s);
extern void write_int(struct generator * g, int i);
extern void write_b(struct generator * g, symbol * b);
extern void write_str(struct generator * g, struct str * str);

extern int K_needed(struct generator * g, struct node * p);
extern int repeat_restore(struct generator * g, struct node * p);

/* Generator for C code. */
extern void generate_program_c(struct generator * g);

#ifndef DISABLE_JAVA
/* Generator for Java code. */
extern void generate_program_java(struct generator * g);
#endif

#ifndef DISABLE_PYTHON
/* Generator for Python code. */
extern void generate_program_python(struct generator * g);
#endif

#ifndef DISABLE_JSX
extern void generate_program_jsx(struct generator * g);
#endif

#ifndef DISABLE_RUST
extern void generate_program_rust(struct generator * g);
#endif

#ifndef DISABLE_GO
extern void generate_program_go(struct generator * g);
#endif
