// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 2000-2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File sscanf.c
*
* Modification History:
*
*   Date        Name        Description
*   02/08/00    george      Creation. Copied from uscanf.c
******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UCONFIG_NO_CONVERSION

#include "unicode/putil.h"
#include "unicode/ustdio.h"
#include "unicode/ustring.h"
#include "uscanf.h"
#include "ufile.h"
#include "ufmt_cmn.h"

#include "cmemory.h"
#include "cstring.h"


U_CAPI int32_t U_EXPORT2
u_sscanf(const char16_t   *buffer,
         const char    *patternSpecification,
         ... )
{
    va_list ap;
    int32_t converted;

    va_start(ap, patternSpecification);
    converted = u_vsscanf(buffer, patternSpecification, ap);
    va_end(ap);

    return converted;
}

U_CAPI int32_t U_EXPORT2
u_sscanf_u(const char16_t *buffer,
           const char16_t *patternSpecification,
           ... )
{
    va_list ap;
    int32_t converted;

    va_start(ap, patternSpecification);
    converted = u_vsscanf_u(buffer, patternSpecification, ap);
    va_end(ap);

    return converted;
}

U_CAPI int32_t  U_EXPORT2 /* U_CAPI ... U_EXPORT2 added by Peter Kirk 17 Nov 2001 */
u_vsscanf(const char16_t   *buffer,
          const char    *patternSpecification,
          va_list        ap)
{
    int32_t converted;
    char16_t *pattern;
    char16_t patBuffer[UFMT_DEFAULT_BUFFER_SIZE];
    int32_t size = (int32_t)uprv_strlen(patternSpecification) + 1;

    /* convert from the default codepage to Unicode */
    if (size >= (int32_t)MAX_UCHAR_BUFFER_SIZE(patBuffer)) {
        pattern = (char16_t *)uprv_malloc(size * sizeof(char16_t));
        if (pattern == nullptr) {
            return 0;
        }
    }
    else {
        pattern = patBuffer;
    }
    u_charsToUChars(patternSpecification, pattern, size);

    /* do the work */
    converted = u_vsscanf_u(buffer, pattern, ap);

    /* clean up */
    if (pattern != patBuffer) {
        uprv_free(pattern);
    }

    return converted;
}

U_CAPI int32_t U_EXPORT2 /* U_CAPI ... U_EXPORT2 added by Peter Kirk 17 Nov 2001 */
u_vsscanf_u(const char16_t *buffer,
            const char16_t *patternSpecification,
            va_list     ap)
{
    int32_t         converted;
    UFILE           inStr;

    inStr.fConverter = nullptr;
    inStr.fFile = nullptr;
    inStr.fOwnFile = false;
#if !UCONFIG_NO_TRANSLITERATION
    inStr.fTranslit = nullptr;
#endif
    inStr.fUCBuffer[0] = 0;
    inStr.str.fBuffer = (char16_t *)buffer;
    inStr.str.fPos = (char16_t *)buffer;
    inStr.str.fLimit = buffer + u_strlen(buffer);

    if (u_locbund_init(&inStr.str.fBundle, "en_US_POSIX") == nullptr) {
        return 0;
    }

    converted = u_scanf_parse(&inStr, patternSpecification, ap);

    u_locbund_close(&inStr.str.fBundle);

    /* return # of items converted */
    return converted;
}

#endif /* #if !UCONFIG_NO_FORMATTING */

