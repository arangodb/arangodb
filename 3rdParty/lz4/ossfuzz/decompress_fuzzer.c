/**
 * This fuzz target attempts to decompress the fuzzed data with the simple
 * decompression function to ensure the decompressor never crashes.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "fuzz_helpers.h"
#include "lz4.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{

    uint32_t seed = FUZZ_seed(&data, &size);
    size_t const dstCapacity = FUZZ_rand32(&seed, 0, 4 * size);
    size_t const smallDictSize = size + 1;
    size_t const largeDictSize = 64 * 1024 - 1;
    size_t const dictSize = MAX(smallDictSize, largeDictSize);
    char* const dst = (char*)malloc(dstCapacity);
    char* const dict = (char*)malloc(dictSize + size);
    char* const largeDict = dict;
    char* const dataAfterDict = dict + dictSize;
    char* const smallDict = dataAfterDict - smallDictSize;

    FUZZ_ASSERT(dst);
    FUZZ_ASSERT(dict);

    /* Prepare the dictionary. The data doesn't matter for decompression. */
    memset(dict, 0, dictSize);
    memcpy(dataAfterDict, data, size);

    /* Decompress using each possible dictionary configuration. */
    /* No dictionary. */
    LZ4_decompress_safe_usingDict((char const*)data, dst, size,
                                  dstCapacity, NULL, 0);
    /* Small external dictonary. */
    LZ4_decompress_safe_usingDict((char const*)data, dst, size,
                                  dstCapacity, smallDict, smallDictSize);
    /* Large external dictionary. */
    LZ4_decompress_safe_usingDict((char const*)data, dst, size,
                                  dstCapacity, largeDict, largeDictSize);
    /* Small prefix. */
    LZ4_decompress_safe_usingDict((char const*)dataAfterDict, dst, size,
                                  dstCapacity, smallDict, smallDictSize);
    /* Large prefix. */
    LZ4_decompress_safe_usingDict((char const*)data, dst, size,
                                  dstCapacity, largeDict, largeDictSize);
    /* Partial decompression. */
    LZ4_decompress_safe_partial((char const*)data, dst, size,
                                dstCapacity, dstCapacity);
    free(dst);
    free(dict);

    return 0;
}
