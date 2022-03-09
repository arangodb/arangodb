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

#include "snappy-test.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

namespace file {

OptionsStub::OptionsStub() = default;
OptionsStub::~OptionsStub() = default;

const OptionsStub &Defaults() {
  static OptionsStub defaults;
  return defaults;
}

StatusStub::StatusStub() = default;
StatusStub::StatusStub(const StatusStub &) = default;
StatusStub &StatusStub::operator=(const StatusStub &) = default;
StatusStub::~StatusStub() = default;

bool StatusStub::ok() { return true; }

StatusStub GetContents(const std::string &filename, std::string *output,
                       const OptionsStub & /* options */) {
  std::FILE *fp = std::fopen(filename.c_str(), "rb");
  if (fp == nullptr) {
    std::perror(filename.c_str());
    std::exit(1);
  }

  output->clear();
  while (!std::feof(fp)) {
    char buffer[4096];
    size_t bytes_read = std::fread(buffer, 1, sizeof(buffer), fp);
    if (bytes_read == 0 && std::ferror(fp)) {
      std::perror("fread");
      std::exit(1);
    }
    output->append(buffer, bytes_read);
  }

  std::fclose(fp);
  return StatusStub();
}

StatusStub SetContents(const std::string &file_name, const std::string &content,
                       const OptionsStub & /* options */) {
  std::FILE *fp = std::fopen(file_name.c_str(), "wb");
  if (fp == nullptr) {
    std::perror(file_name.c_str());
    std::exit(1);
  }

  size_t bytes_written = std::fwrite(content.data(), 1, content.size(), fp);
  if (bytes_written != content.size()) {
    std::perror("fwrite");
    std::exit(1);
  }

  std::fclose(fp);
  return StatusStub();
}

}  // namespace file

namespace snappy {

std::string ReadTestDataFile(const std::string& base, size_t size_limit) {
  std::string contents;
  const char* srcdir = getenv("srcdir");  // This is set by Automake.
  std::string prefix;
  if (srcdir) {
    prefix = std::string(srcdir) + "/";
  }
  file::GetContents(prefix + "testdata/" + base, &contents, file::Defaults()
      ).ok();
  if (size_limit > 0) {
    contents = contents.substr(0, size_limit);
  }
  return contents;
}

std::string StrFormat(const char* format, ...) {
  char buffer[4096];
  std::va_list ap;
  va_start(ap, format);
  std::vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);
  return buffer;
}

LogMessage::~LogMessage() { std::cerr << std::endl; }

LogMessage &LogMessage::operator<<(const std::string &message) {
  std::cerr << message;
  return *this;
}

LogMessage &LogMessage::operator<<(int number) {
  std::cerr << number;
  return *this;
}

#ifdef _MSC_VER
// ~LogMessageCrash calls std::abort() and therefore never exits. This is by
// design, so temporarily disable warning C4722.
#pragma warning(push)
#pragma warning(disable : 4722)
#endif

LogMessageCrash::~LogMessageCrash() {
  std::cerr << std::endl;
  std::abort();
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#ifdef HAVE_LIBZ

ZLib::ZLib()
    : comp_init_(false),
      uncomp_init_(false) {
  Reinit();
}

ZLib::~ZLib() {
  if (comp_init_)   { deflateEnd(&comp_stream_); }
  if (uncomp_init_) { inflateEnd(&uncomp_stream_); }
}

void ZLib::Reinit() {
  compression_level_ = Z_DEFAULT_COMPRESSION;
  window_bits_ = MAX_WBITS;
  mem_level_ =  8;  // DEF_MEM_LEVEL
  if (comp_init_) {
    deflateEnd(&comp_stream_);
    comp_init_ = false;
  }
  if (uncomp_init_) {
    inflateEnd(&uncomp_stream_);
    uncomp_init_ = false;
  }
  first_chunk_ = true;
}

void ZLib::Reset() {
  first_chunk_ = true;
}

// --------- COMPRESS MODE

// Initialization method to be called if we hit an error while
// compressing. On hitting an error, call this method before returning
// the error.
void ZLib::CompressErrorInit() {
  deflateEnd(&comp_stream_);
  comp_init_ = false;
  Reset();
}

int ZLib::DeflateInit() {
  return deflateInit2(&comp_stream_,
                      compression_level_,
                      Z_DEFLATED,
                      window_bits_,
                      mem_level_,
                      Z_DEFAULT_STRATEGY);
}

int ZLib::CompressInit(Bytef *dest, uLongf *destLen,
                       const Bytef *source, uLong *sourceLen) {
  int err;

  comp_stream_.next_in = (Bytef*)source;
  comp_stream_.avail_in = (uInt)*sourceLen;
  if ((uLong)comp_stream_.avail_in != *sourceLen) return Z_BUF_ERROR;
  comp_stream_.next_out = dest;
  comp_stream_.avail_out = (uInt)*destLen;
  if ((uLong)comp_stream_.avail_out != *destLen) return Z_BUF_ERROR;

  if ( !first_chunk_ )   // only need to set up stream the first time through
    return Z_OK;

  if (comp_init_) {      // we've already initted it
    err = deflateReset(&comp_stream_);
    if (err != Z_OK) {
      LOG(WARNING) << "ERROR: Can't reset compress object; creating a new one";
      deflateEnd(&comp_stream_);
      comp_init_ = false;
    }
  }
  if (!comp_init_) {     // first use
    comp_stream_.zalloc = (alloc_func)0;
    comp_stream_.zfree = (free_func)0;
    comp_stream_.opaque = (voidpf)0;
    err = DeflateInit();
    if (err != Z_OK) return err;
    comp_init_ = true;
  }
  return Z_OK;
}

// In a perfect world we'd always have the full buffer to compress
// when the time came, and we could just call Compress().  Alas, we
// want to do chunked compression on our webserver.  In this
// application, we compress the header, send it off, then compress the
// results, send them off, then compress the footer.  Thus we need to
// use the chunked compression features of zlib.
int ZLib::CompressAtMostOrAll(Bytef *dest, uLongf *destLen,
                              const Bytef *source, uLong *sourceLen,
                              int flush_mode) {   // Z_FULL_FLUSH or Z_FINISH
  int err;

  if ( (err=CompressInit(dest, destLen, source, sourceLen)) != Z_OK )
    return err;

  // This is used to figure out how many bytes we wrote *this chunk*
  int compressed_size = comp_stream_.total_out;

  // Some setup happens only for the first chunk we compress in a run
  if ( first_chunk_ ) {
    first_chunk_ = false;
  }

  // flush_mode is Z_FINISH for all mode, Z_SYNC_FLUSH for incremental
  // compression.
  err = deflate(&comp_stream_, flush_mode);

  *sourceLen = comp_stream_.avail_in;

  if ((err == Z_STREAM_END || err == Z_OK)
      && comp_stream_.avail_in == 0
      && comp_stream_.avail_out != 0 ) {
    // we processed everything ok and the output buffer was large enough.
    ;
  } else if (err == Z_STREAM_END && comp_stream_.avail_in > 0) {
    return Z_BUF_ERROR;                            // should never happen
  } else if (err != Z_OK && err != Z_STREAM_END && err != Z_BUF_ERROR) {
    // an error happened
    CompressErrorInit();
    return err;
  } else if (comp_stream_.avail_out == 0) {     // not enough space
    err = Z_BUF_ERROR;
  }

  assert(err == Z_OK || err == Z_STREAM_END || err == Z_BUF_ERROR);
  if (err == Z_STREAM_END)
    err = Z_OK;

  // update the crc and other metadata
  compressed_size = comp_stream_.total_out - compressed_size;  // delta
  *destLen = compressed_size;

  return err;
}

int ZLib::CompressChunkOrAll(Bytef *dest, uLongf *destLen,
                             const Bytef *source, uLong sourceLen,
                             int flush_mode) {   // Z_FULL_FLUSH or Z_FINISH
  const int ret =
    CompressAtMostOrAll(dest, destLen, source, &sourceLen, flush_mode);
  if (ret == Z_BUF_ERROR)
    CompressErrorInit();
  return ret;
}

// This routine only initializes the compression stream once.  Thereafter, it
// just does a deflateReset on the stream, which should be faster.
int ZLib::Compress(Bytef *dest, uLongf *destLen,
                   const Bytef *source, uLong sourceLen) {
  int err;
  if ( (err=CompressChunkOrAll(dest, destLen, source, sourceLen,
                               Z_FINISH)) != Z_OK )
    return err;
  Reset();         // reset for next call to Compress

  return Z_OK;
}


// --------- UNCOMPRESS MODE

int ZLib::InflateInit() {
  return inflateInit2(&uncomp_stream_, MAX_WBITS);
}

// Initialization method to be called if we hit an error while
// uncompressing. On hitting an error, call this method before
// returning the error.
void ZLib::UncompressErrorInit() {
  inflateEnd(&uncomp_stream_);
  uncomp_init_ = false;
  Reset();
}

int ZLib::UncompressInit(Bytef *dest, uLongf *destLen,
                         const Bytef *source, uLong *sourceLen) {
  int err;

  uncomp_stream_.next_in = (Bytef*)source;
  uncomp_stream_.avail_in = (uInt)*sourceLen;
  // Check for source > 64K on 16-bit machine:
  if ((uLong)uncomp_stream_.avail_in != *sourceLen) return Z_BUF_ERROR;

  uncomp_stream_.next_out = dest;
  uncomp_stream_.avail_out = (uInt)*destLen;
  if ((uLong)uncomp_stream_.avail_out != *destLen) return Z_BUF_ERROR;

  if ( !first_chunk_ )   // only need to set up stream the first time through
    return Z_OK;

  if (uncomp_init_) {    // we've already initted it
    err = inflateReset(&uncomp_stream_);
    if (err != Z_OK) {
      LOG(WARNING)
        << "ERROR: Can't reset uncompress object; creating a new one";
      UncompressErrorInit();
    }
  }
  if (!uncomp_init_) {
    uncomp_stream_.zalloc = (alloc_func)0;
    uncomp_stream_.zfree = (free_func)0;
    uncomp_stream_.opaque = (voidpf)0;
    err = InflateInit();
    if (err != Z_OK) return err;
    uncomp_init_ = true;
  }
  return Z_OK;
}

// If you compressed your data a chunk at a time, with CompressChunk,
// you can uncompress it a chunk at a time with UncompressChunk.
// Only difference bewteen chunked and unchunked uncompression
// is the flush mode we use: Z_SYNC_FLUSH (chunked) or Z_FINISH (unchunked).
int ZLib::UncompressAtMostOrAll(Bytef *dest, uLongf *destLen,
                                const Bytef *source, uLong *sourceLen,
                                int flush_mode) {  // Z_SYNC_FLUSH or Z_FINISH
  int err = Z_OK;

  if ( (err=UncompressInit(dest, destLen, source, sourceLen)) != Z_OK ) {
    LOG(WARNING) << "UncompressInit: Error: " << err << " SourceLen: "
                 << *sourceLen;
    return err;
  }

  // This is used to figure out how many output bytes we wrote *this chunk*:
  const uLong old_total_out = uncomp_stream_.total_out;

  // This is used to figure out how many input bytes we read *this chunk*:
  const uLong old_total_in = uncomp_stream_.total_in;

  // Some setup happens only for the first chunk we compress in a run
  if ( first_chunk_ ) {
    first_chunk_ = false;                          // so we don't do this again

    // For the first chunk *only* (to avoid infinite troubles), we let
    // there be no actual data to uncompress.  This sometimes triggers
    // when the input is only the gzip header, say.
    if ( *sourceLen == 0 ) {
      *destLen = 0;
      return Z_OK;
    }
  }

  // We'll uncompress as much as we can.  If we end OK great, otherwise
  // if we get an error that seems to be the gzip footer, we store the
  // gzip footer and return OK, otherwise we return the error.

  // flush_mode is Z_SYNC_FLUSH for chunked mode, Z_FINISH for all mode.
  err = inflate(&uncomp_stream_, flush_mode);

  // Figure out how many bytes of the input zlib slurped up:
  const uLong bytes_read = uncomp_stream_.total_in - old_total_in;
  CHECK_LE(source + bytes_read, source + *sourceLen);
  *sourceLen = uncomp_stream_.avail_in;

  if ((err == Z_STREAM_END || err == Z_OK)  // everything went ok
             && uncomp_stream_.avail_in == 0) {    // and we read it all
    ;
  } else if (err == Z_STREAM_END && uncomp_stream_.avail_in > 0) {
    LOG(WARNING)
      << "UncompressChunkOrAll: Received some extra data, bytes total: "
      << uncomp_stream_.avail_in << " bytes: "
      << std::string(reinterpret_cast<const char *>(uncomp_stream_.next_in),
                     std::min(int(uncomp_stream_.avail_in), 20));
    UncompressErrorInit();
    return Z_DATA_ERROR;       // what's the extra data for?
  } else if (err != Z_OK && err != Z_STREAM_END && err != Z_BUF_ERROR) {
    // an error happened
    LOG(WARNING) << "UncompressChunkOrAll: Error: " << err
                 << " avail_out: " << uncomp_stream_.avail_out;
    UncompressErrorInit();
    return err;
  } else if (uncomp_stream_.avail_out == 0) {
    err = Z_BUF_ERROR;
  }

  assert(err == Z_OK || err == Z_BUF_ERROR || err == Z_STREAM_END);
  if (err == Z_STREAM_END)
    err = Z_OK;

  *destLen = uncomp_stream_.total_out - old_total_out;  // size for this call

  return err;
}

int ZLib::UncompressChunkOrAll(Bytef *dest, uLongf *destLen,
                               const Bytef *source, uLong sourceLen,
                               int flush_mode) {  // Z_SYNC_FLUSH or Z_FINISH
  const int ret =
    UncompressAtMostOrAll(dest, destLen, source, &sourceLen, flush_mode);
  if (ret == Z_BUF_ERROR)
    UncompressErrorInit();
  return ret;
}

int ZLib::UncompressAtMost(Bytef *dest, uLongf *destLen,
                          const Bytef *source, uLong *sourceLen) {
  return UncompressAtMostOrAll(dest, destLen, source, sourceLen, Z_SYNC_FLUSH);
}

// We make sure we've uncompressed everything, that is, the current
// uncompress stream is at a compressed-buffer-EOF boundary.  In gzip
// mode, we also check the gzip footer to make sure we pass the gzip
// consistency checks.  We RETURN true iff both types of checks pass.
bool ZLib::UncompressChunkDone() {
  assert(!first_chunk_ && uncomp_init_);
  // Make sure we're at the end-of-compressed-data point.  This means
  // if we call inflate with Z_FINISH we won't consume any input or
  // write any output
  Bytef dummyin, dummyout;
  uLongf dummylen = 0;
  if ( UncompressChunkOrAll(&dummyout, &dummylen, &dummyin, 0, Z_FINISH)
       != Z_OK ) {
    return false;
  }

  // Make sure that when we exit, we can start a new round of chunks later
  Reset();

  return true;
}

// Uncompresses the source buffer into the destination buffer.
// The destination buffer must be long enough to hold the entire
// decompressed contents.
//
// We only initialize the uncomp_stream once.  Thereafter, we use
// inflateReset, which should be faster.
//
// Returns Z_OK on success, otherwise, it returns a zlib error code.
int ZLib::Uncompress(Bytef *dest, uLongf *destLen,
                     const Bytef *source, uLong sourceLen) {
  int err;
  if ( (err=UncompressChunkOrAll(dest, destLen, source, sourceLen,
                                 Z_FINISH)) != Z_OK ) {
    Reset();                           // let us try to compress again
    return err;
  }
  if ( !UncompressChunkDone() )        // calls Reset()
    return Z_DATA_ERROR;
  return Z_OK;  // stream_end is ok
}

#endif  // HAVE_LIBZ

}  // namespace snappy
