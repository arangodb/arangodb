/*
*******************************************************************************
* Copyright (C) 2009-2011, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* This file contains the class DecimalFormatStaticSets
*
* DecimalFormatStaticSets holds the UnicodeSets that are needed for lenient
* parsing of decimal and group separators.
********************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/unistr.h"
#include "unicode/uniset.h"
#include "unicode/uchar.h"
#include "cmemory.h"
#include "cstring.h"
#include "uassert.h"
#include "ucln_in.h"
#include "umutex.h"

#include "decfmtst.h"

U_NAMESPACE_BEGIN


//------------------------------------------------------------------------------
//
// Unicode Set pattern strings for all of the required constant sets.
//               Initialized with hex values for portability to EBCDIC based machines.
//                Really ugly, but there's no good way to avoid it.
//
//------------------------------------------------------------------------------

static const UChar gDotEquivalentsPattern[] = {
        // [       .    \u2024  \u3002  \uFE12  \uFE52  \uFF0E  \uFF61     ]
        0x005B, 0x002E, 0x2024, 0x3002, 0xFE12, 0xFE52, 0xFF0E, 0xFF61, 0x005D, 0x0000};

static const UChar gCommaEquivalentsPattern[] = {
        // [       ,    \u060C  \u066B  \u3001  \uFE10  \uFE11  \uFE50  \uFE51  \uFF0C  \uFF64    ]
        0x005B, 0x002C, 0x060C, 0x066B, 0x3001, 0xFE10, 0xFE11, 0xFE50, 0xFE51, 0xFF0C, 0xFF64, 0x005D, 0x0000};

static const UChar gOtherGroupingSeparatorsPattern[] = {
        // [       \     SPACE     '      NBSP  \u066C  \u2000     -    \u200A  \u2018  \u2019  \u202F  \u205F  \u3000  \uFF07     ]
        0x005B, 0x005C, 0x0020, 0x0027, 0x00A0, 0x066C, 0x2000, 0x002D, 0x200A, 0x2018, 0x2019, 0x202F, 0x205F, 0x3000, 0xFF07, 0x005D, 0x0000};

static const UChar gDashEquivalentsPattern[] = {
        // [       \      -     HYPHEN  F_DASH  N_DASH   MINUS     ]
        0x005B, 0x005C, 0x002D, 0x2010, 0x2012, 0x2013, 0x2212, 0x005D, 0x0000};

static const UChar gStrictDotEquivalentsPattern[] = {
        // [      .     \u2024  \uFE52  \uFF0E  \uFF61    ]
        0x005B, 0x002E, 0x2024, 0xFE52, 0xFF0E, 0xFF61, 0x005D, 0x0000};

static const UChar gStrictCommaEquivalentsPattern[] = {
        // [       ,    \u066B  \uFE10  \uFE50  \uFF0C     ]
        0x005B, 0x002C, 0x066B, 0xFE10, 0xFE50, 0xFF0C, 0x005D, 0x0000};

static const UChar gStrictOtherGroupingSeparatorsPattern[] = {
        // [       \     SPACE     '      NBSP  \u066C  \u2000     -    \u200A  \u2018  \u2019  \u202F  \u205F  \u3000  \uFF07     ]
        0x005B, 0x005C, 0x0020, 0x0027, 0x00A0, 0x066C, 0x2000, 0x002D, 0x200A, 0x2018, 0x2019, 0x202F, 0x205F, 0x3000, 0xFF07, 0x005D, 0x0000};

static const UChar gStrictDashEquivalentsPattern[] = {
        // [       \      -      MINUS     ]
        0x005B, 0x005C, 0x002D, 0x2212, 0x005D, 0x0000};


DecimalFormatStaticSets *DecimalFormatStaticSets::gStaticSets = NULL;

DecimalFormatStaticSets::DecimalFormatStaticSets(UErrorCode *status)
: fDotEquivalents(NULL),
  fCommaEquivalents(NULL),
  fOtherGroupingSeparators(NULL),
  fDashEquivalents(NULL),
  fStrictDotEquivalents(NULL),
  fStrictCommaEquivalents(NULL),
  fStrictOtherGroupingSeparators(NULL),
  fStrictDashEquivalents(NULL),
  fDefaultGroupingSeparators(NULL),
  fStrictDefaultGroupingSeparators(NULL)
{
    fDotEquivalents                = new UnicodeSet(UnicodeString(TRUE, gDotEquivalentsPattern, -1),                *status);
    fCommaEquivalents              = new UnicodeSet(UnicodeString(TRUE, gCommaEquivalentsPattern, -1),              *status);
    fOtherGroupingSeparators       = new UnicodeSet(UnicodeString(TRUE, gOtherGroupingSeparatorsPattern, -1),       *status);
    fDashEquivalents               = new UnicodeSet(UnicodeString(TRUE, gDashEquivalentsPattern, -1),               *status);
    
    fStrictDotEquivalents          = new UnicodeSet(UnicodeString(TRUE, gStrictDotEquivalentsPattern, -1),          *status);
    fStrictCommaEquivalents        = new UnicodeSet(UnicodeString(TRUE, gStrictCommaEquivalentsPattern, -1),        *status);
    fStrictOtherGroupingSeparators = new UnicodeSet(UnicodeString(TRUE, gStrictOtherGroupingSeparatorsPattern, -1), *status);
    fStrictDashEquivalents         = new UnicodeSet(UnicodeString(TRUE, gStrictDashEquivalentsPattern, -1),         *status);


    fDefaultGroupingSeparators = new UnicodeSet(*fDotEquivalents);
    fDefaultGroupingSeparators->addAll(*fCommaEquivalents);
    fDefaultGroupingSeparators->addAll(*fOtherGroupingSeparators);

    fStrictDefaultGroupingSeparators = new UnicodeSet(*fStrictDotEquivalents);
    fStrictDefaultGroupingSeparators->addAll(*fStrictCommaEquivalents);
    fStrictDefaultGroupingSeparators->addAll(*fStrictOtherGroupingSeparators);

    // Check for null pointers
    if (fDotEquivalents == NULL || fCommaEquivalents == NULL || fOtherGroupingSeparators == NULL || fDashEquivalents == NULL ||
        fStrictDotEquivalents == NULL || fStrictCommaEquivalents == NULL || fStrictOtherGroupingSeparators == NULL || fStrictDashEquivalents == NULL ||
        fDefaultGroupingSeparators == NULL || fStrictOtherGroupingSeparators == NULL) {
        goto ExitConstrDeleteAll;
    }

    // Freeze all the sets
    fDotEquivalents->freeze();
    fCommaEquivalents->freeze();
    fOtherGroupingSeparators->freeze();
    fDashEquivalents->freeze();
    fStrictDotEquivalents->freeze();
    fStrictCommaEquivalents->freeze();
    fStrictOtherGroupingSeparators->freeze();
    fStrictDashEquivalents->freeze();
    fDefaultGroupingSeparators->freeze();
    fStrictDefaultGroupingSeparators->freeze();

    return; // If we reached this point, everything is fine so just exit

ExitConstrDeleteAll: // Remove fPropSets and fRuleSets and return error
    delete fDotEquivalents; fDotEquivalents = NULL;
    delete fCommaEquivalents; fCommaEquivalents = NULL;
    delete fOtherGroupingSeparators; fOtherGroupingSeparators = NULL;
    delete fDashEquivalents; fDashEquivalents = NULL;
    delete fStrictDotEquivalents; fStrictDotEquivalents = NULL;
    delete fStrictCommaEquivalents; fStrictCommaEquivalents = NULL;
    delete fStrictOtherGroupingSeparators; fStrictOtherGroupingSeparators = NULL;
    delete fStrictDashEquivalents; fStrictDashEquivalents = NULL;
    delete fDefaultGroupingSeparators; fDefaultGroupingSeparators = NULL;
    delete fStrictDefaultGroupingSeparators; fStrictDefaultGroupingSeparators = NULL;
    delete fStrictOtherGroupingSeparators; fStrictOtherGroupingSeparators = NULL;

    *status = U_MEMORY_ALLOCATION_ERROR;
}


DecimalFormatStaticSets::~DecimalFormatStaticSets() {
    delete fDotEquivalents; fDotEquivalents = NULL;
    delete fCommaEquivalents; fCommaEquivalents = NULL;
    delete fOtherGroupingSeparators; fOtherGroupingSeparators = NULL;
    delete fDashEquivalents; fDashEquivalents = NULL;
    delete fStrictDotEquivalents; fStrictDotEquivalents = NULL;
    delete fStrictCommaEquivalents; fStrictCommaEquivalents = NULL;
    delete fStrictOtherGroupingSeparators; fStrictOtherGroupingSeparators = NULL;
    delete fStrictDashEquivalents; fStrictDashEquivalents = NULL;
    delete fDefaultGroupingSeparators; fDefaultGroupingSeparators = NULL;
    delete fStrictDefaultGroupingSeparators; fStrictDefaultGroupingSeparators = NULL;
    delete fStrictOtherGroupingSeparators; fStrictOtherGroupingSeparators = NULL;
}


//------------------------------------------------------------------------------
//
//   decfmt_cleanup     Memory cleanup function, free/delete all
//                      cached memory.  Called by ICU's u_cleanup() function.
//
//------------------------------------------------------------------------------
UBool
DecimalFormatStaticSets::cleanup(void)
{
    delete DecimalFormatStaticSets::gStaticSets;
    DecimalFormatStaticSets::gStaticSets = NULL;

    return TRUE;
}

U_CDECL_BEGIN
static UBool U_CALLCONV
decimfmt_cleanup(void)
{
    return DecimalFormatStaticSets::cleanup();
}
U_CDECL_END

void DecimalFormatStaticSets::initSets(UErrorCode *status)
{
    DecimalFormatStaticSets *p;

    UMTX_CHECK(NULL, gStaticSets, p);
    if (p == NULL) {
        p = new DecimalFormatStaticSets(status);

        if (p == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }

        if (U_FAILURE(*status)) {
            delete p;
            return;
        }

        umtx_lock(NULL);
        if (gStaticSets == NULL) {
            gStaticSets = p;
            p = NULL;
        }

        umtx_unlock(NULL);
        if (p != NULL) {
            delete p;
        }

        ucln_i18n_registerCleanup(UCLN_I18N_DECFMT, decimfmt_cleanup);
    }
}

const UnicodeSet *DecimalFormatStaticSets::getSimilarDecimals(UChar32 decimal, UBool strictParse)
{
    UErrorCode status = U_ZERO_ERROR;

    initSets(&status);

    if (U_FAILURE(status)) {
        return NULL;
    }

    if (gStaticSets->fDotEquivalents->contains(decimal)) {
        return strictParse ? gStaticSets->fStrictDotEquivalents : gStaticSets->fDotEquivalents;
    }

    if (gStaticSets->fCommaEquivalents->contains(decimal)) {
        return strictParse ? gStaticSets->fStrictCommaEquivalents : gStaticSets->fCommaEquivalents;
    }

    // if there is no match, return NULL
    return NULL;
}


U_NAMESPACE_END
#endif   // !UCONFIG_NO_FORMATTING
