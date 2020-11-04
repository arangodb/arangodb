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

namespace {

struct dummy_compressor final : irs::compression::compressor {
  virtual irs::bytes_ref compress(irs::byte_type* in, size_t size, irs::bstring& /*buf*/) {
    return irs::bytes_ref::NIL;
  }

  virtual void flush(data_output&) { }
};

struct dummy_decompressor final : irs::compression::decompressor {
  virtual irs::bytes_ref decompress(
      const irs::byte_type* src, size_t src_size,
      irs::byte_type* dst, size_t dst_size) {
    return irs::bytes_ref::NIL;
  }

  virtual bool prepare(data_input&) { return true; }
};

}

TEST(compression_test, registration) {
  struct dummy_compression { };

  constexpr auto type = irs::type<dummy_compression>::get();

  // check absent
  {
    ASSERT_FALSE(irs::compression::exists(type.name()));
    ASSERT_EQ(nullptr, irs::compression::get_compressor(type.name(), {}));
    ASSERT_EQ(nullptr, irs::compression::get_decompressor(type.name(), {}));
    auto visitor = [&type](const irs::string_ref& name) { return name != type.name(); };
    ASSERT_TRUE(irs::compression::visit(visitor));
  }

  static size_t calls_count;
  irs::compression::compression_registrar initial(
     type,
     [](const irs::compression::options&) -> irs::compression::compressor::ptr {
       ++calls_count;
       return std::make_shared<dummy_compressor>();
     },
     []() -> irs::compression::decompressor::ptr {
       ++calls_count;
       return std::make_shared<dummy_decompressor>();
     }
  );
  ASSERT_TRUE(initial); // registered

  // check registered
  {
    ASSERT_TRUE(irs::compression::exists(type.name()));
    ASSERT_EQ(0, calls_count);
    ASSERT_NE(nullptr, irs::compression::get_compressor(type.name(), {}));
    ASSERT_EQ(1, calls_count);
    ASSERT_NE(nullptr, irs::compression::get_decompressor(type.name(), {}));
    ASSERT_EQ(2, calls_count);
    auto visitor = [&type](const irs::string_ref& name) { return name != type.name(); };
    ASSERT_FALSE(irs::compression::visit(visitor));
  }

  irs::compression::compression_registrar duplicate(
     type,
     [](const irs::compression::options&) -> irs::compression::compressor::ptr {  return nullptr;  },
     []() -> irs::compression::decompressor::ptr { return nullptr; }
  );
  ASSERT_FALSE(duplicate); // not registered

  // check registered
  {
    ASSERT_TRUE(irs::compression::exists(type.name()));
    ASSERT_EQ(2, calls_count);
    ASSERT_NE(nullptr, irs::compression::get_compressor(type.name(), {}));
    ASSERT_EQ(3, calls_count);
    ASSERT_NE(nullptr, irs::compression::get_decompressor(type.name(), {}));
    ASSERT_EQ(4, calls_count);
    auto visitor = [&type](const irs::string_ref& name) { return name != type.name(); };
    ASSERT_FALSE(irs::compression::visit(visitor));
  }
}

TEST(compression_test, none) {
  static_assert("iresearch::compression::none" == irs::type<irs::compression::none>::name());
  ASSERT_EQ(nullptr, irs::compression::none().compressor({}));
  ASSERT_EQ(nullptr, irs::compression::none().decompressor());
}

TEST(compression_test, lz4) {
  using namespace iresearch;
  static_assert("iresearch::compression::lz4" == irs::type<irs::compression::lz4>::name());

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
  static_assert("iresearch::compression::delta" == irs::type<irs::compression::delta>::name());

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
