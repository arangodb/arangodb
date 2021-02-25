#include <stdlib.h> /* for exit */
#include <string.h> /* for strlen */
#include <stdio.h> /* for fprintf etc */
#include "header.h"

#define BASE_UNIT   "SnowballProgram"
#define BASE_CLASS  "T" BASE_UNIT

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

    if (p->type != t_external) {
        /* Pascal identifiers are case-insensitive but Snowball identifiers
         * should be case-sensitive.  To address this, we encode the case of
         * the identifier.  For readability of the generated code, the
         * encoding tries to be minimally intrusive for common cases.
         *
         * After the letter which indicates the type and before the "_" we
         * encode the case pattern in the Snowball identifier using "U" for
         * an upper-case letter, "l" for a lower-case letter and nothing for
         * other characters.  Any trailing string of "l" is omitted (since
         * it's redundant and decreases readability).
         *
         * Identifiers without any upper-case encode most simply, e.g. I_foo2
         *
         * A capitalised identifier is also concise, e.g. IU_Foo2
         *
         * All-caps gives a string of Us, e.g. IUUUUUUUU_SHOUTING
         *
         * But any example can be handled, e.g. IUllU_Foo79_Bar
         *
         * We don't try to solve this problem for external identifiers - it
         * seems more helpful to leave those alone and encourage snowball
         * program authors to avoid naming externals which only differ by
         * case.
         */
        int i, len = SIZE(p->b);
        int lower_pending = 0;
        write_char(g, "SBIrxg"[p->type]);
        for (i = 0; i != len; ++i) {
            int ch = p->b[i];
            if (ch >= 'a' && ch <= 'z') {
                ++lower_pending;
            } else if (ch >= 'A' && ch <= 'Z') {
                while (lower_pending) {
                    write_char(g, 'l');
                    --lower_pending;
                }
                write_char(g, 'U');
            }
        }

        write_char(g, '_');
    }
    write_b(g, p->b);
}

static void write_literal_string(struct generator * g, symbol * p) {
    int i;
    write_char(g, '\'');
    for (i = 0; i < SIZE(p); i++) {
        int ch = p[i];
        if (ch == '\'') {
            write_string(g, "''");
        } else if (32 <= ch && ch < 127) {
            write_char(g, ch);
        } else {
            write_char(g, '\'');
            write_char(g, '#');
            write_int (g, ch);
            write_char(g, '\'');
        }
    }
    write_char(g, '\'');
}

static void write_margin(struct generator * g) {
    int i;
    for (i = 0; i < g->margin; i++) write_string(g, "    ");
}

/* Write a variable declaration. */
static void write_declare(struct generator * g,
                          char * declaration,
                          struct node * p) {
    struct str * temp = g->outbuf;
    g->outbuf = g->declarations;
    write_string(g, "    ");
    writef(g, declaration, p);
    write_string(g, ";");
    write_newline(g);
    g->outbuf = temp;
}

static void write_comment(struct generator * g, struct node * p) {
    if (!g->options->comments) return;
    write_margin(g);
    write_string(g, "{ ");
    write_comment_content(g, p);
    write_string(g, " }");
    write_newline(g);
}

static void write_block_start(struct generator * g) {
    w(g, "~MBegin~+~N");
}

static void write_block_end(struct generator * g) {   /* block end */
    w(g, "~-~MEnd;~N");
}

static void write_savecursor(struct generator * g, struct node * p,
                             struct str * savevar) {
    g->B[0] = str_data(savevar);
    g->S[1] = "";
    if (p->mode != m_forward) g->S[1] = "FLimit - ";
    write_declare(g, "~B0 : Integer", p);
    writef(g, "~M~B0 := ~S1FCursor;~N" , p);
}

static void restore_string(struct node * p, struct str * out, struct str * savevar) {
    str_clear(out);
    str_append_string(out, "FCursor := ");
    if (p->mode != m_forward) str_append_string(out, "FLimit - ");
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
    write_string(g, p->mode == m_forward ? "Inc(FCursor);" : "Dec(FCursor);");
    write_newline(g);
}

static void wsetlab_begin(struct generator * g) {
    w(g, "~MRepeat~N~+");
}

static void wsetlab_end(struct generator * g, int n) {
    w(g, "~-~MUntil True;~N");
    w(g, "lab");
    write_int(g, n);
    w(g, ":~N");
}

static void wgotol(struct generator * g, int n) {
    write_margin(g);
    write_string(g, "goto lab");
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
    switch (g->failure_label) {
        case x_return:
            write_string(g, "Begin Result := False; Exit; End;");
            break;
        default:
            write_string(g, "goto lab");
            write_int(g, g->failure_label);
            write_string(g, ";");
    }
    write_newline(g);
    g->unreachable = true;
}

static void write_failure_if(struct generator * g, char * s, struct node * p) {

    writef(g, "~MIf (", p);
    writef(g, s, p);
    writef(g, ") Then~N", p);
    write_block_start(g);
    write_failure(g);
    write_block_end(g);
    g->unreachable = false;
}

/* if at limit fail */
static void write_check_limit(struct generator * g, struct node * p) {
    if (p->mode == m_forward) {
        write_failure_if(g, "FCursor >= FLimit", p);
    } else {
        write_failure_if(g, "FCursor <= FBkLimit", p);
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
            case 'V':
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
            write_varname(g, p->name); break;
        case c_number:
            write_int(g, p->number); break;
        case c_maxint:
            write_string(g, "MAXINT"); break;
        case c_minint:
            write_string(g, "MININT"); break;
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
            w(g, "FCursor"); break;
        case c_limit:
            w(g, p->mode == m_forward ? "FLimit" : "FBkLimit"); break;
        case c_len:
        case c_size:
            w(g, "Length(current)");
            break;
        case c_lenof:
        case c_sizeof:
            g->V[0] = p->name;
            w(g, "Length(~V0)");
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
    wsetlab_begin(g);

    if (keep_c) write_savecursor(g, p, savevar);

    p = p->left;
    str_clear(g->failure_str);

    if (p == 0) {
        /* p should never be 0 after an or: there should be at least two
         * sub nodes. */
        fprintf(stderr, "Error: \"or\" node without children nodes.");
        exit(1);
    }
    while (p->right) {
        g->failure_label = new_label(g);
        wsetlab_begin(g);
        generate(g, p);
        if (!g->unreachable) {
            wgotol(g, out_lab);
            end_unreachable = false;
        }
        wsetlab_end(g, g->failure_label);
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
    writef(g,"~MFBkLimit := FCursor; FCursor := FLimit;~N", p);
    generate(g, p->left);
    w(g, "~MFCursor := FBkLimit;~N");
}


static void generate_not(struct generator * g, struct node * p) {
    struct str * savevar = vars_newname(g);
    int keep_c = K_needed(g, p->left);

    int a0 = g->failure_label, l;
    struct str * a1 = str_copy(g->failure_str);

    write_comment(g, p);
    if (keep_c) {
        write_block_start(g);
        write_savecursor(g, p, savevar);
    }

    g->failure_label = new_label(g);
    str_clear(g->failure_str);

    wsetlab_begin(g);
    l = g->failure_label;

    generate(g, p->left);

    g->failure_label = a0;
    str_delete(g->failure_str);
    g->failure_str = a1;

    if (!g->unreachable) write_failure(g);

    wsetlab_end(g, l);
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

    wsetlab_begin(g);
    generate(g, p->left);
    wsetlab_end(g, g->failure_label);
    g->unreachable = false;

    str_delete(savevar);
}

static void generate_set(struct generator * g, struct node * p) {
    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 := True;~N", p);
}

static void generate_unset(struct generator * g, struct node * p) {
    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 := False;~N", p);
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
        w(g, "~M~V0();~N");
    } else {
        g->failure_label = new_label(g);
        str_clear(g->failure_str);

        wsetlab_begin(g);
        generate(g, p->left);
        wsetlab_end(g, g->failure_label);
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

    write_comment(g, p);
    w(g, "~MWhile True Do~N");
    w(g, "~{");

    if (keep_c) write_savecursor(g, p, savevar);

    g->failure_label = new_label(g);
    wsetlab_begin(g);
    generate(g, p->left);

    if (g->unreachable) {
        /* Cannot break out of this loop: therefore the code after the
         * end of the loop is unreachable.*/
        end_unreachable = true;
    } else {
        /* include for goto; omit for gopast */
        if (style == 1) write_restorecursor(g, p, savevar);
        g->I[0] = golab;
        w(g, "~Mgoto lab~I0;~N");
    }
    g->unreachable = false;
    wsetlab_end(g, g->failure_label);
    if (keep_c) write_restorecursor(g, p, savevar);

    g->failure_label = a0;
    str_delete(g->failure_str);
    g->failure_str = a1;

    write_check_limit(g, p);
    write_inc_cursor(g, p);

    g->I[0] = golab;
    w(g, "~}lab~I0:~N");
    str_delete(savevar);
    g->unreachable = end_unreachable;
}

static void generate_loop(struct generator * g, struct node * p) {
    struct str * loopvar = vars_newname(g);
    write_comment(g, p);
    g->B[0] = str_data(loopvar);
    write_declare(g, "~B0 : Integer", p);
    w(g, "~MFor ~B0 := ");
    generate_AE(g, p->AE);
    writef(g, " DownTo 1 Do~N", p);
    writef(g, "~{", p);

    generate(g, p->left);

    w(g, "~}");
    str_delete(loopvar);
    g->unreachable = false;
}

static void generate_repeat_or_atleast(struct generator * g, struct node * p, struct str * loopvar) {
    struct str * savevar = vars_newname(g);
    int keep_c = repeat_restore(g, p->left);
    int replab = new_label(g);
    g->I[0] = replab;
    writef(g, "lab~I0:~N~MWhile True Do~N~{", p);

    if (keep_c) write_savecursor(g, p, savevar);

    g->failure_label = new_label(g);
    str_clear(g->failure_str);
    wsetlab_begin(g);
    generate(g, p->left);

    if (!g->unreachable) {
        if (loopvar != 0) {
            g->B[0] = str_data(loopvar);
            w(g, "~MDec(~B0);~N");
        }

        g->I[0] = replab;
        w(g, "~Mgoto lab~I0;~N");
    }

    wsetlab_end(g, g->failure_label);
    g->unreachable = false;

    if (keep_c) write_restorecursor(g, p, savevar);

    w(g, "~MBreak;~N~}");
    str_delete(savevar);
}

static void generate_repeat(struct generator * g, struct node * p) {
    write_comment(g, p);
    generate_repeat_or_atleast(g, p, NULL);
}

static void generate_atleast(struct generator * g, struct node * p) {
    struct str * loopvar = vars_newname(g);

    write_comment(g, p);
    w(g, "~{");
    g->B[0] = str_data(loopvar);

    write_declare(g, "~B0 : Integer", p);
    w(g, "~M~B0 := ");
    generate_AE(g, p->AE);
    w(g, ";~N");
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
    w(g, "~}");
    str_delete(loopvar);
}

static void generate_setmark(struct generator * g, struct node * p) {
    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~W0 := FCursor;~N", p);
}

static void generate_tomark(struct generator * g, struct node * p) {
    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? ">" : "<";

    w(g, "~MIf (FCursor ~S0 "); generate_AE(g, p->AE); w(g, ") Then~N");
    write_block_start(g);
    write_failure(g);
    write_block_end(g);
    g->unreachable = false;
    w(g, "~MFCursor := "); generate_AE(g, p->AE); writef(g, ";~N", p);
}

static void generate_atmark(struct generator * g, struct node * p) {
    write_comment(g, p);
    w(g, "~MIf (FCursor <> "); generate_AE(g, p->AE); writef(g, ") Then~N", p);
    write_block_start(g);
    write_failure(g);
    write_block_end(g);
    g->unreachable = false;
}

static void generate_hop(struct generator * g, struct node * p) {
    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "+" : "-";

    w(g, "~{~MC := FCursor ~S0 ");
    generate_AE(g, p->AE);
    w(g, ";~N");

    g->S[0] = p->mode == m_forward ? "0" : "FBkLimit";

    write_failure_if(g, "(~S0 > C) Or (C > FLimit)", p);
    writef(g, "~MFCursor := c;~N", p);
    writef(g, "~}", p);
}

static void generate_delete(struct generator * g, struct node * p) {
    write_comment(g, p);
    writef(g, "~MSliceDel;~N", p);
}


static void generate_next(struct generator * g, struct node * p) {
    write_comment(g, p);
    write_check_limit(g, p);
    write_inc_cursor(g, p);
}

static void generate_tolimit(struct generator * g, struct node * p) {
    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "FLimit" : "FBkLimit";
    writef(g, "~MFCursor := ~S0;~N", p);
}

static void generate_atlimit(struct generator * g, struct node * p) {
    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "FLimit" : "FBkLimit";
    g->S[1] = p->mode == m_forward ? "<" : ">";
    write_failure_if(g, "FCursor ~S1 ~S0", p);
}

static void generate_leftslice(struct generator * g, struct node * p) {
    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "FBra" : "FKet";
    writef(g, "~M~S0 := FCursor;~N", p);
}

static void generate_rightslice(struct generator * g, struct node * p) {
    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "FKet" : "FBra";
    writef(g, "~M~S0 := FCursor;~N", p);
}

static void generate_assignto(struct generator * g, struct node * p) {
    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 := AssignTo();~N", p);
}

static void generate_sliceto(struct generator * g, struct node * p) {
    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 := SliceTo();~N", p);
}

static void generate_address(struct generator * g, struct node * p) {
    symbol * b = p->literalstring;
    if (b != 0) {
        write_literal_string(g, b);
    } else {
        write_varname(g, p->name);
    }
}

static void generate_insert(struct generator * g, struct node * p, int style) {

    int keep_c = style == c_attach;
    write_comment(g, p);
    if (p->mode == m_backward) keep_c = !keep_c;
    if (keep_c) w(g, "~{~MC := FCursor;~N");
    writef(g, "~Minsert(FCursor, FCursor, ", p);
    generate_address(g, p);
    writef(g, ");~N", p);
    if (keep_c) w(g, "~MFCursor := C;~N~}");
}

static void generate_assignfrom(struct generator * g, struct node * p) {
    int keep_c = p->mode == m_forward; /* like 'attach' */

    write_comment(g, p);
    if (keep_c) writef(g, "~{~MC := FCursor;~N", p);
    if (p->mode == m_forward) {
        writef(g, "~Minsert(FCursor, FLimit, ", p);
    } else {
        writef(g, "~Minsert(FBkLimit, FCursor, ", p);
    }
    generate_address(g, p);
    writef(g, ");~N", p);
    if (keep_c) w(g, "~MFCursor := c;~N~}");
}

static void generate_slicefrom(struct generator * g, struct node * p) {
    write_comment(g, p);
    w(g, "~MSliceFrom(");
    generate_address(g, p);
    writef(g, ");~N", p);
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
        w(g, "~MIf (FCursor ~S0 "); generate_AE(g, q->AE); w(g, ") Then~N");
        write_block_start(g);
        write_failure(g);
        write_block_end(g);
        g->unreachable = false;

        g->B[0] = str_data(varname);
        write_declare(g, "~B0 : Integer", p);
        if (p->mode == m_forward) {
            w(g, "~M~B0 := FLimit - FCursor;~N");
            w(g, "~MFLimit := ");
        } else {
            w(g, "~M~B0 := FBkLimit;~N");
            w(g, "~MFBkLimit := ");
        }
        generate_AE(g, q->AE); writef(g, ";~N", q);

        if (p->mode == m_forward) {
            str_assign(g->failure_str, "FLimit := FLimit + ");
            str_append(g->failure_str, varname);
            str_append_ch(g->failure_str, ';');
        } else {
            str_assign(g->failure_str, "FBkLimit := ");
            str_append(g->failure_str, varname);
            str_append_ch(g->failure_str, ';');
        }
    } else {
        write_savecursor(g, p, savevar);
        generate(g, p->left);

        if (!g->unreachable) {
            g->B[0] = str_data(varname);
            write_declare(g, "~B0 : Integer", p);
            if (p->mode == m_forward) {
                w(g, "~M~B0 := FLimit - FCursor;~N");
                w(g, "~MFLimit := FCursor;~N");
            } else {
                w(g, "~M~B0 := FBkLimit;~N");
                w(g, "~MFBkLimit := FCursor;~N");
            }
            write_restorecursor(g, p, savevar);

            if (p->mode == m_forward) {
                str_assign(g->failure_str, "FLimit := FLimit + ");
                str_append(g->failure_str, varname);
                str_append_ch(g->failure_str, ';');
            } else {
                str_assign(g->failure_str, "FBkLimit := ");
                str_append(g->failure_str, varname);
                str_append_ch(g->failure_str, ';');
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
    struct str * savevar = vars_newname(g);
    g->B[0] = str_data(savevar);
    write_comment(g, p);
    g->V[0] = p->name;

    {
        struct str * saved_output = g->outbuf;
        str_clear(g->failure_str);
        g->outbuf = g->failure_str;
        writef(g, "~V0 := FCurrent; "
                  "FCurrent := ~B0_Current; "
                  "FCursor := ~B0_Cursor; "
                  "FLimit := ~B0_Limit; "
                  "FBkLimit := ~B0_BkLimit; "
                  "FBra := ~B0_Bra; "
                  "FKet := ~B0_Ket;", p);
        g->failure_str = g->outbuf;
        g->outbuf = saved_output;
    }

    write_declare(g, "~B0_Current : AnsiString", p);
    write_declare(g, "~B0_Cursor : Integer", p);
    write_declare(g, "~B0_Limit : Integer", p);
    write_declare(g, "~B0_BkLimit : Integer", p);
    write_declare(g, "~B0_Bra : Integer", p);
    write_declare(g, "~B0_Ket : Integer", p);
    writef(g, "~{"
              "~M~B0_Current := FCurrent;~N"
              "{ ~M~B0_Current := Copy(FCurrent, 1, FLimit); }~N"
              "~M~B0_Cursor := FCursor;~N"
              "~M~B0_Limit := FLimit;~N"
              "~M~B0_BkLimit := FBkLimit;~N"
              "~M~B0_Bra := FBra;~N"
              "~M~B0_Ket := FKet;~N"
              "~MFCurrent := ~V0;~N"
              "~MFCursor := 0;~N"
              "~MFLimit := Length(current);~N", p);
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
    w(g, "~M~W0 := ");

    if (s != 0) {
        g->S[0] = s;
        w(g, "~W0 ~S0 ");
    }

    generate_AE(g, p->AE);
    w(g, ";~N");
}

static void generate_integer_test(struct generator * g, struct node * p, char * s) {

    w(g, "~MIf Not (");
    generate_AE(g, p->left);
    write_char(g, ' ');
    write_string(g, s);
    write_char(g, ' ');
    generate_AE(g, p->AE);
    w(g, ") Then~N");
    write_block_start(g);
    write_failure(g);
    write_block_end(g);
    g->unreachable = false;
}

static void generate_call(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    write_failure_if(g, "Not ~V0", p);
}

static void generate_grouping(struct generator * g, struct node * p, int complement) {

    struct grouping * q = p->name->grouping;
    g->S[0] = p->mode == m_forward ? "" : "Bk";
    g->S[1] = complement ? "Out" : "In";
    g->V[0] = p->name;
    g->I[0] = q->smallest_ch;
    g->I[1] = q->largest_ch;
    write_failure_if(g, "Not (~S1Grouping~S0(~V0, ~I0, ~I1))", p);
}

static void generate_namedstring(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "" : "Bk";
    g->V[0] = p->name;
    write_failure_if(g, "Not (EqV~S0(~V0))", p);
}

static void generate_literalstring(struct generator * g, struct node * p) {
    symbol * b = p->literalstring;
    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "" : "Bk";
    g->I[0] = SIZE(b);
    g->L[0] = b;
    write_failure_if(g, "Not (EqS~S0(~I0, ~L0))", p);
}

static void generate_define(struct generator * g, struct node * p) {
    struct str *saved_output;
    struct str *saved_declarations;

    /* Generate function header. */
    g->V[0] = p->name;
    w(g, "~N~MFunction T~n.~W0 : Boolean;~N");

    /* Save output*/
    saved_output = g->outbuf;
    saved_declarations = g->declarations;

    g->outbuf = str_new();
    g->declarations = str_new();

    g->next_label = 0;
    g->var_number = 0;

    str_clear(g->failure_str);
    g->failure_label = x_return;
    g->unreachable = false;

    /* Generate function body. */
    w(g, "~{");
    generate(g, p->left);
    if (!g->unreachable) w(g, "~MResult := True;~N");
    w(g, "~}");

    str_append_string(saved_output, "Var\n");
    str_append_string(saved_output, "    C : Integer;\n");

    if (p->amongvar_needed) {
        str_append_string(saved_output, "    AmongVar : Integer;\n");
    }

    if (g->var_number) {
        str_append(saved_output, g->declarations);
    }

    if (g->next_label) {
        int i, num = g->next_label;

        str_append_string(saved_output, "Label\n");

        for (i = 0; i < num; ++i) {
            str_append_string(saved_output, "    lab");
            str_append_int(saved_output, i);
            str_append_string(saved_output, i == num - 1 ? ";\n" : ",\n");
        }
    }

    str_append(saved_output, g->outbuf);
    str_delete(g->declarations);
    str_delete(g->outbuf);
    g->declarations = saved_declarations;
    g->outbuf = saved_output;
}

static void generate_substring(struct generator * g, struct node * p) {
    struct among * x = p->among;

    write_comment(g, p);

    g->S[0] = p->mode == m_forward ? "" : "Bk";
    g->I[0] = x->number;
    g->I[1] = x->literalstring_count;

    if (!x->amongvar_needed) {
        write_failure_if(g, "FindAmong~S0(a_~I0, ~I1) = 0", p);
    } else {
        writef(g, "~MAmongVar := FindAmong~S0(a_~I0, ~I1);~N", p);
        write_failure_if(g, "AmongVar = 0", p);
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
        write_comment(g, p);
        w(g, "~MCase AmongVar Of~N~+");
        for (i = 1; i <= x->command_count; i++) {
            g->I[0] = i;
            w(g, "~M~I0:~N~{");
            generate(g, x->commands[i - 1]);
            w(g, "~}");
            g->unreachable = false;
        }
        write_block_end(g);
    }
}

static void generate_booltest(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    write_failure_if(g, "Not (~V0)", p);
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
        case c_mathassign:    generate_integer_assign(g, p, NULL); break;
        case c_plusassign:    generate_integer_assign(g, p, "+"); break;
        case c_minusassign:   generate_integer_assign(g, p, "-"); break;
        case c_multiplyassign:generate_integer_assign(g, p, "*"); break;
        case c_divideassign:  generate_integer_assign(g, p, "/"); break;
        case c_eq:            generate_integer_test(g, p, "="); break;
        case c_ne:            generate_integer_test(g, p, "<>"); break;
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

/* Class declaration generation. */
static void generate_unit_start(struct generator * g) {
    write_start_comment(g, "{ ", " }");
    w(g, "Unit ~n;~N~N{$HINTS OFF}~N~NInterface~N~NUses " BASE_UNIT ";~N");
}

static void generate_unit_end(struct generator * g) {
    w(g, "~NEnd.~N");
}

static void generate_class_begin(struct generator * g) {
    w(g, "~NType~N~+~MT~n = Class(" BASE_CLASS ")~N");
}

static void generate_class_end(struct generator * g) {
    w(g, "~}~NImplementation~N");
}

static void generate_method_decl(struct generator * g, struct name * q) {
    g->V[0] = q;
    w(g, "~MFunction ~W0 : Boolean;");
    if (q->type == t_external) {
        w(g, " Override;");
    }
    w(g, "~N");
}

static void generate_method_decls(struct generator * g) {
    struct name * q;

    w(g, "~Mpublic~N~+");
    w(g, "~MConstructor Create;~N");

    for (q = g->analyser->names; q; q = q->next) {
        if (q->type == t_external) {
            generate_method_decl(g, q);
        }
    }
    w(g, "~-");

    w(g, "~Mprivate~N~+");
    for (q = g->analyser->names; q; q = q->next) {
        if (q->type == t_routine) {
            generate_method_decl(g, q);
        }
    }
    w(g, "~-");
}

static void generate_member_decls(struct generator * g) {
    struct name * q;
    w(g, "~Mprivate~N~+");
    for (q = g->analyser->names; q; q = q->next) {
        g->V[0] = q;
        switch (q->type) {
            case t_string:
                w(g, "~M~W0 : AnsiString;~N");
                break;
            case t_integer:
                w(g, "~M~W0 : Integer;~N");
                break;
            case t_boolean:
                w(g, "~M~W0 : Boolean;~N");
                break;
        }
    }

    w(g, "~-");
}

static void generate_among_decls(struct generator * g) {
    struct among *a = g->analyser->amongs;

    w(g, "~Mprivate~N~+");

    while (a != 0) {
        g->I[0] = a->number;
        w(g, "~Ma_~I0 : Array Of TAmong;~N");
        a = a->next;
    }

    w(g, "~-");
}

static void generate_among_table(struct generator * g, struct among * x) {
    int i;
    struct amongvec * v = x->b;

    g->I[0] = x->number;
    g->I[1] = x->literalstring_count;

    w(g, "~MSetLength(a_~I0, ~I1);~N~+");

    for (i = 0; i < x->literalstring_count; i++, v++) {
        g->I[1] = i;

        /* Write among's string. */
        g->L[0] = v->b;
        w(g, "~Ma_~I0[~I1].Str  := ~L0;~N");

        /* Write among's index & result. */
        g->I[2] = v->i;
        w(g, "~Ma_~I0[~I1].Index := ~I2;~N");
        g->I[2] = v->result;
        w(g, "~Ma_~I0[~I1].Result := ~I2;~N");

        /* Write among's handler. */
        w(g, "~Ma_~I0[~I1].Method := ");
        if (v->function == 0) {
            w(g, "nil;~N~N");
        } else {
            g->V[0] = v->function;
            w(g, "Self.~W0;~N~N");
        }
    }
    w(g, "~-");
}

static void generate_amongs(struct generator * g) {
    struct among * a = g->analyser->amongs;
    while (a != 0) {
        generate_among_table(g, a);
        a = a->next;
    }
}

static void generate_constructor(struct generator * g) {
    w(g, "~N~MConstructor T~n.Create;~N~{");
    generate_amongs(g);
    w(g, "~}");
}

static void generate_methods(struct generator * g) {
    struct node * p = g->analyser->program;
    while (p != 0) {
        generate(g, p);
        p = p->right;
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
    g->I[0] = size - 1;
    w(g, "~N~MConst~+~N~M~W0 : Array [0..~I0] Of Char = (~N~+");
    for (i = 0; i < size; i++) {
        if (i != 0) w(g, ",~N");
        g->I[0] = map[i];
        w(g, "~MChr(~I0)");
    }
    w(g, "~N~-~M);~N~-");

    lose_b(map);
}

static void generate_groupings(struct generator * g) {
    struct grouping * q;
    for (q = g->analyser->groupings; q; q = q->next) {
        if (q->name->used)
            generate_grouping_table(g, q);
    }
}

extern void generate_program_pascal(struct generator * g) {
    g->outbuf = str_new();
    g->failure_str = str_new();

    generate_unit_start(g);

    /* Generate class declaration. */
    generate_class_begin(g);
    generate_member_decls(g);
    generate_among_decls(g);
    generate_method_decls(g);
    generate_class_end(g);

    /* generate implementation. */
    generate_groupings(g);
    generate_constructor(g);
    generate_methods(g);

    generate_unit_end(g);

    output_str(g->options->output_src, g->outbuf);
    str_delete(g->failure_str);
    str_delete(g->outbuf);
}
