////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "store/store_utils.hpp"
#ifdef IRESEARCH_SSE2
#include "store/store_utils_simd.hpp"
#endif
#include "utils/bytes_utils.hpp"

using namespace irs;

namespace tests {
namespace detail {

//////////////////////////////////////////////////////////////////////////////
/// @class bytes_input
//////////////////////////////////////////////////////////////////////////////
class bytes_input final: public data_input, public bytes_ref {
 public:
  bytes_input() = default;
  explicit bytes_input(const bytes_ref& data);
  bytes_input(bytes_input&& rhs) noexcept;
  bytes_input& operator=(bytes_input&& rhs) noexcept;
  bytes_input& operator=(const bytes_ref& data);

  void skip(size_t size) {
    assert(pos_ + size <= this->end());
    pos_ += size;
  }

  void seek(size_t pos) {
    assert(this->begin() + pos <= this->end());
    pos_ = this->begin() + pos;
  }

  virtual size_t file_pointer() const override {
    return std::distance(this->begin(), pos_);
  }

  virtual size_t length() const override {
    return this->size();
  }

  virtual bool eof() const override {
    return pos_ >= this->end();
  }

  virtual const byte_type* read_buffer(size_t /*count*/, BufferHint /*hint*/) override {
    return nullptr;
  }

  virtual byte_type read_byte() override final {
    assert(pos_ < this->end());
    return *pos_++;
  }

  virtual size_t read_bytes(byte_type* b, size_t size) override final;

  // append to buf
  void read_bytes(bstring& buf, size_t size);

  virtual int32_t read_int() override final {
    return irs::read<uint32_t>(pos_);
  }

  virtual int64_t read_long() override final {
    return irs::read<uint64_t>(pos_);
  }

  virtual uint32_t read_vint() override final {
    return irs::vread<uint32_t>(pos_);
  }

  virtual uint64_t read_vlong() override final {
    return irs::vread<uint64_t>(pos_);
  }

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  bstring buf_;
  const byte_type* pos_{ buf_.c_str() };
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // bytes_input

// ----------------------------------------------------------------------------
// --SECTION--                                       bytes_input implementation
// ----------------------------------------------------------------------------

bytes_input::bytes_input(const bytes_ref& data)
  : buf_(data.c_str(), data.size()),
    pos_(this->buf_.c_str()) {
  this->data_ = buf_.data();
  this->size_ = data.size();
}

bytes_input::bytes_input(bytes_input&& other) noexcept
  : buf_(std::move(other.buf_)),
    pos_(other.pos_) {
  this->data_ = buf_.data();
  this->size_ = other.size();
  other.pos_ = other.buf_.c_str();
  other.size_ = 0;
}

bytes_input& bytes_input::operator=(const bytes_ref& data) {
  if (this != &data) {
    buf_.assign(data.c_str(), data.size());
    pos_ = this->buf_.c_str();
    this->data_ = buf_.data();
    this->size_ = data.size();
  }

  return *this;
}

bytes_input& bytes_input::operator=(bytes_input&& other) noexcept {
  if (this != &other) {
    buf_ = std::move(other.buf_);
    pos_ = buf_.c_str();
    this->data_ = buf_.data();
    this->size_ = other.size();
    other.pos_ = other.buf_.c_str();
    other.size_ = 0;
  }

  return *this;
}

void bytes_input::read_bytes(bstring& buf, size_t size) {
  auto used = buf.size();

  buf.resize(used + size);

  #ifdef IRESEARCH_DEBUG
    const auto read = read_bytes(&(buf[0]) + used, size);
    assert(read == size);
    UNUSED(read);
  #else
    read_bytes(&(buf[0]) + used, size);
  #endif // IRESEARCH_DEBUG
}

size_t bytes_input::read_bytes(byte_type* b, size_t size) {
  assert(pos_ + size <= this->end());
  size = std::min(size, size_t(std::distance(pos_, this->end())));
  std::memcpy(b, pos_, sizeof(byte_type) * size);
  pos_ += size;
  return size;
}


void avg_encode_decode_core(size_t step, size_t count) {
  std::vector<uint64_t> values;
  values.reserve(count);
  for (size_t i = 1; i <= count; ++i) {
    values.push_back(i*step);
  }

  auto encoded = values;
  const auto stats = irs::encode::avg::encode(encoded.data(), encoded.data() + encoded.size());
  ASSERT_EQ(values[0], stats.first);
  ASSERT_EQ(step, stats.second);
  ASSERT_TRUE(
    std::all_of(encoded.begin(), encoded.end(), [](uint64_t v) { return 0 == v; })
  );

  auto success = true;
  auto begin = values.begin();
  irs::encode::avg::visit(
    stats.first, stats.second, encoded.data(), encoded.data() + encoded.size(),
    [&begin, &success](uint64_t value) {
      success = success && (value == *begin);
      ++begin;
  });
  ASSERT_TRUE(success);
  ASSERT_EQ(values.end(), begin);

  auto decoded = encoded;
  irs::encode::avg::decode(stats.first, stats.second, decoded.data(), decoded.data() + decoded.size());
  ASSERT_EQ(values, decoded);
}

void delta_encode_decode_core(size_t step, size_t count) {
  std::vector<size_t> values;
  values.reserve(count);
  for (size_t i = 1; i <= count; ++i) {
    values.push_back(i*step);
  }

  auto encoded = values;
  irs::encode::delta::encode(encoded.begin(), encoded.end());
  ASSERT_TRUE(
    std::all_of(encoded.begin(), encoded.end(), [step](int v) { return step == v; })
  );

  auto decoded = encoded;
  irs::encode::delta::decode(decoded.begin(), decoded.end());
  ASSERT_EQ(values, decoded);
}

void packed_read_write_core(const std::vector<uint32_t> &src) {
  const size_t BLOCK_SIZE = 128;
  uint32_t encoded[BLOCK_SIZE];
  const auto blocks = src.size() / BLOCK_SIZE;
  assert(blocks);

  // compress data to stream
  irs::bstring buf;
  irs::bytes_output out(buf);

  // write first n compressed blocks
  {
    auto begin = src.data();
    for (size_t i = 0; i < blocks; ++i) {
      irs::encode::bitpack::write_block(out, begin, BLOCK_SIZE, encoded);
      begin += BLOCK_SIZE;
    }
  }

  // decompress data from stream
  std::vector<uint32_t> read(src.size());
  irs::bytes_ref_input in(buf);

  // read first n compressed blocks
  {
    auto begin = read.data();
    for (size_t i = 0; i < blocks; ++i) {
      irs::encode::bitpack::read_block(in, BLOCK_SIZE, encoded, begin);
      begin += BLOCK_SIZE;
    }
  }

  ASSERT_EQ(src, read);
}

void packed_read_write_block_core(const std::vector<uint32_t> &src) {
  const size_t BLOCK_SIZE = 128;
  uint32_t encoded[BLOCK_SIZE];

  // compress data to stream
  irs::bstring buf;
  irs::bytes_output out(buf);
  irs::encode::bitpack::write_block(out, src.data(), encoded);

  // decompress data from stream
  std::vector<uint32_t> read(src.size());
  irs::bytes_ref_input in(buf);
  irs::encode::bitpack::read_block(in, encoded, read.data());

  ASSERT_EQ(src, read);
}

using irs::data_input;
using irs::data_output;

template<typename T>
void read_write_core( 
    const std::vector<T>& src, 
    const std::function<T(data_input&)>& reader,
    const std::function<void(data_output&,T)>& writer) {
  irs::bstring buf;
  irs::bytes_output out(buf);
  std::for_each(
    src.begin(), src.end(), 
    [&out,&writer](const T& v){ writer(out, v); }
  );

  bytes_input in(buf);
  std::for_each(
    src.begin(), src.end(), 
    [&in,&reader](const T& v){ ASSERT_EQ(v, reader(in)); }
  );
}

// Since NaN != NaN according to the ieee754
template<typename T>
void read_write_core_nan(
    const std::function<T(data_input&)>& reader,
    const std::function<void(data_output&,T)>& writer) {
  irs::bstring buf;
  irs::bytes_output out(buf);
  writer(out, std::numeric_limits<T>::quiet_NaN());
  writer(out, std::numeric_limits<T>::signaling_NaN());

  bytes_input in(buf);
  ASSERT_TRUE(std::isnan(reader(in)));
  ASSERT_TRUE(std::isnan(reader(in)));
}

template<typename Cont>
void read_write_core_container(
    const Cont& src,
    const std::function<Cont(data_input&)>& reader,
    const std::function<data_output&(data_output&,const Cont&)>& writer) {
  irs::bstring buf;
  irs::bytes_output out(buf);
  writer(out, src);

  bytes_input in(buf);
  const Cont read = reader( in);
  ASSERT_EQ(src, read);
}

void read_write_block(const std::vector<uint32_t>& source, std::vector<uint32_t>& enc_dec_buf) {
  // write block
  irs::bstring buf;
  irs::bytes_output out(buf);
  irs::encode::bitpack::write_block(out, &source[0], source.size(), &enc_dec_buf[0]);

  // read block
  bytes_input in(buf);
  std::vector<uint32_t> read(source.size());
  irs::encode::bitpack::read_block(in, source.size(), &enc_dec_buf[0], read.data());

  ASSERT_EQ(source, read);
}

void read_write_block(const std::vector<uint32_t>& source) {
  // intermediate buffer for encoding/decoding
  std::vector<uint32_t> enc_dec_buf(source.size());
  read_write_block(source, enc_dec_buf);
}

void shift_pack_unpack_core_32(uint32_t value, bool flag) {
  const uint32_t packed = shift_pack_32(value, flag);
  uint32_t unpacked;
  ASSERT_EQ(flag, shift_unpack_32(packed, unpacked));
  ASSERT_EQ(value, unpacked);
}

void shift_pack_unpack_core_64(uint64_t value, bool flag) {
  const uint64_t packed = shift_pack_64(value, flag);
  uint64_t unpacked;
  ASSERT_EQ(flag, shift_unpack_64(packed, unpacked));
  ASSERT_EQ(value, unpacked);
}

template<typename T>
void vencode_from_array(T expected_value, size_t expected_length) {
  ASSERT_EQ(expected_length, irs::bytes_io<T>::vsize(expected_value));
  irs::byte_type buf[irs::bytes_io<T>::const_max_vsize]{};

  // write data
  {
    auto* ptr = buf;
    irs::vwrite<T>(ptr, expected_value);
    ASSERT_EQ(expected_length, std::distance(buf, ptr));
  }

  // read data
  {
    const auto* ptr = buf;
    ASSERT_EQ(expected_value, irs::vread<T>(ptr));
    ASSERT_EQ(expected_length, std::distance((const irs::byte_type*)(buf), ptr));
  }

  // skip data
  {
    const auto* ptr = buf;
    irs::vskip<T>(ptr);
    ASSERT_GE(ptr, buf);
    ASSERT_EQ(expected_length, std::distance((const irs::byte_type*)(buf), ptr));
  }
}

#ifdef IRESEARCH_SSE2

void read_write_optimized(const std::vector<uint32_t>& source, std::vector<uint32_t>& enc_dec_buf) {
  // write block
  irs::bstring buf;
  irs::bytes_output out(buf);
  irs::encode::bitpack::write_block_simd(out, &source[0], source.size(), &enc_dec_buf[0]);

  // read block
  bytes_input in(buf);
  std::vector<uint32_t> read(source.size());
  irs::encode::bitpack::read_block_simd(in, source.size(), &enc_dec_buf[0], read.data());

  ASSERT_EQ(source, read);
}

void read_write_block_optimized(const std::vector<uint32_t>& source, std::vector<uint32_t>& enc_dec_buf) {
  // write block
  ASSERT_EQ(128, source.size());
  irs::bstring buf;
  irs::bytes_output out(buf);
  irs::encode::bitpack::write_block_simd(out, &source[0], &enc_dec_buf[0]);

  // read block
  bytes_input in(buf);
  std::vector<uint32_t> read(source.size());
  irs::encode::bitpack::read_block_simd(in, &enc_dec_buf[0], read.data());

  ASSERT_EQ(source, read);
}

void read_write_optimized(const std::vector<uint32_t>& source) {
  // intermediate buffer for encoding/decoding
  std::vector<uint32_t> enc_dec_buf(source.size());
  read_write_optimized(source, enc_dec_buf);
}

void read_write_block_optimized(const std::vector<uint32_t>& source) {
  // intermediate buffer for encoding/decoding
  ASSERT_EQ(128, source.size());
  std::vector<uint32_t> enc_dec_buf(source.size());
  read_write_block_optimized(source, enc_dec_buf);
}

#endif

} // detail
} // tests

TEST(store_utils_tests, vint_from_array) {
  tests::detail::vencode_from_array(UINT32_C(0), 1);
  tests::detail::vencode_from_array(UINT32_C(1), 1);
  tests::detail::vencode_from_array(UINT32_C(100), 1);
  tests::detail::vencode_from_array(UINT32_C(127), 1);
  tests::detail::vencode_from_array(UINT32_C(128), 2);
  tests::detail::vencode_from_array(UINT32_C(16383), 2);
  tests::detail::vencode_from_array(UINT32_C(16384), 3);
  tests::detail::vencode_from_array(UINT32_C(32000), 3);
  tests::detail::vencode_from_array(UINT32_C(2097151), 3);
  tests::detail::vencode_from_array(UINT32_C(2097152), 4);
  tests::detail::vencode_from_array(UINT32_C(268435455), 4);
  tests::detail::vencode_from_array(UINT32_C(268435456), 5);
  tests::detail::vencode_from_array(std::numeric_limits<uint32_t>::max(), 5);
}

TEST(store_utils_tests, vlong_from_array) {
  tests::detail::vencode_from_array(UINT64_C(0), 1);
  tests::detail::vencode_from_array(UINT64_C(1), 1);
  tests::detail::vencode_from_array(UINT64_C(100), 1);
  tests::detail::vencode_from_array(UINT64_C(127), 1);
  tests::detail::vencode_from_array(UINT64_C(128), 2);
  tests::detail::vencode_from_array(UINT64_C(16383), 2);
  tests::detail::vencode_from_array(UINT64_C(16384), 3);
  tests::detail::vencode_from_array(UINT64_C(32000), 3);
  tests::detail::vencode_from_array(UINT64_C(2097151), 3);
  tests::detail::vencode_from_array(UINT64_C(2097152), 4);
  tests::detail::vencode_from_array(UINT64_C(268435455), 4);
  tests::detail::vencode_from_array(UINT64_C(268435456), 5);
  tests::detail::vencode_from_array(UINT64_C(34359738367), 5);
  tests::detail::vencode_from_array(UINT64_C(34359738368), 6);
  tests::detail::vencode_from_array(UINT64_C(4398046511103), 6);
  tests::detail::vencode_from_array(UINT64_C(4398046511104), 7);
  tests::detail::vencode_from_array(UINT64_C(72057594037927935), 8);
  tests::detail::vencode_from_array(UINT64_C(72057594037927936), 9);
  tests::detail::vencode_from_array(std::numeric_limits<uint64_t>::max(), 10);
}

TEST(store_utils_tests, zvfloat_read_write) {
  tests::detail::read_write_core<float_t>(
    {
      std::numeric_limits<float_t>::max(), 
      std::numeric_limits<float_t>::min(), 
      0.f,
      std::numeric_limits<float_t>::infinity(),
      -std::numeric_limits<float_t>::infinity(),
      125.f,
      53.f,
      -1.f,
      823132.4332f,
      132621123.234f
      -21532764.631984f,
      -9847.23427f
    },
    irs::read_zvfloat,
    irs::write_zvfloat
  );

  /* NaN case */
  tests::detail::read_write_core_nan<float_t>(
    irs::read_zvfloat,
    irs::write_zvfloat
  );
}

TEST(store_utils_tests, zvdouble_read_write) {
  tests::detail::read_write_core<double_t>(
    {
      std::numeric_limits<double_t>::max(),
      std::numeric_limits<double_t>::min(),
      std::numeric_limits<float_t>::max(),
      std::numeric_limits<float_t>::min(),
      0.,
      std::numeric_limits<double_t>::infinity(),
      -std::numeric_limits<double_t>::infinity(),
      123.,
      43.,
      -1.,
      247280479234.1239171,
      3694208.1320823132,
      -19274316.123,
      -98743098097.34352532
    },
    irs::read_zvdouble,
    irs::write_zvdouble
  );

  /* NaN case */
  tests::detail::read_write_core_nan<double_t>(
    irs::read_zvdouble,
    irs::write_zvdouble
  );
}

TEST( store_utils_tests, size_read_write) {
  tests::detail::read_write_core<size_t>(
  {
      std::numeric_limits<size_t>::max(),
      std::numeric_limits<size_t>::min(),
      0,
      size_t(2037323242),
      size_t(123132142),
      size_t(12371792192121),
      size_t(9719496156)
  },
  irs::read_size,
  irs::write_size);
}

TEST(store_utils_tests, zvint_read_write) {
  tests::detail::read_write_core<int32_t>(
  {
    std::numeric_limits<int32_t>::max(),
    std::numeric_limits<int32_t>::min(),
    0,
    103282101,
    51916519,
    -911728376,
    -10725017
  },
  irs::read_zvint,
  irs::write_zvint);
}

TEST(store_utils_tests, zvlong_read_write) {
  tests::detail::read_write_core<int64_t>(
  {
    std::numeric_limits<int64_t>::max(),
    std::numeric_limits<int64_t>::min(),
    0LL,
    -7179394719192822LL,
    -9197947324326483264LL,
    -9184236868362391274LL,
    -91724962191921979LL
  },
  irs::read_zvlong,
  irs::write_zvlong);
}

TEST(store_utils_tests, std_string_read_write) {
  tests::detail::read_write_core<std::string>(
  {
    std::string(),
    std::string(""),
    std::string("quick brown fox 1232141"),
    std::string("jumps over the 0218021"),
    std::string("lazy p1230142hlds"),
    std::string("dob  sdofjasoufdsa")
  },
  irs::read_string<std::string>,
  irs::write_string<std::string>);
}

TEST(store_utils_tests, bytes_read_write) {
  tests::detail::read_write_core<bstring>(
  {
    bstring(),
    bstring(irs::ref_cast<byte_type>(irs::string_ref("qalsdflsajfd"))),
    bstring(irs::ref_cast<byte_type>(irs::string_ref("jfdldsflaflj"))),
    bstring(irs::ref_cast<byte_type>(irs::string_ref("102174174010"))),
    bstring(irs::ref_cast<byte_type>(irs::string_ref("0182ljdskfaof")))
  },
  irs::read_string<bstring>,
  irs::write_string<bstring>);
}

TEST( store_utils_tests, string_vector_read_write) {
  typedef std::unordered_set<std::string> container_t;

  container_t src{
    "quick", "brown", "fox", 
    "jumps", "over", "the", 
    "lazy", "dog", "mustard"
  };

  irs::bstring buf;
  irs::bytes_output out(buf);
  write_strings(out, src);

  tests::detail::bytes_input in(buf);
  const container_t readed = read_strings<container_t>(in);

  ASSERT_EQ(src, readed);
}

TEST(store_utils_tests, packed_read_write_32) {
  // 1024
  {
    // common case 
    const std::vector<uint32_t> src = {
      3030, 72395, 205968, 277100, 341904, 353021, 423561, 513432, 587145, 587945, 633571, 674202, 767787, 887192, 889164, 905105,
      1026301, 1231820, 1318617, 1424604, 1489142, 1553313, 1776298, 1981378, 1998939, 2327716, 2527259, 2527367, 2573432, 2664741,
      2810148, 2868801, 2921289, 3056197, 3199760, 3211350, 3230015, 3533106, 3581250, 3768342, 3776133, 3785251, 3878275, 3902497,
      3907880, 4318221, 4388349, 4480186, 4520455, 4728578, 4885110, 5046714, 5241849, 5527782, 5560423, 5609030, 5622298, 5695920,
      5906646, 6077402, 6082814, 6395152, 6694781, 6847414, 7108981, 7175244, 7321394, 7432840, 7459353, 7511111, 7514527, 7711522,
      7719236, 7878712, 8014998, 8227494, 8230122, 8302921, 8318151, 8646953, 8660065, 8736620, 8960913, 8961867, 9012861, 9055464,
      9114427, 9209181, 9350992, 9508654, 9603490, 9755375, 9825926, 9910241, 9922351, 9935381, 9956143, 9962773, 10181638, 10348602,
      10554921, 10600490, 10631480, 10722446, 10811084, 10874393, 10891114, 11237275, 11259624, 11306245, 11413140, 11417915, 11457732,
      11481245, 11492444, 11504343, 11550038, 11634573, 11776393, 11780857, 11911868, 11962707, 12106653, 12122567, 12151515, 12173263, 
      12190074, 12226745, 12281810, 12503799, 12574082, 12582289, 12665129, 12681029, 12726752, 12780545, 12809516, 12830051, 12877512, 
      12987141, 13050539, 13330630, 13388011, 13425398, 13571963, 13583607, 13687637, 13694484, 13726883, 13915697, 13931117, 13951828, 
      13952250, 13989290, 14117540, 14353345, 14354303, 14592486, 14643982, 14869897, 14882964, 14922521, 14988498, 15034308, 15056055, 
      15113782, 15240790, 15369680, 15380427, 15411172, 15417317, 15729090, 15764133, 15853821, 15859615, 16247583, 16292407, 16389640, 
      16456558, 16547523, 16714620, 16802418, 16973425, 16998745, 17213626, 17388894, 17448060, 17521967, 17733688, 17812442, 17896068, 
      18024738, 18182124, 18417043, 18616591, 18716327, 18903691, 18905410, 19065064, 19137080, 19338047, 19376652, 19943695, 20095034, 
      20128976, 20236832, 20404099, 20443485, 20461787, 20469688, 20564037, 20687638, 21071154, 21108124, 21119164, 21282187, 21384708, 
      21596932, 21835360, 21951692, 21952774, 21988119, 22053273, 22214416, 22265244, 22368011, 22902158, 23035057, 23062854, 23101785, 
      23158388, 23307757, 23308698, 23333133, 23516090, 23560529, 23581872, 23849523, 23932364, 23941131, 24013655, 24306855, 24402926, 
      24422845, 24559654, 24844350, 24908096, 24933890, 25067853, 25073899, 25199597, 25205137, 25297213, 25344868, 25561102, 25577090, 
      25762081, 25812856, 25973881, 26025536, 26040377, 26045584, 26167826, 26287339, 26307879, 26417338, 26444921, 26467447, 26770089, 
      26811752, 27138094, 27207948, 27351988, 27434582, 27454340, 27706894, 27956428, 28018972, 28091211, 28219714, 28267572, 28461768, 
      28512980, 28675844, 28924140, 29084164, 29258193, 29334550, 29339206, 29455024, 29528836, 29713611, 29860461, 30011377, 30058618, 
      30318222, 30447327, 30623491, 30699058, 30749709, 30756496, 30874628, 30884430, 31025203, 31060386, 31074195, 31248530, 31258593, 
      31266971, 31349650, 31859070, 32173415, 32260384, 32390022, 32425225, 32440072, 32481976, 32730736, 32934072, 33201377, 33201772, 
      33317236, 33407069, 33424498, 33460799, 33504418, 33798859, 33804151, 33913769, 33917067, 34110462, 34454288, 34508142, 34528222, 
      34671641, 34775611, 34855035, 34870081, 35074205, 35079320, 35160557, 35188791, 35245884, 35259678, 35330799, 35475581, 35509847, 
      35846943, 36020771, 36041382, 36177147, 36214567, 36302770, 36306406, 36355752, 36456177, 36483021, 36513782, 36650612, 36822072, 
      36826190, 36851220, 36917215, 37037456, 37080817, 37096003, 37140301, 37220335, 37267913, 37272861, 37363414, 37619446, 37795971,
      38053665, 38095590, 38151043, 38178151, 38226216, 38321717, 38505215, 38775807, 38800920, 38896175, 38915176, 38941327, 38985105,
      39036135, 39191641, 39257297, 39349127, 39384537, 39407828, 39432656, 39432995, 39537805, 39617114, 39659524, 39708244, 39758228, 
      39925416, 40008860, 40042359, 40059932, 40089298, 40217868, 40260734, 40280757, 40297628, 40380695, 40505164, 40529737, 40634659, 
      40642563, 40653586, 40656170, 40657071, 40832961, 40867075, 40883874, 40984002, 41103494, 41142544, 41249898, 41259435, 41315837, 
      41316673, 41461170, 41516004, 41560514, 41585764, 41587551, 41791166, 41937007, 41946371, 42518818, 42581797, 42765600, 42788931, 
      43049426, 43112229, 43426062, 43473091, 43528385, 43551847, 43612140, 43710573, 43810495, 43856352, 43898164, 43903351, 43971386, 
      44183604, 44185283, 44267233, 44361973, 44372353, 44387742, 44498035, 44539556, 44741773, 45011892, 45130443, 45274399, 45595888, 
      45638826, 45777636, 45956146, 46023661, 46182917, 46332978, 46674222, 46725115, 46739400, 46885044, 46939210, 46986330, 47096315, 
      47150729, 47173786, 47339706, 47587369, 47593653, 47607843, 47622336, 47650704, 47666088, 47707984, 48145341, 48208403, 48234439, 
      48256231, 48257236, 48439339, 48701870, 48725426, 48736595, 48771381, 48777929, 48931276, 48993831, 49038428, 49043520, 49047071, 
      49092014, 49154401, 49373957, 49378824, 49391253, 49398436, 49531441, 49553690, 49749672, 49869485, 49885775, 49956538, 49990974, 
      49999948, 50018968, 50031577, 50071847, 50119328, 50242138, 50244455, 50307787, 50382470, 50703409, 50903807, 50973514, 50994735, 
      51510480, 51539603, 51571040, 51736811, 51762665, 51781698, 51824425, 51926367, 52082487, 52209938, 52228561, 52250127, 52340287, 
      52410702, 52477005, 52625909, 52664391, 52714315, 52755165, 53028357, 53111346, 53286736, 53318137, 53425228, 53460441, 53507757, 
      53732905, 53742580, 53751709, 53837284, 53904021, 54050288, 54076119, 54118678, 54170971, 54248014, 54313293, 54430676, 54476933, 
      54560172, 54639136, 54687924, 54697039, 55026881, 55187526, 55285998, 55396273, 55451018, 55456010, 55483858, 55530993, 55581479,
      55617678, 55643140, 55711490, 55795019, 55872508, 55907774, 55956812, 55966335, 56074724, 56174940, 56279311, 56307133, 56355458,
      56441420, 56475752, 56765429, 56819669, 56867170, 56894252, 56934306, 56987251, 57047841, 57107319, 57309480, 57349389, 57381902,
      57394133, 57512127, 57640202, 57714333, 57784918, 57976001, 58028107, 58324115, 58450564, 58845723, 58852925, 59037601, 59042908,
      59051180, 59054387, 59154371, 59186767, 59234190, 59236583, 59326756, 59330106, 59504511, 59521083, 59560955, 59568943, 59851909, 
      60262263, 60496720, 60701114, 60772075, 60925811, 61050439, 61082168, 61197020, 61232080, 61280230, 61488917, 61514779, 61653497, 
      61689170, 61797494, 61834467, 61848370, 61887114, 61915907, 61999624, 62030738, 62123935, 62213980, 62252101, 62461492, 62541905, 
      62545432, 62818196, 63019513, 63033353, 63144859, 63171160, 63395246, 63407068, 63469851, 63552965, 63589576, 63606148, 63621818, 
      63657264, 63689207, 64063444, 64233161, 64488768, 64570920, 64577287, 64587247, 64689094, 64697522, 64741465, 64876208, 64929526, 
      64972972, 65036832, 65060509, 65398798, 65454568, 65513134, 65525301, 65799215, 65919715, 65980543, 66153961, 66258490, 66268761, 
      66299380, 66366779, 66453947, 66499459, 66511008, 66725125, 66832847, 66854223, 66967657, 67005036, 67141651, 67150360, 67246928, 
      67251243, 67514303, 67709671, 67723727, 67839316, 68121832, 68134550, 68360875, 68374319, 68384130, 68495718, 68560312, 68586345, 
      68627655, 68753844, 68910276, 69049298, 69130990, 69357507, 69455082, 69535252, 69598388, 69695297, 69788330, 70123484, 70146806, 
      70403234, 70445489, 70553617, 70641851, 70784480, 70819098, 70924547, 70955882, 70971766, 71318486, 71348283, 71352089, 71620845, 
      71622587, 71874909, 71986540, 72079282, 72326339, 72367275, 72563717, 72567152, 72622522, 72744005, 72840053, 73013570, 73016330, 
      73262722, 73288690, 73298927, 73351362, 73611503, 73726948, 73765135, 73783059, 73955370, 73995044, 74133733, 74175107, 74195437, 
      74283872, 74341719, 74420079, 74449325, 74600408, 74667381, 74812681, 74859225, 74888383, 74984654, 75094506, 75331022, 75611209, 
      75968302, 76042834, 76074027, 76196327, 76208930, 76237768, 76635554, 76663658, 76838659, 76932423, 76933523, 76977429, 77225066, 
      77250275, 77317711, 77437919, 77492016, 77507158, 77592424, 77603444, 77608669, 77642302, 77690741, 77775756, 77865920, 78006115, 
      78042486, 78064832, 78105813, 78186987, 78197386, 78317007, 78825518, 79119200, 79330854, 79350551, 79373571, 79535573, 79677461, 
      80185437, 80207023, 80220892, 80313781, 80377737, 80411463, 80537229, 80577641, 80608880, 80737583, 80743862, 80783197, 80811497, 
      80825979, 81073594, 81141534, 81168284, 81227858, 81324393, 81369726, 81396499, 81410341, 81494496, 81716773, 81737293, 81858726, 
      82054158, 82164292, 82170002, 82281375, 82564550, 82744324, 82841353, 83276405, 83331113, 83333575, 83403209, 83950922, 83972894, 
      84064074, 84321069, 84391633, 84586608, 84660027, 84723809, 84727649, 84770657, 84892222, 84910329, 84911741, 85112189, 85208510,
      85212646, 85391127, 85406601, 85654618, 85743479, 85876964, 85916881, 85945523, 85948472, 86155307, 86340193, 86368263, 86480022, 
      86499754, 86595898, 86749937, 86760768, 86772410, 86910171, 87154123, 87171003, 87397742, 87417480, 87616690, 87729972, 87738055, 
      87744736, 87849235, 87850013, 87938180, 88073439, 88075262, 88112680, 88133296, 88166084, 88377396, 88378293, 88504624, 88510491, 
      88621281, 88643939, 89010611, 89161909, 89199063, 89328658, 89384754, 89587389, 89673802, 89689261, 89721180, 89761998, 89859769, 
      89861627, 89886667, 89893088, 90200591, 90272369, 90300429, 90401903, 90403940, 90484554, 90762508, 90775022, 90795700, 90887137, 
      90915175, 91391637, 91431386, 91529935, 91760251, 91813249, 91813582, 91865976, 91867203, 91881929, 92002094, 92054813, 92133122, 
      92181697, 92228043, 92342396, 92404018, 92670038, 92791649, 92841929, 92854306, 92926574, 92975970, 93127398, 93310874, 93539762, 
      93549837, 93728836, 93739667, 93892270, 94123021, 94236323, 94337502, 94375328, 94380005, 94386994, 94499720, 94510759, 94588636, 
      94805625, 94931508, 94939106, 95034068, 95124549, 95147169, 95149146, 95472904, 95621551, 95775352, 95894432, 95941094, 96044123,
      96195304, 96275284, 96322550, 96638297, 96639040, 96747586, 96843244, 96993196, 97050544, 97151578, 97204246, 97338976, 97581492,
      97902670, 98678331, 98939333, 99068431, 99075327, 99167331, 99190344, 99440294, 99514282, 99518947, 99605157, 99700854, 99903781, 
      99975302
    };
    tests::detail::packed_read_write_core(src);
    
    // all equals case
    std::vector<uint32_t> all_eq_src(src.size(), 5);
    tests::detail::packed_read_write_core(all_eq_src);
  }
  
  // 512
  {
    // common case 
    const std::vector<uint32_t> src = {
      91635, 257997, 630975, 839092, 1203968, 1266432, 1432506, 1435281, 1462196, 1609051, 1657377, 1663937, 2282450, 2346424, 2411911, 2481097, 
      2902054, 3001196, 3101678, 3166734, 3237536, 3617069, 3664329, 3964378, 4030057, 4295980, 4422917, 4477957, 4773068, 5000571, 5022204, 5116086,
      5406129, 5525585, 5686429, 5802447, 5935105, 6045503, 6189963, 6220796, 6510146, 6645824, 6661592, 6958864, 7433398, 7465236, 7547806, 8171421, 
      8962694, 9149587, 9396521, 9468471, 9598650, 9840665, 10438146, 10469919, 10569454, 10683379, 10911386, 11528291, 11645977, 11895844, 11899327, 
      12083780, 12412559, 12583150, 12610651, 12613121, 13644244, 13675369, 13750735, 13769564, 13952100, 14137663, 14321952, 14371879, 15030062, 15329780,
      15405786, 15519175, 15648033, 15676774, 15713283, 16038341, 16103567, 16246888, 16439396, 16578537, 17059483, 17174215, 17341033, 17372458, 17394400,
      17413881, 17511812, 17914863, 17935180, 18388894, 18422292, 18964993, 19060808, 19264448, 19431484, 19582768, 19706877, 19864610, 19977913, 20154817,
      20316328, 20357105, 20408345, 20481587, 20691328, 21074609, 21234818, 21310132, 21366649, 21698232, 21789015, 21796391, 21870085, 22042318, 22064670,
      22079295, 22111659, 22126996, 22319409, 22495389, 22520133, 22746775, 23095502, 23128210, 23192183, 23388261, 23460089, 23466422, 24041747, 24291889,
      24387585, 24651874, 24673603, 24920792, 25055723, 25056445, 25079583, 25203053, 25219762, 25247125, 25789847, 25841676, 25854209, 26065965, 26213673,
      26257797, 26600250, 26625277, 26733336, 26970793, 27259896, 27351447, 27392913, 27393080, 27638308, 27689942, 28036113, 28093801, 28335234, 28678952,
      28917032, 29356826, 29364249, 29576431, 29766426, 29827112, 30362880, 30638810, 31065816, 31088751, 31132777, 31221659, 31496268, 31541936, 31579364,
      31674932, 31793861, 31882438, 32238557, 32366732, 32613593, 32767950, 32839606, 33569472, 33569659, 33720856, 33900529, 34218955, 34313554, 35063740,
      35140819, 35198214, 35634817, 35713783, 35778737, 36001034, 36103543, 36241645, 36310691, 36440190, 36719005, 36878123, 36911043, 37759836, 38066063, 
      38100421, 38315380, 38366562, 38434448, 38507106, 38728935, 38831109, 39259494, 39303769, 39582875, 39762992, 39766481, 39941314, 40025513, 40886450, 
      41166828, 41557959, 41589986, 41693170, 42016144, 42053411, 42243151, 42310539, 42961014, 43055425, 43114404, 43182913, 43666132, 43855485, 45168759, 
      45403181, 45503086, 45560602, 45571630, 46159025, 46658225, 46699076, 46762958, 46936091, 46957765, 46986412, 47100449, 47831181, 47836047, 48508096,
      49276647, 49296644, 50173449, 50259969, 50499103, 50635708, 50691991, 51169697, 51181798, 51745251, 51764801, 51869964, 51932263, 52132857, 52470427,
      52628910, 52679752, 52773556, 52917819, 53375751, 53426209, 53757780, 53795188, 53865384, 54176039, 54210883, 54227563, 54231900, 54463515, 54726963,
      54821290, 55028566, 55082823, 55146439, 55203289, 55415701, 55716796, 56253528, 56565103, 56634137, 56687737, 56896734, 57009688, 57025026, 57404897, 
      57558613, 58196567, 58502749, 58811983, 59011189, 59735587, 60023157, 60182827, 60286347, 60943904, 61235759, 61383177, 61435963, 61528510, 61691046,
      61880838, 62141177, 62268355, 62326954, 62675708, 62856756, 63430779, 63619191, 63626616, 64109817, 64699352, 64716572, 64961514, 65088786, 65200035,
      65295589, 65350237, 65840386, 65978796, 65992295, 66146709, 66434558, 66686707, 66706080, 66721454, 67377689, 68246271, 68258853, 68470291, 68584790,
      68702161, 69662017, 69743423, 69792475, 69853813, 69885652, 70206463, 70334607, 71148802, 71302350, 71401453, 71667391, 71917640, 71956875, 71976842,
      72330523, 72337813, 72425863, 72905913, 72955933, 73224141, 73282957, 73528160, 73670626, 74082313, 74116480, 74260551, 74328997, 74514196, 74615786,
      74642575, 74718183, 74719802, 74754624, 74789007, 74826808, 74965167, 74974767, 75253332, 75542169, 75785026, 75990477, 76094478, 76397634, 76449781,
      76536895, 76715638, 76970461, 77043137, 77072833, 77381524, 77654443, 77888603, 78047330, 78161583, 78209435, 78269584, 78327839, 78644914, 78687322,
      79324956, 79450125, 79472637, 79736943, 79834875, 80108016, 80110389, 80311583, 80489186, 80643842, 81117199, 81351261, 81605155, 81637632, 81640703,
      81737602, 82917449, 83172155, 83819300, 83864226, 84240690, 84624026, 84822521, 84888450, 84994836, 85025736, 85243899, 85617107, 85638914, 85937991,
      86441644, 86492449, 86593815, 86716121, 86994443, 87098149, 87133576, 87161417, 87226428, 87292062, 87519605, 87712495, 87852890, 87881063, 88452390,
      88647917, 88660052, 89258592, 90492696, 90512673, 90563433, 90660931, 91085015, 91283477, 91512591, 92043735, 92238779, 92466227, 92578844, 92935868,
      93353779, 93693680, 93723010, 93921037, 94044762, 94270717, 94492026, 94546836, 94769736, 94813746, 95260957, 95467065, 95504873, 95562155, 95619590,
      95687266, 95976023, 96223131, 96233290, 96350984, 96386091, 96910540, 96953078, 96980924, 96993282, 97244420, 97252963, 97378815, 97519175, 97713503,
      97744216, 97833020, 97862271, 97918262, 98313283, 98615184, 99055584, 99206079, 99331019, 99343531, 99404330, 99489043, 99617829, 99785097
    };
    tests::detail::packed_read_write_core(src);
    
    // all equals case
    std::vector<uint32_t> all_eq_src(src.size(), 5);
    tests::detail::packed_read_write_core(all_eq_src);
  }
 
  // 128 
  {
    // common case
    const std::vector<uint32_t> src = {
      356781, 677003, 2283481, 4514882, 4957746, 6327851, 6490047, 6569811, 8000979, 8347627, 8470292, 8693072, 9612689, 11196992, 11720782, 
      13227106, 13480180, 14137087, 14435989, 14472035, 15987504, 16184259, 17242488, 19152609, 20844922, 23838025, 24128867, 24258643, 24509465, 
      24972006, 26108685, 27062473, 27737165, 29434988, 31187733, 32106181, 32267237, 32324755, 32353201, 32896983, 33631170, 33656796, 33683319, 
      33984644, 35339062, 36105434, 36302526, 36418428, 38289798, 38669849, 41761816, 42958104, 43278513, 43546168, 44814114, 44844910, 47300613, 
      47566801, 47802784, 47809849, 49006798, 50979550, 51030266, 51071422, 51806473, 52131368, 52750642, 52973049, 54457016, 55435765, 56334987, 
      58005966, 58414252, 59288445, 59344859, 59907581, 60782876, 61511610, 61986402, 62662249, 63206145, 63536473, 64676929, 64792434, 65597611, 
      65669123, 65831801, 67698285, 68037637, 69366211, 69883578, 70312964, 70344319, 71090062, 71316735, 74750061, 75514663, 76013834, 77028235, 
      77071624, 77379459, 79654138, 80742822, 82652069, 84114075, 84168823, 84589767, 84998898, 86053769, 86628211, 86677949, 86876908, 87057203, 
      87688623, 87966702, 88468890, 90825729, 91356618, 91543691, 91586402, 92745195, 93131606, 93437291, 95193854, 95829037, 96644312, 98075750, 99014452
    };
    tests::detail::packed_read_write_core(src);
    tests::detail::packed_read_write_block_core(src);

    // all equals case
    std::vector<uint32_t> all_eq_src(src.size(), 5);
    tests::detail::packed_read_write_core(all_eq_src);
    tests::detail::packed_read_write_block_core(all_eq_src);
  }
}

TEST(store_utils_tests, read_write_block) {
  // block_size = 128
  const size_t block_size = 128;
  // distinct values
  std::vector<uint32_t> data = {
    867377632,	904649657,	354461109,	576026921,	406163632,
    168409093,	33485512 , 611136354 , 140275004 , 654422173,
    405770063,	390577167,	780047069,	438362754,	469076575,
    916930378,	291775422,	169687154,	834852341,	811869909,
    250897257,	852383167,	478986610,	257699679,	112290896,
    648885334,	897578972,	235499871,	368212067,	20494714,
    321165319,	993744046,	334855956,	339418651,	23411270,
    486634346,	258313717,	319757878,	608722518,	331995880,
    116102182,	801348392,	256163092,	332114117,	304988840,
    917258980,	686173811,	948343613,	786828070,	319530963,
    578518934,	881904875,	144381596,	948206742,	876042799,
    636018099,	941670974,	795607349,	487169927,	365985618,
    883623659,	853001164,	723334064,	582408314,	570539073,
    863140586,	99184400 , 621307734 , 404880591 , 544074242,
    395871575,	383432524,	469395010,	462667762,	721641738,
    306107286,	379618512,	17517346 , 735771377 , 147846584,
    858436879,	499675853,	539719264,	842602895,	870115838,
    236179208,	927978513,	657234182,	205163278,	100358377,
    970958186,	277229354,	952603794,	234804978,	489958521,
    378864765,	482550401,	587171069,	368374855,	835303649,
    113016087,	220060336,	205821727,	794302091,	393689790,
    18366964 , 835940475 , 988552545 , 976790514 , 736784554,
    332759224,	951688629,	413866856,	245001983,	839481147,
    575871539,	536021091,	313731186,	553136314,	291903625,
    916491807,	629745928,	217677834,	787133498,	167330024,
    793647012,	474689745,	228759450
  };
  tests::detail::read_write_block(data);
  // all equals
  tests::detail::read_write_block(std::vector<uint32_t>(block_size, 5));
  // all except first are equal, dirty buffer
  {
    std::vector<uint32_t> end_dec_buf(block_size, std::numeric_limits<uint32_t>::max());
    std::vector<uint32_t> src(block_size, 1); src[0] = 0;
    tests::detail::read_write_block(src, end_dec_buf);
  }
}

TEST(store_utils_tests, shift_pack_unpack_32) {
  tests::detail::shift_pack_unpack_core_32(2343242, true);
  tests::detail::shift_pack_unpack_core_32(2343242, false);
  tests::detail::shift_pack_unpack_core_32(63456345, true);
  tests::detail::shift_pack_unpack_core_32(63456345, false);
  tests::detail::shift_pack_unpack_core_32(0, true);
  tests::detail::shift_pack_unpack_core_32(0, false);
  tests::detail::shift_pack_unpack_core_32(0x7FFFFFFF, true);
  tests::detail::shift_pack_unpack_core_32(0x7FFFFFFF, false);
}

TEST(store_utils_tests, shift_pack_unpack_64) {
  tests::detail::shift_pack_unpack_core_64(333618218293788671LL, true);
  tests::detail::shift_pack_unpack_core_64(333618218293788671LL, false);
  tests::detail::shift_pack_unpack_core_64(8653986421, true);
  tests::detail::shift_pack_unpack_core_64(8653986421, false);
  tests::detail::shift_pack_unpack_core_64(0, true);
  tests::detail::shift_pack_unpack_core_64(0, false);
  tests::detail::shift_pack_unpack_core_64(0x7FFFFFFFFFFFFFFFLL, true);
  tests::detail::shift_pack_unpack_core_64(0x7FFFFFFFFFFFFFFFLL, false);
}

TEST(store_utils_tests, delta_encode_decode) {
  tests::detail::delta_encode_decode_core(1, 1000); // step = 1, count = 1000
  tests::detail::delta_encode_decode_core(128, 1000); // step = 128, count = 1000
  tests::detail::delta_encode_decode_core(1000, 1); // step = 1000, count = 1
}

TEST(store_utils_tests, avg_encode_decode) {
  tests::detail::avg_encode_decode_core(1, 1000); // step = 1, count = 1000
  tests::detail::avg_encode_decode_core(128, 1000); // step = 128, count = 1000

  // single value case
  {
    std::vector<uint64_t> values{ 1000 };

    auto encoded = values;
    const auto stats = irs::encode::avg::encode(encoded.data(), encoded.data() + encoded.size());
    ASSERT_EQ(1000, stats.first);
    ASSERT_EQ(0, stats.second);
    ASSERT_EQ(0, encoded[0]);

    auto decoded = encoded;
    irs::encode::avg::decode(stats.first, stats.second, decoded.data(), decoded.data() + decoded.size());
    ASSERT_EQ(values, decoded);
  }
}

TEST(store_utils_tests, avg_encode_block_read_write) {
  // rl case
  {
    const size_t count = 1024; // 1024 % packed::BLOCK_SIZE_64 == 0
    const uint64_t step = 127;

    std::vector<uint64_t> values;
    values.reserve(count);
    uint64_t value = 0;
    for (size_t i = 0; i < 1024 ; ++i) {
      values.push_back(value += step);
    }

    // avg encode
    auto avg_encoded = values;
    const auto stats = irs::encode::avg::encode(
      avg_encoded.data(), avg_encoded.data() + avg_encoded.size()
    );

    std::vector<uint64_t> buf; // temporary buffer for bit packing
    buf.resize(values.size());

    irs::bstring out_buf;
    irs::bytes_output out(out_buf);
    irs::encode::avg::write_block(
      out, stats.first, stats.second, avg_encoded.data(), avg_encoded.size(), buf.data()
    );

    ASSERT_EQ(
      irs::bytes_io<uint64_t>::vsize(step) + irs::bytes_io<uint64_t>::vsize(step) + irs::bytes_io<uint32_t>::vsize(irs::encode::bitpack::ALL_EQUAL) + irs::bytes_io<uint64_t>::vsize(0), // base + avg + bits + single value
      out_buf.size()
    );

    {
      tests::detail::bytes_input in(out_buf);
      const uint64_t base = in.read_vlong();
      const uint64_t avg= in.read_vlong();
      const uint64_t bits = in.read_vint();
      ASSERT_EQ(uint32_t(irs::encode::bitpack::ALL_EQUAL), bits);
      ASSERT_EQ(step, base);
      ASSERT_EQ(step, avg);
    }

    {
      tests::detail::bytes_input in(out_buf);
      ASSERT_TRUE(irs::encode::avg::check_block_rl64(in, step));
    }

    {
      uint64_t base, avg;
      tests::detail::bytes_input in(out_buf);
      ASSERT_TRUE(irs::encode::avg::read_block_rl64(in, base, avg));
      ASSERT_EQ(step, base);
      ASSERT_EQ(step, avg);
    }

    {
      tests::detail::bytes_input in(out_buf);

      const uint64_t base = in.read_vlong();
      const uint64_t avg = in.read_vlong();
      const uint32_t bits = in.read_vint();
      ASSERT_EQ(uint32_t(irs::encode::bitpack::ALL_EQUAL), bits);

      bool success = true;
      auto begin = values.begin();
      irs::encode::avg::visit_block_rl64(
        in, base, avg, values.size(),
        [&begin, &success](uint64_t value) {
          success = success && *begin == value;
          ++begin;
      });
      ASSERT_TRUE(success);
      ASSERT_EQ(values.end(), begin);
    }
  }
}

#ifdef IRESEARCH_SSE2

TEST(store_utils_tests, read_write_block32_optimized) {
  // block_size = 128
  const size_t block_size = 128;
  // distinct values
  std::vector<uint32_t> data = {
    867377632,	904649657,	354461109,	576026921,	406163632,
    168409093,	33485512 , 611136354 , 140275004 , 654422173,
    405770063,	390577167,	780047069,	438362754,	469076575,
    916930378,	291775422,	169687154,	834852341,	811869909,
    250897257,	852383167,	478986610,	257699679,	112290896,
    648885334,	897578972,	235499871,	368212067,	20494714,
    321165319,	993744046,	334855956,	339418651,	23411270,
    486634346,	258313717,	319757878,	608722518,	331995880,
    116102182,	801348392,	256163092,	332114117,	304988840,
    917258980,	686173811,	948343613,	786828070,	319530963,
    578518934,	881904875,	144381596,	948206742,	876042799,
    636018099,	941670974,	795607349,	487169927,	365985618,
    883623659,	853001164,	723334064,	582408314,	570539073,
    863140586,	99184400 , 621307734 , 404880591 , 544074242,
    395871575,	383432524,	469395010,	462667762,	721641738,
    306107286,	379618512,	17517346 , 735771377 , 147846584,
    858436879,	499675853,	539719264,	842602895,	870115838,
    236179208,	927978513,	657234182,	205163278,	100358377,
    970958186,	277229354,	952603794,	234804978,	489958521,
    378864765,	482550401,	587171069,	368374855,	835303649,
    113016087,	220060336,	205821727,	794302091,	393689790,
    18366964 , 835940475 , 988552545 , 976790514 , 736784554,
    332759224,	951688629,	413866856,	245001983,	839481147,
    575871539,	536021091,	313731186,	553136314,	291903625,
    916491807,	629745928,	217677834,	787133498,	167330024,
    793647012,	474689745,	228759450
  };
  tests::detail::read_write_optimized(data);
  tests::detail::read_write_block_optimized(data);
  // all equals
  tests::detail::read_write_optimized(std::vector<uint32_t>(block_size, 5));
  tests::detail::read_write_block_optimized(std::vector<uint32_t>(block_size, 5));
  // all except first are equal, dirty buffer
  {
    std::vector<uint32_t> end_dec_buf(block_size, std::numeric_limits<uint32_t>::max());
    std::vector<uint32_t> src(block_size, 1); src[0] = 0;
    tests::detail::read_write_optimized(src, end_dec_buf);
    tests::detail::read_write_block_optimized(src);
  }
}

#endif

