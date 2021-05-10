// Copyright 2020 Google Inc. All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "snappy-test.h"

#include "snappy-internal.h"
#include "snappy-sinksource.h"
#include "snappy.h"
#include "snappy_test_data.h"

SNAPPY_FLAG(int32_t, start_len, -1,
            "Starting prefix size for testing (-1: just full file contents)");
SNAPPY_FLAG(int32_t, end_len, -1,
            "Starting prefix size for testing (-1: just full file contents)");
SNAPPY_FLAG(int32_t, bytes, 10485760,
            "How many bytes to compress/uncompress per file for timing");

SNAPPY_FLAG(bool, zlib, true,
            "Run zlib compression (http://www.zlib.net)");
SNAPPY_FLAG(bool, lzo, true,
            "Run LZO compression (http://www.oberhumer.com/opensource/lzo/)");
SNAPPY_FLAG(bool, lz4, true,
            "Run LZ4 compression (https://github.com/lz4/lz4)");
SNAPPY_FLAG(bool, snappy, true, "Run snappy compression");

SNAPPY_FLAG(bool, write_compressed, false,
            "Write compressed versions of each file to <file>.comp");
SNAPPY_FLAG(bool, write_uncompressed, false,
            "Write uncompressed versions of each file to <file>.uncomp");

namespace snappy {

namespace {

#if defined(HAVE_FUNC_MMAP) && defined(HAVE_FUNC_SYSCONF)

// To test against code that reads beyond its input, this class copies a
// string to a newly allocated group of pages, the last of which
// is made unreadable via mprotect. Note that we need to allocate the
// memory with mmap(), as POSIX allows mprotect() only on memory allocated
// with mmap(), and some malloc/posix_memalign implementations expect to
// be able to read previously allocated memory while doing heap allocations.
class DataEndingAtUnreadablePage {
 public:
  explicit DataEndingAtUnreadablePage(const std::string& s) {
    const size_t page_size = sysconf(_SC_PAGESIZE);
    const size_t size = s.size();
    // Round up space for string to a multiple of page_size.
    size_t space_for_string = (size + page_size - 1) & ~(page_size - 1);
    alloc_size_ = space_for_string + page_size;
    mem_ = mmap(NULL, alloc_size_,
                PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    CHECK_NE(MAP_FAILED, mem_);
    protected_page_ = reinterpret_cast<char*>(mem_) + space_for_string;
    char* dst = protected_page_ - size;
    std::memcpy(dst, s.data(), size);
    data_ = dst;
    size_ = size;
    // Make guard page unreadable.
    CHECK_EQ(0, mprotect(protected_page_, page_size, PROT_NONE));
  }

  ~DataEndingAtUnreadablePage() {
    const size_t page_size = sysconf(_SC_PAGESIZE);
    // Undo the mprotect.
    CHECK_EQ(0, mprotect(protected_page_, page_size, PROT_READ|PROT_WRITE));
    CHECK_EQ(0, munmap(mem_, alloc_size_));
  }

  const char* data() const { return data_; }
  size_t size() const { return size_; }

 private:
  size_t alloc_size_;
  void* mem_;
  char* protected_page_;
  const char* data_;
  size_t size_;
};

#else  // defined(HAVE_FUNC_MMAP) && defined(HAVE_FUNC_SYSCONF)

// Fallback for systems without mmap.
using DataEndingAtUnreadablePage = std::string;

#endif

enum CompressorType { ZLIB, LZO, LZ4, SNAPPY };

const char* names[] = {"ZLIB", "LZO", "LZ4", "SNAPPY"};

size_t MinimumRequiredOutputSpace(size_t input_size, CompressorType comp) {
  switch (comp) {
#ifdef ZLIB_VERSION
    case ZLIB:
      return ZLib::MinCompressbufSize(input_size);
#endif  // ZLIB_VERSION

#ifdef LZO_VERSION
    case LZO:
      return input_size + input_size/64 + 16 + 3;
#endif  // LZO_VERSION

#ifdef LZ4_VERSION_NUMBER
    case LZ4:
      return LZ4_compressBound(input_size);
#endif  // LZ4_VERSION_NUMBER

    case SNAPPY:
      return snappy::MaxCompressedLength(input_size);

    default:
      LOG(FATAL) << "Unknown compression type number " << comp;
      return 0;
  }
}

// Returns true if we successfully compressed, false otherwise.
//
// If compressed_is_preallocated is set, do not resize the compressed buffer.
// This is typically what you want for a benchmark, in order to not spend
// time in the memory allocator. If you do set this flag, however,
// "compressed" must be preinitialized to at least MinCompressbufSize(comp)
// number of bytes, and may contain junk bytes at the end after return.
bool Compress(const char* input, size_t input_size, CompressorType comp,
              std::string* compressed, bool compressed_is_preallocated) {
  if (!compressed_is_preallocated) {
    compressed->resize(MinimumRequiredOutputSpace(input_size, comp));
  }

  switch (comp) {
#ifdef ZLIB_VERSION
    case ZLIB: {
      ZLib zlib;
      uLongf destlen = compressed->size();
      int ret = zlib.Compress(
          reinterpret_cast<Bytef*>(string_as_array(compressed)),
          &destlen,
          reinterpret_cast<const Bytef*>(input),
          input_size);
      CHECK_EQ(Z_OK, ret);
      if (!compressed_is_preallocated) {
        compressed->resize(destlen);
      }
      return true;
    }
#endif  // ZLIB_VERSION

#ifdef LZO_VERSION
    case LZO: {
      unsigned char* mem = new unsigned char[LZO1X_1_15_MEM_COMPRESS];
      lzo_uint destlen;
      int ret = lzo1x_1_15_compress(
          reinterpret_cast<const uint8_t*>(input),
          input_size,
          reinterpret_cast<uint8_t*>(string_as_array(compressed)),
          &destlen,
          mem);
      CHECK_EQ(LZO_E_OK, ret);
      delete[] mem;
      if (!compressed_is_preallocated) {
        compressed->resize(destlen);
      }
      break;
    }
#endif  // LZO_VERSION

#ifdef LZ4_VERSION_NUMBER
    case LZ4: {
      int destlen = compressed->size();
      destlen = LZ4_compress_default(input, string_as_array(compressed),
                                     input_size, destlen);
      CHECK_NE(destlen, 0);
      if (!compressed_is_preallocated) {
        compressed->resize(destlen);
      }
      break;
    }
#endif  // LZ4_VERSION_NUMBER

    case SNAPPY: {
      size_t destlen;
      snappy::RawCompress(input, input_size,
                          string_as_array(compressed),
                          &destlen);
      CHECK_LE(destlen, snappy::MaxCompressedLength(input_size));
      if (!compressed_is_preallocated) {
        compressed->resize(destlen);
      }
      break;
    }

    default: {
      return false;     // the asked-for library wasn't compiled in
    }
  }
  return true;
}

bool Uncompress(const std::string& compressed, CompressorType comp, int size,
                std::string* output) {
  // TODO: Switch to [[maybe_unused]] when we can assume C++17.
  (void)size;
  switch (comp) {
#ifdef ZLIB_VERSION
    case ZLIB: {
      output->resize(size);
      ZLib zlib;
      uLongf destlen = output->size();
      int ret = zlib.Uncompress(
          reinterpret_cast<Bytef*>(string_as_array(output)),
          &destlen,
          reinterpret_cast<const Bytef*>(compressed.data()),
          compressed.size());
      CHECK_EQ(Z_OK, ret);
      CHECK_EQ(static_cast<uLongf>(size), destlen);
      break;
    }
#endif  // ZLIB_VERSION

#ifdef LZO_VERSION
    case LZO: {
      output->resize(size);
      lzo_uint destlen;
      int ret = lzo1x_decompress(
          reinterpret_cast<const uint8_t*>(compressed.data()),
          compressed.size(),
          reinterpret_cast<uint8_t*>(string_as_array(output)),
          &destlen,
          NULL);
      CHECK_EQ(LZO_E_OK, ret);
      CHECK_EQ(static_cast<lzo_uint>(size), destlen);
      break;
    }
#endif  // LZO_VERSION

#ifdef LZ4_VERSION_NUMBER
    case LZ4: {
      output->resize(size);
      int destlen = output->size();
      destlen = LZ4_decompress_safe(compressed.data(), string_as_array(output),
                                    compressed.size(), destlen);
      CHECK_NE(destlen, 0);
      CHECK_EQ(size, destlen);
      break;
    }
#endif  // LZ4_VERSION_NUMBER
    case SNAPPY: {
      snappy::RawUncompress(compressed.data(), compressed.size(),
                            string_as_array(output));
      break;
    }

    default: {
      return false;     // the asked-for library wasn't compiled in
    }
  }
  return true;
}

void Measure(const char* data, size_t length, CompressorType comp, int repeats,
             int block_size) {
  // Run tests a few time and pick median running times
  static const int kRuns = 5;
  double ctime[kRuns];
  double utime[kRuns];
  int compressed_size = 0;

  {
    // Chop the input into blocks
    int num_blocks = (length + block_size - 1) / block_size;
    std::vector<const char*> input(num_blocks);
    std::vector<size_t> input_length(num_blocks);
    std::vector<std::string> compressed(num_blocks);
    std::vector<std::string> output(num_blocks);
    for (int b = 0; b < num_blocks; ++b) {
      int input_start = b * block_size;
      int input_limit = std::min<int>((b+1)*block_size, length);
      input[b] = data+input_start;
      input_length[b] = input_limit-input_start;
    }

    // Pre-grow the output buffers so we don't measure string append time.
    for (std::string& compressed_block : compressed) {
      compressed_block.resize(MinimumRequiredOutputSpace(block_size, comp));
    }

    // First, try one trial compression to make sure the code is compiled in
    if (!Compress(input[0], input_length[0], comp, &compressed[0], true)) {
      LOG(WARNING) << "Skipping " << names[comp] << ": "
                   << "library not compiled in";
      return;
    }

    for (int run = 0; run < kRuns; ++run) {
      CycleTimer ctimer, utimer;

      // Pre-grow the output buffers so we don't measure string append time.
      for (std::string& compressed_block : compressed) {
        compressed_block.resize(MinimumRequiredOutputSpace(block_size, comp));
      }

      ctimer.Start();
      for (int b = 0; b < num_blocks; ++b) {
        for (int i = 0; i < repeats; ++i)
          Compress(input[b], input_length[b], comp, &compressed[b], true);
      }
      ctimer.Stop();

      // Compress once more, with resizing, so we don't leave junk
      // at the end that will confuse the decompressor.
      for (int b = 0; b < num_blocks; ++b) {
        Compress(input[b], input_length[b], comp, &compressed[b], false);
      }

      for (int b = 0; b < num_blocks; ++b) {
        output[b].resize(input_length[b]);
      }

      utimer.Start();
      for (int i = 0; i < repeats; ++i) {
        for (int b = 0; b < num_blocks; ++b)
          Uncompress(compressed[b], comp, input_length[b], &output[b]);
      }
      utimer.Stop();

      ctime[run] = ctimer.Get();
      utime[run] = utimer.Get();
    }

    compressed_size = 0;
    for (const std::string& compressed_item : compressed) {
      compressed_size += compressed_item.size();
    }
  }

  std::sort(ctime, ctime + kRuns);
  std::sort(utime, utime + kRuns);
  const int med = kRuns/2;

  float comp_rate = (length / ctime[med]) * repeats / 1048576.0;
  float uncomp_rate = (length / utime[med]) * repeats / 1048576.0;
  std::string x = names[comp];
  x += ":";
  std::string urate = (uncomp_rate >= 0) ? StrFormat("%.1f", uncomp_rate)
                                         : std::string("?");
  std::printf("%-7s [b %dM] bytes %6d -> %6d %4.1f%%  "
              "comp %5.1f MB/s  uncomp %5s MB/s\n",
              x.c_str(),
              block_size/(1<<20),
              static_cast<int>(length), static_cast<uint32_t>(compressed_size),
              (compressed_size * 100.0) / std::max<int>(1, length),
              comp_rate,
              urate.c_str());
}

void CompressFile(const char* fname) {
  std::string fullinput;
  CHECK_OK(file::GetContents(fname, &fullinput, file::Defaults()));

  std::string compressed;
  Compress(fullinput.data(), fullinput.size(), SNAPPY, &compressed, false);

  CHECK_OK(file::SetContents(std::string(fname).append(".comp"), compressed,
                             file::Defaults()));
}

void UncompressFile(const char* fname) {
  std::string fullinput;
  CHECK_OK(file::GetContents(fname, &fullinput, file::Defaults()));

  size_t uncompLength;
  CHECK(snappy::GetUncompressedLength(fullinput.data(), fullinput.size(),
                                      &uncompLength));

  std::string uncompressed;
  uncompressed.resize(uncompLength);
  CHECK(snappy::Uncompress(fullinput.data(), fullinput.size(), &uncompressed));

  CHECK_OK(file::SetContents(std::string(fname).append(".uncomp"), uncompressed,
                             file::Defaults()));
}

void MeasureFile(const char* fname) {
  std::string fullinput;
  CHECK_OK(file::GetContents(fname, &fullinput, file::Defaults()));
  std::printf("%-40s :\n", fname);

  int start_len = (snappy::GetFlag(FLAGS_start_len) < 0)
                      ? fullinput.size()
                      : snappy::GetFlag(FLAGS_start_len);
  int end_len = fullinput.size();
  if (snappy::GetFlag(FLAGS_end_len) >= 0) {
    end_len = std::min<int>(fullinput.size(), snappy::GetFlag(FLAGS_end_len));
  }
  for (int len = start_len; len <= end_len; ++len) {
    const char* const input = fullinput.data();
    int repeats = (snappy::GetFlag(FLAGS_bytes) + len) / (len + 1);
    if (snappy::GetFlag(FLAGS_zlib))
      Measure(input, len, ZLIB, repeats, 1024 << 10);
    if (snappy::GetFlag(FLAGS_lzo))
      Measure(input, len, LZO, repeats, 1024 << 10);
    if (snappy::GetFlag(FLAGS_lz4))
      Measure(input, len, LZ4, repeats, 1024 << 10);
    if (snappy::GetFlag(FLAGS_snappy))
      Measure(input, len, SNAPPY, repeats, 4096 << 10);

    // For block-size based measurements
    if (0 && snappy::GetFlag(FLAGS_snappy)) {
      Measure(input, len, SNAPPY, repeats, 8<<10);
      Measure(input, len, SNAPPY, repeats, 16<<10);
      Measure(input, len, SNAPPY, repeats, 32<<10);
      Measure(input, len, SNAPPY, repeats, 64<<10);
      Measure(input, len, SNAPPY, repeats, 256<<10);
      Measure(input, len, SNAPPY, repeats, 1024<<10);
    }
  }
}

}  // namespace

}  // namespace snappy

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  for (int arg = 1; arg < argc; ++arg) {
    if (snappy::GetFlag(FLAGS_write_compressed)) {
      snappy::CompressFile(argv[arg]);
    } else if (snappy::GetFlag(FLAGS_write_uncompressed)) {
      snappy::UncompressFile(argv[arg]);
    } else {
      snappy::MeasureFile(argv[arg]);
    }
  }
  return 0;
}
