/**
 * This fuzz target attempts to compress the fuzzed data with the simple
 * compression function with an output buffer that may be too small to
 * ensure that the compressor never crashes.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "fuzz_helpers.h"
#include "lz4.h"
#include "lz4hc.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint32_t seed = FUZZ_seed(&data, &size);
    size_t const dstCapacity = FUZZ_rand32(&seed, 0, LZ4_compressBound(size));
    char* const dst = (char*)malloc(dstCapacity);
    char* const rt = (char*)malloc(size);
    int const level = FUZZ_rand32(&seed, LZ4HC_CLEVEL_MIN, LZ4HC_CLEVEL_MAX);

    FUZZ_ASSERT(dst);
    FUZZ_ASSERT(rt);

    /* If compression succeeds it must round trip correctly. */
    {
        int const dstSize = LZ4_compress_HC((const char*)data, dst, size,
                                            dstCapacity, level);
        if (dstSize > 0) {
            int const rtSize = LZ4_decompress_safe(dst, rt, dstSize, size);
            FUZZ_ASSERT_MSG(rtSize == size, "Incorrect regenerated size");
            FUZZ_ASSERT_MSG(!memcmp(data, rt, size), "Corruption!");
        }
    }

    if (dstCapacity > 0) {
        /* Compression succeeds and must round trip correctly. */
        void* state = malloc(LZ4_sizeofStateHC());
        FUZZ_ASSERT(state);
        int compressedSize = size;
        int const dstSize = LZ4_compress_HC_destSize(state, (const char*)data,
                                                     dst, &compressedSize,
                                                     dstCapacity, level);
        FUZZ_ASSERT(dstSize > 0);
        int const rtSize = LZ4_decompress_safe(dst, rt, dstSize, size);
        FUZZ_ASSERT_MSG(rtSize == compressedSize, "Incorrect regenerated size");
        FUZZ_ASSERT_MSG(!memcmp(data, rt, compressedSize), "Corruption!");
        free(state);
    }

    free(dst);
    free(rt);

    return 0;
}
