
#include <stdio.h>   /* printf etc */
#include <stdlib.h>  /* exit */
#include <string.h>  /* memmove */
#include "header.h"

typedef enum {
    e_token_omitted = 0,
    e_unexpected_token = 1,
    e_string_omitted = 2,
    e_unexpected_token_in_among = 3,
    /* For codes above here, report "after " t->previous_token after the error. */
    e_unresolved_substring = 14,
    e_not_allowed_inside_reverse = 15,
    e_empty_grouping = 16,
    e_already_backwards = 17,
    e_empty_among = 18,
    e_adjacent_bracketed_in_among = 19,
    e_substring_preceded_by_substring = 20,
    /* For codes below here, tokeniser->b is printed before the error. */
    e_redeclared = 30,
    e_undeclared = 31,
    e_declared_as_different_mode = 32,
    e_not_of_type_x = 33,
    e_not_of_type_string_or_integer = 34,
    e_misplaced = 35,
    e_redefined = 36,
    e_misused = 37
} error_code;

/* recursive usage: */

static void read_program_(struct analyser * a, int terminator);
static struct node * read_C(struct analyser * a);
static struct node * C_style(struct analyser * a, const char * s, int token);


static void print_node_(struct node * p, int n, const char * s) {

    int i;
    for (i = 0; i < n; i++) fputs(i == n - 1 ? s : "  ", stdout);
    printf("%s ", name_of_token(p->type));
    if (p->name) report_b(stdout, p->name->b);
    if (p->literalstring) {
        printf("'");
        report_b(stdout, p->literalstring);
        printf("'");
    }
    printf("\n");
    if (p->AE) print_node_(p->AE, n+1, "# ");
    if (p->left) print_node_(p->left, n+1, "  ");
    if (p->aux) print_node_(p->aux, n+1, "@ ");
    if (p->right) print_node_(p->right, n, "  ");
}

extern void print_program(struct analyser * a) {
    print_node_(a->program, 0, "  ");
}

static struct node * new_node(struct analyser * a, int type) {
    NEW(node, p);
    p->next = a->nodes; a->nodes = p;
    p->left = 0;
    p->right = 0;
    p->aux = 0;
    p->AE = 0;
    p->name = 0;
    p->literalstring = 0;
    p->mode = a->mode;
    p->line_number = a->tokeniser->line_number;
    p->type = type;
    return p;
}

static const char * name_of_mode(int n) {
    switch (n) {
        case m_backward: return "string backward";
        case m_forward:  return "string forward";
    /*  case m_integer:  return "integer";  */
    }
    fprintf(stderr, "Invalid mode %d in name_of_mode()\n", n);
    exit(1);
}

static const char * name_of_type(int n) {
    switch (n) {
        case 's': return "string";
        case 'i': return "integer";
        case 'r': return "routine";
        case 'R': return "routine or grouping";
        case 'g': return "grouping";
    }
    fprintf(stderr, "Invalid type %d in name_of_type()\n", n);
    exit(1);
}

static const char * name_of_name_type(int code) {
    switch (code) {
        case t_string: return "string";
        case t_boolean: return "boolean";
        case t_integer: return "integer";
        case t_routine: return "routine";
        case t_external: return "external";
        case t_grouping: return "grouping";
    }
    fprintf(stderr, "Invalid type code %d in name_of_name_type()\n", code);
    exit(1);
}

static void count_error(struct analyser * a) {
    struct tokeniser * t = a->tokeniser;
    if (t->error_count >= 20) { fprintf(stderr, "... etc\n"); exit(1); }
    t->error_count++;
}

static void error2(struct analyser * a, error_code n, int x) {
    struct tokeniser * t = a->tokeniser;
    count_error(a);
    fprintf(stderr, "%s:%d: ", t->file, t->line_number);
    if ((int)n >= (int)e_redeclared) report_b(stderr, t->b);
    switch (n) {
        case e_token_omitted:
            fprintf(stderr, "%s omitted", name_of_token(t->omission)); break;
        case e_unexpected_token_in_among:
            fprintf(stderr, "in among(...), ");
            /* fall through */
        case e_unexpected_token:
            fprintf(stderr, "unexpected %s", name_of_token(t->token));
            if (t->token == c_number) fprintf(stderr, " %d", t->number);
            if (t->token == c_name) {
                fprintf(stderr, " ");
                report_b(stderr, t->b);
            } break;
        case e_string_omitted:
            fprintf(stderr, "string omitted"); break;

        case e_unresolved_substring:
            fprintf(stderr, "unresolved substring on line %d", x); break;
        case e_not_allowed_inside_reverse:
            fprintf(stderr, "%s not allowed inside reverse(...)", name_of_token(t->token)); break;
        case e_empty_grouping:
            fprintf(stderr, "empty grouping"); break;
        case e_already_backwards:
            fprintf(stderr, "backwards used when already in this mode"); break;
        case e_empty_among:
            fprintf(stderr, "empty among(...)"); break;
        case e_adjacent_bracketed_in_among:
            fprintf(stderr, "two adjacent bracketed expressions in among(...)"); break;
        case e_substring_preceded_by_substring:
            fprintf(stderr, "substring preceded by another substring on line %d", x); break;

        case e_redeclared:
            fprintf(stderr, " re-declared"); break;
        case e_undeclared:
            fprintf(stderr, " undeclared"); break;
        case e_declared_as_different_mode:
            fprintf(stderr, " declared as %s mode; used as %s mode",
                            name_of_mode(a->mode), name_of_mode(x)); break;
        case e_not_of_type_x:
            fprintf(stderr, " not of type %s", name_of_type(x)); break;
        case e_not_of_type_string_or_integer:
            fprintf(stderr, " not of type string or integer"); break;
        case e_misplaced:
            fprintf(stderr, " misplaced"); break;
        case e_redefined:
            fprintf(stderr, " redefined"); break;
        case e_misused:
            fprintf(stderr, " mis-used as %s mode",
                            name_of_mode(x)); break;
    }
    if ((int)n < (int)e_unresolved_substring && t->previous_token > 0)
        fprintf(stderr, " after %s", name_of_token(t->previous_token));
    fprintf(stderr, "\n");
}

static void error(struct analyser * a, error_code n) { error2(a, n, 0); }

static void error4(struct analyser * a, struct name * q) {
    count_error(a);
    fprintf(stderr, "%s:%d: ", a->tokeniser->file, q->used->line_number);
    report_b(stderr, q->b);
    fprintf(stderr, " undefined\n");
}

static void omission_error(struct analyser * a, int n) {
    a->tokeniser->omission = n;
    error(a, e_token_omitted);
}

static int check_token(struct analyser * a, int code) {
    struct tokeniser * t = a->tokeniser;
    if (t->token != code) { omission_error(a, code); return false; }
    return true;
}

static int get_token(struct analyser * a, int code) {
    struct tokeniser * t = a->tokeniser;
    read_token(t);
    {
        int x = check_token(a, code);
        if (!x) t->token_held = true;
        return x;
    }
}

static struct name * look_for_name(struct analyser * a) {
    symbol * q = a->tokeniser->b;
    struct name * p;
    for (p = a->names; p; p = p->next) {
        symbol * b = p->b;
        int n = SIZE(b);
        if (n == SIZE(q) && memcmp(q, b, n * sizeof(symbol)) == 0) {
            p->referenced = true;
            return p;
        }
    }
    return 0;
}

static struct name * find_name(struct analyser * a) {
    struct name * p = look_for_name(a);
    if (p == 0) error(a, e_undeclared);
    return p;
}

static void check_routine_mode(struct analyser * a, struct name * p, int mode) {
    if (p->mode < 0) p->mode = mode; else
    if (p->mode != mode) error2(a, e_misused, mode);
}

static void check_name_type(struct analyser * a, struct name * p, int type) {
    switch (type) {
        case 's':
            if (p->type == t_string) return;
            break;
        case 'i':
            if (p->type == t_integer) return;
            break;
        case 'b':
            if (p->type == t_boolean) return;
            break;
        case 'R':
            if (p->type == t_grouping) return;
            /* FALLTHRU */
        case 'r':
            if (p->type == t_routine || p->type == t_external) return;
            break;
        case 'g':
            if (p->type == t_grouping) return;
            break;
    }
    error2(a, e_not_of_type_x, type);
}

static void read_names(struct analyser * a, int type) {
    struct tokeniser * t = a->tokeniser;
    if (!get_token(a, c_bra)) return;
    while (true) {
        int token = read_token(t);
        switch (token) {
            case c_len: {
                /* Context-sensitive token - once declared as a name, it loses
                 * its special meaning, for compatibility with older versions
                 * of snowball.
                 */
                static const symbol c_len_lit[] = {
                    'l', 'e', 'n'
                };
                MOVE_TO_B(t->b, c_len_lit);
                goto handle_as_name;
            }
            case c_lenof: {
                /* Context-sensitive token - once declared as a name, it loses
                 * its special meaning, for compatibility with older versions
                 * of snowball.
                 */
                static const symbol c_lenof_lit[] = {
                    'l', 'e', 'n', 'o', 'f'
                };
                MOVE_TO_B(t->b, c_lenof_lit);
                goto handle_as_name;
            }
            case c_name:
handle_as_name:
                if (look_for_name(a) != 0) error(a, e_redeclared); else {
                    NEW(name, p);
                    p->b = copy_b(t->b);
                    p->type = type;
                    p->mode = -1; /* routines, externals */
                    /* We defer assigning counts until after we've eliminated
                     * variables whose values are never used. */
                    p->count = -1;
                    p->referenced = false;
                    p->used_in_among = false;
                    p->used = 0;
                    p->value_used = false;
                    p->initialised = false;
                    p->used_in_definition = false;
                    p->local_to = 0;
                    p->grouping = 0;
                    p->definition = 0;
                    p->declaration_line_number = t->line_number;
                    p->next = a->names;
                    a->names = p;
                    if (token != c_name) {
                        disable_token(t, token);
                    }
                }
                break;
            default:
                if (!check_token(a, c_ket)) t->token_held = true;
                return;
        }
    }
}

static symbol * new_literalstring(struct analyser * a) {
    NEW(literalstring, p);
    p->b = copy_b(a->tokeniser->b);
    p->next = a->literalstrings;
    a->literalstrings = p;
    return p->b;
}

static int read_AE_test(struct analyser * a) {

    struct tokeniser * t = a->tokeniser;
    switch (read_token(t)) {
        case c_assign: return c_mathassign;
        case c_plusassign:
        case c_minusassign:
        case c_multiplyassign:
        case c_divideassign:
        case c_eq:
        case c_ne:
        case c_gr:
        case c_ge:
        case c_ls:
        case c_le: return t->token;
        default: error(a, e_unexpected_token); t->token_held = true; return c_eq;
    }
}

static int binding(int t) {
    switch (t) {
        case c_plus: case c_minus: return 1;
        case c_multiply: case c_divide: return 2;
        default: return -2;
    }
}

static void mark_used_in(struct analyser * a, struct name * q, struct node * p) {
    if (!q->used) {
        q->used = p;
        q->local_to = a->program_end->name;
    } else if (q->local_to) {
        if (q->local_to != a->program_end->name) {
            /* Used in more than one routine/external. */
            q->local_to = NULL;
        }
    }
}

static void name_to_node(struct analyser * a, struct node * p, int type) {
    struct name * q = find_name(a);
    if (q) {
        check_name_type(a, q, type);
        mark_used_in(a, q, p);
    }
    p->name = q;
}

static struct node * read_AE(struct analyser * a, int B) {
    struct tokeniser * t = a->tokeniser;
    struct node * p;
    struct node * q;
    switch (read_token(t)) {
        case c_minus: /* monadic */
            q = read_AE(a, 100);
            if (q->type == c_neg) {
                /* Optimise away double negation, which avoids generators
                 * having to worry about generating "--" (decrement operator
                 * in many languages).
                 */
                p = q->right;
                /* Don't free q, it's in the linked list a->nodes. */
                break;
            }
            p = new_node(a, c_neg);
            p->right = q;
            break;
        case c_bra:
            p = read_AE(a, 0);
            get_token(a, c_ket);
            break;
        case c_name:
            p = new_node(a, c_name);
            name_to_node(a, p, 'i');
            if (p->name) p->name->value_used = true;
            break;
        case c_maxint:
        case c_minint:
            a->int_limits_used = true;
            /* fall through */
        case c_cursor:
        case c_limit:
        case c_len:
        case c_size:
            p = new_node(a, t->token);
            break;
        case c_number:
            p = new_node(a, c_number);
            p->number = t->number;
            break;
        case c_lenof:
        case c_sizeof:
            p = C_style(a, "s", t->token);
            break;
        default:
            error(a, e_unexpected_token);
            t->token_held = true;
            return 0;
    }
    while (true) {
        int token = read_token(t);
        int b = binding(token);
        if (binding(token) <= B) {
            t->token_held = true;
            return p;
        }
        q = new_node(a, token);
        q->left = p;
        q->right = read_AE(a, b);
        p = q;
    }
}

static struct node * read_C_connection(struct analyser * a, struct node * q, int op) {
    struct tokeniser * t = a->tokeniser;
    struct node * p = new_node(a, op);
    struct node * p_end = q;
    p->left = q;
    do {
        q = read_C(a);
        p_end->right = q; p_end = q;
    } while (read_token(t) == op);
    t->token_held = true;
    return p;
}

static struct node * read_C_list(struct analyser * a) {
    struct tokeniser * t = a->tokeniser;
    struct node * p = new_node(a, c_bra);
    struct node * p_end = 0;
    while (true) {
        int token = read_token(t);
        if (token == c_ket) return p;
        if (token < 0) { omission_error(a, c_ket); return p; }
        t->token_held = true;
        {
            struct node * q = read_C(a);
            while (true) {
                token = read_token(t);
                if (token != c_and && token != c_or) {
                    t->token_held = true;
                    break;
                }
                q = read_C_connection(a, q, token);
            }
            if (p_end == 0) p->left = q; else p_end->right = q;
            p_end = q;
        }
    }
}

static struct node * C_style(struct analyser * a, const char * s, int token) {
    int i;
    struct node * p = new_node(a, token);
    for (i = 0; s[i] != 0; i++) switch (s[i]) {
        case 'C':
            p->left = read_C(a); continue;
        case 'D':
            p->aux = read_C(a); continue;
        case 'A':
            p->AE = read_AE(a, 0); continue;
        case 'f':
            get_token(a, c_for); continue;
        case 'S':
            {
                int str_token = read_token(a->tokeniser);
                if (str_token == c_name) name_to_node(a, p, 's'); else
                if (str_token == c_literalstring) p->literalstring = new_literalstring(a);
                else error(a, e_string_omitted);
            }
            continue;
        case 'b':
        case 's':
        case 'i':
            if (get_token(a, c_name)) name_to_node(a, p, s[i]);
            continue;
    }
    return p;
}

static struct node * read_literalstring(struct analyser * a) {
    struct node * p = new_node(a, c_literalstring);
    p->literalstring = new_literalstring(a);
    return p;
}

static void reverse_b(symbol * b) {
    int i = 0; int j = SIZE(b) - 1;
    while (i < j) {
        int ch1 = b[i]; int ch2 = b[j];
        b[i++] = ch2; b[j--] = ch1;
    }
}

static int compare_amongvec(const void *pv, const void *qv) {
    const struct amongvec * p = (const struct amongvec*)pv;
    const struct amongvec * q = (const struct amongvec*)qv;
    symbol * b_p = p->b; int p_size = p->size;
    symbol * b_q = q->b; int q_size = q->size;
    int smaller_size = p_size < q_size ? p_size : q_size;
    int i;
    for (i = 0; i < smaller_size; i++)
        if (b_p[i] != b_q[i]) return b_p[i] - b_q[i];
    if (p_size - q_size)
        return p_size - q_size;
    return p->line_number - q->line_number;
}

#define PTR_NULL_CHECK(P, Q) do {\
        if ((Q) == NULL) {\
            if ((P) != NULL) return 1;\
        } else {\
            if ((P) == NULL) return -1;\
        }\
    } while (0)

static int compare_node(const struct node *p, const struct node *q) {
    PTR_NULL_CHECK(p, q);
    if (q == NULL) {
        /* p must be NULL too. */
        return 0;
    }

    if (p->type != q->type) return p->type > q->type ? 1 : -1;
    if (p->mode != q->mode) return p->mode > q->mode ? 1 : -1;
    if (p->type == c_number) {
        if (p->number != q->number)
            return p->number > q->number ? 1 : -1;
    }

    PTR_NULL_CHECK(p->left, q->left);
    if (p->left) {
        int r = compare_node(p->left, q->left);
        if (r != 0) return r;
    }

    PTR_NULL_CHECK(p->AE, q->AE);
    if (p->AE) {
        int r = compare_node(p->AE, q->AE);
        if (r != 0) return r;
    }

    PTR_NULL_CHECK(p->aux, q->aux);
    if (p->aux) {
        int r = compare_node(p->aux, q->aux);
        if (r != 0) return r;
    }

    PTR_NULL_CHECK(p->name, q->name);
    if (p->name) {
        int r;
        if (SIZE(p->name->b) != SIZE(q->name->b)) {
            return SIZE(p->name->b) - SIZE(q->name->b);
        }
        r = memcmp(p->name->b, q->name->b,
                   SIZE(p->name->b) * sizeof(symbol));
        if (r != 0) return r;
    }

    PTR_NULL_CHECK(p->literalstring, q->literalstring);
    if (p->literalstring) {
        int r;
        if (SIZE(p->literalstring) != SIZE(q->literalstring)) {
            return SIZE(p->literalstring) - SIZE(q->literalstring);
        }
        r = memcmp(p->literalstring, q->literalstring,
                   SIZE(p->literalstring) * sizeof(symbol));
        if (r != 0) return r;
    }

    return compare_node(p->right, q->right);
}

static void make_among(struct analyser * a, struct node * p, struct node * substring) {

    NEW(among, x);
    NEWVEC(amongvec, v, p->number);
    struct node * q = p->left;
    struct amongvec * w0 = v;
    struct amongvec * w1 = v;
    int result = 1;

    int direction = substring != 0 ? substring->mode : p->mode;
    int backward = direction == m_backward;

    if (a->amongs == 0) a->amongs = x; else a->amongs_end->next = x;
    a->amongs_end = x;
    x->next = 0;
    x->b = v;
    x->number = a->among_count++;
    x->function_count = 0;
    x->starter = 0;
    x->nocommand_count = 0;
    x->amongvar_needed = false;

    if (q->type == c_bra) { x->starter = q; q = q->right; }

    while (q) {
        if (q->type == c_literalstring) {
            symbol * b = q->literalstring;
            w1->b = b;           /* pointer to case string */
            w1->action = NULL;   /* action gets filled in below */
            w1->line_number = q->line_number;
            w1->size = SIZE(b);  /* number of characters in string */
            w1->i = -1;          /* index of longest substring */
            w1->result = -1;     /* number of corresponding case expression */
            if (q->left) {
                struct name * function = q->left->name;
                w1->function = function;
                function->used_in_among = true;
                check_routine_mode(a, function, direction);
                x->function_count++;
            } else {
                w1->function = 0;
            }
            w1++;
        } else if (q->left == 0) {
            /* empty command: () */
            w0 = w1;
        } else {
            /* Check for previous action which is the same as this one and use
             * the same action code if we find one.
             */
            int among_result = -1;
            struct amongvec * w;
            for (w = v; w < w0; ++w) {
                if (w->action && compare_node(w->action->left, q->left) == 0) {
                    if (w->result <= 0) {
                        printf("Among code %d isn't positive\n", w->result);
                        exit(1);
                    }
                    among_result = w->result;
                    break;
                }
            }
            if (among_result < 0) {
                among_result = result++;
            }

            while (w0 != w1) {
                w0->action = q;
                w0->result = among_result;
                w0++;
            }
        }
        q = q->right;
    }
    if (w1-v != p->number) { fprintf(stderr, "oh! %d %d\n", (int)(w1-v), p->number); exit(1); }
    x->command_count = result - 1;
    {
        NEWVEC(node*, commands, x->command_count);
        memset(commands, 0, x->command_count * sizeof(struct node*));
        for (w0 = v; w0 < w1; w0++) {
            if (w0->result > 0) {
                /* result == -1 when there's no command. */
                if (w0->result > x->command_count) {
                    fprintf(stderr, "More among codes than expected\n");
                    exit(1);
                }
                if (!commands[w0->result - 1])
                    commands[w0->result - 1] = w0->action;
            } else {
                ++x->nocommand_count;
            }
            if (backward) reverse_b(w0->b);
        }
        x->commands = commands;
    }
    qsort(v, w1 - v, sizeof(struct amongvec), compare_amongvec);

    /* the following loop is O(n squared) */
    for (w0 = w1 - 1; w0 >= v; w0--) {
        symbol * b = w0->b;
        int size = w0->size;
        struct amongvec * w;

        for (w = w0 - 1; w >= v; w--) {
            if (w->size < size && memcmp(w->b, b, w->size * sizeof(symbol)) == 0) {
                w0->i = w - v;  /* fill in index of longest substring */
                break;
            }
        }
    }
    if (backward) for (w0 = v; w0 < w1; w0++) reverse_b(w0->b);

    for (w0 = v; w0 < w1 - 1; w0++)
        if (w0->size == (w0 + 1)->size &&
            memcmp(w0->b, (w0 + 1)->b, w0->size * sizeof(symbol)) == 0) {
            count_error(a);
            fprintf(stderr, "%s:%d: among(...) has repeated string '",
                    a->tokeniser->file, (w0 + 1)->line_number);
            report_b(stderr, (w0 + 1)->b);
            fprintf(stderr, "'\n");
            count_error(a);
            fprintf(stderr, "%s:%d: previously seen here\n",
                    a->tokeniser->file, w0->line_number);
        }

    x->literalstring_count = p->number;
    p->among = x;

    x->substring = substring;
    if (substring != 0) substring->among = x;
    if (x->command_count > 1 ||
        (x->command_count == 1 && x->nocommand_count > 0) ||
        x->starter != 0) {
        /* We need to set among_var rather than just checking if find_among*()
         * returns zero or not.
         */
        x->amongvar_needed = a->amongvar_needed = true;
    }
}

static struct node * read_among(struct analyser * a) {
    struct tokeniser * t = a->tokeniser;
    struct node * p = new_node(a, c_among);
    struct node * p_end = 0;
    int previous_token = -1;
    struct node * substring = a->substring;

    a->substring = 0;
    p->number = 0; /* counts the number of literals */
    if (!get_token(a, c_bra)) return p;
    while (true) {
        struct node * q;
        int token = read_token(t);
        switch (token) {
            case c_literalstring:
                q = read_literalstring(a);
                if (read_token(t) == c_name) {
                    struct node * r = new_node(a, c_name);
                    name_to_node(a, r, 'r');
                    q->left = r;
                }
                else t->token_held = true;
                p->number++; break;
            case c_bra:
                if (previous_token == c_bra) error(a, e_adjacent_bracketed_in_among);
                q = read_C_list(a); break;
            default:
                error(a, e_unexpected_token_in_among);
                previous_token = token;
                continue;
            case c_ket:
                if (p->number == 0) error(a, e_empty_among);
                if (t->error_count == 0) make_among(a, p, substring);
                return p;
        }
        previous_token = token;
        if (p_end == 0) p->left = q; else p_end->right = q;
        p_end = q;
    }
}

static struct node * read_substring(struct analyser * a) {

    struct node * p = new_node(a, c_substring);
    if (a->substring != 0) error2(a, e_substring_preceded_by_substring, a->substring->line_number);
    a->substring = p;
    return p;
}

static void check_modifyable(struct analyser * a) {
    if (!a->modifyable) error(a, e_not_allowed_inside_reverse);
}

static struct node * read_C(struct analyser * a) {
    struct tokeniser * t = a->tokeniser;
    int token = read_token(t);
    switch (token) {
        case c_bra:
            return read_C_list(a);
        case c_backwards:
            {
                int mode = a->mode;
                if (a->mode == m_backward) error(a, e_already_backwards); else a->mode = m_backward;
                {   struct node * p = C_style(a, "C", token);
                    a->mode = mode;
                    return p;
                }
            }
        case c_reverse:
            {
                int mode = a->mode;
                int modifyable = a->modifyable;
                a->modifyable = false;
                a->mode = mode == m_forward ? m_backward : m_forward;
                {
                    struct node * p = C_style(a, "C", token);
                    a->mode = mode;
                    a->modifyable = modifyable;
                    return p;
                }
            }
        case c_not:
        case c_try:
        case c_fail:
        case c_test:
        case c_do:
        case c_goto:
        case c_gopast:
        case c_repeat:
            return C_style(a, "C", token);
        case c_loop:
        case c_atleast:
            return C_style(a, "AC", token);
        case c_setmark: {
            struct node * n = C_style(a, "i", token);
            if (n->name) n->name->initialised = true;
            return n;
        }
        case c_tomark:
        case c_atmark:
        case c_hop:
            return C_style(a, "A", token);
        case c_delete:
            check_modifyable(a);
            /* fall through */
        case c_next:
        case c_tolimit:
        case c_atlimit:
        case c_leftslice:
        case c_rightslice:
        case c_true:
        case c_false:
        case c_debug:
            return new_node(a, token);
        case c_assignto:
        case c_sliceto: {
            struct node *n;
            check_modifyable(a);
            n = C_style(a, "s", token);
            if (n->name) n->name->initialised = true;
            return n;
        }
        case c_assign:
        case c_insert:
        case c_attach:
        case c_slicefrom: {
            struct node *n;
            check_modifyable(a);
            n = C_style(a, "S", token);
            if (n->name) n->name->value_used = true;
            return n;
        }
        case c_setlimit:
            return C_style(a, "CfD", token);
        case c_set:
        case c_unset: {
            struct node * n = C_style(a, "b", token);
            if (n->name) n->name->initialised = true;
            return n;
        }
        case c_dollar: {
            struct tokeniser * t = a->tokeniser;
            read_token(t);
            if (t->token == c_bra) {
                /* Handle newer $(AE REL_OP AE) syntax. */
                struct node * n = read_AE(a, 0);
                read_token(t);
                switch (t->token) {
                    case c_eq:
                    case c_ne:
                    case c_gr:
                    case c_ge:
                    case c_ls:
                    case c_le: {
                        struct node * lhs = n;
                        n = new_node(a, t->token);
                        n->left = lhs;
                        n->AE = read_AE(a, 0);
                        get_token(a, c_ket);
                        break;
                    }
                    default:
                        error(a, e_unexpected_token);
                        t->token_held = true;
                        break;
                }
                return n;
            }

            if (t->token == c_name) {
                struct node * p;
                struct name * q = find_name(a);
                int mode = a->mode;
                int modifyable = a->modifyable;
                if (q && q->type == t_string) {
                    /* Assume for now that $ on string both initialises and
                     * uses the string variable.  FIXME: Can we do better?
                     */
                    q->initialised = true;
                    q->value_used = true;
                    a->mode = m_forward;
                    a->modifyable = true;
                    p = new_node(a, c_dollar);
                    p->left = read_C(a);
                    p->name = q;
                } else {
                    if (q && q->type != t_integer) {
                        /* If $ is used on an unknown name or a name which
                         * isn't a string or an integer then we assume the
                         * unknown name is an integer as $ is used more often
                         * on integers than strings, so hopefully this it less
                         * likely to cause an error avalanche.
                         *
                         * For an unknown name, we'll already have reported an
                         * error.
                         */
                        error(a, e_not_of_type_string_or_integer);
                        q = NULL;
                    }
                    p = new_node(a, read_AE_test(a));
                    p->AE = read_AE(a, 0);

                    if (q) {
                        switch (p->type) {
                            case c_mathassign:
                                q->initialised = true;
                                p->name = q;
                                break;
                            default:
                                /* +=, etc don't "initialise" as they only
                                 * amend an existing value.  Similarly, they
                                 * don't count as using the value.
                                 */
                                p->name = q;
                                break;
                            case c_eq:
                            case c_ne:
                            case c_gr:
                            case c_ge:
                            case c_ls:
                            case c_le:
                                p->left = new_node(a, c_name);
                                p->left->name = q;
                                q->value_used = true;
                                break;
                        }
                    }
                }
                if (q) mark_used_in(a, q, p);
                a->mode = mode;
                a->modifyable = modifyable;
                return p;
            }

            error(a, e_unexpected_token);
            t->token_held = true;
            return new_node(a, c_dollar);
        }
        case c_name:
            {
                struct name * q = find_name(a);
                struct node * p = new_node(a, c_name);
                if (q) {
                    mark_used_in(a, q, p);
                    switch (q->type) {
                        case t_boolean:
                            p->type = c_booltest;
                            q->value_used = true;
                            break;
                        case t_integer:
                            error(a, e_misplaced); /* integer name misplaced */
                            break;
                        case t_string:
                            q->value_used = true;
                            break;
                        case t_routine:
                        case t_external:
                            p->type = c_call;
                            check_routine_mode(a, q, a->mode);
                            break;
                        case t_grouping:
                            p->type = c_grouping; break;
                    }
                }
                p->name = q;
                return p;
            }
        case c_non:
            {
                struct node * p = new_node(a, token);
                read_token(t);
                if (t->token == c_minus) read_token(t);
                if (!check_token(a, c_name)) { omission_error(a, c_name); return p; }
                name_to_node(a, p, 'g');
                return p;
            }
        case c_literalstring:
            return read_literalstring(a);
        case c_among: return read_among(a);
        case c_substring: return read_substring(a);
        default: error(a, e_unexpected_token); return 0;
    }
}

static int next_symbol(symbol * p, symbol * W, int utf8) {
    if (utf8) {
        int ch;
        int j = get_utf8(p, & ch);
        W[0] = ch; return j;
    } else {
        W[0] = p[0]; return 1;
    }
}

static symbol * alter_grouping(symbol * p, symbol * q, int style, int utf8) {
    int j = 0;
    symbol W[1];
    int width;
    if (style == c_plus) {
        while (j < SIZE(q)) {
            width = next_symbol(q + j, W, utf8);
            p = add_to_b(p, 1, W);
            j += width;
        }
    } else {
        while (j < SIZE(q)) {
            int i;
            width = next_symbol(q + j, W, utf8);
            for (i = 0; i < SIZE(p); i++) {
                if (p[i] == W[0]) {
                    memmove(p + i, p + i + 1, (SIZE(p) - i - 1) * sizeof(symbol));
                    SIZE(p)--;
                }
            }
            j += width;
        }
    }
    return p;
}

static void read_define_grouping(struct analyser * a, struct name * q) {
    struct tokeniser * t = a->tokeniser;
    int style = c_plus;
    {
        NEW(grouping, p);
        if (a->groupings == 0) a->groupings = p; else a->groupings_end->next = p;
        a->groupings_end = p;
        if (q) q->grouping = p;
        p->next = 0;
        p->name = q;
        p->line_number = a->tokeniser->line_number;
        p->b = create_b(0);
        while (true) {
            switch (read_token(t)) {
                case c_name:
                    {
                        struct name * r = find_name(a);
                        if (r) {
                            check_name_type(a, r, 'g');
                            p->b = alter_grouping(p->b, r->grouping->b, style, false);
                            r->used_in_definition = true;
                        }
                    }
                    break;
                case c_literalstring:
                    p->b = alter_grouping(p->b, t->b, style, (a->encoding == ENC_UTF8));
                    break;
                default: error(a, e_unexpected_token); return;
            }
            switch (read_token(t)) {
                case c_plus:
                case c_minus: style = t->token; break;
                default: goto label0;
            }
        }
    label0:
        {
            int i;
            int max = 0;
            int min = 1<<16;
            for (i = 0; i < SIZE(p->b); i++) {
                if (p->b[i] > max) max = p->b[i];
                if (p->b[i] < min) min = p->b[i];
            }
            p->largest_ch = max;
            p->smallest_ch = min;
            if (min == 1<<16) error(a, e_empty_grouping);
        }
        t->token_held = true; return;
    }
}

static void read_define_routine(struct analyser * a, struct name * q) {
    struct node * p = new_node(a, c_define);
    a->amongvar_needed = false;
    if (q) {
        check_name_type(a, q, 'R');
        if (q->definition != 0) error(a, e_redefined);
        if (q->mode < 0) q->mode = a->mode; else
        if (q->mode != a->mode) error2(a, e_declared_as_different_mode, q->mode);
    }
    p->name = q;
    if (a->program == 0) a->program = p; else a->program_end->right = p;
    a->program_end = p;
    get_token(a, c_as);
    p->left = read_C(a);
    if (q) q->definition = p->left;

    if (a->substring != 0) {
        error2(a, e_unresolved_substring, a->substring->line_number);
        a->substring = 0;
    }
    p->amongvar_needed = a->amongvar_needed;
}

static void read_define(struct analyser * a) {
    if (get_token(a, c_name)) {
        struct name * q = find_name(a);
        int type;
        if (q) {
            type = q->type;
        } else {
            /* No declaration, so sniff next token - if it is 'as' then parse
             * as a routine, otherwise as a grouping.
             */
            if (read_token(a->tokeniser) == c_as) {
                type = t_routine;
            } else {
                type = t_grouping;
            }
            a->tokeniser->token_held = true;
        }

        if (type == t_grouping) {
            read_define_grouping(a, q);
        } else {
            read_define_routine(a, q);
        }
    }
}

static void read_backwardmode(struct analyser * a) {
    int mode = a->mode;
    a->mode = m_backward;
    if (get_token(a, c_bra)) {
        read_program_(a, c_ket);
        check_token(a, c_ket);
    }
    a->mode = mode;
}

static void read_program_(struct analyser * a, int terminator) {
    struct tokeniser * t = a->tokeniser;
    while (true) {
        switch (read_token(t)) {
            case c_strings:     read_names(a, t_string); break;
            case c_booleans:    read_names(a, t_boolean); break;
            case c_integers:    read_names(a, t_integer); break;
            case c_routines:    read_names(a, t_routine); break;
            case c_externals:   read_names(a, t_external); break;
            case c_groupings:   read_names(a, t_grouping); break;
            case c_define:      read_define(a); break;
            case c_backwardmode:read_backwardmode(a); break;
            case c_ket:
                if (terminator == c_ket) return;
                /* fall through */
            default:
                error(a, e_unexpected_token); break;
            case -1:
                if (terminator >= 0) omission_error(a, c_ket);
                return;
        }
    }
}

static void remove_dead_assignments(struct node * p, struct name * q) {
    if (p->name == q) {
        switch (p->type) {
            case c_assignto:
            case c_sliceto:
            case c_mathassign:
            case c_plusassign:
            case c_minusassign:
            case c_multiplyassign:
            case c_divideassign:
            case c_setmark:
            case c_set:
            case c_unset:
            case c_dollar:
                /* c_true is a no-op. */
                p->type = c_true;
                break;
            default:
                /* There are no read accesses to this variable, so any
                 * references must be assignments.
                 */
                fprintf(stderr, "Unhandled type of dead assignment via %s\n",
                        name_of_token(p->type));
                exit(1);
        }
    }
    if (p->AE) remove_dead_assignments(p->AE, q);
    if (p->left) remove_dead_assignments(p->left, q);
    if (p->aux) remove_dead_assignments(p->aux, q);
    if (p->right) remove_dead_assignments(p->right, q);
}

extern void read_program(struct analyser * a) {
    read_program_(a, -1);
    {
        struct name * q = a->names;
        while (q) {
            switch (q->type) {
                case t_external: case t_routine:
                    if (q->used && q->definition == 0) error4(a, q);
                    break;
                case t_grouping:
                    if (q->used && q->grouping == 0) error4(a, q);
                    break;
            }
            q = q->next;
        }
    }

    if (a->tokeniser->error_count == 0) {
        struct name * q = a->names;
        struct name ** ptr = &(a->names);
        while (q) {
            if (!q->referenced) {
                fprintf(stderr, "%s:%d: warning: %s '",
                        a->tokeniser->file,
                        q->declaration_line_number,
                        name_of_name_type(q->type));
                report_b(stderr, q->b);
                if (q->type == t_routine ||
                    q->type == t_external ||
                    q->type == t_grouping) {
                    fprintf(stderr, "' declared but not defined\n");
                } else {
                    fprintf(stderr, "' defined but not used\n");
                    q = q->next;
                    *ptr = q;
                    continue;
                }
            } else if (q->type == t_routine || q->type == t_grouping) {
                /* It's OK to define a grouping but only use it to define other
                 * groupings.
                 */
                if (!q->used && !q->used_in_definition) {
                    int line_num;
                    if (q->type == t_routine) {
                        line_num = q->definition->line_number;
                    } else {
                        line_num = q->grouping->line_number;
                    }
                    fprintf(stderr, "%s:%d: warning: %s '",
                            a->tokeniser->file,
                            line_num,
                            name_of_name_type(q->type));
                    report_b(stderr, q->b);
                    fprintf(stderr, "' defined but not used\n");
                }
            } else if (q->type == t_external) {
                /* Unused is OK. */
            } else if (!q->initialised) {
                fprintf(stderr, "%s:%d: warning: %s '",
                        a->tokeniser->file,
                        q->declaration_line_number,
                        name_of_name_type(q->type));
                report_b(stderr, q->b);
                fprintf(stderr, "' is never initialised\n");
            } else if (!q->value_used) {
                fprintf(stderr, "%s:%d: warning: %s '",
                        a->tokeniser->file,
                        q->declaration_line_number,
                        name_of_name_type(q->type));
                report_b(stderr, q->b);
                fprintf(stderr, "' is set but never used\n");
                remove_dead_assignments(a->program, q);
                q = q->next;
                *ptr = q;
                continue;
            }
            ptr = &(q->next);
            q = q->next;
        }

        {
            /* Now we've eliminated variables whose values are never used we
             * can number the variables, which is used by some generators.
             */
            int * name_count = a->name_count;
            struct name * n;
            for (n = a->names; n; n = n->next) {
                n->count = name_count[n->type]++;
            }
        }
    }
}

extern struct analyser * create_analyser(struct tokeniser * t) {
    NEW(analyser, a);
    a->tokeniser = t;
    a->nodes = 0;
    a->names = 0;
    a->literalstrings = 0;
    a->program = 0;
    a->amongs = 0;
    a->among_count = 0;
    a->groupings = 0;
    a->mode = m_forward;
    a->modifyable = true;
    { int i; for (i = 0; i < t_size; i++) a->name_count[i] = 0; }
    a->substring = 0;
    a->int_limits_used = false;
    return a;
}

extern void close_analyser(struct analyser * a) {
    {
        struct node * q = a->nodes;
        while (q) {
            struct node * q_next = q->next;
            FREE(q);
            q = q_next;
        }
    }
    {
        struct name * q = a->names;
        while (q) {
            struct name * q_next = q->next;
            lose_b(q->b); FREE(q);
            q = q_next;
        }
    }
    {
        struct literalstring * q = a->literalstrings;
        while (q) {
            struct literalstring * q_next = q->next;
            lose_b(q->b); FREE(q);
            q = q_next;
        }
    }
    {
        struct among * q = a->amongs;
        while (q) {
            struct among * q_next = q->next;
            FREE(q->b);
            FREE(q->commands);
            FREE(q);
            q = q_next;
        }
    }
    {
        struct grouping * q = a->groupings;
        while (q) {
            struct grouping * q_next = q->next;
            lose_b(q->b); FREE(q);
            q = q_next;
        }
    }
    FREE(a);
}
