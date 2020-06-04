#include <stdlib.h> /* for exit */
#include <string.h> /* for strlen */
#include <stdio.h> /* for fprintf etc */
#include <ctype.h> /* for toupper */
#include "header.h"

/* prototypes */

static void generate(struct generator * g, struct node * p);
static void w(struct generator * g, const char * s);
static void writef(struct generator * g, const char * s, struct node * p);

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

    switch (p->type) {
        case t_external:
            write_char(g, toupper(p->b[0]));
            str_append_b_tail(g->outbuf, p->b, 1);
            return;
        default: {
            int ch = "SbirxG"[p->type];
            write_char(g, ch);
            write_char(g, '_');
            break;
        }
    }
    write_b(g, p->b);
}

static void write_varref(struct generator * g, struct name * p) {
    write_string(g, "context.");
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

    int i = 0;
    write_string(g, "\"");
    while (i < SIZE(p)) {
        int ch;
        i += get_utf8(p + i, &ch);
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
    if (!g->options->comments) return;
    /* FIXME could use Go //line syntax if we had original filename */
    write_margin(g);
    write_string(g, "// ");
    write_comment_content(g, p);
    write_newline(g);
}

static void write_block_start(struct generator * g) {

    w(g, "~+{~N");
}

static void write_block_end(struct generator * g)    /* block end */ {

    w(g, "~-~M}~N");
}

static void write_savecursor(struct generator * g, struct node * p,
                             struct str * savevar) {

    g->B[0] = str_data(savevar);
    g->S[1] = "";
    if (p->mode != m_forward) g->S[1] = "env.Limit - ";
    writef(g, "~Mvar ~B0 = ~S1env.Cursor~N", p);
}

static void restore_string(struct node * p, struct str * out, struct str * savevar) {

    str_clear(out);
    str_append_string(out, "env.Cursor = ");
    if (p->mode != m_forward) str_append_string(out, "env.Limit - ");
    str_append(out, savevar);
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
    write_string(g, p->mode == m_forward ? "env.NextChar();" : "env.PrevChar();");
    write_newline(g);
}

static void wsetlab_begin(struct generator * g, int n) {

    g->I[0] = n;
    w(g, "~Mlab~I0: for {~N~+");
}

static void wsetlab_end(struct generator * g, int n) {
    g->I[0] = n;
    w(g, "~Mbreak lab~I0~N");
    w(g, "~-~M}~N");
}

static void wgotol(struct generator * g, int n) {
    g->I[0] = n;
    w(g, "~Mbreak lab~I0~N");
}

static void write_failure(struct generator * g) {

    if (str_len(g->failure_str) != 0) {
        write_margin(g);
        write_str(g, g->failure_str);
        write_newline(g);
    }
    switch (g->failure_label) {
        case x_return:
            w(g, "~Mreturn false~N");
            g->unreachable = true;
            break;
        default:
            g->I[0] = g->failure_label;
            w(g, "~Mbreak lab~I0~N");
            g->unreachable = true;
    }
}

static void write_failure_if(struct generator * g, char * s, struct node * p) {

    writef(g, "~Mif ", p);
    writef(g, s, p);
    writef(g, " ", p);
    write_block_start(g);
    write_failure(g);
    write_block_end(g);
    g->unreachable = false;
}

/* if at limit fail */
static void write_check_limit(struct generator * g, struct node * p) {

    if (p->mode == m_forward) {
        write_failure_if(g, "env.Cursor >= env.Limit", p);
    } else {
        write_failure_if(g, "env.Cursor <= env.LimitBackward", p);
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
            write_string(g, "snowballRuntime.MaxInt"); break;
        case c_minint:
            write_string(g, "snowballRuntime.MinInt"); break;
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
            w(g, "env.Cursor"); break;
        case c_limit:
            w(g, p->mode == m_forward ? "env.Limit" : "env.LimitBackward"); break;
        case c_lenof:
            g->V[0] = p->name;
            w(g, "snowballRuntime.RuneCountInString(~V0)");
            break;
        case c_sizeof:
            g->V[0] = p->name;
            w(g, "len(~V0)");
            break;
        case c_len:
            w(g, "snowballRuntime.RuneCountInString(env.Current())");
            break;
        case c_size:
            w(g, "len(env.Current())");
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
        exit(1);
    }
    while (p->right != 0) {
        int label = new_label(g);
        g->failure_label = label;
        wsetlab_begin(g, label);
        generate(g, p);
        if (!g->unreachable) {
            wgotol(g, out_lab);
            end_unreachable = false;
        }
        w(g, "~-~M}~N");
        g->unreachable = false;
        if (keep_c) write_restorecursor(g, p, savevar);
        p = p->right;
    }

    g->failure_label = a0;
    str_delete(g->failure_str);
    g->failure_str = a1;

    generate(g, p);
    wsetlab_end(g, out_lab);
    if (!end_unreachable) {
        g->unreachable = false;
    }
    str_delete(savevar);
}

static void generate_backwards(struct generator * g, struct node * p) {

    write_comment(g, p);
    writef(g,"~Menv.LimitBackward = env.Cursor~N"
             "~Menv.Cursor = env.Limit~N", p);
    generate(g, p->left);
    w(g, "~Menv.Cursor = env.LimitBackward~N");
}


static void generate_not(struct generator * g, struct node * p) {

    struct str * savevar = vars_newname(g);
    int keep_c = K_needed(g, p->left);

    int a0 = g->failure_label;
    struct str * a1 = str_copy(g->failure_str);
    int label = new_label(g);
    g->failure_label = label;

    write_comment(g, p);
    if (keep_c) {
        write_savecursor(g, p, savevar);
    }

    str_clear(g->failure_str);

    wsetlab_begin(g, label);

    generate(g, p->left);

    g->failure_label = a0;
    str_delete(g->failure_str);
    g->failure_str = a1;

    if (!g->unreachable) write_failure(g);
    w(g, "~-~M}~N");

    g->unreachable = false;

    if (keep_c) write_restorecursor(g, p, savevar);
    str_delete(savevar);
}


static void generate_try(struct generator * g, struct node * p) {

    struct str * savevar = vars_newname(g);
    int keep_c = K_needed(g, p->left);
    int label = new_label(g);
    g->failure_label = label;

    write_comment(g, p);
    if (keep_c) {
        write_savecursor(g, p, savevar);
        restore_string(p, g->failure_str, savevar);
    }
    wsetlab_begin(g, label);
    generate(g, p->left);
    wsetlab_end(g, label);
    g->unreachable = false;

    str_delete(savevar);
}

static void generate_set(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 = true~N", p);
}

static void generate_unset(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 = false~N", p);
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

    if (p->left->type == c_call) {
        /* Optimise do <call> */
        write_comment(g, p->left);
        g->V[0] = p->left->name;
        w(g, "~M~W0(env, context)~N");
    } else {
        int label = new_label(g);
        g->failure_label = label;
        str_clear(g->failure_str);

        wsetlab_begin(g, label);
        generate(g, p->left);
        wsetlab_end(g, label);
        g->unreachable = false;
    }

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
    w(g, "~Mgolab~I0: for {~N~+");
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
        w(g, "~Mbreak golab~I0~N");
    }
    g->unreachable = false;
    w(g, "~-~M}~N");
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
    w(g, "~Mfor _ = range make([]struct{},");
    generate_AE(g, p->AE);
    writef(g, ") {~+~N", p);

    generate(g, p->left);

    w(g, "~-~M}~N");
    str_delete(loopvar);
    g->unreachable = false;
}

static void generate_repeat_or_atleast(struct generator * g, struct node * p, struct str * loopvar) {

    struct str * savevar = vars_newname(g);
    int keep_c = repeat_restore(g, p->left);
    int replab = new_label(g);
    g->I[0] = replab;
    writef(g, "~Mreplab~I0: for{~N~+", p);

    if (keep_c) write_savecursor(g, p, savevar);

    g->failure_label = new_label(g);
    str_clear(g->failure_str);
    g->I[0] = g->failure_label;
    w(g, "~Mlab~I0: for _ = range [2]struct{}{} {~N~+");
    generate(g, p->left);

    if (!g->unreachable) {
        if (loopvar != 0) {
            g->B[0] = str_data(loopvar);
            w(g, "~M~B0--~N");
        }

        g->I[0] = replab;
        w(g, "~Mcontinue replab~I0~N");
    }
    w(g, "~-~M}~N");
    g->unreachable = false;

    if (keep_c) write_restorecursor(g, p, savevar);

    g->I[0] = replab;
    w(g, "~Mbreak replab~I0~N~-~M}~N");
    str_delete(savevar);
}

static void generate_repeat(struct generator * g, struct node * p) {
    write_comment(g, p);
    generate_repeat_or_atleast(g, p, NULL);
}

static void generate_atleast(struct generator * g, struct node * p) {

    struct str * loopvar = vars_newname(g);
    write_comment(g, p);
    g->B[0] = str_data(loopvar);
    w(g, "~Mvar ~B0 = ");
    generate_AE(g, p->AE);
    w(g, "~N");
    {
        int a0 = g->failure_label;
        struct str * a1 = str_copy(g->failure_str);

        generate_repeat_or_atleast(g, p, loopvar);

        g->failure_label = a0;
        str_delete(g->failure_str);
        g->failure_str = a1;
    }
    g->B[0] = str_data(loopvar);
    write_failure_if(g, "~B0 > 0", p);
    str_delete(loopvar);
}

static void generate_setmark(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 = env.Cursor~N", p);
}

static void generate_tomark(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? ">" : "<";

    w(g, "~Mif env.Cursor ~S0 "); generate_AE(g, p->AE);
    writef(g, " ", p);
    write_block_start(g);
    write_failure(g);
    write_block_end(g);
    g->unreachable = false;
    w(g, "~Menv.Cursor = "); generate_AE(g, p->AE); writef(g, "~N", p);
}

static void generate_atmark(struct generator * g, struct node * p) {

    write_comment(g, p);
    w(g, "~Mif env.Cursor != "); generate_AE(g, p->AE);
    writef(g, " ", p);
    write_block_start(g);
    write_failure(g);
    write_block_end(g);
    g->unreachable = false;
}

static void generate_hop(struct generator * g, struct node * p) {

    write_block_start(g);
    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "" : "-";

    w(g, "~Mvar c = env.ByteIndexForHop(~S0(");
    generate_AE(g, p->AE);
    w(g, "))~N");

    g->S[0] = p->mode == m_forward ? "0" : "env.LimitBackward";

    write_failure_if(g, "int32(~S0) > c || c > int32(env.Limit)", p);
    writef(g, "~Menv.Cursor = int(c)~N", p);
    write_block_end(g);
}

static void generate_delete(struct generator * g, struct node * p) {

    write_comment(g, p);
    writef(g, "~Mif !env.SliceDel() {~N"
              "~+~Mreturn false~N~-"
              "~M}~N", p);
}


static void generate_next(struct generator * g, struct node * p) {

    write_comment(g, p);
    write_check_limit(g, p);
    write_inc_cursor(g, p);
}

static void generate_tolimit(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "env.Limit" : "env.LimitBackward";
    writef(g, "~Menv.Cursor = ~S0~N", p);
}

static void generate_atlimit(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "env.Limit" : "env.LimitBackward";
    g->S[1] = p->mode == m_forward ? "<" : ">";
    write_failure_if(g, "env.Cursor ~S1 ~S0", p);
}

static void generate_leftslice(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "env.Bra" : "env.Ket";
    writef(g, "~M~S0 = env.Cursor~N", p);
}

static void generate_rightslice(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "env.Ket" : "env.Bra";
    writef(g, "~M~S0 = env.Cursor~N", p);
}

static void generate_assignto(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 = env.AssignTo()~N", p);
}

static void generate_sliceto(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 = env.SliceTo()~N"
              "~Mif ~V0 == \"\" {~N"
              "~+~Mreturn false~N~-~M}~N", p);
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

    write_block_start(g);
    write_comment(g, p);
    if (p->mode == m_backward) keep_c = !keep_c;
    if (keep_c) w(g, "~Mvar c = env.Cursor~N");
    w(g, "~Mbra, ket := env.Cursor, env.Cursor~N");
    writef(g, "~Menv.Insert(bra, ket, ", p);
    generate_address(g, p);
    writef(g, ");~N", p);
    if (keep_c) w(g, "~Menv.Cursor = c~N");
    write_block_end(g);
}

static void generate_assignfrom(struct generator * g, struct node * p) {

    int keep_c = p->mode == m_forward; /* like 'attach' */

    write_block_start(g);
    write_comment(g, p);
    if (keep_c) writef(g, "~Mvar c = env.Cursor~N", p);
    if (p->mode == m_forward) {
        writef(g, "~Menv.Insert(env.Cursor, env.Limit, ", p);
    } else {
        writef(g, "~Menv.Insert(env.LimitBackward, env.Cursor, ", p);
    }
    generate_address(g, p);
    writef(g, ")~N", p);
    if (keep_c) w(g, "~Menv.Cursor = c~N");
    write_block_end(g);
}


static void generate_slicefrom(struct generator * g, struct node * p) {

    write_comment(g, p);
    w(g, "~Mif !env.SliceFrom(");
    generate_address(g, p);
    writef(g, ") {~N"
              "~+~Mreturn false~N~-~M}~N", p);
}

static void generate_setlimit(struct generator * g, struct node * p) {
    struct str * savevar = vars_newname(g);
    struct str * varname = vars_newname(g);
    write_comment(g, p);
    if (p->left && p->left->type == c_tomark) {
        /* Special case for:
         *
         *   setlimit tomark AE for C
         *
         * All uses of setlimit in the current stemmers we ship follow this
         * pattern, and by special-casing we can avoid having to save and
         * restore c.
         */
        struct node * q = p->left;
        g->S[0] = q->mode == m_forward ? ">" : "<";
        w(g, "~Mif env.Cursor ~S0 "); generate_AE(g, q->AE); w(g, " ");
        write_block_start(g);
        write_failure(g);
        write_block_end(g);
        g->unreachable = false;

        g->B[0] = str_data(varname);
        if (p->mode == m_forward) {
            w(g, "~Mvar ~B0 = env.Limit - env.Cursor~N");
            w(g, "~Menv.Limit = ");
        } else {
            w(g, "~Mvar ~B0 = env.LimitBackward~N");
            w(g, "~Menv.LimitBackward = ");
        }
        generate_AE(g, q->AE); writef(g, ";~N", q);

        if (p->mode == m_forward) {
            str_assign(g->failure_str, "env.Limit += ");
            str_append(g->failure_str, varname);
            str_append_string(g->failure_str, ";");
        } else {
            str_assign(g->failure_str, "env.LimitBackward = ");
            str_append(g->failure_str, varname);
            str_append_string(g->failure_str, ";");
        }
    } else {
        write_savecursor(g, p, savevar);
        generate(g, p->left);

        if (!g->unreachable) {
            g->B[0] = str_data(varname);
            if (p->mode == m_forward) {
                w(g, "~Mvar ~B0 = env.Limit - env.Cursor~N");
                w(g, "~Menv.Limit = env.Cursor~N");
            } else {
                w(g, "~Mvar ~B0 = env.LimitBackward~N");
                w(g, "~Menv.LimitBackward = env.Cursor~N");
            }
            write_restorecursor(g, p, savevar);

            if (p->mode == m_forward) {
                str_assign(g->failure_str, "env.Limit += ");
                str_append(g->failure_str, varname);
                str_append_string(g->failure_str, ";");
            } else {
                str_assign(g->failure_str, "env.LimitBackward = ");
                str_append(g->failure_str, varname);
                str_append_string(g->failure_str, ";");
            }
        }
    }

    if (!g->unreachable) {
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
    struct str * savevar_env = vars_newname(g);
    write_comment(g, p);
    g->V[0] = p->name;
    g->B[0] = str_data(savevar_env);
    writef(g, "~Mvar ~B0 = env.Clone()~N"
              "~Menv.SetCurrent(~V0)~N"
              "~Menv.Cursor = 0~N"
              "~Menv.Limit = len(env.Current())~N", p);
    generate(g, p->left);
    if (!g->unreachable) {
        g->V[0] = p->name;
        g->B[0] = str_data(savevar_env);
        /* Update string variable. */
        w(g, "~M~V0 = env.Current()~N");
        /* Reset env */
        w(g, "~M*env = *~B0~N");
    }
    str_delete(savevar_env);
}

static void generate_integer_assign(struct generator * g, struct node * p, char * s) {

    g->V[0] = p->name;
    g->S[0] = s;
    w(g, "~M~V0 ~S0 "); generate_AE(g, p->AE); w(g, "~N");
}

static void generate_integer_test(struct generator * g, struct node * p, char * s) {

    w(g, "~Mif !(");
    generate_AE(g, p->left);
    write_char(g, ' ');
    write_string(g, s);
    write_char(g, ' ');
    generate_AE(g, p->AE);
    w(g, ")");
    write_block_start(g);
    write_failure(g);
    write_block_end(g);
    g->unreachable = false;
}

static void generate_call(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    write_failure_if(g, "!~W0(env, context)", p);
}

static void generate_grouping(struct generator * g, struct node * p, int complement) {

    struct grouping * q = p->name->grouping;
    g->S[0] = p->mode == m_forward ? "" : "B";
    g->S[1] = complement ? "Out" : "In";
    g->V[0] = p->name;
    g->I[0] = q->smallest_ch;
    g->I[1] = q->largest_ch;
    write_failure_if(g, "!env.~S1Grouping~S0(~W0, ~I0, ~I1)", p);
}

static void generate_namedstring(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "" : "B";
    g->V[0] = p->name;
    write_failure_if(g, "!env.EqS~S0(~V0)", p);
}

static void generate_literalstring(struct generator * g, struct node * p) {
    symbol * b = p->literalstring;
    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "" : "B";
    g->L[0] = b;
    write_failure_if(g, "!env.EqS~S0(~L0)", p);
}

static void generate_setup_context(struct generator * g) {

    struct name * q;
    w(g, "~Mvar context =  &Context {~+~N");
    for (q = g->analyser->names; q; q = q->next) {
        g->V[0] = q;
        switch (q->type) {
            case t_string:
                w(g, "~M~W0: \"\",~N");
                break;
            case t_integer:
                w(g, "~M~W0: 0,~N");
                break;
            case t_boolean:
                w(g, "~M~W0: false,~N");
                break;
        }
    }
    w(g, "~-~M}~N");
    w(g, "~M_ = context~N");
}

static void generate_define(struct generator * g, struct node * p) {
    struct name * q = p->name;

    struct str * saved_output = g->outbuf;

    g->V[0] = q;

    if (q->type == t_routine) {
        w(g, "~N~Mfunc ~W0(env *snowballRuntime.Env, ctx interface{}) bool {~+~N");
        w(g, "~Mcontext := ctx.(*Context)~N");
        w(g, "~M_ = context~N");
    } else {
        w(g, "~N~Mfunc ~W0(env *snowballRuntime.Env) bool {~+~N");
        generate_setup_context(g);
    }
    if (p->amongvar_needed) w(g, "~Mvar among_var int32~N");
    g->outbuf = str_new();

    g->next_label = 0;
    g->var_number = 0;

    str_clear(g->failure_str);
    g->failure_label = x_return;
    g->unreachable = false;
    generate(g, p->left);
    if (!g->unreachable) w(g, "~Mreturn true~N");
    w(g, "~-~M}~N");

    str_append(saved_output, g->outbuf);
    str_delete(g->outbuf);
    g->outbuf = saved_output;
}

static void generate_substring(struct generator * g, struct node * p) {

    struct among * x = p->among;

    write_comment(g, p);

    g->S[0] = p->mode == m_forward ? "" : "B";
    g->I[0] = x->number;

    if (!x->amongvar_needed) {
        write_failure_if(g, "env.FindAmong~S0(~A_~I0, context) == 0", p);
    } else {
        writef(g, "~Mamong_var = env.FindAmong~S0(~A_~I0, context)~N", p);
        write_failure_if(g, "among_var == 0", p);
    }
}

static void generate_among(struct generator * g, struct node * p) {

    struct among * x = p->among;

    if (x->substring == 0) generate_substring(g, p);

    if (x->starter != 0) generate(g, x->starter);

    if (x->command_count == 1 && x->nocommand_count == 0) {
        /* Only one outcome ("no match" already handled). */
        generate(g, x->commands[0]);
    } else if (x->command_count > 0) {
        int i;
        w(g, "~M");
        for (i = 1; i <= x->command_count; i++) {
            g->I[0] = i;
            if (i > 1) w(g, " else ");
            w(g, "if among_var == ~I0 {~N~+");
            generate(g, x->commands[i - 1]);
            w(g, "~-~M}");
            g->unreachable = false;
        }
        w(g, "~N");
    }
}

static void generate_booltest(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    write_failure_if(g, "!~V0", p);
}

static void generate_false(struct generator * g, struct node * p) {

    write_comment(g, p);
    write_failure(g);
}

static void generate_debug(struct generator * g, struct node * p) {
    write_comment(g, p);
    g->I[0] = g->debug_count++;
    g->I[1] = p->line_number;
    writef(g, "~Menv.Debug(~I0, ~I1)~N", p);
}

static void generate(struct generator * g, struct node * p) {

    int a0;
    struct str * a1;

    if (g->unreachable) return;

    a0 = g->failure_label;
    a1 = str_copy(g->failure_str);

    switch (p->type) {
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
        case c_repeat:        generate_repeat(g, p); break;
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

static void generate_class_begin(struct generator * g) {
    w(g, "package ");
    w(g, g->options->package);
    w(g, "~N~N");

    w(g, "import(~N");
    w(g, "  snowballRuntime \"");
    w(g, g->options->go_snowball_runtime);
    w(g, "\"~N)~N~N");
}

static void generate_among_table(struct generator * g, struct among * x) {

    struct amongvec * v = x->b;

    g->I[0] = x->number;
    g->I[1] = x->literalstring_count;

    w(g, "~Mvar A_~I0 = []*snowballRuntime.Among{~N~+");
    {
        int i;
        for (i = 0; i < x->literalstring_count; i++) {
            g->I[0] = v->i;
            g->I[1] = v->result;
            g->L[0] = v->b;
            g->S[0] = ",";

            w(g, "~M&snowballRuntime.Among{Str:~L0, A:~I0, B:~I1, ");
            if (v->function != 0) {
                w(g, "F:");
                write_varname(g, v->function);
            } else {
              w(g, "F:nil");
            }
            w(g, "}~S0~N");
            v++;
        }
    }
    w(g, "~-~M}~N~N");
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

    for (i = 0; i < SIZE(b); i++) set_bit(map, b[i] - q->smallest_ch);

    g->V[0] = q->name;
    g->I[0] = size;
    w(g, "~Mvar ~W0 = []byte{");
    for (i = 0; i < size; i++) {
        write_int(g, map[i]);
        if (i < size - 1) w(g, ", ");
    }
    w(g, "}~N~N");
    lose_b(map);
}

static void generate_groupings(struct generator * g) {
    struct grouping * q;
    for (q = g->analyser->groupings; q; q = q->next) {
        if (q->name->used)
            generate_grouping_table(g, q);
    }
}


static void generate_members(struct generator * g) {

    struct name * q;
    w(g, "type Context struct {~+~N");
    for (q = g->analyser->names; q; q = q->next) {
        g->V[0] = q;
        switch (q->type) {
            case t_string:
                w(g, "~M~W0 string~N");
                break;
            case t_integer:
                w(g, "~M~W0 int~N");
                break;
            case t_boolean:
                w(g, "~M~W0 bool~N");
                break;
        }
    }
    w(g, "~-}~N");
}

static void generate_methods(struct generator * g) {

    struct node * p = g->analyser->program;
    while (p != 0) {
        generate(g, p);
        g->unreachable = false;
        p = p->right;
    }
}

extern void generate_program_go(struct generator * g) {

    g->outbuf = str_new();
    g->failure_str = str_new();

    write_start_comment(g, "//! ", NULL);
    if (g->analyser->int_limits_used) {
        /* std::usize is used in the code generated for usize::MAX and usize::MIN */
        w(g, "use std::usize;~N~N");
    }
    generate_class_begin(g);

    generate_amongs(g);
    generate_groupings(g);

    generate_members(g);
    generate_methods(g);

    output_str(g->options->output_src, g->outbuf);
    str_delete(g->failure_str);
    str_delete(g->outbuf);
}
