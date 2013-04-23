////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for UTF8 string encoding/decoding
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <boost/test/unit_test.hpp>

#include "BasicsC/tri-strings.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

#define TEST_STRING(str) \
  TRI_EscapeUtf8String(str, strlen(str), true, &outLength);

#define TEST_STRING_L(str, len) \
  TRI_EscapeUtf8String(str, len, true, &outLength);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CStringUtf8Setup {
  CStringUtf8Setup () {
    BOOST_TEST_MESSAGE("setup string UTF8 test");
  }

  ~CStringUtf8Setup () {
    BOOST_TEST_MESSAGE("tear-down string UTF8 test");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CStringUtf8Test, CStringUtf8Setup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test quotes and special characters
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_escape_special_chars) {
  size_t outLength;
  char* result;
  
  result = TEST_STRING("abc");
  BOOST_CHECK_EQUAL("abc", result);
  BOOST_CHECK_EQUAL((size_t) 3, outLength);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // empty string
  result = TEST_STRING("");
  BOOST_CHECK_EQUAL("", result);
  BOOST_CHECK_EQUAL((size_t) 0, outLength);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // forward slash 
  result = TEST_STRING("/");
  BOOST_CHECK_EQUAL("\\/", result);
  BOOST_CHECK_EQUAL((size_t) 2, outLength);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // single quote quote
  result = TEST_STRING("'");
  BOOST_CHECK_EQUAL("'", result);
  BOOST_CHECK_EQUAL((size_t) 1, outLength);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // backslash
  result = TEST_STRING("\\");
  BOOST_CHECK_EQUAL("\\\\", result);
  BOOST_CHECK_EQUAL((size_t) 2, outLength);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // two backslashes
  result = TEST_STRING("\\\\");
  BOOST_CHECK_EQUAL("\\\\\\\\", result);
  BOOST_CHECK_EQUAL((size_t) 4, outLength);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // double quote
  result = TEST_STRING("\"");
  BOOST_CHECK_EQUAL("\\\"", result);
  BOOST_CHECK_EQUAL((size_t) 2, outLength);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // two double quotes
  result = TEST_STRING("\"\"");
  BOOST_CHECK_EQUAL("\\\"\\\"", result);
  BOOST_CHECK_EQUAL((size_t) 4, outLength);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // three double quotes
  result = TEST_STRING("\"\"\"");
  BOOST_CHECK_EQUAL("\\\"\\\"\\\"", result);
  BOOST_CHECK_EQUAL((size_t) 6, outLength);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // quote, preceeded by backslash
  result = TEST_STRING("\\\"");
  BOOST_CHECK_EQUAL("\\\\\\\"", result);
  BOOST_CHECK_EQUAL((size_t) 4, outLength);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // newline
  result = TEST_STRING("\n");
  BOOST_CHECK_EQUAL("\\n", result);
  BOOST_CHECK_EQUAL((size_t) 2, outLength);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // cr
  result = TEST_STRING("\r");
  BOOST_CHECK_EQUAL("\\r", result);
  BOOST_CHECK_EQUAL((size_t) 2, outLength);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // tab
  result = TEST_STRING("\t");
  BOOST_CHECK_EQUAL("\\t", result);
  BOOST_CHECK_EQUAL((size_t) 2, outLength);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // form feed
  result = TEST_STRING("\f");
  BOOST_CHECK_EQUAL("\\f", result);
  BOOST_CHECK_EQUAL((size_t) 2, outLength);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  
  // backspace
  result = TEST_STRING("\b");
  BOOST_CHECK_EQUAL("\\b", result);
  BOOST_CHECK_EQUAL((size_t) 2, outLength);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}
 
////////////////////////////////////////////////////////////////////////////////
/// @brief test non-printable
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_non_printable) {
  size_t outLength;
  size_t i;
  char* result;

  for (i = 0; i <= 31; i++) {
    char c = (char) i;
    char input[2];
    char expected[32];

    // char(i)

    if (c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == '\b') {
      continue;
    }

    input[0] = (char) i;
    input[1] = '\0';

    memset(expected, 0, sizeof(expected));
    snprintf(expected, sizeof(expected), "\\u%04X", (int) i);
     
    result = TEST_STRING_L(input, 1);
    BOOST_CHECK_EQUAL(expected, result);
    TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test Japanese
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_japanse) {
  size_t outLength;
  char* result;

  result = TEST_STRING("ジャパン は、イギリスのニュー・ウェーヴバンド。デヴィッド・ ... を構築していった。 日本では初来日でいきなり武道館での公演を行うなど、爆発的な人気を誇ったが、英国ではなかなか人気が出ず、初期は典型的な「ビッグ・イン・ジャパン」状態であった。日本最大級のポータルサイト。検索、オークション、ニュース、メール、コミュニティ、ショッピング、など80以上のサービスを展開。あなたの生活をより豊かにする「ライフ・エンジン」を目指していきます。デヴィッド・シルヴィアンとその弟スティーヴ・ジャンセン、デヴィッドの親友であったミック・カーンを中心に結成。ミック・カーンの兄の結婚式にバンドとして最初のお披露目をした。当初はミック・カーンをリードボーカルとして練習していたが、本番直前になって怖じ気づいたミックがデヴィッド・シルヴィアンに無理矢理頼み込んでボーカルを代わってもらい、以降デヴィッドがリードボーカルとなった。その後高校の同級であったリチャード・バルビエリを誘い、更にオーディションでロブ・ディーンを迎え入れ、デビュー当初のバンドの形態となった。デビュー当初はアイドルとして宣伝されたグループだったが、英国の音楽シーンではほとんど人気が無かった。初期のサウンドは主に黒人音楽やグラムロックをポスト・パンク的に再解釈したものであったが、作品を重ねるごとに耽美的な作風、退廃的な歌詞やシンセサイザーの利用など独自のスタイルを構築していった。日本では初来日でいきなり武道館での公演を行うなど、爆発的な人気を誇ったが、英国ではなかなか人気が出ず、初期は典型的な「ビッグ・イン・ジャパン」状態であった。");
  BOOST_CHECK_EQUAL("\\u30B8\\u30E3\\u30D1\\u30F3 \\u306F\\u3001\\u30A4\\u30AE\\u30EA\\u30B9\\u306E\\u30CB\\u30E5\\u30FC\\u30FB\\u30A6\\u30A7\\u30FC\\u30F4\\u30D0\\u30F3\\u30C9\\u3002\\u30C7\\u30F4\\u30A3\\u30C3\\u30C9\\u30FB ... \\u3092\\u69CB\\u7BC9\\u3057\\u3066\\u3044\\u3063\\u305F\\u3002 \\u65E5\\u672C\\u3067\\u306F\\u521D\\u6765\\u65E5\\u3067\\u3044\\u304D\\u306A\\u308A\\u6B66\\u9053\\u9928\\u3067\\u306E\\u516C\\u6F14\\u3092\\u884C\\u3046\\u306A\\u3069\\u3001\\u7206\\u767A\\u7684\\u306A\\u4EBA\\u6C17\\u3092\\u8A87\\u3063\\u305F\\u304C\\u3001\\u82F1\\u56FD\\u3067\\u306F\\u306A\\u304B\\u306A\\u304B\\u4EBA\\u6C17\\u304C\\u51FA\\u305A\\u3001\\u521D\\u671F\\u306F\\u5178\\u578B\\u7684\\u306A\\u300C\\u30D3\\u30C3\\u30B0\\u30FB\\u30A4\\u30F3\\u30FB\\u30B8\\u30E3\\u30D1\\u30F3\\u300D\\u72B6\\u614B\\u3067\\u3042\\u3063\\u305F\\u3002\\u65E5\\u672C\\u6700\\u5927\\u7D1A\\u306E\\u30DD\\u30FC\\u30BF\\u30EB\\u30B5\\u30A4\\u30C8\\u3002\\u691C\\u7D22\\u3001\\u30AA\\u30FC\\u30AF\\u30B7\\u30E7\\u30F3\\u3001\\u30CB\\u30E5\\u30FC\\u30B9\\u3001\\u30E1\\u30FC\\u30EB\\u3001\\u30B3\\u30DF\\u30E5\\u30CB\\u30C6\\u30A3\\u3001\\u30B7\\u30E7\\u30C3\\u30D4\\u30F3\\u30B0\\u3001\\u306A\\u306980\\u4EE5\\u4E0A\\u306E\\u30B5\\u30FC\\u30D3\\u30B9\\u3092\\u5C55\\u958B\\u3002\\u3042\\u306A\\u305F\\u306E\\u751F\\u6D3B\\u3092\\u3088\\u308A\\u8C4A\\u304B\\u306B\\u3059\\u308B\\u300C\\u30E9\\u30A4\\u30D5\\u30FB\\u30A8\\u30F3\\u30B8\\u30F3\\u300D\\u3092\\u76EE\\u6307\\u3057\\u3066\\u3044\\u304D\\u307E\\u3059\\u3002\\u30C7\\u30F4\\u30A3\\u30C3\\u30C9\\u30FB\\u30B7\\u30EB\\u30F4\\u30A3\\u30A2\\u30F3\\u3068\\u305D\\u306E\\u5F1F\\u30B9\\u30C6\\u30A3\\u30FC\\u30F4\\u30FB\\u30B8\\u30E3\\u30F3\\u30BB\\u30F3\\u3001\\u30C7\\u30F4\\u30A3\\u30C3\\u30C9\\u306E\\u89AA\\u53CB\\u3067\\u3042\\u3063\\u305F\\u30DF\\u30C3\\u30AF\\u30FB\\u30AB\\u30FC\\u30F3\\u3092\\u4E2D\\u5FC3\\u306B\\u7D50\\u6210\\u3002\\u30DF\\u30C3\\u30AF\\u30FB\\u30AB\\u30FC\\u30F3\\u306E\\u5144\\u306E\\u7D50\\u5A5A\\u5F0F\\u306B\\u30D0\\u30F3\\u30C9\\u3068\\u3057\\u3066\\u6700\\u521D\\u306E\\u304A\\u62AB\\u9732\\u76EE\\u3092\\u3057\\u305F\\u3002\\u5F53\\u521D\\u306F\\u30DF\\u30C3\\u30AF\\u30FB\\u30AB\\u30FC\\u30F3\\u3092\\u30EA\\u30FC\\u30C9\\u30DC\\u30FC\\u30AB\\u30EB\\u3068\\u3057\\u3066\\u7DF4\\u7FD2\\u3057\\u3066\\u3044\\u305F\\u304C\\u3001\\u672C\\u756A\\u76F4\\u524D\\u306B\\u306A\\u3063\\u3066\\u6016\\u3058\\u6C17\\u3065\\u3044\\u305F\\u30DF\\u30C3\\u30AF\\u304C\\u30C7\\u30F4\\u30A3\\u30C3\\u30C9\\u30FB\\u30B7\\u30EB\\u30F4\\u30A3\\u30A2\\u30F3\\u306B\\u7121\\u7406\\u77E2\\u7406\\u983C\\u307F\\u8FBC\\u3093\\u3067\\u30DC\\u30FC\\u30AB\\u30EB\\u3092\\u4EE3\\u308F\\u3063\\u3066\\u3082\\u3089\\u3044\\u3001\\u4EE5\\u964D\\u30C7\\u30F4\\u30A3\\u30C3\\u30C9\\u304C\\u30EA\\u30FC\\u30C9\\u30DC\\u30FC\\u30AB\\u30EB\\u3068\\u306A\\u3063\\u305F\\u3002\\u305D\\u306E\\u5F8C\\u9AD8\\u6821\\u306E\\u540C\\u7D1A\\u3067\\u3042\\u3063\\u305F\\u30EA\\u30C1\\u30E3\\u30FC\\u30C9\\u30FB\\u30D0\\u30EB\\u30D3\\u30A8\\u30EA\\u3092\\u8A98\\u3044\\u3001\\u66F4\\u306B\\u30AA\\u30FC\\u30C7\\u30A3\\u30B7\\u30E7\\u30F3\\u3067\\u30ED\\u30D6\\u30FB\\u30C7\\u30A3\\u30FC\\u30F3\\u3092\\u8FCE\\u3048\\u5165\\u308C\\u3001\\u30C7\\u30D3\\u30E5\\u30FC\\u5F53\\u521D\\u306E\\u30D0\\u30F3\\u30C9\\u306E\\u5F62\\u614B\\u3068\\u306A\\u3063\\u305F\\u3002\\u30C7\\u30D3\\u30E5\\u30FC\\u5F53\\u521D\\u306F\\u30A2\\u30A4\\u30C9\\u30EB\\u3068\\u3057\\u3066\\u5BA3\\u4F1D\\u3055\\u308C\\u305F\\u30B0\\u30EB\\u30FC\\u30D7\\u3060\\u3063\\u305F\\u304C\\u3001\\u82F1\\u56FD\\u306E\\u97F3\\u697D\\u30B7\\u30FC\\u30F3\\u3067\\u306F\\u307B\\u3068\\u3093\\u3069\\u4EBA\\u6C17\\u304C\\u7121\\u304B\\u3063\\u305F\\u3002\\u521D\\u671F\\u306E\\u30B5\\u30A6\\u30F3\\u30C9\\u306F\\u4E3B\\u306B\\u9ED2\\u4EBA\\u97F3\\u697D\\u3084\\u30B0\\u30E9\\u30E0\\u30ED\\u30C3\\u30AF\\u3092\\u30DD\\u30B9\\u30C8\\u30FB\\u30D1\\u30F3\\u30AF\\u7684\\u306B\\u518D\\u89E3\\u91C8\\u3057\\u305F\\u3082\\u306E\\u3067\\u3042\\u3063\\u305F\\u304C\\u3001\\u4F5C\\u54C1\\u3092\\u91CD\\u306D\\u308B\\u3054\\u3068\\u306B\\u803D\\u7F8E\\u7684\\u306A\\u4F5C\\u98A8\\u3001\\u9000\\u5EC3\\u7684\\u306A\\u6B4C\\u8A5E\\u3084\\u30B7\\u30F3\\u30BB\\u30B5\\u30A4\\u30B6\\u30FC\\u306E\\u5229\\u7528\\u306A\\u3069\\u72EC\\u81EA\\u306E\\u30B9\\u30BF\\u30A4\\u30EB\\u3092\\u69CB\\u7BC9\\u3057\\u3066\\u3044\\u3063\\u305F\\u3002\\u65E5\\u672C\\u3067\\u306F\\u521D\\u6765\\u65E5\\u3067\\u3044\\u304D\\u306A\\u308A\\u6B66\\u9053\\u9928\\u3067\\u306E\\u516C\\u6F14\\u3092\\u884C\\u3046\\u306A\\u3069\\u3001\\u7206\\u767A\\u7684\\u306A\\u4EBA\\u6C17\\u3092\\u8A87\\u3063\\u305F\\u304C\\u3001\\u82F1\\u56FD\\u3067\\u306F\\u306A\\u304B\\u306A\\u304B\\u4EBA\\u6C17\\u304C\\u51FA\\u305A\\u3001\\u521D\\u671F\\u306F\\u5178\\u578B\\u7684\\u306A\\u300C\\u30D3\\u30C3\\u30B0\\u30FB\\u30A4\\u30F3\\u30FB\\u30B8\\u30E3\\u30D1\\u30F3\\u300D\\u72B6\\u614B\\u3067\\u3042\\u3063\\u305F\\u3002", result);
  BOOST_CHECK_EQUAL((size_t) 4137, outLength);

  char* unescaped = TRI_UnescapeUtf8String(result, strlen(result), &outLength);
  BOOST_CHECK_EQUAL(unescaped, "ジャパン は、イギリスのニュー・ウェーヴバンド。デヴィッド・ ... を構築していった。 日本では初来日でいきなり武道館での公演を行うなど、爆発的な人気を誇ったが、英国ではなかなか人気が出ず、初期は典型的な「ビッグ・イン・ジャパン」状態であった。日本最大級のポータルサイト。検索、オークション、ニュース、メール、コミュニティ、ショッピング、など80以上のサービスを展開。あなたの生活をより豊かにする「ライフ・エンジン」を目指していきます。デヴィッド・シルヴィアンとその弟スティーヴ・ジャンセン、デヴィッドの親友であったミック・カーンを中心に結成。ミック・カーンの兄の結婚式にバンドとして最初のお披露目をした。当初はミック・カーンをリードボーカルとして練習していたが、本番直前になって怖じ気づいたミックがデヴィッド・シルヴィアンに無理矢理頼み込んでボーカルを代わってもらい、以降デヴィッドがリードボーカルとなった。その後高校の同級であったリチャード・バルビエリを誘い、更にオーディションでロブ・ディーンを迎え入れ、デビュー当初のバンドの形態となった。デビュー当初はアイドルとして宣伝されたグループだったが、英国の音楽シーンではほとんど人気が無かった。初期のサウンドは主に黒人音楽やグラムロックをポスト・パンク的に再解釈したものであったが、作品を重ねるごとに耽美的な作風、退廃的な歌詞やシンセサイザーの利用など独自のスタイルを構築していった。日本では初来日でいきなり武道館での公演を行うなど、爆発的な人気を誇ったが、英国ではなかなか人気が出ず、初期は典型的な「ビッグ・イン・ジャパン」状態であった。");
  TRI_FreeString(TRI_CORE_MEM_ZONE, unescaped);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test Korean
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_korean) {
  size_t outLength;
  char* result;

  result = TEST_STRING("코리아닷컴 메일알리미 서비스 중단안내 [안내] 개인정보취급방침 변경 안내 회사소개 | 광고안내 | 제휴안내 | 개인정보취급방침 | 청소년보호정책 | 스팸방지정책 | 사이버고객센터 | 약관안내 | 이메일 무단수집거부 | 서비스 전체보기");
  BOOST_CHECK_EQUAL("\\uCF54\\uB9AC\\uC544\\uB2F7\\uCEF4 \\uBA54\\uC77C\\uC54C\\uB9AC\\uBBF8 \\uC11C\\uBE44\\uC2A4 \\uC911\\uB2E8\\uC548\\uB0B4 [\\uC548\\uB0B4] \\uAC1C\\uC778\\uC815\\uBCF4\\uCDE8\\uAE09\\uBC29\\uCE68 \\uBCC0\\uACBD \\uC548\\uB0B4 \\uD68C\\uC0AC\\uC18C\\uAC1C | \\uAD11\\uACE0\\uC548\\uB0B4 | \\uC81C\\uD734\\uC548\\uB0B4 | \\uAC1C\\uC778\\uC815\\uBCF4\\uCDE8\\uAE09\\uBC29\\uCE68 | \\uCCAD\\uC18C\\uB144\\uBCF4\\uD638\\uC815\\uCC45 | \\uC2A4\\uD338\\uBC29\\uC9C0\\uC815\\uCC45 | \\uC0AC\\uC774\\uBC84\\uACE0\\uAC1D\\uC13C\\uD130 | \\uC57D\\uAD00\\uC548\\uB0B4 | \\uC774\\uBA54\\uC77C \\uBB34\\uB2E8\\uC218\\uC9D1\\uAC70\\uBD80 | \\uC11C\\uBE44\\uC2A4 \\uC804\\uCCB4\\uBCF4\\uAE30", result);
  BOOST_CHECK_EQUAL((size_t) 585, outLength);
  char* unescaped = TRI_UnescapeUtf8String(result, strlen(result), &outLength);
  BOOST_CHECK_EQUAL(unescaped, "코리아닷컴 메일알리미 서비스 중단안내 [안내] 개인정보취급방침 변경 안내 회사소개 | 광고안내 | 제휴안내 | 개인정보취급방침 | 청소년보호정책 | 스팸방지정책 | 사이버고객센터 | 약관안내 | 이메일 무단수집거부 | 서비스 전체보기");
  TRI_FreeString(TRI_CORE_MEM_ZONE, unescaped);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test Chinese
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_chinese) {
  size_t outLength;
  char* result;

  result = TEST_STRING("中华网以中国的市场为核心，致力为当地用户提供流动增值服务、网上娱乐及互联网服务。本公司亦推出网上游戏，及透过其门户网站提供包罗万有的网上产品及服务。");
  BOOST_CHECK_EQUAL("\\u4E2D\\u534E\\u7F51\\u4EE5\\u4E2D\\u56FD\\u7684\\u5E02\\u573A\\u4E3A\\u6838\\u5FC3\\uFF0C\\u81F4\\u529B\\u4E3A\\u5F53\\u5730\\u7528\\u6237\\u63D0\\u4F9B\\u6D41\\u52A8\\u589E\\u503C\\u670D\\u52A1\\u3001\\u7F51\\u4E0A\\u5A31\\u4E50\\u53CA\\u4E92\\u8054\\u7F51\\u670D\\u52A1\\u3002\\u672C\\u516C\\u53F8\\u4EA6\\u63A8\\u51FA\\u7F51\\u4E0A\\u6E38\\u620F\\uFF0C\\u53CA\\u900F\\u8FC7\\u5176\\u95E8\\u6237\\u7F51\\u7AD9\\u63D0\\u4F9B\\u5305\\u7F57\\u4E07\\u6709\\u7684\\u7F51\\u4E0A\\u4EA7\\u54C1\\u53CA\\u670D\\u52A1\\u3002", result);
  BOOST_CHECK_EQUAL((size_t) 444, outLength);

  char* unescaped = TRI_UnescapeUtf8String(result, strlen(result), &outLength);
  BOOST_CHECK_EQUAL(unescaped, "中华网以中国的市场为核心，致力为当地用户提供流动增值服务、网上娱乐及互联网服务。本公司亦推出网上游戏，及透过其门户网站提供包罗万有的网上产品及服务。");
  TRI_FreeString(TRI_CORE_MEM_ZONE, unescaped);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test Hebrew
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_hebrew) {
  size_t outLength;
  char* result;

  result = TEST_STRING("כפי שסופיה קופולה היטיבה לבטא בסרטה אבודים בטוקיו, בתי מלון יוקרתיים בערים גדולות אמנם מציעים אינספור פינוקים, אבל הם גם עלולים לגרום לנו להרגיש בודדים ואומללים מאי פעם. לעומת זאת, B&B, בתים פרטיים שבהם אפשר לישון ולאכול ארוחת בוקר, הם דרך נהדרת להכיר עיר אירופאית כמו מקומיים ולפגוש אנשים מרתקים מרחבי העולם. לטובת מי שנוסע לממלכה בחודשים הקרובים, הגרדיאן הבריטי קיבץ את עשרת ה-B&B המומלצים ביותר בלונדון. כל שנותר הוא לבחור, ולהזמין מראש");
  BOOST_CHECK_EQUAL("\\u05DB\\u05E4\\u05D9 \\u05E9\\u05E1\\u05D5\\u05E4\\u05D9\\u05D4 \\u05E7\\u05D5\\u05E4\\u05D5\\u05DC\\u05D4 \\u05D4\\u05D9\\u05D8\\u05D9\\u05D1\\u05D4 \\u05DC\\u05D1\\u05D8\\u05D0 \\u05D1\\u05E1\\u05E8\\u05D8\\u05D4 \\u05D0\\u05D1\\u05D5\\u05D3\\u05D9\\u05DD \\u05D1\\u05D8\\u05D5\\u05E7\\u05D9\\u05D5, \\u05D1\\u05EA\\u05D9 \\u05DE\\u05DC\\u05D5\\u05DF \\u05D9\\u05D5\\u05E7\\u05E8\\u05EA\\u05D9\\u05D9\\u05DD \\u05D1\\u05E2\\u05E8\\u05D9\\u05DD \\u05D2\\u05D3\\u05D5\\u05DC\\u05D5\\u05EA \\u05D0\\u05DE\\u05E0\\u05DD \\u05DE\\u05E6\\u05D9\\u05E2\\u05D9\\u05DD \\u05D0\\u05D9\\u05E0\\u05E1\\u05E4\\u05D5\\u05E8 \\u05E4\\u05D9\\u05E0\\u05D5\\u05E7\\u05D9\\u05DD, \\u05D0\\u05D1\\u05DC \\u05D4\\u05DD \\u05D2\\u05DD \\u05E2\\u05DC\\u05D5\\u05DC\\u05D9\\u05DD \\u05DC\\u05D2\\u05E8\\u05D5\\u05DD \\u05DC\\u05E0\\u05D5 \\u05DC\\u05D4\\u05E8\\u05D2\\u05D9\\u05E9 \\u05D1\\u05D5\\u05D3\\u05D3\\u05D9\\u05DD \\u05D5\\u05D0\\u05D5\\u05DE\\u05DC\\u05DC\\u05D9\\u05DD \\u05DE\\u05D0\\u05D9 \\u05E4\\u05E2\\u05DD. \\u05DC\\u05E2\\u05D5\\u05DE\\u05EA \\u05D6\\u05D0\\u05EA, B&B, \\u05D1\\u05EA\\u05D9\\u05DD \\u05E4\\u05E8\\u05D8\\u05D9\\u05D9\\u05DD \\u05E9\\u05D1\\u05D4\\u05DD \\u05D0\\u05E4\\u05E9\\u05E8 \\u05DC\\u05D9\\u05E9\\u05D5\\u05DF \\u05D5\\u05DC\\u05D0\\u05DB\\u05D5\\u05DC \\u05D0\\u05E8\\u05D5\\u05D7\\u05EA \\u05D1\\u05D5\\u05E7\\u05E8, \\u05D4\\u05DD \\u05D3\\u05E8\\u05DA \\u05E0\\u05D4\\u05D3\\u05E8\\u05EA \\u05DC\\u05D4\\u05DB\\u05D9\\u05E8 \\u05E2\\u05D9\\u05E8 \\u05D0\\u05D9\\u05E8\\u05D5\\u05E4\\u05D0\\u05D9\\u05EA \\u05DB\\u05DE\\u05D5 \\u05DE\\u05E7\\u05D5\\u05DE\\u05D9\\u05D9\\u05DD \\u05D5\\u05DC\\u05E4\\u05D2\\u05D5\\u05E9 \\u05D0\\u05E0\\u05E9\\u05D9\\u05DD \\u05DE\\u05E8\\u05EA\\u05E7\\u05D9\\u05DD \\u05DE\\u05E8\\u05D7\\u05D1\\u05D9 \\u05D4\\u05E2\\u05D5\\u05DC\\u05DD. \\u05DC\\u05D8\\u05D5\\u05D1\\u05EA \\u05DE\\u05D9 \\u05E9\\u05E0\\u05D5\\u05E1\\u05E2 \\u05DC\\u05DE\\u05DE\\u05DC\\u05DB\\u05D4 \\u05D1\\u05D7\\u05D5\\u05D3\\u05E9\\u05D9\\u05DD \\u05D4\\u05E7\\u05E8\\u05D5\\u05D1\\u05D9\\u05DD, \\u05D4\\u05D2\\u05E8\\u05D3\\u05D9\\u05D0\\u05DF \\u05D4\\u05D1\\u05E8\\u05D9\\u05D8\\u05D9 \\u05E7\\u05D9\\u05D1\\u05E5 \\u05D0\\u05EA \\u05E2\\u05E9\\u05E8\\u05EA \\u05D4-B&B \\u05D4\\u05DE\\u05D5\\u05DE\\u05DC\\u05E6\\u05D9\\u05DD \\u05D1\\u05D9\\u05D5\\u05EA\\u05E8 \\u05D1\\u05DC\\u05D5\\u05E0\\u05D3\\u05D5\\u05DF. \\u05DB\\u05DC \\u05E9\\u05E0\\u05D5\\u05EA\\u05E8 \\u05D4\\u05D5\\u05D0 \\u05DC\\u05D1\\u05D7\\u05D5\\u05E8, \\u05D5\\u05DC\\u05D4\\u05D6\\u05DE\\u05D9\\u05DF \\u05DE\\u05E8\\u05D0\\u05E9", result);
  BOOST_CHECK_EQUAL((size_t) 2189, outLength);

  char* unescaped = TRI_UnescapeUtf8String(result, strlen(result), &outLength);
  BOOST_CHECK_EQUAL(unescaped, "כפי שסופיה קופולה היטיבה לבטא בסרטה אבודים בטוקיו, בתי מלון יוקרתיים בערים גדולות אמנם מציעים אינספור פינוקים, אבל הם גם עלולים לגרום לנו להרגיש בודדים ואומללים מאי פעם. לעומת זאת, B&B, בתים פרטיים שבהם אפשר לישון ולאכול ארוחת בוקר, הם דרך נהדרת להכיר עיר אירופאית כמו מקומיים ולפגוש אנשים מרתקים מרחבי העולם. לטובת מי שנוסע לממלכה בחודשים הקרובים, הגרדיאן הבריטי קיבץ את עשרת ה-B&B המומלצים ביותר בלונדון. כל שנותר הוא לבחור, ולהזמין מראש");
  TRI_FreeString(TRI_CORE_MEM_ZONE, unescaped);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test Arabian
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_arabian) {
  size_t outLength;
  char* result;

  result = TEST_STRING("بان يأسف لمقتل لاجئين سوريين بتركيا المرزوقي يندد بعنف الأمن التونسي تنديد بقتل الجيش السوري مصورا تلفزيونيا 14 قتيلا وعشرات الجرحى بانفجار بالصومال");
  BOOST_CHECK_EQUAL("\\u0628\\u0627\\u0646 \\u064A\\u0623\\u0633\\u0641 \\u0644\\u0645\\u0642\\u062A\\u0644 \\u0644\\u0627\\u062C\\u0626\\u064A\\u0646 \\u0633\\u0648\\u0631\\u064A\\u064A\\u0646 \\u0628\\u062A\\u0631\\u0643\\u064A\\u0627 \\u0627\\u0644\\u0645\\u0631\\u0632\\u0648\\u0642\\u064A \\u064A\\u0646\\u062F\\u062F \\u0628\\u0639\\u0646\\u0641 \\u0627\\u0644\\u0623\\u0645\\u0646 \\u0627\\u0644\\u062A\\u0648\\u0646\\u0633\\u064A \\u062A\\u0646\\u062F\\u064A\\u062F \\u0628\\u0642\\u062A\\u0644 \\u0627\\u0644\\u062C\\u064A\\u0634 \\u0627\\u0644\\u0633\\u0648\\u0631\\u064A \\u0645\\u0635\\u0648\\u0631\\u0627 \\u062A\\u0644\\u0641\\u0632\\u064A\\u0648\\u0646\\u064A\\u0627 14 \\u0642\\u062A\\u064A\\u0644\\u0627 \\u0648\\u0639\\u0634\\u0631\\u0627\\u062A \\u0627\\u0644\\u062C\\u0631\\u062D\\u0649 \\u0628\\u0627\\u0646\\u0641\\u062C\\u0627\\u0631 \\u0628\\u0627\\u0644\\u0635\\u0648\\u0645\\u0627\\u0644", result);
  BOOST_CHECK_EQUAL((size_t) 768, outLength);

  char* unescaped = TRI_UnescapeUtf8String(result, strlen(result), &outLength);
  BOOST_CHECK_EQUAL(unescaped, "بان يأسف لمقتل لاجئين سوريين بتركيا المرزوقي يندد بعنف الأمن التونسي تنديد بقتل الجيش السوري مصورا تلفزيونيا 14 قتيلا وعشرات الجرحى بانفجار بالصومال");
  TRI_FreeString(TRI_CORE_MEM_ZONE, unescaped);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test Russian
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_russian) {
  size_t outLength;
  char* result;
 
  result = TEST_STRING("Голкипер мадридского «Реала» Икер Касильяс призвал своих партнеров сосредоточить все мысли на предстоящем дерби с «Атлетико»");
  BOOST_CHECK_EQUAL("\\u0413\\u043E\\u043B\\u043A\\u0438\\u043F\\u0435\\u0440 \\u043C\\u0430\\u0434\\u0440\\u0438\\u0434\\u0441\\u043A\\u043E\\u0433\\u043E \\u00AB\\u0420\\u0435\\u0430\\u043B\\u0430\\u00BB \\u0418\\u043A\\u0435\\u0440 \\u041A\\u0430\\u0441\\u0438\\u043B\\u044C\\u044F\\u0441 \\u043F\\u0440\\u0438\\u0437\\u0432\\u0430\\u043B \\u0441\\u0432\\u043E\\u0438\\u0445 \\u043F\\u0430\\u0440\\u0442\\u043D\\u0435\\u0440\\u043E\\u0432 \\u0441\\u043E\\u0441\\u0440\\u0435\\u0434\\u043E\\u0442\\u043E\\u0447\\u0438\\u0442\\u044C \\u0432\\u0441\\u0435 \\u043C\\u044B\\u0441\\u043B\\u0438 \\u043D\\u0430 \\u043F\\u0440\\u0435\\u0434\\u0441\\u0442\\u043E\\u044F\\u0449\\u0435\\u043C \\u0434\\u0435\\u0440\\u0431\\u0438 \\u0441 \\u00AB\\u0410\\u0442\\u043B\\u0435\\u0442\\u0438\\u043A\\u043E\\u00BB", result);
  BOOST_CHECK_EQUAL((size_t) 669, outLength);

  char* unescaped = TRI_UnescapeUtf8String(result, strlen(result), &outLength);
  BOOST_CHECK_EQUAL(unescaped, "Голкипер мадридского «Реала» Икер Касильяс призвал своих партнеров сосредоточить все мысли на предстоящем дерби с «Атлетико»");
  TRI_FreeString(TRI_CORE_MEM_ZONE, unescaped);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test Klingon
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_klingon) {
  size_t outLength;
  char* result;
 
  result = TEST_STRING("   ");
  BOOST_CHECK_EQUAL("\\uF8D8\\uF8D7\\uF8D9 \\uF8DA\\uF8DD\\uF8D6 \\uF8D5\\uF8D0\\uF8D8\\uF8D8\\uF8D0\\uF8D8 \\uF8D8\\uF8D0\\uF8D5\\uF8D6\\uF8DD\\uF8DA\\uF8D9\\uF8D7\\uF8D8", result);
  BOOST_CHECK_EQUAL((size_t) 129, outLength);
  
  char* unescaped = TRI_UnescapeUtf8String(result, strlen(result), &outLength);
  BOOST_CHECK_EQUAL(unescaped, "   ");
  TRI_FreeString(TRI_CORE_MEM_ZONE, unescaped);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test Devanagari
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_devanagari) {
  size_t outLength;
  char* result;

  result = TEST_STRING("अ आ इ ई उ ऊ ऋ ॠ ऌ ॡ ए ऐ ओ औ क ख ग घ ङ च छ ज झ ञ ट ठ ड ढ ण त थ द ध न प फ ब भ म य र ल व श ष स ह");
  BOOST_CHECK_EQUAL("\\u0905 \\u0906 \\u0907 \\u0908 \\u0909 \\u090A \\u090B \\u0960 \\u090C \\u0961 \\u090F \\u0910 \\u0913 \\u0914 \\u0915 \\u0916 \\u0917 \\u0918 \\u0919 \\u091A \\u091B \\u091C \\u091D \\u091E \\u091F \\u0920 \\u0921 \\u0922 \\u0923 \\u0924 \\u0925 \\u0926 \\u0927 \\u0928 \\u092A \\u092B \\u092C \\u092D \\u092E \\u092F \\u0930 \\u0932 \\u0935 \\u0936 \\u0937 \\u0938 \\u0939", result);
  BOOST_CHECK_EQUAL((size_t) 328, outLength);

  char* unescaped = TRI_UnescapeUtf8String(result, strlen(result), &outLength);
  BOOST_CHECK_EQUAL(unescaped, "अ आ इ ई उ ऊ ऋ ॠ ऌ ॡ ए ऐ ओ औ क ख ग घ ङ च छ ज झ ञ ट ठ ड ढ ण त थ द ध न प फ ब भ म य र ल व श ष स ह");
  TRI_FreeString(TRI_CORE_MEM_ZONE, unescaped);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test Vietnamese
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_vietnamese) {
  size_t outLength;
  char* result;

  result = TEST_STRING("tɜt kɐː mɔj ŋɨɜj siŋ za ɗew ɗɨɜk tɨɰ zɔ vɐː ɓiŋ ɗɐŋ vej ɲɜn fɜm vɐː kɨɜn. mɔj kɔn ŋɨɜj ɗeu ɗɨɜk tɐːw huɜ ɓɐːn cɔ li ci vɐː lɨɜŋ tɜm vɐː kɜn fɐːj ɗoj sɨ vɜj ɲɐw cɔŋ tiŋ ɓɐŋ hɨw.");
  BOOST_CHECK_EQUAL("t\\u025Ct k\\u0250\\u02D0 m\\u0254j \\u014B\\u0268\\u025Cj si\\u014B za \\u0257ew \\u0257\\u0268\\u025Ck t\\u0268\\u0270 z\\u0254 v\\u0250\\u02D0 \\u0253i\\u014B \\u0257\\u0250\\u014B vej \\u0272\\u025Cn f\\u025Cm v\\u0250\\u02D0 k\\u0268\\u025Cn. m\\u0254j k\\u0254n \\u014B\\u0268\\u025Cj \\u0257eu \\u0257\\u0268\\u025Ck t\\u0250\\u02D0w hu\\u025C \\u0253\\u0250\\u02D0n c\\u0254 li ci v\\u0250\\u02D0 l\\u0268\\u025C\\u014B t\\u025Cm v\\u0250\\u02D0 k\\u025Cn f\\u0250\\u02D0j \\u0257oj s\\u0268 v\\u025Cj \\u0272\\u0250w c\\u0254\\u014B ti\\u014B \\u0253\\u0250\\u014B h\\u0268w.", result);
  BOOST_CHECK_EQUAL((size_t) 516, outLength);
  
  char* unescaped = TRI_UnescapeUtf8String(result, strlen(result), &outLength);
  BOOST_CHECK_EQUAL(unescaped, "tɜt kɐː mɔj ŋɨɜj siŋ za ɗew ɗɨɜk tɨɰ zɔ vɐː ɓiŋ ɗɐŋ vej ɲɜn fɜm vɐː kɨɜn. mɔj kɔn ŋɨɜj ɗeu ɗɨɜk tɐːw huɜ ɓɐːn cɔ li ci vɐː lɨɜŋ tɜm vɐː kɜn fɐːj ɗoj sɨ vɜj ɲɐw cɔŋ tiŋ ɓɐŋ hɨw.");
  TRI_FreeString(TRI_CORE_MEM_ZONE, unescaped);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test Western European
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_western_european) {
  size_t outLength;
  char* result;

  result = TEST_STRING("äöüßÄÖÜ€µ");
  BOOST_CHECK_EQUAL("\\u00E4\\u00F6\\u00FC\\u00DF\\u00C4\\u00D6\\u00DC\\u20AC\\u00B5", result);
  BOOST_CHECK_EQUAL((size_t) 54, outLength);

  char* unescaped = TRI_UnescapeUtf8String(result, strlen(result), &outLength);
  BOOST_CHECK_EQUAL(unescaped, "äöüßÄÖÜ€µ");
  TRI_FreeString(TRI_CORE_MEM_ZONE, unescaped);
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test char length
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_char_length) {
  const char* test = "დახმარებისთვის";

  BOOST_CHECK_EQUAL(14, TRI_CharLengthUtf8String(test));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
