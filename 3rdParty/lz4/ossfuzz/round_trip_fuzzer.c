/**
 * This fuzz target performs a lz4 round-trip test (compress & decompress),
 * compares the result with the original, and calls abort() on corruption.
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
    size_t const dstCapacity = LZ4_compressBound(size);
    char* const dst = (char*)malloc(dstCapacity);
    char* const rt = (char*)malloc(size);

    FUZZ_ASSERT(dst);
    FUZZ_ASSERT(rt);

    /* Compression must succeed and round trip correctly. */
    int const dstSize = LZ4_compress_default((const char*)data, dst,
                                             size, dstCapacity);
    FUZZ_ASSERT(dstSize > 0);

    int const rtSize = LZ4_decompress_safe(dst, rt, dstSize, size);
    FUZZ_ASSERT_MSG(rtSize == size, "Incorrect size");
    FUZZ_ASSERT_MSG(!memcmp(data, rt, size), "Corruption!");

    /* Partial decompression must succeed. */
    {
        size_t const partialCapacity = FUZZ_rand32(&seed, 0, size);
        char* const partial = (char*)malloc(partialCapacity);
        FUZZ_ASSERT(partial);
        int const partialSize = LZ4_decompress_safe_partial(
                dst, partial, dstSize, partialCapacity, partialCapacity);
        FUZZ_ASSERT(partialSize >= 0);
        FUZZ_ASSERT_MSG(partialSize == partialCapacity, "Incorrect size");
        FUZZ_ASSERT_MSG(!memcmp(data, partial, partialSize), "Corruption!");
        free(partial);
    }

    free(dst);
    free(rt);

    return 0;
}
