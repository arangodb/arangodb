// LZ4frame API example : compress a file
// Based on sample code from Zbigniew Jędrzejewski-Szmek

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <lz4frame.h>

#define BUF_SIZE 16*1024
#define LZ4_HEADER_SIZE 19
#define LZ4_FOOTER_SIZE 4

static const LZ4F_preferences_t lz4_preferences = {
    { LZ4F_max256KB, LZ4F_blockLinked, LZ4F_noContentChecksum, LZ4F_frame,
      0 /* content size unknown */, 0 /* no dictID */ , LZ4F_noBlockChecksum },
    0,   /* compression level */
    0,   /* autoflush */
    { 0, 0, 0, 0 },  /* reserved, must be set to 0 */
};

static size_t compress_file(FILE *in, FILE *out, size_t *size_in, size_t *size_out) {
    LZ4F_errorCode_t r;
    LZ4F_compressionContext_t ctx;
    char *src, *buf = NULL;
    size_t size, n, k, count_in = 0, count_out, offset = 0, frame_size;

    r = LZ4F_createCompressionContext(&ctx, LZ4F_VERSION);
    if (LZ4F_isError(r)) {
        printf("Failed to create context: error %zu\n", r);
        return 1;
    }
    r = 1;  /* function result; 1 == error, by default (early exit) */

    src = malloc(BUF_SIZE);
    if (!src) {
        printf("Not enough memory\n");
        goto cleanup;
    }

    frame_size = LZ4F_compressBound(BUF_SIZE, &lz4_preferences);
    size =  frame_size + LZ4_HEADER_SIZE + LZ4_FOOTER_SIZE;
    buf = malloc(size);
    if (!buf) {
        printf("Not enough memory\n");
        goto cleanup;
    }

    n = offset = count_out = LZ4F_compressBegin(ctx, buf, size, &lz4_preferences);
    if (LZ4F_isError(n)) {
        printf("Failed to start compression: error %zu\n", n);
        goto cleanup;
    }

    printf("Buffer size is %zu bytes, header size %zu bytes\n", size, n);

    for (;;) {
        k = fread(src, 1, BUF_SIZE, in);
        if (k == 0)
            break;
        count_in += k;

        n = LZ4F_compressUpdate(ctx, buf + offset, size - offset, src, k, NULL);
        if (LZ4F_isError(n)) {
            printf("Compression failed: error %zu\n", n);
            goto cleanup;
        }

        offset += n;
        count_out += n;
        if (size - offset < frame_size + LZ4_FOOTER_SIZE) {
            printf("Writing %zu bytes\n", offset);

            k = fwrite(buf, 1, offset, out);
            if (k < offset) {
                if (ferror(out))
                    printf("Write failed\n");
                else
                    printf("Short write\n");
                goto cleanup;
            }

            offset = 0;
        }
    }

    n = LZ4F_compressEnd(ctx, buf + offset, size - offset, NULL);
    if (LZ4F_isError(n)) {
        printf("Failed to end compression: error %zu\n", n);
        goto cleanup;
    }

    offset += n;
    count_out += n;
    printf("Writing %zu bytes\n", offset);

    k = fwrite(buf, 1, offset, out);
    if (k < offset) {
        if (ferror(out))
            printf("Write failed\n");
        else
            printf("Short write\n");
        goto cleanup;
    }

    *size_in = count_in;
    *size_out = count_out;
    r = 0;
 cleanup:
    if (ctx)
        LZ4F_freeCompressionContext(ctx);
    free(src);
    free(buf);
    return r;
}

static size_t get_block_size(const LZ4F_frameInfo_t* info) {
    switch (info->blockSizeID) {
        case LZ4F_default:
        case LZ4F_max64KB:  return 1 << 16;
        case LZ4F_max256KB: return 1 << 18;
        case LZ4F_max1MB:   return 1 << 20;
        case LZ4F_max4MB:   return 1 << 22;
        default:
            printf("Impossible unless more block sizes are allowed\n");
            exit(1);
    }
}

static size_t decompress_file(FILE *in, FILE *out) {
    void* const src = malloc(BUF_SIZE);
    void* dst = NULL;
    size_t dstCapacity = 0;
    LZ4F_dctx *dctx = NULL;
    size_t ret;

    /* Initialization */
    if (!src) { perror("decompress_file(src)"); goto cleanup; }
    ret = LZ4F_createDecompressionContext(&dctx, 100);
    if (LZ4F_isError(ret)) {
        printf("LZ4F_dctx creation error: %s\n", LZ4F_getErrorName(ret));
        goto cleanup;
    }

    /* Decompression */
    ret = 1;
    while (ret != 0) {
        /* Load more input */
        size_t srcSize = fread(src, 1, BUF_SIZE, in);
        void* srcPtr = src;
        void* srcEnd = srcPtr + srcSize;
        if (srcSize == 0 || ferror(in)) {
            printf("Decompress: not enough input or error reading file\n");
            goto cleanup;
        }
        /* Allocate destination buffer if it isn't already */
        if (!dst) {
            LZ4F_frameInfo_t info;
            ret = LZ4F_getFrameInfo(dctx, &info, src, &srcSize);
            if (LZ4F_isError(ret)) {
                printf("LZ4F_getFrameInfo error: %s\n", LZ4F_getErrorName(ret));
                goto cleanup;
            }
            /* Allocating enough space for an entire block isn't necessary for
             * correctness, but it allows some memcpy's to be elided.
             */
            dstCapacity = get_block_size(&info);
            dst = malloc(dstCapacity);
            if (!dst) { perror("decompress_file(dst)"); goto cleanup; }
            srcPtr += srcSize;
            srcSize = srcEnd - srcPtr;
        }
        /* Decompress:
         * Continue while there is more input to read and the frame isn't over.
         * If srcPtr == srcEnd then we know that there is no more output left in the
         * internal buffer left to flush.
         */
        while (srcPtr != srcEnd && ret != 0) {
            /* INVARIANT: Any data left in dst has already been written */
            size_t dstSize = dstCapacity;
            ret = LZ4F_decompress(dctx, dst, &dstSize, srcPtr, &srcSize, /* LZ4F_decompressOptions_t */ NULL);
            if (LZ4F_isError(ret)) {
                printf("Decompression error: %s\n", LZ4F_getErrorName(ret));
                goto cleanup;
            }
            /* Flush output */
            if (dstSize != 0){
                size_t written = fwrite(dst, 1, dstSize, out);
                printf("Writing %zu bytes\n", dstSize);
                if (written != dstSize) {
                    printf("Decompress: Failed to write to file\n");
                    goto cleanup;
                }
            }
            /* Update input */
            srcPtr += srcSize;
            srcSize = srcEnd - srcPtr;
        }
    }
    /* Check that there isn't trailing input data after the frame.
     * It is valid to have multiple frames in the same file, but this example
     * doesn't support it.
     */
    ret = fread(src, 1, 1, in);
    if (ret != 0 || !feof(in)) {
        printf("Decompress: Trailing data left in file after frame\n");
        goto cleanup;
    }

cleanup:
    free(src);
    free(dst);
    return LZ4F_freeDecompressionContext(dctx);   /* note : free works on NULL */
}

int compare(FILE* fp0, FILE* fp1)
{
    int result = 0;

    while(0 == result) {
        char b0[1024];
        char b1[1024];
        const size_t r0 = fread(b0, 1, sizeof(b0), fp0);
        const size_t r1 = fread(b1, 1, sizeof(b1), fp1);

        result = (int) r0 - (int) r1;

        if (0 == r0 || 0 == r1) {
            break;
        }
        if (0 == result) {
            result = memcmp(b0, b1, r0);
        }
    }

    return result;
}

int main(int argc, const char **argv) {
    char inpFilename[256] = { 0 };
    char lz4Filename[256] = { 0 };
    char decFilename[256] = { 0 };

    if(argc < 2) {
        printf("Please specify input filename\n");
        return 0;
    }

    snprintf(inpFilename, 256, "%s", argv[1]);
    snprintf(lz4Filename, 256, "%s.lz4", argv[1]);
    snprintf(decFilename, 256, "%s.lz4.dec", argv[1]);

    printf("inp = [%s]\n", inpFilename);
    printf("lz4 = [%s]\n", lz4Filename);
    printf("dec = [%s]\n", decFilename);

    /* compress */
    {   FILE* const inpFp = fopen(inpFilename, "rb");
        FILE* const outFp = fopen(lz4Filename, "wb");
        size_t sizeIn = 0;
        size_t sizeOut = 0;
        size_t ret;

        printf("compress : %s -> %s\n", inpFilename, lz4Filename);
        ret = compress_file(inpFp, outFp, &sizeIn, &sizeOut);
        if (ret) {
            printf("compress : failed with code %zu\n", ret);
            return (int)ret;
        }
        printf("%s: %zu → %zu bytes, %.1f%%\n",
            inpFilename, sizeIn, sizeOut,
            (double)sizeOut / sizeIn * 100);
        printf("compress : done\n");

        fclose(outFp);
        fclose(inpFp);
    }

    /* decompress */
    {   FILE* const inpFp = fopen(lz4Filename, "rb");
        FILE* const outFp = fopen(decFilename, "wb");
        size_t ret;

        printf("decompress : %s -> %s\n", lz4Filename, decFilename);
        ret = decompress_file(inpFp, outFp);
        if (ret) {
            printf("decompress : failed with code %zu\n", ret);
            return (int)ret;
        }
        printf("decompress : done\n");

        fclose(outFp);
        fclose(inpFp);
    }

    /* verify */
    {   FILE* const inpFp = fopen(inpFilename, "rb");
        FILE* const decFp = fopen(decFilename, "rb");

        printf("verify : %s <-> %s\n", inpFilename, decFilename);
        const int cmp = compare(inpFp, decFp);
        if(0 == cmp) {
            printf("verify : OK\n");
        } else {
            printf("verify : NG\n");
        }

        fclose(decFp);
        fclose(inpFp);
    }

    return 0;
}
