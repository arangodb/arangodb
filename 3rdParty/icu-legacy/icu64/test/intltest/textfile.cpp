// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
* Copyright (c) 2004,2011 International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: March 19 2004
* Since: ICU 3.0
**********************************************************************
*/
#include "textfile.h"
#include "cmemory.h"
#include "cstring.h"
#include "intltest.h"
#include "util.h"

// If the symbol CCP is defined, then the 'name' and 'encoding'
// constructor parameters are copied.  Otherwise they are aliased.
// #define CCP

TextFile::TextFile(const char* _name, const char* _encoding, UErrorCode& ec) :
    file(nullptr),
    name(nullptr), encoding(nullptr),
    buffer(nullptr),
    capacity(0),
    lineNo(0)
{
    if (U_FAILURE(ec) || _name == nullptr || _encoding == nullptr) {
        if (U_SUCCESS(ec)) {
            ec = U_ILLEGAL_ARGUMENT_ERROR; 
        }
        return;
    }

#ifdef CCP
    name = uprv_malloc(uprv_strlen(_name) + 1);
    encoding = uprv_malloc(uprv_strlen(_encoding) + 1);
    if (name == 0 || encoding == 0) {
        ec = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    uprv_strcpy(name, _name);
    uprv_strcpy(encoding, _encoding);
#else
    name = (char*) _name;
    encoding = (char*) _encoding;
#endif

    const char* testDir = IntlTest::getSourceTestData(ec);
    if (U_FAILURE(ec)) {
        return;
    }
    if (!ensureCapacity((int32_t)(uprv_strlen(testDir) + uprv_strlen(name) + 1))) {
        ec = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    uprv_strcpy(buffer, testDir);
    uprv_strcat(buffer, name);

    file = T_FileStream_open(buffer, "rb");
    if (file == nullptr) {
        ec = U_ILLEGAL_ARGUMENT_ERROR; 
        return;        
    }
}

TextFile::~TextFile() {
    if (file != nullptr) T_FileStream_close(file);
    if (buffer != nullptr) uprv_free(buffer);
#ifdef CCP
    uprv_free(name);
    uprv_free(encoding);
#endif
}

UBool TextFile::readLine(UnicodeString& line, UErrorCode& ec) {
    if (T_FileStream_eof(file)) {
        return false;
    }
    // Note: 'buffer' may change after ensureCapacity() is called,
    // so don't use 
    //   p=buffer; *p++=c;
    // but rather
    //   i=; buffer[i++]=c;
    int32_t n = 0;
    for (;;) {
        int c = T_FileStream_getc(file); // sic: int, not int32_t
        if (c < 0 || c == 0xD || c == 0xA) {
            // consume 0xA following 0xD
            if (c == 0xD) {
                c = T_FileStream_getc(file);
                if (c != 0xA && c >= 0) {
                    T_FileStream_ungetc(c, file);
                }
            }
            break;
        }
        if (!setBuffer(n++, c, ec)) return false;
    }
    if (!setBuffer(n++, 0, ec)) return false;
    UnicodeString str(buffer, encoding);
    // Remove BOM in first line, if present
    if (lineNo == 0 && str[0] == 0xFEFF) {
        str.remove(0, 1);
    }
    ++lineNo;
    line = str.unescape();
    return true;
}

UBool TextFile::readLineSkippingComments(UnicodeString& line, UErrorCode& ec,
                                         UBool trim) {
    for (;;) {
        if (!readLine(line, ec)) return false;
        // Skip over white space
        int32_t pos = 0;
        ICU_Utility::skipWhitespace(line, pos, true);
        // Ignore blank lines and comment lines
        if (pos == line.length() || line.charAt(pos) == 0x23/*'#'*/) {
            continue;
        }
        // Process line
        if (trim) line.remove(0, pos);
        return true;
    }
}

/**
 * Set buffer[index] to c, growing buffer if necessary. Return true if
 * successful.
 */
UBool TextFile::setBuffer(int32_t index, char c, UErrorCode& ec) {
    if (capacity <= index) {
        if (!ensureCapacity(index+1)) {
            ec = U_MEMORY_ALLOCATION_ERROR;
            return false;
        }
    }
    buffer[index] = c;
    return true;
}

/**
 * Make sure that 'buffer' has at least 'mincapacity' bytes.
 * Return true upon success. Upon return, 'buffer' may change
 * value. In any case, previous contents are preserved.
 */
 #define LOWEST_MIN_CAPACITY 64
UBool TextFile::ensureCapacity(int32_t mincapacity) {
    if (capacity >= mincapacity) {
        return true;
    }

    // Grow by factor of 2 to prevent frequent allocation
    // Note: 'capacity' may be 0
    int32_t i = (capacity < LOWEST_MIN_CAPACITY)? LOWEST_MIN_CAPACITY: capacity;
    while (i < mincapacity) {
        i <<= 1;
        if (i < 0) {
            i = 0x7FFFFFFF;
            break;
        }
    }
    mincapacity = i;

    // Simple realloc() no good; contents not preserved
    // Note: 'buffer' may be 0
    char* newbuffer = (char*) uprv_malloc(mincapacity);
    if (newbuffer == nullptr) {
        return false;
    }
    if (buffer != nullptr) {
        uprv_strncpy(newbuffer, buffer, capacity);
        uprv_free(buffer);
    }
    buffer = newbuffer;
    capacity = mincapacity;
    return true;
}

