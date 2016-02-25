/*
*******************************************************************************
* Copyright (C) 2013-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationinfo.cpp
*
* created on: 2013aug05
* created by: Markus W. Scherer
*/

#include <stdio.h>
#include <string.h>

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "collationdatareader.h"
#include "collationinfo.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

void
CollationInfo::printSizes(int32_t sizeWithHeader, const int32_t indexes[]) {
    int32_t totalSize = indexes[CollationDataReader::IX_TOTAL_SIZE];
    if(sizeWithHeader > totalSize) {
        printf("  header size:                  %6ld\n", (long)(sizeWithHeader - totalSize));
    }

    int32_t length = indexes[CollationDataReader::IX_INDEXES_LENGTH];
    printf("  indexes:          %6ld *4 = %6ld\n", (long)length, (long)length * 4);

    length = getDataLength(indexes, CollationDataReader::IX_REORDER_CODES_OFFSET);
    if(length != 0) {
        printf("  reorder codes:    %6ld *4 = %6ld\n", (long)length / 4, (long)length);
    }

    length = getDataLength(indexes, CollationDataReader::IX_REORDER_TABLE_OFFSET);
    if(length != 0) {
        U_ASSERT(length >= 256);
        printf("  reorder table:                %6ld\n", (long)length);
    }

    length = getDataLength(indexes, CollationDataReader::IX_TRIE_OFFSET);
    if(length != 0) {
        printf("  trie size:                    %6ld\n", (long)length);
    }

    length = getDataLength(indexes, CollationDataReader::IX_RESERVED8_OFFSET);
    if(length != 0) {
        printf("  reserved (offset 8):          %6ld\n", (long)length);
    }

    length = getDataLength(indexes, CollationDataReader::IX_CES_OFFSET);
    if(length != 0) {
        printf("  CEs:              %6ld *8 = %6ld\n", (long)length / 8, (long)length);
    }

    length = getDataLength(indexes, CollationDataReader::IX_RESERVED10_OFFSET);
    if(length != 0) {
        printf("  reserved (offset 10):         %6ld\n", (long)length);
    }

    length = getDataLength(indexes, CollationDataReader::IX_CE32S_OFFSET);
    if(length != 0) {
        printf("  CE32s:            %6ld *4 = %6ld\n", (long)length / 4, (long)length);
    }

    length = getDataLength(indexes, CollationDataReader::IX_ROOT_ELEMENTS_OFFSET);
    if(length != 0) {
        printf("  rootElements:     %6ld *4 = %6ld\n", (long)length / 4, (long)length);
    }

    length = getDataLength(indexes, CollationDataReader::IX_CONTEXTS_OFFSET);
    if(length != 0) {
        printf("  contexts:         %6ld *2 = %6ld\n", (long)length / 2, (long)length);
    }

    length = getDataLength(indexes, CollationDataReader::IX_UNSAFE_BWD_OFFSET);
    if(length != 0) {
        printf("  unsafeBwdSet:     %6ld *2 = %6ld\n", (long)length / 2, (long)length);
    }

    length = getDataLength(indexes, CollationDataReader::IX_FAST_LATIN_TABLE_OFFSET);
    if(length != 0) {
        printf("  fastLatin table:  %6ld *2 = %6ld\n", (long)length / 2, (long)length);
    }

    length = getDataLength(indexes, CollationDataReader::IX_SCRIPTS_OFFSET);
    if(length != 0) {
        printf("  scripts data:     %6ld *2 = %6ld\n", (long)length / 2, (long)length);
    }

    length = getDataLength(indexes, CollationDataReader::IX_COMPRESSIBLE_BYTES_OFFSET);
    if(length != 0) {
        U_ASSERT(length >= 256);
        printf("  compressibleBytes:            %6ld\n", (long)length);
    }

    length = getDataLength(indexes, CollationDataReader::IX_RESERVED18_OFFSET);
    if(length != 0) {
        printf("  reserved (offset 18):         %6ld\n", (long)length);
    }

    printf(" collator binary total size:    %6ld\n", (long)sizeWithHeader);
}

int32_t
CollationInfo::getDataLength(const int32_t indexes[], int32_t startIndex) {
    return indexes[startIndex + 1] - indexes[startIndex];
}

U_NAMESPACE_END

#endif  // !UCONFIG_NO_COLLATION
