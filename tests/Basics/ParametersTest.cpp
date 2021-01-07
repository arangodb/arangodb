////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2007-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "ProgramOptions/Parameters.h"

using namespace arangodb;

TEST(ParametersTest, fromStringEmpty) {
  char const* empty[] = { 
    "", " ", "  ", 
    "#", " #", " # ", 
    "#abc", "#1234", " #1234", "# 1234", "#1234 ", " # 124", " # 124 ",
  };
  
  for (auto v : empty) {
    try {
      arangodb::options::fromString<uint8_t>(v);
      ASSERT_FALSE(true);
    } catch (std::out_of_range const&) {
    }
    
    try {
      arangodb::options::fromString<int64_t>(v);
      ASSERT_FALSE(true);
    } catch (std::out_of_range const&) {
    }
  }
}

TEST(ParametersTest, fromStringInvalid) {
  char const* invalid[] = { 
    "fuxx", "Foxx9", "   999fux", "foxx 99", "abcd fox 99", "99 foxx abc", 
    "abc 99 #foxx", "abc 99 # foxx", 
    "-", " -", "- ", " - ", 
    "-#", "- #", " - #",
    "kb", " kb", "  kb", "kb ", "kb  ", " kb ", " kb #", "#kb",
    "1234 123 kb", "123 1kb", "1 1 m", "1 1m",
  };
  
  for (auto v : invalid) {
    try {
      arangodb::options::fromString<uint8_t>(v);
      ASSERT_FALSE(true);
    } catch (std::out_of_range const&) {
    }
    
    try {
      arangodb::options::fromString<int64_t>(v);
      ASSERT_FALSE(true);
    } catch (std::out_of_range const&) {
    }
  }
}

TEST(ParametersTest, fromStringComments) {
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0#"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0#0"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0#1"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0#2"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0#20"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0 #20"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0 # 20"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0#21952"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0 #21952"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0 #21952 "));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0 # 21952"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0 # 21952 "));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0                   # 21952"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("  0                   # 21952"));
  
  ASSERT_EQ(int64_t(44252), arangodb::options::fromString<int64_t>("44252#"));
  ASSERT_EQ(int64_t(44252), arangodb::options::fromString<int64_t>("44252#0"));
  ASSERT_EQ(int64_t(44252), arangodb::options::fromString<int64_t>("44252#1"));
  ASSERT_EQ(int64_t(44252), arangodb::options::fromString<int64_t>("44252#20"));
  ASSERT_EQ(int64_t(44252), arangodb::options::fromString<int64_t>("44252 #20"));
  ASSERT_EQ(int64_t(44252), arangodb::options::fromString<int64_t>("44252 # 21952"));
  ASSERT_EQ(int64_t(44252), arangodb::options::fromString<int64_t>("44252 # 21952 "));
  ASSERT_EQ(int64_t(44252), arangodb::options::fromString<int64_t>("44252                   # 21952"));
  ASSERT_EQ(int64_t(44252), arangodb::options::fromString<int64_t>("  44252                   # 21952"));
  ASSERT_EQ(int64_t(44252), arangodb::options::fromString<int64_t>("  44252                   # 21952 "));
  
  ASSERT_EQ(int64_t(-44252), arangodb::options::fromString<int64_t>("-44252#"));
  ASSERT_EQ(int64_t(-44252), arangodb::options::fromString<int64_t>("-44252#0"));
  ASSERT_EQ(int64_t(-44252), arangodb::options::fromString<int64_t>("-44252#1"));
  ASSERT_EQ(int64_t(-44252), arangodb::options::fromString<int64_t>("-44252#20"));
  ASSERT_EQ(int64_t(-44252), arangodb::options::fromString<int64_t>("-44252 #20"));
  ASSERT_EQ(int64_t(-44252), arangodb::options::fromString<int64_t>("-44252 # 21952"));
  ASSERT_EQ(int64_t(-44252), arangodb::options::fromString<int64_t>("-44252 # 21952 "));
  ASSERT_EQ(int64_t(-44252), arangodb::options::fromString<int64_t>("-44252                   # 21952"));
  ASSERT_EQ(int64_t(-44252), arangodb::options::fromString<int64_t>("  -44252                   # 21952"));
  ASSERT_EQ(int64_t(-44252), arangodb::options::fromString<int64_t>("  -44252                   # 21952 "));
}

TEST(ParametersTest, fromStringUnits) {
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0k"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0kb"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0KB"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0kib"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0KiB"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0KIB"));
  
  ASSERT_EQ(int64_t(10000), arangodb::options::fromString<int64_t>("10k"));
  ASSERT_EQ(int64_t(10000), arangodb::options::fromString<int64_t>("10kb"));
  ASSERT_EQ(int64_t(10000), arangodb::options::fromString<int64_t>("10KB"));
  ASSERT_EQ(int64_t(10240), arangodb::options::fromString<int64_t>("10kib"));
  ASSERT_EQ(int64_t(10240), arangodb::options::fromString<int64_t>("10KiB"));
  ASSERT_EQ(int64_t(10240), arangodb::options::fromString<int64_t>("10KIB"));

  ASSERT_EQ(int64_t(12345678901000), arangodb::options::fromString<int64_t>("12345678901k"));
  ASSERT_EQ(int64_t(12345678901000), arangodb::options::fromString<int64_t>("12345678901kb"));
  ASSERT_EQ(int64_t(12345678901000), arangodb::options::fromString<int64_t>("12345678901KB"));
  ASSERT_EQ(int64_t(12641975194624), arangodb::options::fromString<int64_t>("12345678901KiB"));
  ASSERT_EQ(int64_t(12641975194624), arangodb::options::fromString<int64_t>("12345678901kib"));
  ASSERT_EQ(int64_t(12641975194624), arangodb::options::fromString<int64_t>("12345678901KIB"));
  ASSERT_EQ(int64_t(12641975194624), arangodb::options::fromString<int64_t>("  12345678901KIB"));
  ASSERT_EQ(int64_t(12641975194624), arangodb::options::fromString<int64_t>("  12345678901KIB  "));
  ASSERT_EQ(int64_t(12641975194624), arangodb::options::fromString<int64_t>("12345678901KIB "));
  
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0m"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0mb"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0MB"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0mib"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0MiB"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0MIB"));
  
  ASSERT_EQ(int64_t(10000000), arangodb::options::fromString<int64_t>("10m"));
  ASSERT_EQ(int64_t(10000000), arangodb::options::fromString<int64_t>("10mb"));
  ASSERT_EQ(int64_t(10000000), arangodb::options::fromString<int64_t>("10MB"));
  ASSERT_EQ(int64_t(10485760), arangodb::options::fromString<int64_t>("10mib"));
  ASSERT_EQ(int64_t(10485760), arangodb::options::fromString<int64_t>("10MiB"));
  ASSERT_EQ(int64_t(10485760), arangodb::options::fromString<int64_t>("10MIB"));
  
  ASSERT_EQ(int64_t(4096000000), arangodb::options::fromString<int64_t>("4096m"));
  ASSERT_EQ(int64_t(4096000000), arangodb::options::fromString<int64_t>("4096mb"));
  ASSERT_EQ(int64_t(4096000000), arangodb::options::fromString<int64_t>("4096MB"));
  ASSERT_EQ(int64_t(4294967296), arangodb::options::fromString<int64_t>("4096mib"));
  ASSERT_EQ(int64_t(4294967296), arangodb::options::fromString<int64_t>("4096MiB"));
  ASSERT_EQ(int64_t(4294967296), arangodb::options::fromString<int64_t>("4096MIB"));
  
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0g"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0gb"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0GB"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0gib"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0GiB"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0GIB"));
  
  ASSERT_EQ(int64_t(2000000000), arangodb::options::fromString<int64_t>("2g"));
  ASSERT_EQ(int64_t(2000000000), arangodb::options::fromString<int64_t>("2gb"));
  ASSERT_EQ(int64_t(2000000000), arangodb::options::fromString<int64_t>("2GB"));
  ASSERT_EQ(int64_t(2147483648), arangodb::options::fromString<int64_t>("2gib"));
  ASSERT_EQ(int64_t(2147483648), arangodb::options::fromString<int64_t>("2GiB"));
  ASSERT_EQ(int64_t(2147483648), arangodb::options::fromString<int64_t>("2GIB"));
  
  ASSERT_EQ(int64_t(10000000000), arangodb::options::fromString<int64_t>("10g"));
  ASSERT_EQ(int64_t(10000000000), arangodb::options::fromString<int64_t>("10gb"));
  ASSERT_EQ(int64_t(10000000000), arangodb::options::fromString<int64_t>("10GB"));
  ASSERT_EQ(int64_t(10737418240), arangodb::options::fromString<int64_t>("10gib"));
  ASSERT_EQ(int64_t(10737418240), arangodb::options::fromString<int64_t>("10GiB"));
  ASSERT_EQ(int64_t(10737418240), arangodb::options::fromString<int64_t>("10GIB"));
  
  ASSERT_EQ(int64_t(512000000000), arangodb::options::fromString<int64_t>("512g"));
  ASSERT_EQ(int64_t(512000000000), arangodb::options::fromString<int64_t>("512gb"));
  ASSERT_EQ(int64_t(512000000000), arangodb::options::fromString<int64_t>("512GB"));
  ASSERT_EQ(int64_t(549755813888), arangodb::options::fromString<int64_t>("512gib"));
  ASSERT_EQ(int64_t(549755813888), arangodb::options::fromString<int64_t>("512GiB"));
  ASSERT_EQ(int64_t(549755813888), arangodb::options::fromString<int64_t>("512GIB"));
}

TEST(ParametersTest, fromStringInvalidUnits) {
  char const* invalid[] = { 
    "123fuxx", "123FUXX", "123f", "123f",
    "123 fuxx", "123 FUXX", "123 f", "123 f",
    "-14 spank",
    "25 kbkb",
    "1245mbmb",
  };
  
  for (auto v : invalid) {
    try {
      arangodb::options::fromString<uint8_t>(v);
      ASSERT_FALSE(true);
    } catch (std::out_of_range const&) {
    }
    
    try {
      arangodb::options::fromString<int64_t>(v);
      ASSERT_FALSE(true);
    } catch (std::out_of_range const&) {
    }
  }
}

TEST(ParametersTest, fromStringUInt8) {
  ASSERT_EQ(uint8_t(0), arangodb::options::fromString<uint8_t>(" 0"));
  ASSERT_EQ(uint8_t(0), arangodb::options::fromString<uint8_t>("0 "));
  ASSERT_EQ(uint8_t(0), arangodb::options::fromString<uint8_t>(" 0 "));
  ASSERT_EQ(uint8_t(1), arangodb::options::fromString<uint8_t>(" 1"));
  ASSERT_EQ(uint8_t(1), arangodb::options::fromString<uint8_t>("1 "));
  ASSERT_EQ(uint8_t(1), arangodb::options::fromString<uint8_t>(" 1 "));
  
  ASSERT_EQ(uint8_t(0), arangodb::options::fromString<uint8_t>("0"));
  ASSERT_EQ(uint8_t(1), arangodb::options::fromString<uint8_t>("1"));
  ASSERT_EQ(uint8_t(2), arangodb::options::fromString<uint8_t>("2"));
  ASSERT_EQ(uint8_t(32), arangodb::options::fromString<uint8_t>("32"));
  ASSERT_EQ(uint8_t(99), arangodb::options::fromString<uint8_t>("99"));
  ASSERT_EQ(uint8_t(255), arangodb::options::fromString<uint8_t>("255"));
  
  char const* tooHigh[] = { "256", "1024", "109878", "999999999999999" };
  for (auto v : tooHigh) {
    try {
      arangodb::options::fromString<uint8_t>(v);
      ASSERT_FALSE(true);
    } catch (std::out_of_range const&) {
    }
  }
  
  char const* negative[] = { "-1", "-10", "   -10", "  -10  ", "-99888684" };
  for (auto v : negative) {
    try {
      arangodb::options::fromString<uint8_t>(v);
      ASSERT_FALSE(true);
    } catch (std::out_of_range const&) {
    }
  }
}

TEST(ParametersTest, fromStringInt64) {
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>(" 0"));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0 "));
  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>(" 0 "));
  ASSERT_EQ(int64_t(1), arangodb::options::fromString<int64_t>(" 1"));
  ASSERT_EQ(int64_t(1), arangodb::options::fromString<int64_t>("1 "));
  ASSERT_EQ(int64_t(1), arangodb::options::fromString<int64_t>(" 1 "));
  ASSERT_EQ(int64_t(299868), arangodb::options::fromString<int64_t>(" 299868 "));
  ASSERT_EQ(int64_t(984373), arangodb::options::fromString<int64_t>("                                  984373"));
  ASSERT_EQ(int64_t(2987726312), arangodb::options::fromString<int64_t>("2987726312                "));

  ASSERT_EQ(int64_t(0), arangodb::options::fromString<int64_t>("0"));
  ASSERT_EQ(int64_t(1), arangodb::options::fromString<int64_t>("1"));
  ASSERT_EQ(int64_t(2), arangodb::options::fromString<int64_t>("2"));
  ASSERT_EQ(int64_t(32), arangodb::options::fromString<int64_t>("32"));
  ASSERT_EQ(int64_t(99), arangodb::options::fromString<int64_t>("99"));
  ASSERT_EQ(int64_t(109878), arangodb::options::fromString<int64_t>("109878"));
  ASSERT_EQ(int64_t(1234567890123), arangodb::options::fromString<int64_t>("1234567890123"));
  ASSERT_EQ(int64_t(9223372036854775807), arangodb::options::fromString<int64_t>("9223372036854775807"));
  ASSERT_EQ(int64_t(9223372036854775807), arangodb::options::fromString<int64_t>("  9223372036854775807  "));
  ASSERT_EQ(INT64_MIN, arangodb::options::fromString<int64_t>("-9223372036854775808"));
  ASSERT_EQ(INT64_MIN, arangodb::options::fromString<int64_t>("  -9223372036854775808  "));
  
  ASSERT_EQ(int64_t(-1), arangodb::options::fromString<int64_t>("-1"));
  ASSERT_EQ(int64_t(-1234567), arangodb::options::fromString<int64_t>("-1234567"));
  
  char const* tooHigh[] = { "-9223372036854775809", "9223372036854775808", "9999999999999999999999999999999999999999999999999999" };
  for (auto v : tooHigh) {
    try {
      arangodb::options::fromString<int64_t>(v);
      ASSERT_FALSE(true);
    } catch (std::out_of_range const&) {
    }
  }
}
  
TEST(ParametersTest, fromStringUInt64) {
  ASSERT_EQ(uint64_t(0), arangodb::options::fromString<uint64_t>(" 0"));
  ASSERT_EQ(uint64_t(0), arangodb::options::fromString<uint64_t>("0 "));
  ASSERT_EQ(uint64_t(0), arangodb::options::fromString<uint64_t>(" 0 "));
  ASSERT_EQ(uint64_t(1), arangodb::options::fromString<uint64_t>(" 1"));
  ASSERT_EQ(uint64_t(1), arangodb::options::fromString<uint64_t>("1 "));
  ASSERT_EQ(uint64_t(1), arangodb::options::fromString<uint64_t>(" 1 "));
  ASSERT_EQ(uint64_t(299868), arangodb::options::fromString<uint64_t>(" 299868 "));
  ASSERT_EQ(uint64_t(984373), arangodb::options::fromString<uint64_t>("                                  984373"));
  ASSERT_EQ(uint64_t(2987726312), arangodb::options::fromString<uint64_t>("2987726312                "));

  ASSERT_EQ(uint64_t(0), arangodb::options::fromString<uint64_t>("0"));
  ASSERT_EQ(uint64_t(1), arangodb::options::fromString<uint64_t>("1"));
  ASSERT_EQ(uint64_t(2), arangodb::options::fromString<uint64_t>("2"));
  ASSERT_EQ(uint64_t(32), arangodb::options::fromString<uint64_t>("32"));
  ASSERT_EQ(uint64_t(99), arangodb::options::fromString<uint64_t>("99"));
  ASSERT_EQ(uint64_t(109878), arangodb::options::fromString<uint64_t>("109878"));
  ASSERT_EQ(uint64_t(1234567890123), arangodb::options::fromString<uint64_t>("1234567890123"));
  ASSERT_EQ(uint64_t(18446744073709551615U), arangodb::options::fromString<uint64_t>("18446744073709551615"));
  ASSERT_EQ(uint64_t(18446744073709551615U), arangodb::options::fromString<uint64_t>("   18446744073709551615  "));
  
  char const* tooHigh[] = { "18446744073709551616", "9999999999999999999999999999999999999999999999999999" };
  for (auto v : tooHigh) {
    try {
      arangodb::options::fromString<uint64_t>(v);
      ASSERT_FALSE(true);
    } catch (std::out_of_range const&) {
    }
  }
  
  char const* negative[] = { "-1", "-10", "   -10", "  -10  ", "-99888684" };
  for (auto v : negative) {
    try {
      arangodb::options::fromString<uint64_t>(v);
      ASSERT_FALSE(true);
    } catch (std::out_of_range const&) {
    }
  }
}
