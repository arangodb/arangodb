/* This is a simple program which uses libstemmer to provide a command
 * line interface for stemming using any of the algorithms provided.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for strlen, memcmp */

#include "libstemmer.h"

#define EMOJI_FACE_THROWING_A_KISS "\xf0\x9f\x98\x98"
#define U_40079 "\xf1\x80\x81\xb9"
static const struct testcase {
    /* Stemmer to use, or 0 to test with all stemmers */
    const char * language;
    /* Character encoding (can be 0 for UTF-8) */
    const char * charenc;
    /* Input string (0 marks end of list) */
    const char * input;
    /* Expected output string (0 means same as input) */
    const char * expect;
} testcases[] = {
    { "en", 0,
      "a" EMOJI_FACE_THROWING_A_KISS "ing",
      "a" EMOJI_FACE_THROWING_A_KISS "e" },
    { "en", 0, U_40079 "wing", 0 },
    // The Finnish stemmer used to damage numbers ending with two or more of
    // the same digit: https://github.com/snowballstem/snowball/issues/66
    { 0, 0, "2000", 0 },
    { 0, 0, "999", 0 },
    { 0, 0, "1000000000", 0 },
    // The Danish stemmer used to damage a number at the end of a word:
    // https://github.com/snowballstem/snowball/issues/81
    { 0, 0, "space1999", 0 },
    { 0, 0, "hal9000", 0 },
    { 0, 0, "0x0e00", 0 },
    { 0, 0, 0, 0 }
};

static void
run_testcase(const char * language, const struct testcase *test)
{
    const char * charenc = test->charenc;
    const char * input = test->input;
    const char * expect = test->expect;
    struct sb_stemmer * stemmer = sb_stemmer_new(language, charenc);
    const sb_symbol * stemmed;
    int len;

    if (expect == NULL) expect = input;
    if (stemmer == 0) {
        if (charenc == NULL) {
            fprintf(stderr, "language `%s' not available for stemming\n", language);
            exit(1);
        } else {
            fprintf(stderr, "language `%s' not available for stemming in encoding `%s'\n", language, charenc);
            exit(1);
        }
    }
    stemmed = sb_stemmer_stem(stemmer, (const unsigned char*)input, strlen(input));
    if (stemmed == NULL) {
        fprintf(stderr, "Out of memory");
        exit(1);
    }

    len = sb_stemmer_length(stemmer);
    if (len != (int)strlen(expect) || memcmp(stemmed, expect, len) != 0) {
        fprintf(stderr, "%s stemmer output for %s was %.*s not %s\n",
                        language, input, len, stemmed, expect);
        exit(1);
    }
    sb_stemmer_delete(stemmer);
}

int
main(int argc, char * argv[])
{
    const char ** all_languages = sb_stemmer_list();
    const struct testcase * p;
    (void)argc;
    (void)argv;
    for (p = testcases; p->input; ++p) {
        const char * language = p->language;
        if (language) {
            run_testcase(language, p);
        } else {
            const char ** l;
            for (l = all_languages; *l; ++l) {
                run_testcase(*l, p);
            }
        }
    }

    return 0;
}
