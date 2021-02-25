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
#include "lz4frame.h"
#include "lz4_helpers.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint32_t seed = FUZZ_seed(&data, &size);
    LZ4F_preferences_t const prefs = FUZZ_randomPreferences(&seed);
    size_t const compressBound = LZ4F_compressFrameBound(size, &prefs);
    size_t const dstCapacity = FUZZ_rand32(&seed, 0, compressBound);
    char* const dst = (char*)malloc(dstCapacity);
    char* const rt = (char*)malloc(size);

    FUZZ_ASSERT(dst);
    FUZZ_ASSERT(rt);

    /* If compression succeeds it must round trip correctly. */
    size_t const dstSize =
            LZ4F_compressFrame(dst, dstCapacity, data, size, &prefs);
    if (!LZ4F_isError(dstSize)) {
        size_t const rtSize = FUZZ_decompressFrame(rt, size, dst, dstSize);
        FUZZ_ASSERT_MSG(rtSize == size, "Incorrect regenerated size");
        FUZZ_ASSERT_MSG(!memcmp(data, rt, size), "Corruption!");
    }

    free(dst);
    free(rt);

    return 0;
}
