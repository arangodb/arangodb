/**********************************************************************
 * © 2016 and later: Unicode, Inc. and others.
 * License & terms of use: http://www.unicode.org/copyright.html
 **********************************************************************
 **********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1999-2003, International Business Machines Corporation and
 * others. All Rights Reserved.
 **********************************************************************/

#include "unaccent.h"

const char UnaccentTransliterator::fgClassID = 0;

/**
 * Constructor
 */
UnaccentTransliterator::UnaccentTransliterator() :
    normalizer("", UNORM_NFD),
    Transliterator("Unaccent", nullptr) {
}

/**
 * Destructor
 */
UnaccentTransliterator::~UnaccentTransliterator() {
}

/**
 * Remove accents from a character using Normalizer.
 */
char16_t UnaccentTransliterator::unaccent(char16_t c) const {
    UnicodeString str(c);
    UErrorCode status = U_ZERO_ERROR;
    UnaccentTransliterator* t = (UnaccentTransliterator*)this;

    t->normalizer.setText(str, status);
    if (U_FAILURE(status)) {
        return c;
    }
    return (char16_t) t->normalizer.next();
}

/**
 * Implement Transliterator API
 */
void UnaccentTransliterator::handleTransliterate(Replaceable& text,
                                                 UTransPosition& index,
                                                 UBool incremental) const {
    UnicodeString str("a");
    while (index.start < index.limit) {
        char16_t c = text.charAt(index.start);
        char16_t d = unaccent(c);
        if (c != d) {
            str.setCharAt(0, d);
            text.handleReplaceBetween(index.start, index.start+1, str);
        }
        index.start++;
    }
}
