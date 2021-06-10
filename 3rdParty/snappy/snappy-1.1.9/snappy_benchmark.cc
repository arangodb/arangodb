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

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "snappy-test.h"

#include "benchmark/benchmark.h"

#include "snappy-internal.h"
#include "snappy-sinksource.h"
#include "snappy.h"
#include "snappy_test_data.h"

namespace snappy {

namespace {

void BM_UFlat(benchmark::State& state) {
  // Pick file to process based on state.range(0).
  int file_index = state.range(0);

  CHECK_GE(file_index, 0);
  CHECK_LT(file_index, ARRAYSIZE(kTestDataFiles));
  std::string contents =
      ReadTestDataFile(kTestDataFiles[file_index].filename,
                       kTestDataFiles[file_index].size_limit);

  std::string zcontents;
  snappy::Compress(contents.data(), contents.size(), &zcontents);
  char* dst = new char[contents.size()];

  for (auto s : state) {
    CHECK(snappy::RawUncompress(zcontents.data(), zcontents.size(), dst));
    benchmark::DoNotOptimize(dst);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(contents.size()));
  state.SetLabel(kTestDataFiles[file_index].label);

  delete[] dst;
}
BENCHMARK(BM_UFlat)->DenseRange(0, ARRAYSIZE(kTestDataFiles) - 1);

struct SourceFiles {
  SourceFiles() {
    for (int i = 0; i < kFiles; i++) {
      std::string contents = ReadTestDataFile(kTestDataFiles[i].filename,
                                              kTestDataFiles[i].size_limit);
      max_size = std::max(max_size, contents.size());
      sizes[i] = contents.size();
      snappy::Compress(contents.data(), contents.size(), &zcontents[i]);
    }
  }
  static constexpr int kFiles = ARRAYSIZE(kTestDataFiles);
  std::string zcontents[kFiles];
  size_t sizes[kFiles];
  size_t max_size = 0;
};

void BM_UFlatMedley(benchmark::State& state) {
  static const SourceFiles* const source = new SourceFiles();

  std::vector<char> dst(source->max_size);

  for (auto s : state) {
    for (int i = 0; i < SourceFiles::kFiles; i++) {
      CHECK(snappy::RawUncompress(source->zcontents[i].data(),
                                  source->zcontents[i].size(), dst.data()));
      benchmark::DoNotOptimize(dst);
    }
  }

  int64_t source_sizes = 0;
  for (int i = 0; i < SourceFiles::kFiles; i++) {
    source_sizes += static_cast<int64_t>(source->sizes[i]);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          source_sizes);
}
BENCHMARK(BM_UFlatMedley);

void BM_UValidate(benchmark::State& state) {
  // Pick file to process based on state.range(0).
  int file_index = state.range(0);

  CHECK_GE(file_index, 0);
  CHECK_LT(file_index, ARRAYSIZE(kTestDataFiles));
  std::string contents =
      ReadTestDataFile(kTestDataFiles[file_index].filename,
                       kTestDataFiles[file_index].size_limit);

  std::string zcontents;
  snappy::Compress(contents.data(), contents.size(), &zcontents);

  for (auto s : state) {
    CHECK(snappy::IsValidCompressedBuffer(zcontents.data(), zcontents.size()));
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(contents.size()));
  state.SetLabel(kTestDataFiles[file_index].label);
}
BENCHMARK(BM_UValidate)->DenseRange(0, ARRAYSIZE(kTestDataFiles) - 1);

void BM_UValidateMedley(benchmark::State& state) {
  static const SourceFiles* const source = new SourceFiles();

  for (auto s : state) {
    for (int i = 0; i < SourceFiles::kFiles; i++) {
      CHECK(snappy::IsValidCompressedBuffer(source->zcontents[i].data(),
                                            source->zcontents[i].size()));
    }
  }

  int64_t source_sizes = 0;
  for (int i = 0; i < SourceFiles::kFiles; i++) {
    source_sizes += static_cast<int64_t>(source->sizes[i]);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          source_sizes);
}
BENCHMARK(BM_UValidateMedley);

void BM_UIOVec(benchmark::State& state) {
  // Pick file to process based on state.range(0).
  int file_index = state.range(0);

  CHECK_GE(file_index, 0);
  CHECK_LT(file_index, ARRAYSIZE(kTestDataFiles));
  std::string contents =
      ReadTestDataFile(kTestDataFiles[file_index].filename,
                       kTestDataFiles[file_index].size_limit);

  std::string zcontents;
  snappy::Compress(contents.data(), contents.size(), &zcontents);

  // Uncompress into an iovec containing ten entries.
  const int kNumEntries = 10;
  struct iovec iov[kNumEntries];
  char *dst = new char[contents.size()];
  size_t used_so_far = 0;
  for (int i = 0; i < kNumEntries; ++i) {
    iov[i].iov_base = dst + used_so_far;
    if (used_so_far == contents.size()) {
      iov[i].iov_len = 0;
      continue;
    }

    if (i == kNumEntries - 1) {
      iov[i].iov_len = contents.size() - used_so_far;
    } else {
      iov[i].iov_len = contents.size() / kNumEntries;
    }
    used_so_far += iov[i].iov_len;
  }

  for (auto s : state) {
    CHECK(snappy::RawUncompressToIOVec(zcontents.data(), zcontents.size(), iov,
                                       kNumEntries));
    benchmark::DoNotOptimize(iov);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(contents.size()));
  state.SetLabel(kTestDataFiles[file_index].label);

  delete[] dst;
}
BENCHMARK(BM_UIOVec)->DenseRange(0, 4);

void BM_UFlatSink(benchmark::State& state) {
  // Pick file to process based on state.range(0).
  int file_index = state.range(0);

  CHECK_GE(file_index, 0);
  CHECK_LT(file_index, ARRAYSIZE(kTestDataFiles));
  std::string contents =
      ReadTestDataFile(kTestDataFiles[file_index].filename,
                       kTestDataFiles[file_index].size_limit);

  std::string zcontents;
  snappy::Compress(contents.data(), contents.size(), &zcontents);
  char* dst = new char[contents.size()];

  for (auto s : state) {
    snappy::ByteArraySource source(zcontents.data(), zcontents.size());
    snappy::UncheckedByteArraySink sink(dst);
    CHECK(snappy::Uncompress(&source, &sink));
    benchmark::DoNotOptimize(sink);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(contents.size()));
  state.SetLabel(kTestDataFiles[file_index].label);

  std::string s(dst, contents.size());
  CHECK_EQ(contents, s);

  delete[] dst;
}

BENCHMARK(BM_UFlatSink)->DenseRange(0, ARRAYSIZE(kTestDataFiles) - 1);

void BM_ZFlat(benchmark::State& state) {
  // Pick file to process based on state.range(0).
  int file_index = state.range(0);

  CHECK_GE(file_index, 0);
  CHECK_LT(file_index, ARRAYSIZE(kTestDataFiles));
  std::string contents =
      ReadTestDataFile(kTestDataFiles[file_index].filename,
                       kTestDataFiles[file_index].size_limit);
  char* dst = new char[snappy::MaxCompressedLength(contents.size())];

  size_t zsize = 0;
  for (auto s : state) {
    snappy::RawCompress(contents.data(), contents.size(), dst, &zsize);
    benchmark::DoNotOptimize(dst);
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(contents.size()));
  const double compression_ratio =
      static_cast<double>(zsize) / std::max<size_t>(1, contents.size());
  state.SetLabel(StrFormat("%s (%.2f %%)", kTestDataFiles[file_index].label,
                           100.0 * compression_ratio));
  VLOG(0) << StrFormat("compression for %s: %d -> %d bytes",
                       kTestDataFiles[file_index].label, contents.size(),
                       zsize);
  delete[] dst;
}
BENCHMARK(BM_ZFlat)->DenseRange(0, ARRAYSIZE(kTestDataFiles) - 1);

void BM_ZFlatAll(benchmark::State& state) {
  const int num_files = ARRAYSIZE(kTestDataFiles);

  std::vector<std::string> contents(num_files);
  std::vector<char*> dst(num_files);

  int64_t total_contents_size = 0;
  for (int i = 0; i < num_files; ++i) {
    contents[i] = ReadTestDataFile(kTestDataFiles[i].filename,
                                   kTestDataFiles[i].size_limit);
    dst[i] = new char[snappy::MaxCompressedLength(contents[i].size())];
    total_contents_size += contents[i].size();
  }

  size_t zsize = 0;
  for (auto s : state) {
    for (int i = 0; i < num_files; ++i) {
      snappy::RawCompress(contents[i].data(), contents[i].size(), dst[i],
                          &zsize);
      benchmark::DoNotOptimize(dst);
    }
  }

  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          total_contents_size);

  for (char* dst_item : dst) {
    delete[] dst_item;
  }
  state.SetLabel(StrFormat("%d kTestDataFiles", num_files));
}
BENCHMARK(BM_ZFlatAll);

void BM_ZFlatIncreasingTableSize(benchmark::State& state) {
  CHECK_GT(ARRAYSIZE(kTestDataFiles), 0);
  const std::string base_content = ReadTestDataFile(
      kTestDataFiles[0].filename, kTestDataFiles[0].size_limit);

  std::vector<std::string> contents;
  std::vector<char*> dst;
  int64_t total_contents_size = 0;
  for (int table_bits = kMinHashTableBits; table_bits <= kMaxHashTableBits;
       ++table_bits) {
    std::string content = base_content;
    content.resize(1 << table_bits);
    dst.push_back(new char[snappy::MaxCompressedLength(content.size())]);
    total_contents_size += content.size();
    contents.push_back(std::move(content));
  }

  size_t zsize = 0;
  for (auto s : state) {
    for (size_t i = 0; i < contents.size(); ++i) {
      snappy::RawCompress(contents[i].data(), contents[i].size(), dst[i],
                          &zsize);
      benchmark::DoNotOptimize(dst);
    }
  }

  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          total_contents_size);

  for (char* dst_item : dst) {
    delete[] dst_item;
  }
  state.SetLabel(StrFormat("%d tables", contents.size()));
}
BENCHMARK(BM_ZFlatIncreasingTableSize);

}  // namespace

}  // namespace snappy
