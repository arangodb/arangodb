/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* simplepatternformatter.cpp
*/
#include "simplepatternformatter.h"
#include "cstring.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

typedef enum SimplePatternFormatterCompileState {
    INIT,
    APOSTROPHE,
    PLACEHOLDER
} SimplePatternFormatterCompileState;

class SimplePatternFormatterIdBuilder {
public:
    SimplePatternFormatterIdBuilder() : id(0), idLen(0) { }
    ~SimplePatternFormatterIdBuilder() { }
    void reset() { id = 0; idLen = 0; }
    int32_t getId() const { return id; }
    void appendTo(UChar *buffer, int32_t *len) const;
    UBool isValid() const { return (idLen > 0); }
    void add(UChar ch);
private:
    int32_t id;
    int32_t idLen;
    SimplePatternFormatterIdBuilder(
            const SimplePatternFormatterIdBuilder &other);
    SimplePatternFormatterIdBuilder &operator=(
            const SimplePatternFormatterIdBuilder &other);
};

void SimplePatternFormatterIdBuilder::appendTo(
        UChar *buffer, int32_t *len) const {
    int32_t origLen = *len;
    int32_t kId = id;
    for (int32_t i = origLen + idLen - 1; i >= origLen; i--) {
        int32_t digit = kId % 10;
        buffer[i] = digit + 0x30;
        kId /= 10;
    }
    *len = origLen + idLen;
}

void SimplePatternFormatterIdBuilder::add(UChar ch) {
    id = id * 10 + (ch - 0x30);
    idLen++;
}

SimplePatternFormatter::SimplePatternFormatter() :
        noPlaceholders(),
        placeholders(),
        placeholderSize(0),
        placeholderCount(0) {
}

SimplePatternFormatter::SimplePatternFormatter(const UnicodeString &pattern) :
        noPlaceholders(),
        placeholders(),
        placeholderSize(0),
        placeholderCount(0) {
    UErrorCode status = U_ZERO_ERROR;
    compile(pattern, status);
}

SimplePatternFormatter::SimplePatternFormatter(
        const SimplePatternFormatter &other) :
        noPlaceholders(other.noPlaceholders),
        placeholders(),
        placeholderSize(0),
        placeholderCount(other.placeholderCount) {
    placeholderSize = ensureCapacity(other.placeholderSize);
    uprv_memcpy(
            placeholders.getAlias(),
            other.placeholders.getAlias(),
            placeholderSize * sizeof(PlaceholderInfo));
}

SimplePatternFormatter &SimplePatternFormatter::operator=(
        const SimplePatternFormatter& other) {
    if (this == &other) {
        return *this;
    }
    noPlaceholders = other.noPlaceholders;
    placeholderSize = ensureCapacity(other.placeholderSize);
    placeholderCount = other.placeholderCount;
    uprv_memcpy(
            placeholders.getAlias(),
            other.placeholders.getAlias(),
            placeholderSize * sizeof(PlaceholderInfo));
    return *this;
}

SimplePatternFormatter::~SimplePatternFormatter() {
}

UBool SimplePatternFormatter::compile(
        const UnicodeString &pattern, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return FALSE;
    }
    const UChar *patternBuffer = pattern.getBuffer();
    int32_t patternLength = pattern.length();
    UChar *buffer = noPlaceholders.getBuffer(patternLength);
    int32_t len = 0;
    placeholderSize = 0;
    placeholderCount = 0;
    SimplePatternFormatterCompileState state = INIT;
    SimplePatternFormatterIdBuilder idBuilder;
    for (int32_t i = 0; i < patternLength; ++i) {
        UChar ch = patternBuffer[i];
        switch (state) {
        case INIT:
            if (ch == 0x27) {
                state = APOSTROPHE;
            } else if (ch == 0x7B) {
                state = PLACEHOLDER;
                idBuilder.reset();
            } else {
               buffer[len++] = ch;
            }
            break;
        case APOSTROPHE:
            if (ch == 0x27) {
                buffer[len++] = 0x27;
            } else if (ch == 0x7B) {
                buffer[len++] = 0x7B;
            } else {
                buffer[len++] = 0x27;
                buffer[len++] = ch;
            }
            state = INIT;
            break;
        case PLACEHOLDER:
            if (ch >= 0x30 && ch <= 0x39) {
                idBuilder.add(ch);
            } else if (ch == 0x7D && idBuilder.isValid()) {
                if (!addPlaceholder(idBuilder.getId(), len)) {
                    status = U_MEMORY_ALLOCATION_ERROR;
                    return FALSE;
                }
                state = INIT;
            } else {
                buffer[len++] = 0x7B;
                idBuilder.appendTo(buffer, &len);
                buffer[len++] = ch;
                state = INIT;
            }
            break;
        default:
            U_ASSERT(FALSE);
            break;
        }
    }
    switch (state) {
    case INIT:
        break;
    case APOSTROPHE:
        buffer[len++] = 0x27;
        break;
    case PLACEHOLDER:
        buffer[len++] = 0X7B;
        idBuilder.appendTo(buffer, &len);
        break;
    default:
        U_ASSERT(false);
        break;
    }
    noPlaceholders.releaseBuffer(len);
    return TRUE;
}

UBool SimplePatternFormatter::startsWithPlaceholder(int32_t id) const {
    if (placeholderSize == 0) {
        return FALSE;
    }
    return (placeholders[0].offset == 0 && placeholders[0].id == id);
}

UnicodeString& SimplePatternFormatter::format(
        const UnicodeString &arg0,
        UnicodeString &appendTo,
        UErrorCode &status) const {
    const UnicodeString *params[] = {&arg0};
    return format(
            params,
            UPRV_LENGTHOF(params),
            appendTo,
            NULL,
            0,
            status);
}

UnicodeString& SimplePatternFormatter::format(
        const UnicodeString &arg0,
        const UnicodeString &arg1,
        UnicodeString &appendTo,
        UErrorCode &status) const {
    const UnicodeString *params[] = {&arg0, &arg1};
    return format(
            params,
            UPRV_LENGTHOF(params),
            appendTo,
            NULL,
            0,
            status);
}

UnicodeString& SimplePatternFormatter::format(
        const UnicodeString &arg0,
        const UnicodeString &arg1,
        const UnicodeString &arg2,
        UnicodeString &appendTo,
        UErrorCode &status) const {
    const UnicodeString *params[] = {&arg0, &arg1, &arg2};
    return format(
            params,
            UPRV_LENGTHOF(params),
            appendTo,
            NULL,
            0,
            status);
}

static void updatePlaceholderOffset(
        int32_t placeholderId,
        int32_t placeholderOffset,
        int32_t *offsetArray,
        int32_t offsetArrayLength) {
    if (placeholderId < offsetArrayLength) {
        offsetArray[placeholderId] = placeholderOffset;
    }
}

static void appendRange(
        const UnicodeString &src,
        int32_t start,
        int32_t end,
        UnicodeString &dest) {
    dest.append(src, start, end - start);
}

UnicodeString& SimplePatternFormatter::format(
        const UnicodeString * const *placeholderValues,
        int32_t placeholderValueCount,
        UnicodeString &appendTo,
        int32_t *offsetArray,
        int32_t offsetArrayLength,
        UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return appendTo;
    }
    if (placeholderValueCount < placeholderCount) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return appendTo;
    }
    for (int32_t i = 0; i < offsetArrayLength; ++i) {
        offsetArray[i] = -1;
    }
    if (placeholderSize == 0) {
        appendTo.append(noPlaceholders);
        return appendTo;
    }
    if (placeholders[0].offset > 0 ||
            placeholderValues[placeholders[0].id] != &appendTo) {
        appendRange(
                noPlaceholders,
                0,
                placeholders[0].offset,
                appendTo);
        updatePlaceholderOffset(
                placeholders[0].id,
                appendTo.length(),
                offsetArray,
                offsetArrayLength);
        appendTo.append(*placeholderValues[placeholders[0].id]);
    } else {
        updatePlaceholderOffset(
                placeholders[0].id,
                0,
                offsetArray,
                offsetArrayLength);
    }
    for (int32_t i = 1; i < placeholderSize; ++i) {
        appendRange(
                noPlaceholders,
                placeholders[i - 1].offset,
                placeholders[i].offset,
                appendTo);
        updatePlaceholderOffset(
                placeholders[i].id,
                appendTo.length(),
                offsetArray,
                offsetArrayLength);
        appendTo.append(*placeholderValues[placeholders[i].id]);
    }
    appendRange(
            noPlaceholders,
            placeholders[placeholderSize - 1].offset,
            noPlaceholders.length(),
            appendTo);
    return appendTo;
}

int32_t SimplePatternFormatter::ensureCapacity(
        int32_t desiredCapacity, int32_t allocationSize) {
    if (allocationSize < desiredCapacity) {
        allocationSize = desiredCapacity;
    }
    if (desiredCapacity <= placeholders.getCapacity()) {
        return desiredCapacity;
    }
    // allocate new buffer
    if (placeholders.resize(allocationSize, placeholderSize) == NULL) {
        return placeholders.getCapacity();
    }
    return desiredCapacity;
}

UBool SimplePatternFormatter::addPlaceholder(int32_t id, int32_t offset) {
    if (ensureCapacity(placeholderSize + 1, 2 * placeholderSize) < placeholderSize + 1) {
        return FALSE;
    }
    ++placeholderSize;
    PlaceholderInfo *placeholderEnd = &placeholders[placeholderSize - 1];
    placeholderEnd->offset = offset;
    placeholderEnd->id = id;
    if (id >= placeholderCount) {
        placeholderCount = id + 1;
    }
    return TRUE;
}
    
U_NAMESPACE_END
