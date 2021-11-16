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
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Basics/EncodingUtils.h"
#include "Basics/StringBuffer.h"
#include "Basics/fasthash.h"

#include <velocypack/Buffer.h>

#include "gtest/gtest.h"

using namespace arangodb;
  
namespace {
constexpr char shortString[] = "this is a text that is going to be compressed in various ways";
constexpr char mediumString[] = "ジャパン は、イギリスのニュー・ウェーヴバンド。デヴィッド・ ... を構築していった。 日本では初来日でいきなり武道館での公演を行うなど、爆発的な人気を誇ったが、英国ではなかなか人気が出ず、初期は典型的な「ビッグ・イン・ジャパン」状態であった。日本最大級のポータルサイト。検索、オークション、ニュース、メール、コミュニティ、ショッピング、など80以上のサービスを展開。あなたの生活をより豊かにする「ライフ・エンジン」を目指していきます。デヴィッド・シルヴィアンとその弟スティーヴ・ジャンセン、デヴィッドの親友であったミック・カーンを中心に結成。ミック・カーンの兄の結婚式にバンドとして最初のお披露目をした。当初はミック・カーンをリードボーカルとして練習していたが、本番直前になって怖じ気づいたミックがデヴィッド・シルヴィアンに無理矢理頼み込んでボーカルを代わってもらい、以降デヴィッドがリードボーカルとなった。その後高校の同級であったリチャード・バルビエリを誘い、更にオーディションでロブ・ディーンを迎え入れ、デビュー当初のバンドの形態となった。デビュー当初はアイドルとして宣伝されたグループだったが、英国の音楽シーンではほとんど人気が無かった。初期のサウンドは主に黒人音楽やグラムロックをポスト・パンク的に再解釈したものであったが、作品を重ねるごとに耽美的な作風、退廃的な歌詞やシンセサイザーの利用など独自のスタイルを構築していった。日本では初来日でいきなり武道館での公演を行うなど、爆発的な人気を誇ったが、英国ではなかなか人気が出ず、初期は典型的な「ビッグ・イン・ジャパン」状態であった。";
}

TEST(EncodingUtilsTest, testStringBufferGzipInflateDeflate) {
  basics::StringBuffer buffer(1024, true);

  // test with an empty input
  {
    buffer.deflate();
    EXPECT_EQ(0, buffer.size());
    EXPECT_EQ(14311807558968942501ULL, fasthash64(buffer.data(), buffer.size(), 0xdeadbeef));
  }

  // test with a short string first
  {
    buffer.append(::shortString);

    EXPECT_EQ(61, buffer.size());
    EXPECT_EQ(6019303676778172486ULL, fasthash64(buffer.data(), buffer.size(), 0xdeadbeef));
 
    // deflate the string
    buffer.deflate();
  
    EXPECT_EQ(61, buffer.size());
    EXPECT_EQ(9241756237550896492ULL, fasthash64(buffer.data(), buffer.size(), 0xdeadbeef));

    // now inflate it. we should be back at the original size & content
    basics::StringBuffer inflated;
    buffer.inflate(inflated);

    EXPECT_EQ(61, inflated.size());
    EXPECT_EQ(6019303676778172486ULL, fasthash64(inflated.data(), inflated.size(), 0xdeadbeef));
  }

  // now try a longer string
  buffer.clear();
  {
    buffer.append(::mediumString);

    EXPECT_EQ(2073, buffer.size());
    EXPECT_EQ(9970172085949113508ULL,fasthash64(buffer.data(), buffer.size(), 0xdeadbeef));
 
    // deflate the string
    buffer.deflate();
  
    EXPECT_EQ(902, buffer.size());
    EXPECT_EQ(1472963238402282948ULL, fasthash64(buffer.data(), buffer.size(), 0xdeadbeef));

    // now inflate it. we should be back at the original size & content
    basics::StringBuffer inflated;
    buffer.inflate(inflated);

    EXPECT_EQ(2073, inflated.size());
    EXPECT_EQ(9970172085949113508ULL,fasthash64(inflated.data(), inflated.size(), 0xdeadbeef));
  }

  // now with a 1 MB string
  buffer.clear();
  {
    for (size_t i = 0; i < 1024 * 1024; ++i) {
      buffer.appendChar(char(i));
    }
    EXPECT_EQ(1024 * 1024, buffer.size());
    EXPECT_EQ(1187188410444752048ULL, fasthash64(buffer.data(), buffer.size(), 0xdeadbeef));
 
    // deflate the string
    buffer.deflate();
  
    EXPECT_EQ(4396, buffer.size());
    EXPECT_EQ(8778813652976263194ULL, fasthash64(buffer.data(), buffer.size(), 0xdeadbeef));

    // now inflate it. we should be back at the original size & content
    basics::StringBuffer inflated;
    buffer.inflate(inflated);

    EXPECT_EQ(1024 * 1024, inflated.size());
    EXPECT_EQ(1187188410444752048ULL, fasthash64(inflated.data(), inflated.size(), 0xdeadbeef));
  }

  // test deflating with empty input
  buffer.clear();
  {
    buffer.deflate();
    EXPECT_EQ(0, buffer.size());
    EXPECT_EQ(14311807558968942501ULL, fasthash64(buffer.data(), buffer.size(), 0xdeadbeef));
  }
  
  // test inflating with broken input
  buffer.clear();
  {
    buffer.append("this-is-broken-deflated-content");

    basics::StringBuffer inflated;
    buffer.inflate(inflated);
    EXPECT_EQ(0, inflated.size());
    EXPECT_EQ(14311807558968942501ULL, fasthash64(inflated.data(), inflated.size(), 0xdeadbeef));
  }
}

TEST(EncodingUtilsTest, testVPackBufferGzipInflateDeflate) {
  velocypack::Buffer<uint8_t> buffer;
  
  // test with an empty input
  {
    velocypack::Buffer<uint8_t> deflated;
    EXPECT_EQ(TRI_ERROR_NO_ERROR, encoding::gzipDeflate(buffer.data(), buffer.size(), deflated));
    EXPECT_EQ(0, deflated.size());
    EXPECT_EQ(14311807558968942501ULL, fasthash64(deflated.data(), deflated.size(), 0xdeadbeef));
  }

  // test with a short string first
  {
    buffer.append(::shortString, strlen(::shortString));

    EXPECT_EQ(61, buffer.size());
    EXPECT_EQ(6019303676778172486ULL, fasthash64(buffer.data(), buffer.size(), 0xdeadbeef));
 
    // deflate the string
    velocypack::Buffer<uint8_t> deflated;
    EXPECT_EQ(TRI_ERROR_NO_ERROR, encoding::gzipDeflate(buffer.data(), buffer.size(), deflated));
  
    EXPECT_EQ(61, deflated.size());
    EXPECT_EQ(9241756237550896492ULL, fasthash64(deflated.data(), deflated.size(), 0xdeadbeef));

    // now inflate it. we should be back at the original size & content
    velocypack::Buffer<uint8_t> inflated;
    EXPECT_EQ(TRI_ERROR_NO_ERROR, encoding::gzipInflate(deflated.data(), deflated.size(), inflated));

    EXPECT_EQ(61, inflated.size());
    EXPECT_EQ(6019303676778172486ULL, fasthash64(inflated.data(), inflated.size(), 0xdeadbeef));
  }

  // now try a longer string
  buffer.clear();
  {
    buffer.append(::mediumString, strlen(::mediumString));

    EXPECT_EQ(2073, buffer.size());
    EXPECT_EQ(9970172085949113508ULL,fasthash64(buffer.data(), buffer.size(), 0xdeadbeef));
 
    // deflate the string
    velocypack::Buffer<uint8_t> deflated;
    EXPECT_EQ(TRI_ERROR_NO_ERROR, encoding::gzipDeflate(buffer.data(), buffer.size(), deflated));
  
    EXPECT_EQ(902, deflated.size());
    EXPECT_EQ(1472963238402282948ULL, fasthash64(deflated.data(), deflated.size(), 0xdeadbeef));

    // now inflate it. we should be back at the original size & content
    velocypack::Buffer<uint8_t> inflated;
    EXPECT_EQ(TRI_ERROR_NO_ERROR, encoding::gzipInflate(deflated.data(), deflated.size(), inflated));

    EXPECT_EQ(2073, inflated.size());
    EXPECT_EQ(9970172085949113508ULL,fasthash64(inflated.data(), inflated.size(), 0xdeadbeef));
  }

  // now with a 1 MB string
  buffer.clear();
  {
    for (size_t i = 0; i < 1024 * 1024; ++i) {
      buffer.push_back(char(i));
    }
    EXPECT_EQ(1024 * 1024, buffer.size());
    EXPECT_EQ(1187188410444752048ULL, fasthash64(buffer.data(), buffer.size(), 0xdeadbeef));
 
    // deflate the string
    velocypack::Buffer<uint8_t> deflated;
    EXPECT_EQ(TRI_ERROR_NO_ERROR, encoding::gzipDeflate(buffer.data(), buffer.size(), deflated));
  
    EXPECT_EQ(4396, deflated.size());
    EXPECT_EQ(8778813652976263194ULL, fasthash64(deflated.data(), deflated.size(), 0xdeadbeef));

    // now inflate it. we should be back at the original size & content
    velocypack::Buffer<uint8_t> inflated;
    EXPECT_EQ(TRI_ERROR_NO_ERROR, encoding::gzipInflate(deflated.data(), deflated.size(), inflated));

    EXPECT_EQ(1024 * 1024, inflated.size());
    EXPECT_EQ(1187188410444752048ULL, fasthash64(inflated.data(), inflated.size(), 0xdeadbeef));
  }

  // test deflating with empty input
  buffer.clear();
  {
    velocypack::Buffer<uint8_t> deflated;
    EXPECT_EQ(TRI_ERROR_NO_ERROR, encoding::gzipDeflate(buffer.data(), buffer.size(), deflated));
    EXPECT_EQ(0, deflated.size());
    EXPECT_EQ(14311807558968942501ULL, fasthash64(deflated.data(), deflated.size(), 0xdeadbeef));
  }
  
  // test inflating with broken input
  buffer.clear();
  {
    buffer.append("this-is-broken-deflated-content");

    velocypack::Buffer<uint8_t> inflated;
    EXPECT_EQ(TRI_ERROR_INTERNAL, encoding::gzipInflate(buffer.data(), buffer.size(), inflated));
    EXPECT_EQ(0, inflated.size());
    EXPECT_EQ(14311807558968942501ULL, fasthash64(inflated.data(), inflated.size(), 0xdeadbeef));
  }
}
