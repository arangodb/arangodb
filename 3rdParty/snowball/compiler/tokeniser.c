
#include <stdio.h>   /* stderr etc */
#include <stdlib.h>  /* malloc free */
#include <string.h>  /* strlen */
#include <ctype.h>   /* isalpha etc */
#include "header.h"

struct system_word {
    int s_size;      /* size of system word */
    const byte * s;  /* pointer to the system word */
    int code;        /* its internal code */
};


/* ASCII collating assumed in syswords.c */

#include "syswords.h"

#define INITIAL_INPUT_BUFFER_SIZE 8192

static int hex_to_num(int ch);

static int smaller(int a, int b) { return a < b ? a : b; }

extern symbol * get_input(const char * filename) {
    FILE * input = fopen(filename, "r");
    if (input == 0) { return 0; }
    {
        symbol * u = create_b(INITIAL_INPUT_BUFFER_SIZE);
        int size = 0;
        while (true) {
            int ch = getc(input);
            if (ch == EOF) break;
            if (size >= CAPACITY(u)) u = increase_capacity(u, size);
            u[size++] = ch;
        }
        fclose(input);
        SIZE(u) = size;
        return u;
    }
}

static void error(struct tokeniser * t, const char * s1, int n, symbol * p, const char * s2) {
    if (t->error_count == 20) { fprintf(stderr, "... etc\n"); exit(1); }
    fprintf(stderr, "%s:%d: ", t->file, t->line_number);
    if (s1) fprintf(stderr, "%s", s1);
    if (p) {
        int i;
        for (i = 0; i < n; i++) fprintf(stderr, "%c", p[i]);
    }
    if (s2) fprintf(stderr, "%s", s2);
    fprintf(stderr, "\n");
    t->error_count++;
}

static void error1(struct tokeniser * t, const char * s) {
    error(t, s, 0,0, 0);
}

static void error2(struct tokeniser * t, const char * s) {
    error(t, "unexpected end of text after ", 0,0, s);
}

static int compare_words(int m, symbol * p, int n, const byte * q) {
    if (m != n) return m - n;
    {
        int i; for (i = 0; i < n; i++) {
            int diff = p[i] - q[i];
            if (diff) return diff;
        }
    }
    return 0;
}

static int find_word(int n, symbol * p) {
    int i = 0; int j = vocab->code;
    do {
        int k = i + (j - i)/2;
        const struct system_word * w = vocab + k;
        int diff = compare_words(n, p, w->s_size, w->s);
        if (diff == 0) return w->code;
        if (diff < 0) j = k; else i = k;
    } while (j - i != 1);
    return -1;
}

static int get_number(int n, symbol * p) {
    int x = 0;
    int i; for (i = 0; i < n; i++) x = 10*x + p[i] - '0';
    return x;
}

static int eq_s(struct tokeniser * t, const char * s) {
    int l = strlen(s);
    if (SIZE(t->p) - t->c < l) return false;
    {
        int i;
        for (i = 0; i < l; i++) if (t->p[t->c + i] != s[i]) return false;
    }
    t->c += l; return true;
}

static int white_space(struct tokeniser * t, int ch) {
    switch (ch) {
        case '\n':
            t->line_number++;
            /* fall through */
        case '\r':
        case '\t':
        case ' ':
            return true;
    }
    return false;
}

static symbol * find_in_m(struct tokeniser * t, int n, symbol * p) {
    struct m_pair * q;
    for (q = t->m_pairs; q; q = q->next) {
        symbol * name = q->name;
        if (n == SIZE(name) && memcmp(name, p, n * sizeof(symbol)) == 0) return q->value;
    }
    return 0;
}

static int read_literal_string(struct tokeniser * t, int c) {
    symbol * p = t->p;
    int ch;
    SIZE(t->b) = 0;
    while (true) {
        if (c >= SIZE(p)) { error2(t, "'"); return c; }
        ch = p[c];
        if (ch == '\n') { error1(t, "string not terminated"); return c; }
        c++;
        if (ch == t->m_start) {
            /* Inside insert characters. */
            int c0 = c;
            int newlines = false; /* no newlines as yet */
            int black_found = false; /* no printing chars as yet */
            while (true) {
                if (c >= SIZE(p)) { error2(t, "'"); return c; }
                ch = p[c]; c++;
                if (ch == t->m_end) break;
                if (!white_space(t, ch)) black_found = true;
                if (ch == '\n') newlines = true;
                if (newlines && black_found) {
                    error1(t, "string not terminated");
                    return c;
                }
            }
            if (!newlines) {
                int n = c - c0 - 1;    /* macro size */
                int firstch = p[c0];
                symbol * q = find_in_m(t, n, p + c0);
                if (q == 0) {
                    if (n == 1 && (firstch == '\'' || firstch == t->m_start))
                        t->b = add_to_b(t->b, 1, p + c0);
                    else if (n >= 3 && firstch == 'U' && p[c0 + 1] == '+') {
                        int codepoint = 0;
                        int x;
                        if (t->uplusmode == UPLUS_DEFINED) {
                            /* See if found with xxxx upper-cased. */
                            symbol * uc = create_b(n);
                            int i;
                            for (i = 0; i != n; ++i) {
                                uc[i] = toupper(p[c0 + i]);
                            }
                            q = find_in_m(t, n, uc);
                            lose_b(uc);
                            if (q != 0) {
                                t->b = add_to_b(t->b, SIZE(q), q);
                                continue;
                            }
                            error1(t, "Some U+xxxx stringdefs seen but not this one");
                        } else {
                            t->uplusmode = UPLUS_UNICODE;
                        }
                        for (x = c0 + 2; x != c - 1; ++x) {
                            int hex = hex_to_num(p[x]);
                            if (hex < 0) {
                                error1(t, "Bad hex digit following U+");
                                break;
                            }
                            codepoint = (codepoint << 4) | hex;
                        }
                        if (t->encoding == ENC_UTF8) {
                            if (codepoint < 0 || codepoint > 0x01ffff) {
                                error1(t, "character values exceed 0x01ffff");
                            }
                            /* Ensure there's enough space for a max length
                             * UTF-8 sequence. */
                            if (CAPACITY(t->b) < SIZE(t->b) + 3) {
                                t->b = increase_capacity(t->b, 3);
                            }
                            SIZE(t->b) += put_utf8(codepoint, t->b + SIZE(t->b));
                        } else {
                            symbol sym;
                            if (t->encoding == ENC_SINGLEBYTE) {
                                /* Only ISO-8859-1 is handled this way - for
                                 * other single-byte character sets you need
                                 * stringdef all the U+xxxx codes you use
                                 * like - e.g.:
                                 *
                                 * stringdef U+0171   hex 'FB'
                                 */
                                if (codepoint < 0 || codepoint > 0xff) {
                                    error1(t, "character values exceed 256");
                                }
                            } else {
                                if (codepoint < 0 || codepoint > 0xffff) {
                                    error1(t, "character values exceed 64K");
                                }
                            }
                            sym = codepoint;
                            t->b = add_to_b(t->b, 1, &sym);
                        }
                    } else
                        error(t, "string macro '", n, p + c0, "' undeclared");
                } else
                    t->b = add_to_b(t->b, SIZE(q), q);
            }
        } else {
            if (ch == '\'') return c;
            if (ch < 0 || ch >= 0x80) {
                if (t->encoding != ENC_WIDECHARS) {
                    /* We don't really want people using non-ASCII literal
                     * strings, but historically it's worked for single-byte
                     * and UTF-8 if the source encoding matches what the
                     * generated stemmer works in and it seems unfair to just
                     * suddenly make this a hard error.`
                     */
                    fprintf(stderr,
                            "%s:%d: warning: Non-ASCII literal strings aren't "
                            "portable - use stringdef instead\n",
                            t->file, t->line_number);
                } else {
                    error1(t, "Non-ASCII literal strings aren't "
                              "portable - use stringdef instead");
                }
            }
            t->b = add_to_b(t->b, 1, p + c - 1);
        }
    }
}

static int next_token(struct tokeniser * t) {
    symbol * p = t->p;
    int c = t->c;
    int ch;
    int code = -1;
    while (true) {
        if (c >= SIZE(p)) { t->c = c; return -1; }
        ch = p[c];
        if (white_space(t, ch)) { c++; continue; }
        if (isalpha(ch)) {
            int c0 = c;
            while (c < SIZE(p) && (isalnum(p[c]) || p[c] == '_')) c++;
            code = find_word(c - c0, p + c0);
            if (code < 0 || t->token_disabled[code]) {
                t->b = move_to_b(t->b, c - c0, p + c0);
                code = c_name;
            }
        } else
        if (isdigit(ch)) {
            int c0 = c;
            while (c < SIZE(p) && isdigit(p[c])) c++;
            t->number = get_number(c - c0, p + c0);
            code = c_number;
        } else
        if (ch == '\'') {
            c = read_literal_string(t, c + 1);
            code = c_literalstring;
        } else
        {
            int lim = smaller(2, SIZE(p) - c);
            int i;
            for (i = lim; i > 0; i--) {
                code = find_word(i, p + c);
                if (code >= 0) { c += i; break; }
            }
        }
        if (code >= 0) {
            t->c = c;
            return code;
        }
        error(t, "'", 1, p + c, "' unknown");
        c++;
        continue;
    }
}

static int next_char(struct tokeniser * t) {
    if (t->c >= SIZE(t->p)) return -1;
    return t->p[t->c++];
}

static int next_real_char(struct tokeniser * t) {
    while (true) {
        int ch = next_char(t);
        if (!white_space(t, ch)) return ch;
    }
}

static void read_chars(struct tokeniser * t) {
    int ch = next_real_char(t);
    if (ch < 0) { error2(t, "stringdef"); return; }
    {
        int c0 = t->c-1;
        while (true) {
            ch = next_char(t);
            if (white_space(t, ch) || ch < 0) break;
        }
        t->b2 = move_to_b(t->b2, t->c - c0 - 1, t->p + c0);
    }
}

static int decimal_to_num(int ch) {
    if ('0' <= ch && ch <= '9') return ch - '0';
    return -1;
}

static int hex_to_num(int ch) {
    if ('0' <= ch && ch <= '9') return ch - '0';
    if ('a' <= ch && ch <= 'f') return ch - 'a' + 10;
    if ('A' <= ch && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static void convert_numeric_string(struct tokeniser * t, symbol * p, int base) {
    int c = 0; int d = 0;
    while (true) {
        while (c < SIZE(p) && p[c] == ' ') c++;
        if (c == SIZE(p)) break;
        {
            int number = 0;
            while (c != SIZE(p)) {
                int ch = p[c];
                if (ch == ' ') break;
                if (base == 10) {
                    ch = decimal_to_num(ch);
                    if (ch < 0) {
                        error1(t, "decimal string contains non-digits");
                        return;
                    }
                } else {
                    ch = hex_to_num(ch);
                    if (ch < 0) {
                        error1(t, "hex string contains non-hex characters");
                        return;
                    }
                }
                number = base * number + ch;
                c++;
            }
            if (t->encoding == ENC_SINGLEBYTE) {
                if (number < 0 || number > 0xff) {
                    error1(t, "character values exceed 256");
                    return;
                }
            } else {
                if (number < 0 || number > 0xffff) {
                    error1(t, "character values exceed 64K");
                    return;
                }
            }
            if (t->encoding == ENC_UTF8)
                d += put_utf8(number, p + d);
            else
                p[d++] = number;
        }
    }
    SIZE(p) = d;
}

extern int read_token(struct tokeniser * t) {
    symbol * p = t->p;
    int held = t->token_held;
    t->token_held = false;
    if (held) return t->token;
    while (true) {
        int code = next_token(t);
        switch (code) {
            case c_comment1: /*  slash-slash comment */
                while (t->c < SIZE(p) && p[t->c] != '\n') t->c++;
                continue;
            case c_comment2: /* slash-star comment */
                while (true) {
                    if (t->c >= SIZE(p)) {
                        error1(t, "/* comment not terminated");
                        t->token = -1;
                        return -1;
                    }
                    if (p[t->c] == '\n') t->line_number++;
                    if (eq_s(t, "*/")) break;
                    t->c++;
                }
                continue;
            case c_stringescapes: {
                int ch1 = next_real_char(t);
                int ch2 = next_real_char(t);
                if (ch2 < 0) {
                    error2(t, "stringescapes");
                    continue;
                }
                if (ch1 == '\'') {
                    error1(t, "first stringescape cannot be '");
                    continue;
                }
                t->m_start = ch1;
                t->m_end = ch2;
                continue;
            }
            case c_stringdef: {
                int base = 0;
                read_chars(t);
                code = read_token(t);
                if (code == c_hex) { base = 16; code = read_token(t); } else
                if (code == c_decimal) { base = 10; code = read_token(t); }
                if (code != c_literalstring) {
                    error1(t, "string omitted after stringdef");
                    continue;
                }
                if (base > 0) convert_numeric_string(t, t->b, base);
                {   NEW(m_pair, q);
                    q->next = t->m_pairs;
                    q->name = copy_b(t->b2);
                    q->value = copy_b(t->b);
                    t->m_pairs = q;
                    if (t->uplusmode != UPLUS_DEFINED &&
                        (SIZE(t->b2) >= 3 && t->b2[0] == 'U' && t->b2[1] == '+')) {
                        if (t->uplusmode == UPLUS_UNICODE) {
                            error1(t, "U+xxxx already used with implicit meaning");
                        } else {
                            t->uplusmode = UPLUS_DEFINED;
                        }
                    }
                }
                continue;
            }
            case c_get:
                code = read_token(t);
                if (code != c_literalstring) {
                    error1(t, "string omitted after get"); continue;
                }
                t->get_depth++;
                if (t->get_depth > 10) {
                    fprintf(stderr, "get directives go 10 deep. Looping?\n");
                    exit(1);
                }
                {
                    NEW(input, q);
                    char * file = b_to_s(t->b);
                    symbol * u = get_input(file);
                    if (u == 0) {
                        struct include * r;
                        for (r = t->includes; r; r = r->next) {
                            symbol * b = copy_b(r->b);
                            b = add_to_b(b, SIZE(t->b), t->b);
                            free(file);
                            file = b_to_s(b);
                            u = get_input(file);
                            lose_b(b);
                            if (u != 0) break;
                        }
                    }
                    if (u == 0) {
                        error(t, "Can't get '", SIZE(t->b), t->b, "'");
                        exit(1);
                    }
                    memmove(q, t, sizeof(struct input));
                    t->next = q;
                    t->p = u;
                    t->c = 0;
                    t->file = file;
                    t->file_needs_freeing = true;
                    t->line_number = 1;
                }
                p = t->p;
                continue;
            case -1:
                if (t->next) {
                    lose_b(p);
                    {
                        struct input * q = t->next;
                        memmove(t, q, sizeof(struct input)); p = t->p;
                        FREE(q);
                    }
                    t->get_depth--;
                    continue;
                }
                /* fall through */
            default:
                t->previous_token = t->token;
                t->token = code;
                return code;
        }
    }
}

extern const char * name_of_token(int code) {
    int i;
    for (i = 1; i < vocab->code; i++)
        if ((vocab + i)->code == code) return (const char *)(vocab + i)->s;
    switch (code) {
        case c_mathassign:   return "=";
        case c_name:         return "name";
        case c_number:       return "number";
        case c_literalstring:return "literal";
        case c_neg:          return "neg";
        case c_grouping:     return "grouping";
        case c_call:         return "call";
        case c_booltest:     return "Boolean test";
        case -2:             return "start of text";
        case -1:             return "end of text";
        default:             return "?";
    }
}

extern void disable_token(struct tokeniser * t, int code) {
    t->token_disabled[code] = 1;
}

extern struct tokeniser * create_tokeniser(symbol * p, char * file) {
    NEW(tokeniser, t);
    t->next = 0;
    t->p = p;
    t->c = 0;
    t->file = file;
    t->file_needs_freeing = false;
    t->line_number = 1;
    t->b = create_b(0);
    t->b2 = create_b(0);
    t->m_start = -1;
    t->m_pairs = 0;
    t->get_depth = 0;
    t->error_count = 0;
    t->token_held = false;
    t->token = -2;
    t->previous_token = -2;
    t->uplusmode = UPLUS_NONE;
    memset(t->token_disabled, 0, sizeof(t->token_disabled));
    return t;
}

extern void close_tokeniser(struct tokeniser * t) {
    lose_b(t->b);
    lose_b(t->b2);
    {
        struct m_pair * q = t->m_pairs;
        while (q) {
            struct m_pair * q_next = q->next;
            lose_b(q->name);
            lose_b(q->value);
            FREE(q);
            q = q_next;
        }
    }
    {
        struct input * q = t->next;
        while (q) {
            struct input * q_next = q->next;
            FREE(q);
            q = q_next;
        }
    }
    if (t->file_needs_freeing) free(t->file);
    FREE(t);
}
