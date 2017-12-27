
#include <stdlib.h> /* for exit */
#include <string.h> /* for strlen */
#include <stdio.h> /* for fprintf etc */
#include "header.h"

/* prototypes */

static void generate(struct generator * g, struct node * p);
static void w(struct generator * g, const char * s);
static void writef(struct generator * g, const char * s, struct node * p);


enum special_labels {
    x_return = -1
};

static int new_label(struct generator * g) {

    return g->next_label++;
}

static struct str * vars_newname(struct generator * g) {

    struct str * output;
    g->var_number++;
    output = str_new();
    str_append_string(output, "v_");
    str_append_int(output, g->var_number);
    return output;
}


/* Write routines for items from the syntax tree */

static void write_varname(struct generator * g, struct name * p) {

    int ch = "SBIrxg"[p->type];
    if (p->type != t_external)
    {
        write_char(g, ch);
        write_char(g, '_');
    }
    str_append_b(g->outbuf, p->b);
}

static void write_varref(struct generator * g, struct name * p) {

    /* In java, references look just the same */
    write_varname(g, p);
}

static void write_hexdigit(struct generator * g, int n) {

    write_char(g, n < 10 ? n + '0' : n - 10 + 'A');
}

static void write_hex(struct generator * g, int ch) {

    write_string(g, "\\u");
    {
        int i;
        for (i = 12; i >= 0; i -= 4) write_hexdigit(g, ch >> i & 0xf);
    }
}

static void write_literal_string(struct generator * g, symbol * p) {

    int i;
    write_string(g, "\"");
    for (i = 0; i < SIZE(p); i++) {
        int ch = p[i];
        if (32 <= ch && ch < 127) {
            if (ch == '\"' || ch == '\\') write_string(g, "\\");
            write_char(g, ch);
        } else {
            write_hex(g, ch);
        }
    }
    write_string(g, "\"");
}

static void write_margin(struct generator * g) {

    int i;
    for (i = 0; i < g->margin; i++) write_string(g, "    ");
}

static void write_comment(struct generator * g, struct node * p) {

    write_margin(g);
    write_string(g, "// ");
    write_string(g, name_of_token(p->type));
    if (p->name != 0) {
        write_string(g, " ");
        str_append_b(g->outbuf, p->name->b);
    }
    write_string(g, ", line ");
    write_int(g, p->line_number);
    write_newline(g);
}

static void write_block_start(struct generator * g) {

    w(g, "~M{~+~N");
}

static void write_block_end(struct generator * g)    /* block end */ {

    w(g, "~-~M}~N");
}

static void write_savecursor(struct generator * g, struct node * p,
                             struct str * savevar) {

    g->B[0] = str_data(savevar);
    g->S[1] = "";
    if (p->mode != m_forward) g->S[1] = "limit - ";
    writef(g, "~Mint ~B0 = ~S1cursor;~N", p);
}

static void restore_string(struct node * p, struct str * out, struct str * savevar) {

    str_clear(out);
    str_append_string(out, "cursor = ");
    if (p->mode != m_forward) str_append_string(out, "limit - ");
    str_append(out, savevar);
    str_append_string(out, ";");
}

static void write_restorecursor(struct generator * g, struct node * p,
                                struct str * savevar) {

    struct str * temp = str_new();
    write_margin(g);
    restore_string(p, temp, savevar);
    write_str(g, temp);
    write_newline(g);
    str_delete(temp);
}

static void write_inc_cursor(struct generator * g, struct node * p) {

    write_margin(g);
    write_string(g, p->mode == m_forward ? "cursor++;" : "cursor--;");
    write_newline(g);
}

static void wsetlab_begin(struct generator * g, int n) {

    w(g, "~Mlab");
    write_int(g, n);
    w(g, ": do {~+~N");
}

static void wsetlab_end(struct generator * g) {

    w(g, "~-~M} while (false);~N");
}

static void wgotol(struct generator * g, int n) {

    write_margin(g);
    write_string(g, "break lab");
    write_int(g, n);
    write_string(g, ";");
    write_newline(g);
}

static void write_failure(struct generator * g) {

    if (str_len(g->failure_str) != 0) {
        write_margin(g);
        write_str(g, g->failure_str);
        write_newline(g);
    }
    write_margin(g);
    switch (g->failure_label)
    {
        case x_return:
            write_string(g, "return false;");
            g->unreachable = true;
            break;
        default:
            write_string(g, "break lab");
            write_int(g, g->failure_label);
            write_string(g, ";");
            g->unreachable = true;
    }
    write_newline(g);
}

static void write_failure_if(struct generator * g, char * s, struct node * p) {

    writef(g, "~Mif (", p);
    writef(g, s, p);
    writef(g, ")~N", p);
    write_block_start(g);
    write_failure(g);
    write_block_end(g);
    g->unreachable = false;
}

/* if at limit fail */
static void write_check_limit(struct generator * g, struct node * p) {

    if (p->mode == m_forward) {
        write_failure_if(g, "cursor >= limit", p);
    } else {
        write_failure_if(g, "cursor <= limit_backward", p);
    }
}

/* Formatted write. */
static void writef(struct generator * g, const char * input, struct node * p) {
    int i = 0;
    int l = strlen(input);

    while (i < l) {
        int ch = input[i++];
        if (ch != '~') {
            write_char(g, ch);
            continue;
        }
        switch (input[i++]) {
            default: write_char(g, input[i - 1]); continue;
            case 'C': write_comment(g, p); continue;
            case 'f': write_block_start(g);
                      write_failure(g);
                      g->unreachable = false;
                      write_block_end(g);
                      continue;
            case 'M': write_margin(g); continue;
            case 'N': write_newline(g); continue;
            case '{': write_block_start(g); continue;
            case '}': write_block_end(g); continue;
            case 'S': write_string(g, g->S[input[i++] - '0']); continue;
            case 'B': write_b(g, g->B[input[i++] - '0']); continue;
            case 'I': write_int(g, g->I[input[i++] - '0']); continue;
            case 'V': write_varref(g, g->V[input[i++] - '0']); continue;
            case 'W': write_varname(g, g->V[input[i++] - '0']); continue;
            case 'L': write_literal_string(g, g->L[input[i++] - '0']); continue;
            case '+': g->margin++; continue;
            case '-': g->margin--; continue;
            case 'n': write_string(g, g->options->name); continue;
        }
    }
}

static void w(struct generator * g, const char * s) {
    writef(g, s, 0);
}

static void generate_AE(struct generator * g, struct node * p) {
    const char * s;
    switch (p->type) {
        case c_name:
            write_varref(g, p->name); break;
        case c_number:
            write_int(g, p->number); break;
        case c_maxint:
            write_string(g, "Integer.MAX_VALUE"); break;
        case c_minint:
            write_string(g, "Integer.MIN_VALUE"); break;
        case c_neg:
            write_char(g, '-'); generate_AE(g, p->right); break;
        case c_multiply:
            s = " * "; goto label0;
        case c_plus:
            s = " + "; goto label0;
        case c_minus:
            s = " - "; goto label0;
        case c_divide:
            s = " / ";
        label0:
            write_char(g, '('); generate_AE(g, p->left);
            write_string(g, s); generate_AE(g, p->right); write_char(g, ')'); break;
        case c_cursor:
            w(g, "cursor"); break;
        case c_limit:
            w(g, p->mode == m_forward ? "limit" : "limit_backward"); break;
        case c_lenof: /* Same as sizeof() for Java. */
        case c_sizeof:
            g->V[0] = p->name;
            w(g, "~V0.length()");
            break;
        case c_len: /* Same as size() for Java. */
        case c_size:
            w(g, "current.length()");
            break;
    }
}

static void generate_bra(struct generator * g, struct node * p) {

    write_comment(g, p);
    p = p->left;
    while (p) {
        generate(g, p);
        p = p->right;
    }
}

static void generate_and(struct generator * g, struct node * p) {

    struct str * savevar = vars_newname(g);
    int keep_c = K_needed(g, p->left);

    write_comment(g, p);

    if (keep_c) write_savecursor(g, p, savevar);

    p = p->left;
    while (p) {
        generate(g, p);
        if (g->unreachable) break;
        if (keep_c && p->right != 0) write_restorecursor(g, p, savevar);
        p = p->right;
    }
    str_delete(savevar);
}

static void generate_or(struct generator * g, struct node * p) {

    struct str * savevar = vars_newname(g);
    int keep_c = K_needed(g, p->left);

    int a0 = g->failure_label;
    struct str * a1 = str_copy(g->failure_str);

    int out_lab = new_label(g);
    int end_unreachable = true;

    write_comment(g, p);
    wsetlab_begin(g, out_lab);

    if (keep_c) write_savecursor(g, p, savevar);

    p = p->left;
    str_clear(g->failure_str);

    if (p == 0) {
        /* p should never be 0 after an or: there should be at least two
         * sub nodes. */
        fprintf(stderr, "Error: \"or\" node without children nodes.");
        exit (1);
    }
    while (p->right != 0) {
        g->failure_label = new_label(g);
        wsetlab_begin(g, g->failure_label);
        generate(g, p);
        if (!g->unreachable) {
            wgotol(g, out_lab);
            end_unreachable = false;
        }
        wsetlab_end(g);
        g->unreachable = false;
        if (keep_c) write_restorecursor(g, p, savevar);
        p = p->right;
    }

    g->failure_label = a0;
    str_delete(g->failure_str);
    g->failure_str = a1;

    generate(g, p);
    wsetlab_end(g);
    g->unreachable = end_unreachable;
    str_delete(savevar);
}

static void generate_backwards(struct generator * g, struct node * p) {

    write_comment(g, p);
    writef(g, "~Mlimit_backward = cursor;~N"
              "~Mcursor = limit;~N", p);
    generate(g, p->left);
    w(g, "~Mcursor = limit_backward;~N");
}


static void generate_not(struct generator * g, struct node * p) {

    struct str * savevar = vars_newname(g);
    int keep_c = K_needed(g, p->left);

    int a0 = g->failure_label;
    struct str * a1 = str_copy(g->failure_str);

    write_comment(g, p);
    if (keep_c) {
        write_block_start(g);
        write_savecursor(g, p, savevar);
    }

    g->failure_label = new_label(g);
    str_clear(g->failure_str);

    wsetlab_begin(g, g->failure_label);

    generate(g, p->left);

    g->failure_label = a0;
    str_delete(g->failure_str);
    g->failure_str = a1;

    if (!g->unreachable) write_failure(g);

    wsetlab_end(g);
    g->unreachable = false;

    if (keep_c) write_restorecursor(g, p, savevar);
    if (keep_c) write_block_end(g);
    str_delete(savevar);
}


static void generate_try(struct generator * g, struct node * p) {

    struct str * savevar = vars_newname(g);
    int keep_c = K_needed(g, p->left);

    write_comment(g, p);
    if (keep_c) write_savecursor(g, p, savevar);

    g->failure_label = new_label(g);
    if (keep_c) restore_string(p, g->failure_str, savevar);

    wsetlab_begin(g, g->failure_label);
    generate(g, p->left);
    wsetlab_end(g);
    g->unreachable = false;

    str_delete(savevar);
}

static void generate_set(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 = true;~N", p);
}

static void generate_unset(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 = false;~N", p);
}

static void generate_fail(struct generator * g, struct node * p) {

    write_comment(g, p);
    generate(g, p->left);
    if (!g->unreachable) write_failure(g);
}

/* generate_test() also implements 'reverse' */

static void generate_test(struct generator * g, struct node * p) {

    struct str * savevar = vars_newname(g);
    int keep_c = K_needed(g, p->left);

    write_comment(g, p);

    if (keep_c) {
        write_savecursor(g, p, savevar);
    }

    generate(g, p->left);

    if (!g->unreachable) {
        if (keep_c) {
            write_restorecursor(g, p, savevar);
        }
    }
    str_delete(savevar);
}

static void generate_do(struct generator * g, struct node * p) {

    struct str * savevar = vars_newname(g);
    int keep_c = K_needed(g, p->left);
    write_comment(g, p);
    if (keep_c) write_savecursor(g, p, savevar);

    g->failure_label = new_label(g);
    str_clear(g->failure_str);

    wsetlab_begin(g, g->failure_label);
    generate(g, p->left);
    wsetlab_end(g);
    g->unreachable = false;

    if (keep_c) write_restorecursor(g, p, savevar);
    str_delete(savevar);
}

static void generate_GO(struct generator * g, struct node * p, int style) {

    int end_unreachable = false;
    struct str * savevar = vars_newname(g);
    int keep_c = style == 1 || repeat_restore(g, p->left);

    int a0 = g->failure_label;
    struct str * a1 = str_copy(g->failure_str);

    int golab = new_label(g);
    g->I[0] = golab;
    write_comment(g, p);
    w(g, "~Mgolab~I0: while(true)~N");
    w(g, "~{");

    if (keep_c) write_savecursor(g, p, savevar);

    g->failure_label = new_label(g);
    wsetlab_begin(g, g->failure_label);
    generate(g, p->left);

    if (g->unreachable) {
        /* Cannot break out of this loop: therefore the code after the
         * end of the loop is unreachable.*/
        end_unreachable = true;
    } else {
        /* include for goto; omit for gopast */
        if (style == 1) write_restorecursor(g, p, savevar);
        g->I[0] = golab;
        w(g, "~Mbreak golab~I0;~N");
    }
    g->unreachable = false;
    wsetlab_end(g);
    if (keep_c) write_restorecursor(g, p, savevar);

    g->failure_label = a0;
    str_delete(g->failure_str);
    g->failure_str = a1;

    write_check_limit(g, p);
    write_inc_cursor(g, p);
    write_block_end(g);
    str_delete(savevar);
    g->unreachable = end_unreachable;
}

static void generate_loop(struct generator * g, struct node * p) {

    struct str * loopvar = vars_newname(g);
    write_comment(g, p);
    g->B[0] = str_data(loopvar);
    w(g, "~Mfor (int ~B0 = ");
    generate_AE(g, p->AE);
    g->B[0] = str_data(loopvar);
    writef(g, "; ~B0 > 0; ~B0--)~N", p);
    writef(g, "~{", p);

    generate(g, p->left);

    w(g, "~}");
    str_delete(loopvar);
    g->unreachable = false;
}

static void generate_repeat(struct generator * g, struct node * p, struct str * loopvar) {

    struct str * savevar = vars_newname(g);
    int keep_c = repeat_restore(g, p->left);
    int replab = new_label(g);
    g->I[0] = replab;
    write_comment(g, p);
    writef(g, "~Mreplab~I0: while(true)~N~{", p);

    if (keep_c) write_savecursor(g, p, savevar);

    g->failure_label = new_label(g);
    str_clear(g->failure_str);
    wsetlab_begin(g, g->failure_label);
    generate(g, p->left);

    if (!g->unreachable) {
        if (loopvar != 0) {
            g->B[0] = str_data(loopvar);
            w(g, "~M~B0--;~N");
        }

        g->I[0] = replab;
        w(g, "~Mcontinue replab~I0;~N");
    }

    wsetlab_end(g);
    g->unreachable = false;

    if (keep_c) write_restorecursor(g, p, savevar);

    g->I[0] = replab;
    w(g, "~Mbreak replab~I0;~N~}");
    str_delete(savevar);
}

static void generate_atleast(struct generator * g, struct node * p) {

    struct str * loopvar = vars_newname(g);
    write_comment(g, p);
    w(g, "~{");
    g->B[0] = str_data(loopvar);
    w(g, "~Mint ~B0 = ");
    generate_AE(g, p->AE);
    w(g, ";~N");
    {
        int a0 = g->failure_label;
        struct str * a1 = str_copy(g->failure_str);

        generate_repeat(g, p, loopvar);

        g->failure_label = a0;
        str_delete(g->failure_str);
        g->failure_str = a1;
    }
    g->B[0] = str_data(loopvar);
    write_failure_if(g, "~B0 > 0", p);
    w(g, "~}");
    str_delete(loopvar);
}

static void generate_setmark(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 = cursor;~N", p);
}

static void generate_tomark(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? ">" : "<";

    w(g, "~Mif (cursor ~S0 "); generate_AE(g, p->AE); w(g, ")~N");
    write_block_start(g);
    write_failure(g);
    write_block_end(g);
    g->unreachable = false;
    w(g, "~Mcursor = "); generate_AE(g, p->AE); writef(g, ";~N", p);
}

static void generate_atmark(struct generator * g, struct node * p) {

    write_comment(g, p);
    w(g, "~Mif (cursor != "); generate_AE(g, p->AE); writef(g, ")~N", p);
    write_block_start(g);
    write_failure(g);
    write_block_end(g);
    g->unreachable = false;
}

static void generate_hop(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "+" : "-";

    w(g, "~{~Mint c = cursor ~S0 ");
    generate_AE(g, p->AE);
    w(g, ";~N");

    g->S[0] = p->mode == m_forward ? "0" : "limit_backward";

    write_failure_if(g, "~S0 > c || c > limit", p);
    writef(g, "~Mcursor = c;~N", p);
    writef(g, "~}", p);
}

static void generate_delete(struct generator * g, struct node * p) {

    write_comment(g, p);
    writef(g, "~Mslice_del();~N", p);
}


static void generate_next(struct generator * g, struct node * p) {

    write_comment(g, p);
    write_check_limit(g, p);
    write_inc_cursor(g, p);
}

static void generate_tolimit(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "limit" : "limit_backward";
    writef(g, "~Mcursor = ~S0;~N", p);
}

static void generate_atlimit(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "limit" : "limit_backward";
    g->S[1] = p->mode == m_forward ? "<" : ">";
    write_failure_if(g, "cursor ~S1 ~S0", p);
}

static void generate_leftslice(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "bra" : "ket";
    writef(g, "~M~S0 = cursor;~N", p);
}

static void generate_rightslice(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "ket" : "bra";
    writef(g, "~M~S0 = cursor;~N", p);
}

static void generate_assignto(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 = assign_to(~V0);~N", p);
}

static void generate_sliceto(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 = slice_to(~V0);~N", p);
}

static void generate_address(struct generator * g, struct node * p) {

    symbol * b = p->literalstring;
    if (b != 0) {
        write_literal_string(g, b);
    } else {
        write_varref(g, p->name);
    }
}

static void generate_insert(struct generator * g, struct node * p, int style) {

    int keep_c = style == c_attach;
    write_comment(g, p);
    if (p->mode == m_backward) keep_c = !keep_c;
    if (keep_c) w(g, "~{~Mint c = cursor;~N");
    writef(g, "~Minsert(cursor, cursor, ", p);
    generate_address(g, p);
    writef(g, ");~N", p);
    if (keep_c) w(g, "~Mcursor = c;~N~}");
}

static void generate_assignfrom(struct generator * g, struct node * p) {

    int keep_c = p->mode == m_forward; /* like 'attach' */

    write_comment(g, p);
    if (keep_c) writef(g, "~{~Mint c = cursor;~N", p);
    if (p->mode == m_forward) {
        writef(g, "~Minsert(cursor, limit, ", p);
    } else {
        writef(g, "~Minsert(limit_backward, cursor, ", p);
    }
    generate_address(g, p);
    writef(g, ");~N", p);
    if (keep_c) w(g, "~Mcursor = c;~N~}");
}


static void generate_slicefrom(struct generator * g, struct node * p) {

    write_comment(g, p);
    w(g, "~Mslice_from(");
    generate_address(g, p);
    writef(g, ");~N", p);
}

static void generate_setlimit(struct generator * g, struct node * p) {
    struct str * savevar = vars_newname(g);
    struct str * varname = vars_newname(g);
    write_comment(g, p);
    write_savecursor(g, p, savevar);
    generate(g, p->left);

    if (!g->unreachable) {
        g->B[0] = str_data(varname);
        if (p->mode == m_forward) {
            w(g, "~Mint ~B0 = limit - cursor;~N");
            w(g, "~Mlimit = cursor;~N");
        } else {
            w(g, "~Mint ~B0 = limit_backward;~N");
            w(g, "~Mlimit_backward = cursor;~N");
        }
        write_restorecursor(g, p, savevar);

        if (p->mode == m_forward) {
            str_assign(g->failure_str, "limit += ");
            str_append(g->failure_str, varname);
            str_append_ch(g->failure_str, ';');
        } else {
            str_assign(g->failure_str, "limit_backward = ");
            str_append(g->failure_str, varname);
            str_append_ch(g->failure_str, ';');
        }
        generate(g, p->aux);

        if (!g->unreachable) {
            write_margin(g);
            write_str(g, g->failure_str);
            write_newline(g);
        }
    }
    str_delete(varname);
    str_delete(savevar);
}

/* dollar sets snowball up to operate on a string variable as if it were the
 * current string */
static void generate_dollar(struct generator * g, struct node * p) {

    struct str * savevar = vars_newname(g);
    write_comment(g, p);
    g->V[0] = p->name;

    ++g->copy_from_count;
    str_assign(g->failure_str, "copy_from(");
    str_append(g->failure_str, savevar);
    str_append_string(g->failure_str, ");");
    g->B[0] = str_data(savevar);
    writef(g, "~{~M~n ~B0 = this;~N"
             "~Mcurrent = new StringBuffer(~V0.toString());~N"
             "~Mcursor = 0;~N"
             "~Mlimit = current.length();~N", p);
    generate(g, p->left);
    if (!g->unreachable) {
        write_margin(g);
        write_str(g, g->failure_str);
        write_newline(g);
    }
    w(g, "~}");
    str_delete(savevar);
}

static void generate_integer_assign(struct generator * g, struct node * p, char * s) {

    g->V[0] = p->name;
    g->S[0] = s;
    w(g, "~M~V0 ~S0 "); generate_AE(g, p->AE); w(g, ";~N");
}

static void generate_integer_test(struct generator * g, struct node * p, char * s) {

    g->V[0] = p->name;
    g->S[0] = s;
    w(g, "~Mif (!(~V0 ~S0 "); generate_AE(g, p->AE); w(g, "))~N");
    write_block_start(g);
    write_failure(g);
    write_block_end(g);
    g->unreachable = false;
}

static void generate_call(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    write_failure_if(g, "!~V0()", p);
}

static void generate_grouping(struct generator * g, struct node * p, int complement) {

    struct grouping * q = p->name->grouping;
    g->S[0] = p->mode == m_forward ? "" : "_b";
    g->S[1] = complement ? "out" : "in";
    g->V[0] = p->name;
    g->I[0] = q->smallest_ch;
    g->I[1] = q->largest_ch;
    write_failure_if(g, "!(~S1_grouping~S0(~V0, ~I0, ~I1))", p);
}

static void generate_namedstring(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "" : "_b";
    g->V[0] = p->name;
    write_failure_if(g, "!(eq_s~S0(~V0))", p);
}

static void generate_literalstring(struct generator * g, struct node * p) {
    symbol * b = p->literalstring;
    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "" : "_b";
    g->L[0] = b;
    write_failure_if(g, "!(eq_s~S0(~L0))", p);
}

static void generate_define(struct generator * g, struct node * p) {
    struct name * q = p->name;

    struct str * saved_output = g->outbuf;

    /* We currently make functions used in among public as this seems to
     * be required to allow the SnowballProgram base class to invoke them.
     * FIXME: Is this avoidable?
     */
    if (q->type == t_routine && !q->used_in_among) {
        g->S[0] = "private";
    } else {
        g->S[0] = "public";
    }
    g->V[0] = q;
    w(g, "~N~M~S0 boolean ~V0() {~+~N");

    g->outbuf = str_new();

    g->next_label = 0;
    g->var_number = 0;

    if (p->amongvar_needed) w(g, "~Mint among_var;~N");
    str_clear(g->failure_str);
    g->failure_label = x_return;
    g->unreachable = false;
    generate(g, p->left);
    if (!g->unreachable) w(g, "~Mreturn true;~N");
    w(g, "~}");

    str_append(saved_output, g->outbuf);
    str_delete(g->outbuf);
    g->outbuf = saved_output;
}

static void generate_substring(struct generator * g, struct node * p) {

    struct among * x = p->among;

    write_comment(g, p);

    g->S[0] = p->mode == m_forward ? "" : "_b";
    g->I[0] = x->number;

    if (x->command_count == 0 && x->starter == 0) {
        write_failure_if(g, "find_among~S0(a_~I0) == 0", p);
    } else {
        writef(g, "~Mamong_var = find_among~S0(a_~I0);~N", p);
        write_failure_if(g, "among_var == 0", p);
    }
}

static void generate_among(struct generator * g, struct node * p) {

    struct among * x = p->among;
    int case_number = 1;

    if (x->substring == 0) generate_substring(g, p);
    if (x->command_count == 0 && x->starter == 0) return;

    if (x->starter != 0) generate(g, x->starter);

    p = p->left;
    if (p != 0 && p->type != c_literalstring) p = p->right;
    w(g, "~Mswitch (among_var) {~N~+");
    w(g, "~Mcase 0:~N~+");
    write_failure(g);
    g->unreachable = false;
    w(g, "~-");

    while (p) {
        if (p->type == c_bra && p->left != 0) {
            g->I[0] = case_number++;
            w(g, "~Mcase ~I0:~N~+");
            generate(g, p);
            if (!g->unreachable) w(g, "~Mbreak;~N");
            w(g, "~-");
            g->unreachable = false;
        }
        p = p->right;
    }
    write_block_end(g);
}

static void generate_booltest(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    write_failure_if(g, "!(~V0)", p);
}

static void generate_false(struct generator * g, struct node * p) {

    write_comment(g, p);
    write_failure(g);
}

static void generate_debug(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->I[0] = g->debug_count++;
    g->I[1] = p->line_number;
    writef(g, "~Mdebug(~I0, ~I1);~N", p);
}

static void generate(struct generator * g, struct node * p) {

    int a0;
    struct str * a1;

    if (g->unreachable) return;

    a0 = g->failure_label;
    a1 = str_copy(g->failure_str);

    switch (p->type)
    {
        case c_define:        generate_define(g, p); break;
        case c_bra:           generate_bra(g, p); break;
        case c_and:           generate_and(g, p); break;
        case c_or:            generate_or(g, p); break;
        case c_backwards:     generate_backwards(g, p); break;
        case c_not:           generate_not(g, p); break;
        case c_set:           generate_set(g, p); break;
        case c_unset:         generate_unset(g, p); break;
        case c_try:           generate_try(g, p); break;
        case c_fail:          generate_fail(g, p); break;
        case c_reverse:
        case c_test:          generate_test(g, p); break;
        case c_do:            generate_do(g, p); break;
        case c_goto:          generate_GO(g, p, 1); break;
        case c_gopast:        generate_GO(g, p, 0); break;
        case c_repeat:        generate_repeat(g, p, 0); break;
        case c_loop:          generate_loop(g, p); break;
        case c_atleast:       generate_atleast(g, p); break;
        case c_setmark:       generate_setmark(g, p); break;
        case c_tomark:        generate_tomark(g, p); break;
        case c_atmark:        generate_atmark(g, p); break;
        case c_hop:           generate_hop(g, p); break;
        case c_delete:        generate_delete(g, p); break;
        case c_next:          generate_next(g, p); break;
        case c_tolimit:       generate_tolimit(g, p); break;
        case c_atlimit:       generate_atlimit(g, p); break;
        case c_leftslice:     generate_leftslice(g, p); break;
        case c_rightslice:    generate_rightslice(g, p); break;
        case c_assignto:      generate_assignto(g, p); break;
        case c_sliceto:       generate_sliceto(g, p); break;
        case c_assign:        generate_assignfrom(g, p); break;
        case c_insert:
        case c_attach:        generate_insert(g, p, p->type); break;
        case c_slicefrom:     generate_slicefrom(g, p); break;
        case c_setlimit:      generate_setlimit(g, p); break;
        case c_dollar:        generate_dollar(g, p); break;
        case c_mathassign:    generate_integer_assign(g, p, "="); break;
        case c_plusassign:    generate_integer_assign(g, p, "+="); break;
        case c_minusassign:   generate_integer_assign(g, p, "-="); break;
        case c_multiplyassign:generate_integer_assign(g, p, "*="); break;
        case c_divideassign:  generate_integer_assign(g, p, "/="); break;
        case c_eq:            generate_integer_test(g, p, "=="); break;
        case c_ne:            generate_integer_test(g, p, "!="); break;
        case c_gr:            generate_integer_test(g, p, ">"); break;
        case c_ge:            generate_integer_test(g, p, ">="); break;
        case c_ls:            generate_integer_test(g, p, "<"); break;
        case c_le:            generate_integer_test(g, p, "<="); break;
        case c_call:          generate_call(g, p); break;
        case c_grouping:      generate_grouping(g, p, false); break;
        case c_non:           generate_grouping(g, p, true); break;
        case c_name:          generate_namedstring(g, p); break;
        case c_literalstring: generate_literalstring(g, p); break;
        case c_among:         generate_among(g, p); break;
        case c_substring:     generate_substring(g, p); break;
        case c_booltest:      generate_booltest(g, p); break;
        case c_false:         generate_false(g, p); break;
        case c_true:          break;
        case c_debug:         generate_debug(g, p); break;
        default: fprintf(stderr, "%d encountered\n", p->type);
                 exit(1);
    }

    g->failure_label = a0;
    str_delete(g->failure_str);
    g->failure_str = a1;
}

static void generate_start_comment(struct generator * g) {

    w(g, "// This file was generated automatically by the Snowball to Java compiler~N");
    w(g, "// http://snowballstem.org/~N~N");
}

static void generate_class_begin(struct generator * g) {

    w(g, "package ");
    w(g, g->options->package);
    w(g, ";~N~N");

    w(g, "import ");
    w(g, g->options->among_class);
    w(g, ";~N"
         "~N"
         " /**~N"
         "  * This class was automatically generated by a Snowball to Java compiler~N"
         "  * It implements the stemming algorithm defined by a snowball script.~N"
         "  */~N"
         "~N"
         "public class ~n extends ");

    w(g, g->options->parent_class_name);
    w(g, " {~+~N"
         "~N"
         "~Mprivate static final long serialVersionUID = 1L;~N"
         "~N");
}

static void generate_class_end(struct generator * g) {

    w(g, "~N}");
    w(g, "~N~N");
}

static void generate_equals(struct generator * g) {

    w(g, "~N"
         "~Mpublic boolean equals( Object o ) {~N"
         "~+~Mreturn o instanceof ");
    w(g, g->options->name);
	 w(g, ";~N~-~M}~N"
	      "~N"
	      "~Mpublic int hashCode() {~N"
	      "~+~Mreturn ");
    w(g, g->options->name);
	 w(g, ".class.getName().hashCode();~N"
	      "~-~M}~N");
    w(g, "~N~N");
}

static void generate_among_table(struct generator * g, struct among * x) {

    struct amongvec * v = x->b;

    g->I[0] = x->number;

    w(g, "~Mprivate final static Among a_~I0[] = {~N~+");
    {
        int i;
        for (i = 0; i < x->literalstring_count; i++) {
            g->I[0] = v->i;
            g->I[1] = v->result;
            g->L[0] = v->b;
            g->S[0] = i < x->literalstring_count - 1 ? "," : "";

            w(g, "~Mnew Among(~L0, ~I0, ~I1");
            if (v->function != 0) {
                w(g, ", \"");
                write_varname(g, v->function);
                w(g, "\", ~n.class");
            }
            w(g, ")~S0~N");
            v++;
        }
    }
    w(g, "~-~M};~N~N");
}

static void generate_amongs(struct generator * g) {
    struct among * x;
    for (x = g->analyser->amongs; x; x = x->next) {
        generate_among_table(g, x);
    }
}

static void set_bit(symbol * b, int i) { b[i/8] |= 1 << i%8; }

static void generate_grouping_table(struct generator * g, struct grouping * q) {

    int range = q->largest_ch - q->smallest_ch + 1;
    int size = (range + 7)/ 8;  /* assume 8 bits per symbol */
    symbol * b = q->b;
    symbol * map = create_b(size);
    int i;
    for (i = 0; i < size; i++) map[i] = 0;

    /* Using unicode would require revision here */

    for (i = 0; i < SIZE(b); i++) set_bit(map, b[i] - q->smallest_ch);

    g->V[0] = q->name;

    w(g, "~Mprivate static final char ~V0[] = {");
    for (i = 0; i < size; i++) {
        write_int(g, map[i]);
        if (i < size - 1) w(g, ", ");
    }
    w(g, " };~N~N");
    lose_b(map);
}

static void generate_groupings(struct generator * g) {
    struct grouping * q;
    for (q = g->analyser->groupings; q; q = q->next) {
        generate_grouping_table(g, q);
    }
}

static void generate_members(struct generator * g) {

    struct name * q;
    for (q = g->analyser->names; q; q = q->next) {
        g->V[0] = q;
        switch (q->type) {
            case t_string:
                w(g, "    private ");
                w(g, g->options->string_class);
                w(g, " ~W0 = new ");
                w(g, g->options->string_class);
                w(g, "();~N");
                break;
            case t_integer:
                w(g, "    private int ~W0;~N");
                break;
            case t_boolean:
                w(g, "    private boolean ~W0;~N");
                break;
        }
    }
    w(g, "~N");
}

static void generate_copyfrom(struct generator * g) {

    struct name * q;
    if (g->copy_from_count == 0) return;
    w(g, "~Mprivate void copy_from(~n other) {~+~N");
    for (q = g->analyser->names; q != 0; q = q->next) {
        g->V[0] = q;
        switch (q->type) {
            case t_string:
            case t_integer:
            case t_boolean:
                w(g, "~M~W0 = other.~W0;~N");
                break;
        }
    }
    w(g, "~Msuper.copy_from(other);~N");
    w(g, "~-~M}~N");
}

static void generate_methods(struct generator * g) {

    struct node * p;
    for (p = g->analyser->program; p; p = p->right) {
        generate(g, p);
        g->unreachable = false;
    }
}

extern void generate_program_java(struct generator * g) {

    g->outbuf = str_new();
    g->failure_str = str_new();

    generate_start_comment(g);
    generate_class_begin(g);

    generate_amongs(g);
    generate_groupings(g);

    generate_members(g);
    generate_methods(g);
    generate_copyfrom(g);
    generate_equals(g);

    generate_class_end(g);

    output_str(g->options->output_src, g->outbuf);
    str_delete(g->failure_str);
    str_delete(g->outbuf);
}
