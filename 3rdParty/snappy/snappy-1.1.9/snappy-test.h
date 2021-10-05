// Copyright 2011 Google Inc. All Rights Reserved.
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
//
// Various stubs for the unit tests for the open-source version of Snappy.

#ifndef THIRD_PARTY_SNAPPY_OPENSOURCE_SNAPPY_TEST_H_
#define THIRD_PARTY_SNAPPY_OPENSOURCE_SNAPPY_TEST_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "snappy-stubs-internal.h"

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_WINDOWS_H
// Needed to be able to use std::max without workarounds in the source code.
// https://support.microsoft.com/en-us/help/143208/prb-using-stl-in-windows-program-can-cause-min-max-conflicts
#define NOMINMAX
#include <windows.h>
#endif

#define InitGoogle(argv0, argc, argv, remove_flags) ((void)(0))

#ifdef HAVE_LIBZ
#include "zlib.h"
#endif

#ifdef HAVE_LIBLZO2
#include "lzo/lzo1x.h"
#endif

#ifdef HAVE_LIBLZ4
#include "lz4.h"
#endif

namespace file {

// Stubs the class file::Options.
//
// This class should not be instantiated explicitly. It should only be used by
// passing file::Defaults() to file::GetContents() / file::SetContents().
class OptionsStub {
 public:
  OptionsStub();
  OptionsStub(const OptionsStub &) = delete;
  OptionsStub &operator=(const OptionsStub &) = delete;
  ~OptionsStub();
};

const OptionsStub &Defaults();

// Stubs the class absl::Status.
//
// This class should not be instantiated explicitly. It should only be used by
// passing the result of file::GetContents() / file::SetContents() to
// CHECK_OK().
class StatusStub {
 public:
  StatusStub();
  StatusStub(const StatusStub &);
  StatusStub &operator=(const StatusStub &);
  ~StatusStub();

  bool ok();
};

StatusStub GetContents(const std::string &file_name, std::string *output,
                       const OptionsStub & /* options */);

StatusStub SetContents(const std::string &file_name, const std::string &content,
                       const OptionsStub & /* options */);

}  // namespace file

namespace snappy {

#define FLAGS_test_random_seed 301

std::string ReadTestDataFile(const std::string& base, size_t size_limit);

// A std::sprintf() variant that returns a std::string.
// Not safe for general use due to truncation issues.
std::string StrFormat(const char* format, ...);

// A wall-time clock. This stub is not super-accurate, nor resistant to the
// system time changing.
class CycleTimer {
 public:
  inline CycleTimer() : real_time_us_(0) {}
  inline ~CycleTimer() = default;

  inline void Start() {
#ifdef WIN32
    QueryPerformanceCounter(&start_);
#else
    ::gettimeofday(&start_, nullptr);
#endif
  }

  inline void Stop() {
#ifdef WIN32
    LARGE_INTEGER stop;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&stop);
    QueryPerformanceFrequency(&frequency);

    double elapsed = static_cast<double>(stop.QuadPart - start_.QuadPart) /
        frequency.QuadPart;
    real_time_us_ += elapsed * 1e6 + 0.5;
#else
    struct ::timeval stop;
    ::gettimeofday(&stop, nullptr);

    real_time_us_ += 1000000 * (stop.tv_sec - start_.tv_sec);
    real_time_us_ += (stop.tv_usec - start_.tv_usec);
#endif
  }

  inline double Get() { return real_time_us_ * 1e-6; }

 private:
  int64_t real_time_us_;
#ifdef WIN32
  LARGE_INTEGER start_;
#else
  struct ::timeval start_;
#endif
};

// Logging.

class LogMessage {
 public:
  inline LogMessage() = default;
  ~LogMessage();

  LogMessage &operator<<(const std::string &message);
  LogMessage &operator<<(int number);
};

class LogMessageCrash : public LogMessage {
 public:
  inline LogMessageCrash() = default;
  ~LogMessageCrash();
};

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".

class LogMessageVoidify {
 public:
  inline LogMessageVoidify() = default;
  inline ~LogMessageVoidify() = default;

  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  inline void operator&(const LogMessage &) {}
};

// Asserts, both versions activated in debug mode only,
// and ones that are always active.

#define CRASH_UNLESS(condition)  \
  SNAPPY_PREDICT_TRUE(condition) \
      ? (void)0                  \
      : snappy::LogMessageVoidify() & snappy::LogMessageCrash()

#define LOG(level) LogMessage()
#define VLOG(level) \
  true ? (void)0 : snappy::LogMessageVoidify() & snappy::LogMessage()

#define CHECK(cond) CRASH_UNLESS(cond)
#define CHECK_LE(a, b) CRASH_UNLESS((a) <= (b))
#define CHECK_GE(a, b) CRASH_UNLESS((a) >= (b))
#define CHECK_EQ(a, b) CRASH_UNLESS((a) == (b))
#define CHECK_NE(a, b) CRASH_UNLESS((a) != (b))
#define CHECK_LT(a, b) CRASH_UNLESS((a) < (b))
#define CHECK_GT(a, b) CRASH_UNLESS((a) > (b))
#define CHECK_OK(cond) (cond).ok()

#ifdef HAVE_LIBZ

// Object-oriented wrapper around zlib.
class ZLib {
 public:
  ZLib();
  ~ZLib();

  // Wipe a ZLib object to a virgin state.  This differs from Reset()
  // in that it also breaks any state.
  void Reinit();

  // Call this to make a zlib buffer as good as new.  Here's the only
  // case where they differ:
  //    CompressChunk(a); CompressChunk(b); CompressChunkDone();   vs
  //    CompressChunk(a); Reset(); CompressChunk(b); CompressChunkDone();
  // You'll want to use Reset(), then, when you interrupt a compress
  // (or uncompress) in the middle of a chunk and want to start over.
  void Reset();

  // According to the zlib manual, when you Compress, the destination
  // buffer must have size at least src + .1%*src + 12.  This function
  // helps you calculate that.  Augment this to account for a potential
  // gzip header and footer, plus a few bytes of slack.
  static int MinCompressbufSize(int uncompress_size) {
    return uncompress_size + uncompress_size/1000 + 40;
  }

  // Compresses the source buffer into the destination buffer.
  // sourceLen is the byte length of the source buffer.
  // Upon entry, destLen is the total size of the destination buffer,
  // which must be of size at least MinCompressbufSize(sourceLen).
  // Upon exit, destLen is the actual size of the compressed buffer.
  //
  // This function can be used to compress a whole file at once if the
  // input file is mmap'ed.
  //
  // Returns Z_OK if success, Z_MEM_ERROR if there was not
  // enough memory, Z_BUF_ERROR if there was not enough room in the
  // output buffer. Note that if the output buffer is exactly the same
  // size as the compressed result, we still return Z_BUF_ERROR.
  // (check CL#1936076)
  int Compress(Bytef *dest, uLongf *destLen,
               const Bytef *source, uLong sourceLen);

  // Uncompresses the source buffer into the destination buffer.
  // The destination buffer must be long enough to hold the entire
  // decompressed contents.
  //
  // Returns Z_OK on success, otherwise, it returns a zlib error code.
  int Uncompress(Bytef *dest, uLongf *destLen,
                 const Bytef *source, uLong sourceLen);

  // Uncompress data one chunk at a time -- ie you can call this
  // more than once.  To get this to work you need to call per-chunk
  // and "done" routines.
  //
  // Returns Z_OK if success, Z_MEM_ERROR if there was not
  // enough memory, Z_BUF_ERROR if there was not enough room in the
  // output buffer.

  int UncompressAtMost(Bytef *dest, uLongf *destLen,
                       const Bytef *source, uLong *sourceLen);

  // Checks gzip footer information, as needed.  Mostly this just
  // makes sure the checksums match.  Whenever you call this, it
  // will assume the last 8 bytes from the previous UncompressChunk
  // call are the footer.  Returns true iff everything looks ok.
  bool UncompressChunkDone();

 private:
  int InflateInit();       // sets up the zlib inflate structure
  int DeflateInit();       // sets up the zlib deflate structure

  // These init the zlib data structures for compressing/uncompressing
  int CompressInit(Bytef *dest, uLongf *destLen,
                   const Bytef *source, uLong *sourceLen);
  int UncompressInit(Bytef *dest, uLongf *destLen,
                     const Bytef *source, uLong *sourceLen);
  // Initialization method to be called if we hit an error while
  // uncompressing. On hitting an error, call this method before
  // returning the error.
  void UncompressErrorInit();

  // Helper function for Compress
  int CompressChunkOrAll(Bytef *dest, uLongf *destLen,
                         const Bytef *source, uLong sourceLen,
                         int flush_mode);
  int CompressAtMostOrAll(Bytef *dest, uLongf *destLen,
                          const Bytef *source, uLong *sourceLen,
                          int flush_mode);

  // Likewise for UncompressAndUncompressChunk
  int UncompressChunkOrAll(Bytef *dest, uLongf *destLen,
                           const Bytef *source, uLong sourceLen,
                           int flush_mode);

  int UncompressAtMostOrAll(Bytef *dest, uLongf *destLen,
                            const Bytef *source, uLong *sourceLen,
                            int flush_mode);

  // Initialization method to be called if we hit an error while
  // compressing. On hitting an error, call this method before
  // returning the error.
  void CompressErrorInit();

  int compression_level_;   // compression level
  int window_bits_;         // log base 2 of the window size used in compression
  int mem_level_;           // specifies the amount of memory to be used by
                            // compressor (1-9)
  z_stream comp_stream_;    // Zlib stream data structure
  bool comp_init_;          // True if we have initialized comp_stream_
  z_stream uncomp_stream_;  // Zlib stream data structure
  bool uncomp_init_;        // True if we have initialized uncomp_stream_

  // These are used only with chunked compression.
  bool first_chunk_;       // true if we need to emit headers with this chunk
};

#endif  // HAVE_LIBZ

}  // namespace snappy

#endif  // THIRD_PARTY_SNAPPY_OPENSOURCE_SNAPPY_TEST_H_
