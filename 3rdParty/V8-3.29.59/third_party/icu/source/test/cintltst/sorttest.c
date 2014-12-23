/*
*******************************************************************************
*
*   Copyright (C) 2003-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  sorttest.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2003aug04
*   created by: Markus W. Scherer
*
*   Test internal sorting functions.
*/

#include <stdio.h>

#include "unicode/utypes.h"
#include "unicode/ucol.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "cintltst.h"
#include "uarrsort.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

static void
SortTest() {
    uint16_t small[]={ 8, 1, 2, 5, 4, 3, 7, 6 };
    int32_t medium[]={ 10, 8, 1, 2, 5, 5, -1, 6, 4, 3, 9, 7, 5 };
    uint32_t large[]={ 21, 10, 20, 19, 11, 12, 13, 10, 10, 10, 10,
                       8, 1, 2, 5, 10, 10, 4, 17, 18, 3, 9, 10, 7, 6, 14, 15, 16 };

    int32_t i;
    UErrorCode errorCode;

    /* sort small array (stable) */
    errorCode=U_ZERO_ERROR;
    uprv_sortArray(small, LENGTHOF(small), sizeof(small[0]), uprv_uint16Comparator, NULL, TRUE, &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("uprv_sortArray(small) failed - %s\n", u_errorName(errorCode));
        return;
    }
    for(i=1; i<LENGTHOF(small); ++i) {
        if(small[i-1]>small[i]) {
            log_err("uprv_sortArray(small) mis-sorted [%d]=%u > [%d]=%u\n", i-1, small[i-1], i, small[i]);
            return;
        }
    }

    /* for medium, add bits that will not be compared, to test stability */
    for(i=0; i<LENGTHOF(medium); ++i) {
        medium[i]=(medium[i]<<4)|i;
    }

    /* sort medium array (stable) */
    uprv_sortArray(medium, LENGTHOF(medium), sizeof(medium[0]), uprv_int32Comparator, NULL, TRUE, &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("uprv_sortArray(medium) failed - %s\n", u_errorName(errorCode));
        return;
    }
    for(i=1; i<LENGTHOF(medium); ++i) {
        if(medium[i-1]>=medium[i]) {
            log_err("uprv_sortArray(medium) mis-sorted [%d]=%u > [%d]=%u\n", i-1, medium[i-1], i, medium[i]);
            return;
        }
    }

    /* sort large array (not stable) */
    errorCode=U_ZERO_ERROR;
    uprv_sortArray(large, LENGTHOF(large), sizeof(large[0]), uprv_uint32Comparator, NULL, FALSE, &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("uprv_sortArray(large) failed - %s\n", u_errorName(errorCode));
        return;
    }
    for(i=1; i<LENGTHOF(large); ++i) {
        if(large[i-1]>large[i]) {
            log_err("uprv_sortArray(large) mis-sorted [%d]=%u > [%d]=%u\n", i-1, large[i-1], i, large[i]);
            return;
        }
    }
}

#if !UCONFIG_NO_COLLATION

/*
 * Fill an array with semi-random short strings.
 * Vary them enough to be interesting, but create duplicates.
 * With CYCLE=10 characters per STR_LEN=3 string positions there are only 1000 unique strings.
 * NUM_LINES should be larger than this.
 */
#define NUM_LINES 10000
#define STR_LEN 3
#define CYCLE 10

/*
 * Use characters beyond the Latin Extended A block to avoid a collator fastpath.
 * They should sort unique, so that we can later use a binary comparison for string equality.
 */
#define BASE_CHAR 0x200

typedef struct Line {
    UChar s[STR_LEN];
    int32_t recordNumber;
} Line;

static void
printLines(const Line *lines) {
#if 0
    int32_t i, j;
    for(i=0; i<NUM_LINES; ++i) {
        const Line *line=lines+i;
        for(j=0; j<STR_LEN; ++j) {
            printf("%04x ", line->s[j]);
        }
        printf(" #%5d\n", line->recordNumber);
    }
#endif
}

/* Use a collator so that the comparisons are not essentially free, for simple benchmarking. */
static int32_t U_EXPORT2
linesComparator(const void *context, const void *left, const void *right) {
    const UCollator *coll=(const UCollator *)context;
    const Line *leftLine=(const Line *)left;
    const Line *rightLine=(const Line *)right;
    /* compare the strings but not the record number */
    return ucol_strcoll(coll, leftLine->s, STR_LEN, rightLine->s, STR_LEN);
}

static void StableSortTest() {
    UErrorCode errorCode=U_ZERO_ERROR;
    UCollator *coll;
    Line *lines, *p;
    UChar s[STR_LEN];
    int32_t i, j;

    coll=ucol_open("root", &errorCode);
    if(U_FAILURE(errorCode)) {
        log_data_err("ucol_open(root) failed - %s\n", u_errorName(errorCode));
        return;
    }

    lines=p=(Line *)uprv_malloc(NUM_LINES*sizeof(Line));
    uprv_memset(lines, 0, NUM_LINES*sizeof(Line));  /* avoid uninitialized memory */

    for(j=0; j<STR_LEN; ++j) { s[j]=BASE_CHAR; }
    j=0;
    for(i=0; i<NUM_LINES; ++i) {
        UChar c;
        u_memcpy(p->s, s, STR_LEN);
        p->recordNumber=i;
        /* Modify the string for the next line. */
        c=s[j]+1;
        if(c==BASE_CHAR+CYCLE) { c=BASE_CHAR; }
        s[j]=c;
        if(++j==STR_LEN) { j=0; }
        ++p;
    }
    puts("\n* lines before sorting");
    printLines(lines);

    uprv_sortArray(lines, NUM_LINES, (int32_t)sizeof(Line),
                   linesComparator, coll, TRUE, &errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("uprv_sortArray() failed - %s\n", u_errorName(errorCode));
        return;
    }
    puts("* lines after sorting");
    printLines(lines);

    /* Verify that the array is sorted correctly. */
    p=lines;
    for(i=1; i<NUM_LINES; ++i) {
        Line *q=p+1;  /* =lines+i */
        /* Binary comparison first, for speed. In this case, equal strings must be identical. */
        int32_t diff=u_strCompare(p->s, STR_LEN, q->s, STR_LEN, FALSE);
        if(diff==0) {
            if(p->recordNumber>=q->recordNumber) {
                log_err("equal strings %d and %d out of order at sorted index %d\n",
                        (int)p->recordNumber, (int)q->recordNumber, (int)i);
                break;
            }
        } else {
            /* Compare unequal strings with the collator. */
            diff=ucol_strcoll(coll, p->s, STR_LEN, q->s, STR_LEN);
            if(diff>=0) {
                log_err("unequal strings %d and %d out of order at sorted index %d\n",
                        (int)p->recordNumber, (int)q->recordNumber, (int)i);
                break;
            }
        }
        p=q;
    }

    uprv_free(lines);
    ucol_close(coll);
}

#endif  /* !UCONFIG_NO_COLLATION */

void
addSortTest(TestNode** root);

void
addSortTest(TestNode** root) {
    addTest(root, &SortTest, "tsutil/sorttest/SortTest");
#if !UCONFIG_NO_COLLATION
    addTest(root, &StableSortTest, "tsutil/sorttest/StableSortTest");
#endif
}
