////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "store/store_utils.hpp"
#include "utils/lz4compression.hpp"
#include "utils/delta_compression.hpp"

#include <numeric>
#include <random>

TEST(compression_test, lz4) {
  using namespace iresearch;

  std::vector<size_t> data(2047, 0);
  std::random_device rnd_device;
  std::mt19937 mersenne_engine {rnd_device()};
  std::uniform_int_distribution<size_t> dist {1, 2142152};
  auto generator = [&dist, &mersenne_engine](){ return dist(mersenne_engine); };

  compression::lz4::lz4decompressor decompressor;
  compression::lz4::lz4compressor compressor;
  ASSERT_EQ(0, compressor.acceleration());

  for (size_t i = 0; i < 10; ++i) {
    std::generate(data.begin(), data.end(), generator);

    bstring compression_buf;
    bstring data_buf(data.size()*sizeof(size_t), 0);
    std::memcpy(&data_buf[0], data.data(), data_buf.size());

    ASSERT_EQ(
      bytes_ref(reinterpret_cast<const byte_type*>(data.data()), data.size()*sizeof(size_t)),
      bytes_ref(data_buf)
    );

    const auto compressed = compressor.compress(&data_buf[0], data_buf.size(), compression_buf);
    ASSERT_EQ(compressed, bytes_ref(compression_buf.c_str(), compressed.size()));

    // lz4 doesn't modify data_buf
    ASSERT_EQ(
      bytes_ref(reinterpret_cast<const byte_type*>(data.data()), data.size()*sizeof(size_t)),
      bytes_ref(data_buf)
    );

    bstring decompression_buf(data_buf.size(), 0); // ensure we have enough space in buffer
    const auto decompressed = decompressor.decompress(&compression_buf[0], compressed.size(),
                                                      &decompression_buf[0], decompression_buf.size());

    ASSERT_EQ(data_buf, decompression_buf);
    ASSERT_EQ(data_buf, decompressed);
  }
}

TEST(compression_test, delta) {
  using namespace iresearch;

  std::vector<uint64_t> data(2047, 0);
  std::random_device rnd_device;
  std::mt19937 mersenne_engine {rnd_device()};
  std::uniform_int_distribution<uint64_t> dist {1, 52};
  auto generator = [&dist, &mersenne_engine](){ return dist(mersenne_engine); };

  compression::delta_decompressor decompressor;
  compression::delta_compressor compressor;

  for (size_t i = 0; i < 10; ++i) {
    std::generate(data.begin(), data.end(), generator);

    bstring compression_buf;
    bstring data_buf(data.size()*sizeof(size_t), 0);
    std::memcpy(&data_buf[0], data.data(), data_buf.size());

    ASSERT_EQ(
      bytes_ref(reinterpret_cast<const byte_type*>(data.data()), data.size()*sizeof(size_t)),
      bytes_ref(data_buf)
    );

    const auto compressed = compressor.compress(&data_buf[0], data_buf.size(), compression_buf);
    ASSERT_EQ(compressed, bytes_ref(compression_buf.c_str(), compressed.size()));

    bstring decompression_buf(data_buf.size(), 0); // ensure we have enough space in buffer
    const auto decompressed = decompressor.decompress(&compression_buf[0], compressed.size(),
                                                      &decompression_buf[0], decompression_buf.size());

    ASSERT_EQ(
      bytes_ref(reinterpret_cast<const byte_type*>(data.data()), data.size()*sizeof(size_t)),
      bytes_ref(decompression_buf)
    );
    ASSERT_EQ(
      bytes_ref(reinterpret_cast<const byte_type*>(data.data()), data.size()*sizeof(size_t)),
      bytes_ref(decompressed)
    );
  }
}
