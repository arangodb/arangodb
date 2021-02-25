#include <ctype.h>   /* for toupper etc */
#include <stdio.h>   /* for fprintf etc */
#include <stdlib.h>  /* for free etc */
#include <string.h>  /* for strcmp */
#include "header.h"

#define DEFAULT_JAVA_PACKAGE "org.tartarus.snowball.ext"
#define DEFAULT_JAVA_BASE_CLASS "org.tartarus.snowball.SnowballProgram"
#define DEFAULT_JAVA_AMONG_CLASS "org.tartarus.snowball.Among"
#define DEFAULT_JAVA_STRING_CLASS "java.lang.StringBuilder"

#define DEFAULT_GO_PACKAGE "snowball"
#define DEFAULT_GO_SNOWBALL_RUNTIME "github.com/snowballstem/snowball/go"

#define DEFAULT_CS_NAMESPACE "Snowball"
#define DEFAULT_CS_BASE_CLASS "Stemmer"
#define DEFAULT_CS_AMONG_CLASS "Among"
#define DEFAULT_CS_STRING_CLASS "StringBuilder"

#define DEFAULT_JS_BASE_CLASS "BaseStemmer"

#define DEFAULT_PYTHON_BASE_CLASS "BaseStemmer"

static int eq(const char * s1, const char * s2) {
    return strcmp(s1, s2) == 0;
}

static void print_arglist(int exit_code) {
    FILE * f = exit_code ? stderr : stdout;
    fprintf(f, "Usage: snowball SOURCE_FILE... [OPTIONS]\n\n"
               "Supported options:\n"
               "  -o[utput] file\n"
               "  -s[yntax]\n"
               "  -comments\n"
#ifndef DISABLE_JAVA
               "  -j[ava]\n"
#endif
#ifndef DISABLE_CSHARP
               "  -cs[harp]\n"
#endif
               "  -c++\n"
#ifndef DISABLE_PASCAL
               "  -pascal\n"
#endif
#ifndef DISABLE_PYTHON
               "  -py[thon]\n"
#endif
#ifndef DISABLE_JS
               "  -js\n"
#endif
#ifndef DISABLE_RUST
               "  -rust\n"
#endif
#ifndef DISABLE_GO
               "  -go\n"
#endif
               "  -w[idechars]\n"
               "  -u[tf8]\n"
               "  -n[ame] class name\n"
               "  -ep[refix] string\n"
               "  -vp[refix] string\n"
               "  -i[nclude] directory\n"
               "  -r[untime] path to runtime headers\n"
               "  -p[arentclassname] fully qualified parent class name\n"
#if !defined(DISABLE_JAVA) || !defined(DISABLE_CSHARP)
               "  -P[ackage] package name for stemmers\n"
               "  -S[tringclass] StringBuffer-compatible class\n"
               "  -a[mongclass] fully qualified name of the Among class\n"
#endif
#ifndef DISABLE_GO
               "  -gop[ackage] Go package name for stemmers\n"
               "  -gor[untime] Go snowball runtime package\n"
#endif
               "  --help        display this help and exit\n"
               "  --version     output version information and exit\n"
           );
    exit(exit_code);
}

static void check_lim(int i, int argc) {
    if (i >= argc) {
        fprintf(stderr, "argument list is one short\n");
        print_arglist(1);
    }
}

static FILE * get_output(symbol * b) {
    char * s = b_to_s(b);
    FILE * output = fopen(s, "w");
    if (output == 0) {
        fprintf(stderr, "Can't open output %s\n", s);
        exit(1);
    }
    free(s);
    return output;
}

static int read_options(struct options * o, int argc, char * argv[]) {
    char * s;
    int i = 1;
    int new_argc = 1;
    /* Note down the last option used to specify an explicit encoding so
     * we can warn we ignored it for languages with a fixed encoding.
     */
    const char * encoding_opt = NULL;

    /* set defaults: */

    o->output_file = 0;
    o->syntax_tree = false;
    o->comments = false;
    o->externals_prefix = NULL;
    o->variables_prefix = 0;
    o->runtime_path = 0;
    o->parent_class_name = NULL;
    o->string_class = NULL;
    o->among_class = NULL;
    o->package = NULL;
    o->go_snowball_runtime = DEFAULT_GO_SNOWBALL_RUNTIME;
    o->name = NULL;
    o->make_lang = LANG_C;
    o->includes = 0;
    o->includes_end = 0;
    o->encoding = ENC_SINGLEBYTE;

    /* read options: */

    while (i < argc) {
        s = argv[i++];
        if (s[0] != '-') {
            /* Non-option argument - shuffle down. */
            argv[new_argc++] = s;
            continue;
        }

        {
            if (eq(s, "-o") || eq(s, "-output")) {
               check_lim(i, argc);
                o->output_file = argv[i++];
                continue;
            }
            if (eq(s, "-n") || eq(s, "-name")) {
                /* Copy here so we do not free an argv item. */
                const char * name = NULL;
                char * new_name = NULL;
                size_t len = 0;

                check_lim(i, argc);
                name = argv[i++];

                len = strlen(name);
                new_name = malloc(len+1);
                memcpy(new_name, name, len);
                new_name[len] = '\0';

                o->name = new_name;
                continue;
            }
#ifndef DISABLE_JS
            if (eq(s, "-js")) {
                o->make_lang = LANG_JAVASCRIPT;
                continue;
            }
#endif
#ifndef DISABLE_RUST
            if (eq(s, "-rust")) {
                o->make_lang = LANG_RUST;
                continue;
            }
#endif
#ifndef DISABLE_GO
            if (eq(s, "-go")) {
                o->make_lang = LANG_GO;
                continue;
            }
#endif
#ifndef DISABLE_JAVA
            if (eq(s, "-j") || eq(s, "-java")) {
                o->make_lang = LANG_JAVA;
                continue;
            }
#endif
#ifndef DISABLE_CSHARP
            if (eq(s, "-cs") || eq(s, "-csharp")) {
                o->make_lang = LANG_CSHARP;
                continue;
            }
#endif
            if (eq(s, "-c++")) {
                o->make_lang = LANG_CPLUSPLUS;
                continue;
            }
#ifndef DISABLE_PASCAL
            if (eq(s, "-pascal")) {
                o->make_lang = LANG_PASCAL;
                continue;
            }
#endif
#ifndef DISABLE_PYTHON
            if (eq(s, "-py") || eq(s, "-python")) {
                o->make_lang = LANG_PYTHON;
                continue;
            }
#endif
            if (eq(s, "-w") || eq(s, "-widechars")) {
                encoding_opt = s;
                o->encoding = ENC_WIDECHARS;
                continue;
            }
            if (eq(s, "-s") || eq(s, "-syntax")) {
                o->syntax_tree = true;
                continue;
            }
            if (eq(s, "-comments")) {
                o->comments = true;
                continue;
            }
            if (eq(s, "-ep") || eq(s, "-eprefix")) {
                check_lim(i, argc);
                o->externals_prefix = argv[i++];
                continue;
            }
            if (eq(s, "-vp") || eq(s, "-vprefix")) {
                check_lim(i, argc);
                o->variables_prefix = argv[i++];
                continue;
            }
            if (eq(s, "-i") || eq(s, "-include")) {
                check_lim(i, argc);

                {
                    NEW(include, p);
                    symbol * b = add_s_to_b(0, argv[i++]);
                    b = add_s_to_b(b, "/");
                    p->next = 0; p->b = b;

                    if (o->includes == 0) o->includes = p; else
                                          o->includes_end->next = p;
                    o->includes_end = p;
                }
                continue;
            }
            if (eq(s, "-r") || eq(s, "-runtime")) {
                check_lim(i, argc);
                o->runtime_path = argv[i++];
                continue;
            }
            if (eq(s, "-u") || eq(s, "-utf8")) {
                encoding_opt = s;
                o->encoding = ENC_UTF8;
                continue;
            }
            if (eq(s, "-p") || eq(s, "-parentclassname")) {
                check_lim(i, argc);
                o->parent_class_name = argv[i++];
                continue;
            }
#if !defined(DISABLE_JAVA) || !defined(DISABLE_CSHARP)
            if (eq(s, "-P") || eq(s, "-Package")) {
                check_lim(i, argc);
                o->package = argv[i++];
                continue;
            }
            if (eq(s, "-S") || eq(s, "-stringclass")) {
                check_lim(i, argc);
                o->string_class = argv[i++];
                continue;
            }
            if (eq(s, "-a") || eq(s, "-amongclass")) {
                check_lim(i, argc);
                o->among_class = argv[i++];
                continue;
            }
#endif
#ifndef DISABLE_GO
            if (eq(s, "-gop") || eq(s, "-gopackage")) {
                check_lim(i, argc);
                o->package = argv[i++];
                continue;
            }
            if (eq(s, "-gor") || eq(s, "-goruntime")) {
                check_lim(i, argc);
                o->go_snowball_runtime = argv[i++];
                continue;
            }
#endif
            if (eq(s, "--help")) {
                print_arglist(0);
            }

            if (eq(s, "--version")) {
                printf("Snowball compiler version " SNOWBALL_VERSION "\n");
                exit(0);
            }

            fprintf(stderr, "'%s' misplaced\n", s);
            print_arglist(1);
        }
    }
    if (new_argc == 1) {
        fprintf(stderr, "no source files specified\n");
        print_arglist(1);
    }
    argv[new_argc] = NULL;

    /* Set language-dependent defaults. */
    switch (o->make_lang) {
        case LANG_C:
        case LANG_CPLUSPLUS:
            encoding_opt = NULL;
            break;
        case LANG_CSHARP:
            o->encoding = ENC_WIDECHARS;
            if (!o->parent_class_name)
                o->parent_class_name = DEFAULT_CS_BASE_CLASS;
            if (!o->string_class)
                o->string_class = DEFAULT_CS_STRING_CLASS;
            if (!o->among_class)
                o->among_class = DEFAULT_CS_AMONG_CLASS;
            if (!o->package)
                o->package = DEFAULT_CS_NAMESPACE;
            break;
        case LANG_GO:
            o->encoding = ENC_UTF8;
            if (!o->package)
                o->package = DEFAULT_GO_PACKAGE;
            break;
        case LANG_JAVA:
            o->encoding = ENC_WIDECHARS;
            if (!o->parent_class_name)
                o->parent_class_name = DEFAULT_JAVA_BASE_CLASS;
            if (!o->string_class)
                o->string_class = DEFAULT_JAVA_STRING_CLASS;
            if (!o->among_class)
                o->among_class = DEFAULT_JAVA_AMONG_CLASS;
            if (!o->package)
                o->package = DEFAULT_JAVA_PACKAGE;
            break;
        case LANG_JAVASCRIPT:
            o->encoding = ENC_WIDECHARS;
            if (!o->parent_class_name)
                o->parent_class_name = DEFAULT_JS_BASE_CLASS;
            break;
        case LANG_PYTHON:
            o->encoding = ENC_WIDECHARS;
            if (!o->parent_class_name)
                o->parent_class_name = DEFAULT_PYTHON_BASE_CLASS;
            break;
        case LANG_RUST:
            o->encoding = ENC_UTF8;
            break;
        default:
            break;
    }

    if (encoding_opt) {
        fprintf(stderr, "warning: %s only meaningful for C and C++\n",
                encoding_opt);
    }

    if (o->make_lang != LANG_C && o->make_lang != LANG_CPLUSPLUS) {
        if (o->runtime_path) {
            fprintf(stderr, "warning: -r/-runtime only meaningful for C and C++\n");
        }
        if (o->externals_prefix) {
            fprintf(stderr, "warning: -ep/-eprefix only meaningful for C and C++\n");
        }
    }
    if (!o->externals_prefix) o->externals_prefix = "";

    if (!o->name && o->output_file) {
        /* Default class name to basename of output_file - this is the standard
         * convention for at least Java and C#.
         */
        const char * slash = strrchr(o->output_file, '/');
        size_t len;
        const char * leaf = (slash == NULL) ? o->output_file : slash + 1;

        slash = strrchr(leaf, '\\');
        if (slash != NULL) leaf = slash + 1;

        {
            const char * dot = strchr(leaf, '.');
            len = (dot == NULL) ? strlen(leaf) : (size_t)(dot - leaf);
        }

        {
            char * new_name = malloc(len + 1);
            switch (o->make_lang) {
                case LANG_CSHARP:
                case LANG_PASCAL:
                    /* Upper case initial letter. */
                    memcpy(new_name, leaf, len);
                    new_name[0] = toupper(new_name[0]);
                    break;
                case LANG_JAVASCRIPT:
                case LANG_PYTHON: {
                    /* Upper case initial letter and change each
                     * underscore+letter or hyphen+letter to an upper case
                     * letter.
                     */
                    size_t i, j = 0;
                    int uc_next = true;
                    for (i = 0; i != len; ++i) {
                        unsigned char ch = leaf[i];
                        if (ch == '_' || ch == '-') {
                            uc_next = true;
                        } else {
                            if (uc_next) {
                                new_name[j] = toupper(ch);
                                uc_next = false;
                            } else {
                                new_name[j] = ch;
                            }
                            ++j;
                        }
                    }
                    len = j;
                    break;
                }
                default:
                    /* Just copy. */
                    memcpy(new_name, leaf, len);
                    break;
            }
            new_name[len] = '\0';
            o->name = new_name;
        }
    }

    return new_argc;
}

extern int main(int argc, char * argv[]) {

    int i;
    NEW(options, o);
    argc = read_options(o, argc, argv);
    {
        char * file = argv[1];
        symbol * u = get_input(file);
        if (u == 0) {
            fprintf(stderr, "Can't open input %s\n", file);
            exit(1);
        }
        {
            struct tokeniser * t = create_tokeniser(u, file);
            struct analyser * a = create_analyser(t);
            struct input ** next_input_ptr = &(t->next);
            a->encoding = t->encoding = o->encoding;
            t->includes = o->includes;
            /* If multiple source files are specified, set up the others to be
             * read after the first in order, using the same mechanism as
             * 'get' uses. */
            for (i = 2; i != argc; ++i) {
                NEW(input, q);
                file = argv[i];
                u = get_input(file);
                if (u == 0) {
                    fprintf(stderr, "Can't open input %s\n", file);
                    exit(1);
                }
                q->p = u;
                q->c = 0;
                q->file = file;
                q->file_needs_freeing = false;
                q->line_number = 1;
                *next_input_ptr = q;
                next_input_ptr = &(q->next);
            }
            *next_input_ptr = NULL;
            read_program(a);
            if (t->error_count > 0) exit(1);
            if (o->syntax_tree) print_program(a);
            close_tokeniser(t);
            if (!o->syntax_tree) {
                struct generator * g;

                const char * s = o->output_file;
                if (!s) {
                    fprintf(stderr, "Please include the -o option\n");
                    print_arglist(1);
                }
                g = create_generator(a, o);
                if (o->make_lang == LANG_C || o->make_lang == LANG_CPLUSPLUS) {
                    symbol * b = add_s_to_b(0, s);
                    b = add_s_to_b(b, ".h");
                    o->output_h = get_output(b);
                    b[SIZE(b) - 1] = 'c';
                    if (o->make_lang == LANG_CPLUSPLUS) {
                        b = add_s_to_b(b, "c");
                    }
                    o->output_src = get_output(b);
                    lose_b(b);

                    generate_program_c(g);
                    fclose(o->output_src);
                    fclose(o->output_h);
                }
#ifndef DISABLE_JAVA
                if (o->make_lang == LANG_JAVA) {
                    symbol * b = add_s_to_b(0, s);
                    b = add_s_to_b(b, ".java");
                    o->output_src = get_output(b);
                    lose_b(b);
                    generate_program_java(g);
                    fclose(o->output_src);
                }
#endif
#ifndef DISABLE_PASCAL
                if (o->make_lang == LANG_PASCAL) {
                    symbol *b = add_s_to_b(0, s);
                    b = add_s_to_b(b, ".pas");
                    o->output_src = get_output(b);
                    lose_b(b);
                    generate_program_pascal(g);
                    fclose(o->output_src);
                }
#endif
#ifndef DISABLE_PYTHON
                if (o->make_lang == LANG_PYTHON) {
                    symbol * b = add_s_to_b(0, s);
                    b = add_s_to_b(b, ".py");
                    o->output_src = get_output(b);
                    lose_b(b);
                    generate_program_python(g);
                    fclose(o->output_src);
                }
#endif
#ifndef DISABLE_JS
                if (o->make_lang == LANG_JAVASCRIPT) {
                    symbol * b = add_s_to_b(0, s);
                    b = add_s_to_b(b, ".js");
                    o->output_src = get_output(b);
                    lose_b(b);
                    generate_program_js(g);
                    fclose(o->output_src);
                }
#endif
#ifndef DISABLE_CSHARP
                if (o->make_lang == LANG_CSHARP) {
                    symbol * b = add_s_to_b(0, s);
                    b = add_s_to_b(b, ".cs");
                    o->output_src = get_output(b);
                    lose_b(b);
                    generate_program_csharp(g);
                    fclose(o->output_src);
                }
#endif
#ifndef DISABLE_RUST
                if (o->make_lang == LANG_RUST) {
                    symbol * b = add_s_to_b(0, s);
                    b = add_s_to_b(b, ".rs");
                    o->output_src = get_output(b);
                    lose_b(b);
                    generate_program_rust(g);
                    fclose(o->output_src);
                }
#endif
#ifndef DISABLE_GO
                if (o->make_lang == LANG_GO) {
                    symbol * b = add_s_to_b(0, s);
                    b = add_s_to_b(b, ".go");
                    o->output_src = get_output(b);
                    lose_b(b);
                    generate_program_go(g);
                    fclose(o->output_src);
                }
#endif
                close_generator(g);
            }
            close_analyser(a);
        }
        lose_b(u);
    }
    {   struct include * p = o->includes;
        while (p) {
            struct include * q = p->next;
            lose_b(p->b); FREE(p); p = q;
        }
    }
    free((void*)o->name);
    FREE(o);
    if (space_count) fprintf(stderr, "%d blocks unfreed\n", space_count);
    return 0;
}
