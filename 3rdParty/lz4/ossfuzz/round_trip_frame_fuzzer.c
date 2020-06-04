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
#include "lz4frame.h"
#include "lz4_helpers.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint32_t seed = FUZZ_seed(&data, &size);
    LZ4F_preferences_t const prefs = FUZZ_randomPreferences(&seed);
    size_t const dstCapacity = LZ4F_compressFrameBound(size, &prefs);
    char* const dst = (char*)malloc(dstCapacity);
    char* const rt = (char*)malloc(size);

    FUZZ_ASSERT(dst);
    FUZZ_ASSERT(rt);

    /* Compression must succeed and round trip correctly. */
    size_t const dstSize =
            LZ4F_compressFrame(dst, dstCapacity, data, size, &prefs);
    FUZZ_ASSERT(!LZ4F_isError(dstSize));
    size_t const rtSize = FUZZ_decompressFrame(rt, size, dst, dstSize);
    FUZZ_ASSERT_MSG(rtSize == size, "Incorrect regenerated size");
    FUZZ_ASSERT_MSG(!memcmp(data, rt, size), "Corruption!");

    free(dst);
    free(rt);

    return 0;
}
