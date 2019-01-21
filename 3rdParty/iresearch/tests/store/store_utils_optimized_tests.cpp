////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifdef IRESEARCH_SSE2

#include "tests_shared.hpp"
#include "store/store_utils.hpp"
#include "store/store_utils_optimized.hpp"

namespace {

void read_write_block_optimized(const std::vector<uint32_t>& source, std::vector<uint32_t>& enc_dec_buf) {
  // write block
  iresearch::bytes_output out;
  irs::encode::bitpack::write_block_optimized(out, &source[0], source.size(), &enc_dec_buf[0]);

  // read block
  iresearch::bytes_input in(out);
  std::vector<uint32_t> read(source.size());
  irs::encode::bitpack::read_block_optimized(in, source.size(), &enc_dec_buf[0], read.data());

  ASSERT_EQ(source, read);
}

void read_write_block_optimized(const std::vector<uint32_t>& source) {
  // intermediate buffer for encoding/decoding
  std::vector<uint32_t> enc_dec_buf(source.size());
  read_write_block_optimized(source, enc_dec_buf);
}

}

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
  tests::detail::read_write_block_optimized(data);
  // all equals
  tests::detail::read_write_block_optimized(std::vector<uint32_t>(block_size, 5));
  // all except first are equal, dirty buffer
  {
    std::vector<uint32_t> end_dec_buf(block_size, std::numeric_limits<uint32_t>::max());
    std::vector<uint32_t> src(block_size, 1); src[0] = 0;
    tests::detail::read_write_block_optimized(src, end_dec_buf);
  }
}

#endif // IRESEARCH_SSE2
