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
#include "store/memory_directory.hpp"
#include "store/store_utils.hpp"
#include "utils/string.hpp"

using namespace iresearch;

TEST(memory_index_output_tests, reset) {
  memory_file file{ irs::memory_allocator::global() };
  memory_index_output out(file);

  std::vector<std::string> data0{
    "JhPHnhXNpYvbEjq", "g7gyby2SdUdNvGQ", "U98XcyIXfllcXZH", "4ibtjQKrCYNXiVj", "NkxevgneHQsS43D",
    "CkSTrCYOLqYNvvY", "6U9Ye5jTBItN8l9", "69cwjIUu5C8VfbL", "isp25Lua9zoZJMX", "naLGb64dJPKflqI",
    "fcgWxthncllqjCc", "HfYNfB3H4hhRqOJ", "MlWiQmdTanRjmFP", "A8rQhYANfoLLX7s", "NcqEzShxKehR4Xk",
    "qufYsQKbsSddt6Z", "ylkrc2Ks5XBQNw3", "aMnUYWt2nFMBrym", "pLrBC2CIGe747CI", "A71DZO5FekXjd94",
    "TDlPw3x3yLkP9Zh", "o71RcnwQCWusoBF", "zWBwGnqIXimoZVG", "2VV0N388HixOMry", "VCOYY4ezKnhfsd8",
    "LIRWmNgCwmBeUHU", "roerCNsxOIOzpyw", "rNV3uP1RTJArKq2", "RNpHK0SdzffO9UU", "W2ydkfmSyzsLBQp",
    "T87A8xZ4y2XCsQh", "8K0UlwDwO8Nn2Zj", "I1n5sUTCLeGMBdm", "ZMkW85eqS7UZVt0", "6MEyOHo5ow6VXZA",
    "ZJbmLBTxgYF0bx8", "7lXcionqcEIXwhy", "35uOTpyl1zFYsy3", "VNHdXeNPR4mFM07", "vtynqrb2yMUxhx4",
    "m3FYKk12UzHICpA", "6OoUF1Qu4WGTJZm", "ydx0PgXLrCMziL5", "CxfuGVzMpgX7pGd", "HKiG3GInGyYDL4Z",
    "PJqhLXODbAcwVkV", "I8kXANrB85cob6c", "StNikW1Eeahpi0L", "8MG2lAxZwOrRuPu", "kKLlAuK4yU3ZFGl" 
  };

  std::for_each(
    data0.begin(),
    data0.end(),
    [&out] (const std::string& s) {
      write_string(out, s.c_str(), s.size());
  });

  out.flush();
  out.reset();
  
  file.reset();

  std::vector<std::string> data1{
    "OqywZv86RA4t0tz", "jxk02FZHJDLcYtf", "y7Q9yvg7mcI2Lfs", "JlwseJYxl6uNdW6",
    "7hNRIjJAK4UxArR", "3FpWo4b3oBQEI6Z", "xhzqKAka4qHUq5N", "TardpkWsSp4JbS7",
    "zSXLzUW3Z4PoSVj", "4jDq9HTug3CWdPM", "c6ak6QFYRkwM98r", "3ICqMuUveAXliAy",
    "vPYzG6043GHcaSw", "h9CekmMPGupfEoT", "YgoZpWx6LIHiX6X", "lnyKxlZ3JKrVZcg",
    "nkvAJ4Gi0cpIXFD", "xnhtaUFAnKlFvHs", "1UKIapWMyivlSCE", "w58G0sVrvUApvMc",
    "vMS8cEFeQQNtHKH", "1VznSLlCDPv6PoN", "X4uhkTyy1TaUmZ2", "PC7o8gh9E2jYBZz",
    "yKaotpyPaGC2Iwt", "ofrKEFNO5jgUlQ2", "QDB3Xv6eFBwlkvL", "EpIaQf3MDhwn4k6",
    "9JXzjgl8U2ehumk", "ygz7aMgOU5wqdVw", "scksXrYojKYTUgM", "2uQdV38MEJFgDUd",
    "jlaq7lhTrsWiYUk", "QtEhXSMQaUZogEP", "hb2i0REojUQOO9l", "TfKJTxGeGLO7DGI",
    "4xom8m3oNonaPgj", "708zrcsjsOMd1cA", "Fm7NYzezfw25zGv", "lmwUMdwdHa766QN",
    "TYJISGvr8Wcadkn", "STFEhNDe78VEyZp", "3BHag0mVfbmr2wQ", "HDRau9wa3paVsNz",
    "loVKjLS7qwDpUFc", "j0TluqvROLMMheb", "9zm8hOUGnhwT7dB", "DWJHR2a22l5M6PB",
    "CIvs4EyDjz59q1V", "EIKi8LuYghXWHD3"
  };

  std::for_each(
    data1.begin(),
    data1.end(),
    [&out] (const std::string& s) {
    write_string(out, s.c_str(), s.size());
  });

  out.flush();

  memory_index_input in(file);
  std::for_each(
    data1.begin(),
    data1.end(),
    [&in] (const std::string& s) {
    const std::string cs = read_string<std::string>(in);
    EXPECT_EQ(s, cs);
  });
}
