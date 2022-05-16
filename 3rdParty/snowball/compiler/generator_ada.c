#include <stdlib.h> /* for exit */
#include <string.h> /* for strlen */
#include <stdio.h> /* for fprintf etc */
#include <ctype.h>
#include <limits.h>
#include "header.h"

/* prototypes */

static void generate(struct generator * g, struct node * p);
static void generate_next(struct generator * g, struct node * p);
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

    int ch = p->b[0];
    if (p->type != t_external) {
        write_char(g, "SBIRXG"[p->type]);
        write_char(g, '_');
    }   
    write_char(g, toupper(ch));
    str_append_b_tail(g->outbuf, p->b, 1);

    ch = p->b[SIZE(p->b) - 1];
    if (ch == '_') {
        write_char(g, 'E');
    }
}

static void write_varref(struct generator * g, struct name * p) {  /* reference to variable */
    if (p->type < t_routine) write_string(g, "Z.");
    write_varname(g, p);
}

static void write_literal_string(struct generator * g, symbol * p) {
    int i;
    // Ada supports UTF-8 literal strings, we only need to escape the quote and
    // special characters.
    write_char(g, '"');
    for (i = 0; i < SIZE(p); i++) {
        int ch = p[i];
        if (ch == '"') {
            write_string(g, "\"\"");
        } else if (ch < 32 || ch == 127) {
            printf("In write_literal_string, can't handle non-graphic character 0x%02x currently\n", (int)p[i]);
            exit(1);
        } else if (ch <= 255) {
            write_char(g, ch);
        } else {
            printf("In write_literal_string, can't convert p[%d] to char because it's 0x%02x\n", i, (int)p[i]);
            exit(1);
        }
    }
    write_char(g, '"');
}

static void write_margin(struct generator * g) {
    int i;
    for (i = 0; i < g->margin; i++) write_string(g, "   ");
}

/* Write a variable declaration. */
static void write_declare(struct generator * g,
                          char * declaration,
                          struct node * p) {
    struct str * temp = g->outbuf;
    g->outbuf = g->declarations;
    write_string(g, "   ");
    writef(g, declaration, p);
    write_string(g, ";");
    write_newline(g);
    g->outbuf = temp;
}

static void write_comment(struct generator * g, struct node * p) {
    if (g->options->comments) {
        write_margin(g);
        write_string(g, "--  ");
        write_comment_content(g, p);
        write_newline(g);
    }
}

static void write_block_start(struct generator * g) {
    w(g, "~Mbegin~+~N");
}

static void write_block_end(struct generator * g) {   /* block end */
    w(g, "~-~Mend;~N");
}

static void restore_string(struct node * p, struct str * out, struct str * savevar) {

    str_clear(out);
    str_append_string(out, "Z.C := ");
    if (p->mode != m_forward) str_append_string(out, "Z.L - ");
    str_append(out, savevar);
    str_append_string(out, ";");
}

static void write_savecursor(struct generator * g, struct node * p,
                             struct str * savevar) {
    g->B[0] = str_data(savevar);
    g->S[1] = "";
    if (p->mode != m_forward) g->S[1] = "Z.L - ";
    write_declare(g, "   ~B0 : Char_Index", p);
    writef(g, "~M~B0 := ~S1Z.C;~N" , p);
}

static void write_restorecursor(struct generator * g, struct node * p, struct str * savevar) {
    write_margin(g);
    if (p->mode == m_forward) {
        write_string(g, "Z.C := ");
    } else {
        write_string(g, "Z.C := Z.L - ");
    }
    write_str(g, savevar);
    write_string(g, ";");
    write_newline(g);
}

static void wsetl(struct generator * g, int n) {

    write_newline(g);
    write_margin(g);
    write_string(g, "<<lab");
    write_int(g, n);
    write_string(g, ">>");
    write_newline(g);
    g->line_labelled = g->line_count;
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
            write_string(g, "Result := False;");
            write_newline(g);
            write_margin(g);
            write_string(g, "return;");
            break;
        default:
            write_string(g, "goto lab");
            write_int(g, g->failure_label);
            write_string(g, ";");
            g->label_used = 1;
    }
    write_newline(g);
    g->unreachable = true;
}

static void write_failure_if(struct generator * g, char * s, struct node * p) {

    writef(g, "~Mif ", p);
    writef(g, s, p);
    writef(g, " then~N~+", p);
    write_failure(g);
    writef(g, "~-~Mend if;~N", p);
    g->unreachable = false;
}

/* if at limit fail */
static void write_check_limit(struct generator * g, struct node * p) {
    if (p->mode == m_forward) {
        write_failure_if(g, "Z.C >= Z.L", p);
    } else {
        write_failure_if(g, "Z.C <= Z.Lb", p);
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
            case 'f': 
                      write_failure(g);
                      g->unreachable = false;
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

static int need_among_var(struct node *p) {

    while (p) {
        if (p->type == c_substring || p->type == c_among) {
            return 1;
        }
        if (p->right && need_among_var(p->right)) {
            return 1;
        }
        p = p->left;
    }
    return 0;
}

static int need_among_handler(struct among *a) {
    int i;
    struct amongvec * v = a->b;

    for (i = 0; i < a->literalstring_count; i++, v++) {
        if (v->function != 0) {
            return 1;
        }
    }

    return 0;
}

static void generate_AE(struct generator * g, struct node * p) {
    const char * s;
    switch (p->type) {
        case c_name:
            write_varref(g, p->name); break;
        case c_number:
            // Avoid `parentheses required for unary minus` error from gnat.
            if (p->number < 0)
                write_char(g, '(');
            write_int(g, p->number);
            if (p->number < 0)
                write_char(g, ')');
            break;
        case c_maxint:
            write_string(g, "Integer'Last"); break;
        case c_minint:
            write_string(g, "Integer'First"); break;
        case c_neg:
            write_string(g, "(-"); generate_AE(g, p->right); write_char(g, ')'); break;
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
            w(g, "Z.C"); break;
        case c_limit:
            w(g, p->mode == m_forward ? "Z.L" : "Z.Lb"); break;
        case c_len:
            w(g, "Length_Utf8 (Z)");
            break;
        case c_size:
            w(g, "Length (Z)");
            break;
        case c_lenof:
        case c_sizeof:
            g->V[0] = p->name;
            w(g, "Length_Utf8 (~V0)");
            break;
        default:
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
    int used = g->label_used;
    int keep_c = K_needed(g, p->left);

    int a0 = g->failure_label;
    struct str * a1 = str_copy(g->failure_str);

    int out_lab = new_label(g);
    int end_unreachable = true;

    write_comment(g, p);

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
        g->label_used = 0;
        generate(g, p);
        if (!g->unreachable) {
            wgotol(g, out_lab);
            end_unreachable = false;
        }
        
        if (g->label_used)
            wsetl(g, g->failure_label);
        g->unreachable = false;
        if (keep_c) {
            write_restorecursor(g, p, savevar);
        }
        p = p->right;
    }
    g->label_used = used;
    g->failure_label = a0;
    str_delete(g->failure_str);
    g->failure_str = a1;

    generate(g, p);
    wsetl(g, out_lab);
    if (!end_unreachable) {
        g->unreachable = false;
    }
    str_delete(savevar);
}

static void generate_backwards(struct generator * g, struct node * p) {
    writef(g, "~MZ.Lb := Z.C; Z.C := Z.L;~C~N", p);
    generate(g, p->left);
    w(g, "~MZ.C := Z.Lb;~N");
}


static void generate_not(struct generator * g, struct node * p) {
    struct str * savevar = vars_newname(g);
    int keep_c = K_needed(g, p->left);

    int a0 = g->failure_label, l;
    struct str * a1 = str_copy(g->failure_str);

    write_comment(g, p);
    if (keep_c) {
        write_savecursor(g, p, savevar);
    }

    g->failure_label = new_label(g);
    str_clear(g->failure_str);

    l = g->failure_label;

    generate(g, p->left);

    g->failure_label = a0;
    str_delete(g->failure_str);
    g->failure_str = a1;

    if (!g->unreachable) write_failure(g);

    if (g->label_used)
        wsetl(g, l);

    g->unreachable = false;

    if (keep_c) write_restorecursor(g, p, savevar);
    str_delete(savevar);
}


static void generate_try(struct generator * g, struct node * p) {
    struct str * savevar;
    int keep_c = K_needed(g, p->left);

    g->failure_label = new_label(g);
    g->label_used = 0;
    str_clear(g->failure_str);

    write_comment(g, p);
    if (keep_c) {
        savevar = vars_newname(g);
        write_savecursor(g, p, savevar);
        restore_string(p, g->failure_str, savevar);
    }    

    generate(g, p->left);
    if (g->label_used)
        wsetl(g, g->failure_label);
    g->unreachable = false;

    if (keep_c) {
        str_delete(savevar);
    }
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
        w(g, "~M~V0 (Z, Result);~N");
    } else {
        g->failure_label = new_label(g);
        str_clear(g->failure_str);

        generate(g, p->left);
        if (g->label_used)
            wsetl(g, g->failure_label);
        g->unreachable = false;
    }

    if (keep_c) write_restorecursor(g, p, savevar);
    str_delete(savevar);
}

static void generate_GO_grouping(struct generator * g, struct node * p, int is_goto, int complement) {

    struct grouping * q = p->name->grouping;
    g->S[0] = p->mode == m_forward ? "" : "_Backward";
    g->S[1] = complement ? "In" : "Out";
    g->S[2] = g->options->encoding == ENC_UTF8 ? "" : "";
    g->V[0] = p->name;
    g->I[0] = q->smallest_ch;
    g->I[1] = q->largest_ch;
    if (is_goto) {
        writef(g, "~M~S1_Grouping~S0~S2 (Z, ~V0, ~I0, ~I1, True, C);", p);
        write_failure_if(g, "C < 0", p);
    } else {
        writef(g, "~C"
              "~M~S1_Grouping~S0~S2 (Z, ~V0, ~I0, ~I1, True, C);~N", p);
        write_failure_if(g, "C < 0", p);
        
        if (p->mode == m_forward)
            w(g, "~MZ.C := Z.C + C;~N");
        else
            w(g, "~MZ.C := Z.C - C;~N");
    }
}

static void generate_GO(struct generator * g, struct node * p, int style) {
    int end_unreachable = false;
    int used = g->label_used;
    /* Initialise to NULL to suppress bogus "may be used uninitialised" warning. */
    struct str * savevar = NULL;
    int keep_c = style == 1 || repeat_restore(g, p->left);
    int a0 = g->failure_label;

    int golab = new_label(g);

    if (p->left->type == c_grouping || p->left->type == c_non) {
        /* Special case for "goto" or "gopast" when used on a grouping or an
         * inverted grouping - the movement of c by the matching action is
         * exactly what we want! */
#ifdef OPTIMISATION_WARNINGS
        printf("Optimising %s %s\n", style ? "goto" : "gopast", p->left->type == c_non ? "non" : "grouping");
#endif
        if (g->options->comments) {
            writef(g, "~M~C", p);
        }
        generate_GO_grouping(g, p->left, style, p->left->type == c_non);
        return;
    }

    write_comment(g, p);
    w(g, "~Mloop~N~+");

    if (keep_c) {
        savevar = vars_newname(g);
        write_savecursor(g, p, savevar);
    }

    g->failure_label = new_label(g);
    g->label_used = 0;
    str_clear(g->failure_str);
    generate(g, p->left);

    if (g->unreachable) {
        /* Cannot break out of this loop: therefore the code after the
         * end of the loop is unreachable.*/
        end_unreachable = true;
    } else {
        /* include for goto; omit for gopast */
        if (style == 1) write_restorecursor(g, p, savevar);
        g->I[0] = golab;
        w(g, "~Mexit;~N");
    }
    g->unreachable = false;
    if (g->label_used)
        wsetl(g, g->failure_label);
    if (keep_c) {
        write_restorecursor(g, p, savevar);
        str_delete(savevar);
    }
    g->label_used = used;
    g->failure_label = a0;

    write_check_limit(g, p);
    generate_next(g, p);

    g->I[0] = golab;
    w(g, "~-~Mend loop;~N");
    g->unreachable = end_unreachable;
}

static void generate_loop(struct generator * g, struct node * p) {
    struct str * loopvar = vars_newname(g);
    write_comment(g, p);
    g->B[0] = str_data(loopvar);
    write_declare(g, "   ~B0 : Integer", p);
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
    wsetl(g, replab);
    writef(g, "~N~Mloop~N~+", p);

    if (keep_c) write_savecursor(g, p, savevar);

    g->failure_label = new_label(g);
    g->label_used = 0;
    generate(g, p->left);

    if (!g->unreachable) {
        if (loopvar != 0) {
            g->B[0] = str_data(loopvar);
            w(g, "~M~B0 := ~B0 - 1;~N");
        }

        g->I[0] = replab;
        w(g, "~Mgoto lab~I0;~N");
    }
    if (g->label_used)
        wsetl(g, g->failure_label);
    g->unreachable = false;

    if (keep_c) write_restorecursor(g, p, savevar);

    w(g, "~N~Mexit;~N~-~Mend loop;~N");
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

    write_declare(g, "   ~B0 : Integer", p);
    w(g, "~M~B0 := ");
    generate_AE(g, p->AE);
    w(g, ";~N");
    {
        int a0 = g->failure_label;

        generate_repeat_or_atleast(g, p, loopvar);

        g->failure_label = a0;
    }
    g->B[0] = str_data(loopvar);
    write_failure_if(g, "~B0 > 0", p);
    w(g, "~}");
    str_delete(loopvar);
}

static void generate_setmark(struct generator * g, struct node * p) {
    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 := Z.C;~N", p);
}

static void generate_tomark(struct generator * g, struct node * p) {
    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? ">" : "<";

    w(g, "~Mif Z.C ~S0 "); generate_AE(g, p->AE); w(g, " then~N");
    write_failure(g);
    w(g, "~Mend if;~N");
    g->unreachable = false;
    w(g, "~MZ.C := "); generate_AE(g, p->AE); writef(g, ";~N", p);
}

static void generate_atmark(struct generator * g, struct node * p) {
    write_comment(g, p);
    w(g, "~Mif Z.C /= "); generate_AE(g, p->AE); writef(g, " then~N~+", p);
    write_failure(g);
    w(g, "~-~Mend if;~N");
    g->unreachable = false;
}

static void generate_hop(struct generator * g, struct node * p) {
    g->S[0] = p->mode == m_forward ? "" : "_Backward";
    if (g->options->encoding == ENC_UTF8) {
        w(g, "~MC := Skip_Utf8~S0 (Z, ");
        generate_AE(g, p->AE); writef(g, ");~C~N", p);
        write_failure_if(g, "C < 0", p);
    } else {
        w(g, "~MC := Z.C ~S0 ");
        generate_AE(g, p->AE);
        writef(g, ";~C~N", p);
        if (p->mode == m_forward) {
            write_failure_if(g, "C > Z.L or C < Z.C", p);
        } else {
            write_failure_if(g, "C < Z.Lb or C > Z.C", p);
        }
    }
    writef(g, "~MZ.C := C;~N", p);
}

static void generate_delete(struct generator * g, struct node * p) {
    write_comment(g, p);
    writef(g, "~MSlice_Del (Z);~N", p);
}

static void generate_next(struct generator * g, struct node * p) {
    write_comment(g, p);
    if (p->mode == m_forward)
        w(g, "~MC := Skip_Utf8 (Z);~N");
    else
        w(g, "~MC := Skip_Utf8_Backward (Z);~N");
    write_failure_if(g, "C < 0", p);
    w(g, "~MZ.C := C;~N");
}

static void generate_tolimit(struct generator * g, struct node * p) {
    g->S[0] = p->mode == m_forward ? "" : "b";
    writef(g, "~MZ.C := Z.L~S0;~C~N", p);
}

static void generate_atlimit(struct generator * g, struct node * p) {
    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "" : "b";
    g->S[1] = p->mode == m_forward ? "<" : ">";
    write_failure_if(g, "Z.C ~S1 Z.L~S0", p);
}

static void generate_leftslice(struct generator * g, struct node * p) {
    g->S[0] = p->mode == m_forward ? "Bra" : "Ket";
    writef(g, "~MZ.~S0 := Z.C;~C~N", p);
}

static void generate_rightslice(struct generator * g, struct node * p) {
    g->S[0] = p->mode == m_forward ? "Ket" : "Bra";
    writef(g, "~MZ.~S0 := Z.C;~C~N", p);
}

static void generate_assignto(struct generator * g, struct node * p) {
    g->V[0] = p->name;
    writef(g, "~M~V0 := Assign_To (Z, ~V0);~C~N", p);
    write_failure_if(g, "~V0 == 0", p);
}

static void generate_sliceto(struct generator * g, struct node * p) {
    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 := Ada.Strings.Unbounded.To_Unbounded_String (Slice_To (Z));~N", p);
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
    if (keep_c) w(g, "~MC := Z.C;~N");
    writef(g, "~MInsert (Z, Z.C, Z.C, ", p);
    generate_address(g, p);
    writef(g, ");~N", p);
    if (keep_c) w(g, "~MZ.C := C;~N");
}

static void generate_assignfrom(struct generator * g, struct node * p) {
    int keep_c = p->mode == m_forward; /* like 'attach' */

    write_comment(g, p);
    if (keep_c) writef(g, "~MC := Z.C;~N", p);
    if (p->mode == m_forward) {
        writef(g, "~MInsert (Z, Z.C, Z.L, ", p);
    } else {
        writef(g, "~MInsert (Z, Z.Lb, Z.C, ", p);
    }
    generate_address(g, p);
    writef(g, ");~N", p);
    if (keep_c) w(g, "~MZ.C := C;~N");
}

static void generate_slicefrom(struct generator * g, struct node * p) {
    write_comment(g, p);
    w(g, "~MSlice_From (Z, ");
    generate_address(g, p);
    writef(g, ");~N", p);
}

static void generate_setlimit(struct generator * g, struct node * p) {
    struct str * savevar = vars_newname(g);
    struct str * varname = vars_newname(g);

    g->B[0] = str_data(varname);
    write_declare(g, "   ~B0 : Integer", p);
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

        ++g->keep_count;

        g->S[0] = q->mode == m_forward ? ">" : "<";

        w(g, "~Mif Z.C ~S0 "); generate_AE(g, q->AE); writef(g, " then~N~+", q);
        w(g, "~MResult := False;~N");
        w(g, "~Mreturn;~-~N");
        w(g, "~Mend if;~N");
        w(g, "~M~B0");
        g->unreachable = false;

        if (p->mode == m_forward) {
            w(g, " := Z.L - Z.C; Z.L := ");
        } else {
            w(g, " := Z.Lb; Z.Lb := ");
        }
        generate_AE(g, q->AE);
        w(g, ";~N");

        if (p->mode == m_forward) {
            str_assign(g->failure_str, "Z.L := Z.L + ");
            str_append(g->failure_str, varname);
            str_append_ch(g->failure_str, ';');
        } else {
            str_assign(g->failure_str, "Z.Lb := ");
            str_append(g->failure_str, varname);
            str_append_ch(g->failure_str, ';');
        }

    } else {
        write_savecursor(g, p, savevar);

        generate(g, p->left);

        if (!g->unreachable) {
            g->B[0] = str_data(varname);
            if (p->mode == m_forward) {
                w(g, "~M~B0 := Z.L - Z.C;~N");
                w(g, "~MZ.L := Z.C;~N");
            } else {
                w(g, "~M~B0 := Z.Lb;~N");
                w(g, "~MZ.Lb := Z.C;~N");
            }
            write_restorecursor(g, p, savevar);

            if (p->mode == m_forward) {
                str_assign(g->failure_str, "Z.L := Z.L + ");
                str_append(g->failure_str, varname);
                str_append_ch(g->failure_str, ';');
            } else {
                str_assign(g->failure_str, "Z.Lb := ");
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
    w(g, "~M~V0 := ");

    if (s != 0) {
        g->S[0] = s;
        w(g, "~V0 ~S0 ");
    }

    generate_AE(g, p->AE);
    w(g, ";~N");
}

static void generate_integer_test(struct generator * g, struct node * p, char * s) {

    w(g, "~Mif not (");
    generate_AE(g, p->left);
    write_char(g, ' ');
    write_string(g, s);
    write_char(g, ' ');
    generate_AE(g, p->AE);
    w(g, ") then~+~N");
    write_failure(g);
    w(g, "~-~Mend if;~N");
    g->unreachable = false;
}

static void generate_integer_function(struct generator * g, struct node * p, char * s) {

    w(g, "~MResult := (");
    generate_AE(g, p->left);
    write_char(g, ' ');
    write_string(g, s);
    write_char(g, ' ');
    generate_AE(g, p->AE);
    w(g, ");~N");
    g->unreachable = false;
}

static void generate_call(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    writef(g, "~M~V0 (Z, Result);~N", p);
    write_failure_if(g, "not Result", p);
}

static void generate_grouping(struct generator * g, struct node * p, int complement) {

    struct grouping * q = p->name->grouping;
    g->S[0] = p->mode == m_forward ? "" : "_Backward";
    g->S[1] = complement ? "Out_" : "In_";
    g->S[2] = g->options->encoding == ENC_UTF8 ? "" : "";
    g->V[0] = p->name;
    g->I[0] = q->smallest_ch;
    g->I[1] = q->largest_ch;
    writef(g, "~M~S1Grouping~S0~S2 (Z, ~V0, ~I0, ~I1, False, C);~N", p);
    write_failure_if(g, "C /= 0", p);
}

static void generate_namedstring(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "" : "_Backward";
    g->V[0] = p->name;
    writef(g, "~MC := Eq_S~S0 (Z, Ada.Strings.Unbounded.To_String (~V0));", p);
    write_failure_if(g, "C = 0", p);
}

static void generate_literalstring(struct generator * g, struct node * p) {
    symbol * b = p->literalstring;
    write_comment(g, p);
    g->S[0] = p->mode == m_forward ? "" : "_Backward";
    g->L[0] = b;
    writef(g, "~MC := Eq_S~S0 (Z, ~L0);~N", p);
    write_failure_if(g, "C = 0", p);
    if (p->mode == m_forward) {
        writef(g, "~MZ.C := Z.C + C;~N", p);
    } else {
        writef(g, "~MZ.C := Z.C - C;~N", p);
    }
}

static void generate_define(struct generator * g, struct node * p) {
    struct str *saved_output;
    struct str *saved_declarations;

    /* Generate function header. */
    g->V[0] = p->name;
    w(g, "~N~Mprocedure ~W0 (Z : in out Context_Type; Result : out Boolean) is~N");

    /* Save output*/
    saved_output = g->outbuf;
    saved_declarations = g->declarations;

    g->outbuf = str_new();
    g->declarations = str_new();

    g->next_label = 0;
    g->var_number = 0;

    g->failure_label = x_return;
    g->unreachable = false;

    /* Generate function body. */
    w(g, "~{");
    switch (p->left->type) {
        case c_eq:            generate_integer_function(g, p->left, "="); break;
        case c_ne:            generate_integer_function(g, p->left, "/="); break;
        case c_gr:            generate_integer_function(g, p->left, ">"); break;
        case c_ge:            generate_integer_function(g, p->left, ">="); break;
        case c_ls:            generate_integer_function(g, p->left, "<"); break;
        case c_le:            generate_integer_function(g, p->left, "<="); break;
        default:
            generate(g, p->left);
            if (!g->unreachable) w(g, "~N~MResult := True;~N");
            str_append_string(saved_output, "      C : Result_Index;\n");
            if (need_among_var(p->left) || 1) {
                str_append_string(saved_output, "      A : Integer;\n");
            }
            break;
    }
    g->V[0] = p->name;
    w(g, "~-~Mend ~W0;~N");

    if (g->var_number) {
        str_append(saved_output, g->declarations);
    }

    str_append(saved_output, g->outbuf);
    str_delete(g->declarations);
    str_delete(g->outbuf);
    g->declarations = saved_declarations;
    g->outbuf = saved_output;
}

static void generate_substring(struct generator * g, struct node * p) {
    struct among * x = p->among;
    int block = -1;
    unsigned int bitmap = 0;
    struct amongvec * among_cases = x->b;
    int c;
    int empty_case = -1;
    int n_cases = 0;
    symbol cases[2];
    int shortest_size = INT_MAX;
    int call_done = 0;
    int need_handler = need_among_handler(x);

    write_comment(g, p);

    g->S[0] = p->mode == m_forward ? "" : "_Backward";
    g->I[0] = x->number;

    /* In forward mode with non-ASCII UTF-8 characters, the first character
     * of the string will often be the same, so instead look at the last
     * common character position.
     *
     * In backward mode, we can't match if there are fewer characters before
     * the current position than the minimum length.
     */
    for (c = 0; c < x->literalstring_count; ++c) {
        int size = among_cases[c].size;
        if (size != 0 && size < shortest_size) {
            shortest_size = size;
        }
    }

    for (c = 0; c < x->literalstring_count; ++c) {
        symbol ch;
        if (among_cases[c].size == 0) {
            empty_case = c;
            continue;
        }
        if (p->mode == m_forward) {
            ch = among_cases[c].b[shortest_size - 1];
        } else {
            ch = among_cases[c].b[among_cases[c].size - 1];
        }
        if (n_cases == 0) {
            block = ch >> 5;
        } else if (ch >> 5 != block) {
            block = -1;
            if (n_cases > 2) break;
        }
        if (block == -1) {
            if (n_cases > 0 && ch == cases[0]) continue;
            if (n_cases < 2) {
                cases[n_cases++] = ch;
            } else if (ch != cases[1]) {
                ++n_cases;
                break;
            }
        } else {
            if ((bitmap & (1u << (ch & 0x1f))) == 0) {
                bitmap |= 1u << (ch & 0x1f);
                if (n_cases < 2)
                    cases[n_cases] = ch;
                ++n_cases;
            }
        }
    }

    if (block != -1 || n_cases <= 2) {
        char buf[64];
        char buf2[128];
        char buf3[64];
        g->I[2] = block;
        g->I[3] = bitmap;
        g->I[4] = shortest_size - 1;
        g->S[3] = buf3;
        snprintf(buf3, sizeof(buf3), "16#%x#", bitmap);
        if (p->mode == m_forward) {
            if (shortest_size == 1)
                sprintf(buf, "Z.C");
            else
                sprintf(buf, "Z.C + %d", shortest_size - 1);
            snprintf(buf2, sizeof(buf2), "Character'Pos (Z.P (%s + 1))", buf);
            g->S[1] = buf;
            g->S[2] = buf2;
            if (shortest_size == 1) {
                writef(g, "~Mif Z.C >= Z.L", p);
            } else {
                writef(g, "~Mif Z.C + ~I4 >= Z.L", p);
            }
        } else {
            g->S[1] = "Z.C - 1";
            g->S[2] = "Character'Pos (Z.P (Z.C))";
            if (shortest_size == 1) {
                writef(g, "~Mif Z.C <= Z.Lb", p);
            } else {
                writef(g, "~Mif Z.C - ~I4 <= Z.Lb", p);
            }
        }
        if (n_cases == 0) {
            /* We get this for the degenerate case: among ( '' )
             * This doesn't seem to be a useful construct, but it is
             * syntactically valid.
             */
        } else if (n_cases == 1) {
            g->I[4] = cases[0];
            writef(g, " or else ~S2 /= ~I4", p);
        } else if (n_cases == 2) {
            g->I[4] = cases[0];
            g->I[5] = cases[1];
            writef(g, " or else (~S2 /= ~I4 and then ~S2 /= ~I5)", p);
        } else {
            writef(g, " or else Check_Among (Z, ~S1, ~I2, ~S3)", p);
        }
        writef(g, " then~+~N", p);
        if (empty_case != -1) {
            /* If the among includes the empty string, it can never fail
             * so not matching the bitmap means we match the empty string.
             */
            g->I[4] = among_cases[empty_case].result;
            writef(g, "~MA := ~I4;~-~N~Melse~+~C", p);
            if (need_handler) {
                writef(g, "~MFind_Among~S0 (Z, A_~I0, Among_String, Among_Handler'Access, A);~N", p);
            } else {
                writef(g, "~MFind_Among~S0 (Z, A_~I0, Among_String, null, A);~N", p);
            }
            write_failure_if(g, "A = 0", p);
            call_done = 1;
        } else {
            writef(g, "~f~C", p);
        }
        writef(g, "~-~Mend if;~N", p);
    } else {
#ifdef OPTIMISATION_WARNINGS
        printf("Couldn't shortcut among %d\n", x->number);
#endif
    }

    if (!call_done) {
        if (need_handler) {
            writef(g, "~MFind_Among~S0 (Z, A_~I0, Among_String, Among_Handler'Access, A);~N", p);
        } else {
            writef(g, "~MFind_Among~S0 (Z, A_~I0, Among_String, null, A);~N", p);
        }
        write_failure_if(g, "A = 0", p);
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
        w(g, "~Mcase A is~N~+");
        for (i = 1; i <= x->command_count; i++) {
            g->I[0] = i;
            w(g, "~Mwhen ~I0 =>~N");
            g->margin++;
            generate(g, x->commands[i - 1]);
            g->margin--;
            g->unreachable = false;
        }
        w(g, "~Mwhen others =>~N");
        w(g, "~M   null;~N");
        w(g, "~-~Mend case;~N");
    }
}

static void generate_booltest(struct generator * g, struct node * p) {

    write_comment(g, p);
    g->V[0] = p->name;
    write_failure_if(g, "not ~V0", p);
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
        case c_ne:            generate_integer_test(g, p, "/="); break;
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
    g->margin = 0;
    write_start_comment(g, "--  ", NULL);
}

static void generate_method_decl(struct generator * g, struct name * q) {
    g->V[0] = q;
    w(g, "~Mprocedure ~W0 (Z : in out Context_Type; Result : out Boolean);~N");
}

static void generate_method_decls(struct generator * g, enum name_types type) {
    struct name * q;
    struct among * a = g->analyser->amongs;
    int need_handler = 0;

    for (q = g->analyser->names; q; q = q->next) {
        if ((enum name_types)q->type == type) {
            generate_method_decl(g, q);
        }
    }

    while (a != 0 && need_handler == 0) {
        need_handler = need_among_handler(a);
        a = a->next;
    }
    if (need_handler) {
        w(g, "~N~Mprocedure Among_Handler (Context : in out Stemmer.Context_Type'Class; Operation : in Operation_Index; Result : out Boolean);~N");
    }
}

static int has_string_variable(struct generator * g) {
    struct name * q;
    for (q = g->analyser->names; q; q = q->next) {
        g->V[0] = q;
        if (q->type == t_string) {
            return 1;
        }
    }

    return 0;
}

static void generate_member_decls(struct generator * g) {
    struct name * q;
    int count = 0;

    
    for (q = g->analyser->names; q; q = q->next) {
        if (q->type == t_string || q->type == t_integer || q->type == t_boolean)
            count++;
    }

    w(g, "   type Context_Type is new Stemmer.Context_Type with");
    if (count > 0) {
        w(g, " record~N~+");
        for (q = g->analyser->names; q; q = q->next) {
            g->V[0] = q;
            switch (q->type) {
                case t_string:
                    w(g, "~M~W0 : Ada.Strings.Unbounded.Unbounded_String;~N");
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
        w(g, "~-   end record;~N");
    } else {
        w(g, " null record;~N");
    }
}

static int generate_among_string(struct generator * g, struct among * x, int count) {
    int i;
    struct amongvec * v = x->b;
    int limit = count == 0 ? 38 : 80;

    g->I[0] = x->number;

    for (i = 0; i < x->literalstring_count; i++, v++) {
        /* Write among's string. */
        g->L[0] = v->b;
        g->I[1] = i;
        if (count + SIZE(v->b) > limit) {
            w(g, "~N~M& ");
            count = 3;
            limit = 80;
        } else if (count > 0) {
            w(g, " & ");
        }
        w(g, "~L0");
        count += SIZE(v->b) + 5;
    }
    return count;
}

static int generate_among_table(struct generator * g, struct among * x, int start_pos, int *operation) {
    int i;
    struct amongvec * v = x->b;

    g->I[0] = x->number;

    g->I[1] = x->literalstring_count - 1;
    w(g, "~MA_~I0 : constant Among_Array_Type (0 .. ~I1) := ~+(~N");

    v = x->b;
    for (i = 0; i < x->literalstring_count; i++, v++) {
        g->I[1] = start_pos;

        /* Write among's string position. */
        if (x->literalstring_count == 1) {
            w(g, "~Mothers => (~I1, ");
        } else {
            w(g, "~M(~I1, ");
        }
        start_pos = start_pos + SIZE(v->b);
        g->I[1] = start_pos - 1;
        w(g, "~I1, ");

        /* Write among's index & result. */
        g->I[2] = v->i;
        w(g, "~I2, ");
        g->I[2] = v->result;
        w(g, "~I2, ");

        /* Write among's handler. */
        if (v->function == 0) {
            w(g, "0)");
        } else {
            *operation = *operation + 1;
            g->I[1] = *operation;
            w(g, "~I1)");
        }
        if (i + 1 < x->literalstring_count) {
            w(g, ",~N");
        }
    }
    w(g, ");~-~N~N");
    return start_pos;
}

static int generate_amongs(struct generator * g) {
    struct among * a = g->analyser->amongs;
    int count;
    int start_pos;
    
    w(g, "~MAmong_String : constant String := ~+");
    count = 0;
    while (a != 0) {
        count = generate_among_string(g, a, count);
        a = a->next;
    }
    w(g, ";~N~-~N");

    int operation = 0;
    start_pos = 1;
    a = g->analyser->amongs;
    while (a != 0) {
        start_pos = generate_among_table(g, a, start_pos, &operation);
        a = a->next;
    }
    return operation;
}

static int generate_constructor(struct generator * g) {
    return generate_amongs(g);
}

static void generate_methods(struct generator * g) {
    struct node * p = g->analyser->program;
    while (p != 0) {
        generate(g, p);
        p = p->right;
    }
}

static int generate_operations_dispatcher(struct generator * g) {
    struct among * a = g->analyser->amongs;
    int i;
    int operation = 0;

    w(g, "~N~Mprocedure Among_Handler (Context : in out Stemmer.Context_Type'Class; Operation : in Operation_Index; Result : out Boolean) is~N");
    w(g, "~Mbegin~+~N~M");
    w(g, "case Operation is~+~N~M");
    a = g->analyser->amongs;
    while (a != 0) {
        struct amongvec * v = a->b;
        for (i = 0; i < a->literalstring_count; i++, v++) {
            if (v->function != 0) {
                operation++;
                g->I[2] = operation;
                w(g, "when ~I2 =>~N~M");
                g->V[0] = v->function;
                w(g, "   ~W0 (Context_Type (Context), Result);~N~M");
            }
        }
        a = a->next;
    }
    w(g, "when others =>~N~M");
    w(g, "   Result := False;~-~N~Mend case;~-~N~M");
    w(g, "end Among_Handler;~N~-");
    return operation;
}

static void set_bit(symbol * b, int i) { b[i/8] |= 1 << i%8; }

static void generate_grouping_table(struct generator * g, struct grouping * q) {

    int range = q->largest_ch - q->smallest_ch + 1;
    int size = (range + 7)/ 8;  /* assume 8 bits per symbol */
    symbol * b = q->b;
    symbol * map = create_b(size);
    int i;
    int count = 0;
    int need_comma = 0;

    for (i = 0; i < size; i++) map[i] = 0;

    /* Using unicode would require revision here */

    for (i = 0; i < SIZE(b); i++) set_bit(map, b[i] - q->smallest_ch);

    g->V[0] = q->name;
    g->I[0] = 8 * size - 1;
    w(g, "~N~M~W0 : constant Grouping_Array (0 .. ~I0) := (~N~+~M");
    for (i = 0; i < size; i++) {
        unsigned char m = map[i];
        int j;
        count++;
        if (i != 0) {
            w(g, ",~N~M");
            need_comma = 0;
        }
        for (j = 0; j < 8; j++) {
            if (need_comma)
                w(g, ", ");
                
            if (m & (1 << j)) {
                w(g, "True");
            } else {
                w(g, "False");
            }
            need_comma = 1;
        }
    }
    w(g, "~N~-~M);~N");
    
    lose_b(map);
}

static void generate_groupings(struct generator * g) {
    struct grouping * q;
    for (q = g->analyser->groupings; q; q = q->next) {
        if (q->name->used)
            generate_grouping_table(g, q);
    }
}

extern void generate_program_ada(struct generator * g) {

    g->outbuf = str_new();
    g->failure_str = str_new();

    generate_unit_start(g);

    /* generate implementation. */
    w(g, "package body Stemmer.");
    w(g, g->options->package);
    w(g, " is~N~+~N");
    w(g, "~Mpragma Style_Checks (\"-mr\");~N");
    w(g, "~Mpragma Warnings (Off, \"*variable*is never read and never assigned*\");~N");
    w(g, "~Mpragma Warnings (Off, \"*mode could be*instead of*\");~N");
    w(g, "~Mpragma Warnings (Off, \"*formal parameter.*is not modified*\");~N");
    w(g, "~Mpragma Warnings (Off, \"*this line is too long*\");~N");
    w(g, "~Mpragma Warnings (Off, \"*is not referenced*\");~N");
    w(g, "~N");

    generate_method_decls(g, t_routine);
    generate_groupings(g);

    int operations = generate_constructor(g);
    generate_methods(g);
    if (operations > 0) {
        generate_operations_dispatcher(g);
    }

    w(g, "end Stemmer.");
    w(g, g->options->package);
    w(g, ";~N");

    output_str(g->options->output_src, g->outbuf);

    str_clear(g->outbuf);

    g->margin = 0;
    write_start_comment(g, "--  ", NULL);
    if (has_string_variable(g)) {
        w(g, "private with Ada.Strings.Unbounded;~N");
    }
    w(g, "package Stemmer.");
    w(g, g->options->package);
    w(g, " with SPARK_Mode is~N~+");
    w(g, "   type Context_Type is new Stemmer.Context_Type with private;~N");
    w(g, "   procedure Stem (Z : in out Context_Type; Result : out Boolean);~N");
    w(g, "private~N");
    generate_member_decls(g);
    w(g, "end Stemmer.");
    w(g, g->options->package);
    w(g, ";~N");
    output_str(g->options->output_h, g->outbuf);
    str_delete(g->failure_str);
    str_delete(g->outbuf);
}
