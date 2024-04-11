// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
* Copyright (c) 2004-2011, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: March 22 2004
* Since: ICU 3.0
**********************************************************************
*/
#include "tokiter.h"
#include "textfile.h"
#include "patternprops.h"
#include "util.h"
#include "uprops.h"

TokenIterator::TokenIterator(TextFile* r) {
    reader = r;
    done = haveLine = false;
    pos = lastpos = -1;
}

TokenIterator::~TokenIterator() {
}

UBool TokenIterator::next(UnicodeString& token, UErrorCode& ec) {
    if (done || U_FAILURE(ec)) {
        return false;
    }
    token.truncate(0);
    for (;;) {
        if (!haveLine) {
            if (!reader->readLineSkippingComments(line, ec)) {
                done = true;
                return false;
            }
            haveLine = true;
            pos = 0;
        }
        lastpos = pos;
        if (!nextToken(token, ec)) {
            haveLine = false;
            if (U_FAILURE(ec)) return false;
            continue;
        }
        return true;
    }
}

int32_t TokenIterator::getLineNumber() const {
    return reader->getLineNumber();
}

/**
 * Read the next token from 'this->line' and append it to 'token'.
 * Tokens are separated by Pattern_White_Space.  Tokens may also be
 * delimited by double or single quotes.  The closing quote must match
 * the opening quote.  If a '#' is encountered, the rest of the line
 * is ignored, unless it is backslash-escaped or within quotes.
 * @param token the token is appended to this StringBuffer
 * @param ec input-output error code
 * @return true if a valid token is found, or false if the end
 * of the line is reached or an error occurs
 */
UBool TokenIterator::nextToken(UnicodeString& token, UErrorCode& ec) {
    ICU_Utility::skipWhitespace(line, pos, true);
    if (pos == line.length()) {
        return false;
    }
    char16_t c = line.charAt(pos++);
    char16_t quote = 0;
    switch (c) {
    case 34/*'"'*/:
    case 39/*'\\'*/:
        quote = c;
        break;
    case 35/*'#'*/:
        return false;
    default:
        token.append(c);
        break;
    }
    while (pos < line.length()) {
        c = line.charAt(pos); // 16-bit ok
        if (c == 92/*'\\'*/) {
            UChar32 c32 = line.unescapeAt(pos);
            if (c32 < 0) {
                ec = U_MALFORMED_UNICODE_ESCAPE;
                return false;
            }
            token.append(c32);
        } else if ((quote != 0 && c == quote) ||
                   (quote == 0 && PatternProps::isWhiteSpace(c))) {
            ++pos;
            return true;
        } else if (quote == 0 && c == '#') {
            return true; // do NOT increment
        } else {
            token.append(c);
            ++pos;
        }
    }
    if (quote != 0) {
        ec = U_UNTERMINATED_QUOTE;
        return false;
    }
    return true;
}
