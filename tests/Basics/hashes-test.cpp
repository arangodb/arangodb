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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "Basics/Utf8Helper.h"
#include "Basics/directories.h"
#include "Basics/fasthash.h"
#include "Basics/files.h"
#include "Basics/hashes.h"

#include "icu-helper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

class CHashesTest : public ::testing::Test {
 protected:
  CHashesTest() {
    IcuInitializer::setup("./3rdParty/V8/v8/third_party/icu/common/icudtl.dat");
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test fasthash64
////////////////////////////////////////////////////////////////////////////////

TEST_F(CHashesTest, tst_fasthash64_uint64) {
  uint64_t value;

  value = 0;
  EXPECT_TRUE(606939172421154273ULL == fasthash64(&value, sizeof(value), 0x12345678));
  EXPECT_TRUE(606939172421154273ULL == fasthash64_uint64(value, 0x12345678));

  value = 1;
  EXPECT_TRUE(2986466439906256014ULL ==
              fasthash64(&value, sizeof(value), 0x12345678));
  EXPECT_TRUE(2986466439906256014ULL == fasthash64_uint64(value, 0x12345678));

  value = 123456;
  EXPECT_TRUE(10846706210321519612ULL ==
              fasthash64(&value, sizeof(value), 0x12345678));
  EXPECT_TRUE(10846706210321519612ULL == fasthash64_uint64(value, 0x12345678));

  value = 123456789012345ULL;
  EXPECT_TRUE(11872028338155052138ULL ==
              fasthash64(&value, sizeof(value), 0x12345678));
  EXPECT_TRUE(11872028338155052138ULL == fasthash64_uint64(value, 0x12345678));

  value = 0xffffff000000ULL;
  EXPECT_TRUE(5064027312035038651ULL ==
              fasthash64(&value, sizeof(value), 0x12345678));
  EXPECT_TRUE(5064027312035038651ULL == fasthash64_uint64(value, 0x12345678));

  value = 0xffffffffffffULL;
  EXPECT_TRUE(12472603196990564371ULL ==
              fasthash64(&value, sizeof(value), 0x12345678));
  EXPECT_TRUE(12472603196990564371ULL == fasthash64_uint64(value, 0x12345678));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test fasthash64
////////////////////////////////////////////////////////////////////////////////

TEST_F(CHashesTest, tst_fasthash64) {
  std::string buffer;

  buffer = "";
  EXPECT_TRUE(5555116246627715051ULL ==
              fasthash64(buffer.c_str(), buffer.size(), 0x12345678));

  buffer = " ";
  EXPECT_TRUE(4304446254109062897ULL ==
              fasthash64(buffer.c_str(), buffer.size(), 0x12345678));

  buffer = "abc";
  EXPECT_TRUE(14147965635343636579ULL ==
              fasthash64(buffer.c_str(), buffer.size(), 0x12345678));

  buffer = "ABC";
  EXPECT_TRUE(3265783561331679725ULL ==
              fasthash64(buffer.c_str(), buffer.size(), 0x12345678));

  buffer = "der kuckuck und der Esel, die hatten einen Streit";
  EXPECT_TRUE(13782917465498480784ULL ==
              fasthash64(buffer.c_str(), buffer.size(), 0x12345678));

  buffer = "Fox you have stolen the goose, give she back again";
  EXPECT_TRUE(5079926258749101985ULL ==
              fasthash64(buffer.c_str(), buffer.size(), 0x12345678));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test fasthash64 unaligned reads
////////////////////////////////////////////////////////////////////////////////

TEST_F(CHashesTest, tst_fasthash64_unaligned) {
  std::string buffer;

  buffer = " der kuckuck und der Esel, die hatten einen Streit";
  EXPECT_TRUE(13782917465498480784ULL ==
              fasthash64(buffer.c_str() + 1, buffer.size() - 1, 0x12345678));

  buffer = "  der kuckuck und der Esel, die hatten einen Streit";
  EXPECT_TRUE(13782917465498480784ULL ==
              fasthash64(buffer.c_str() + 2, buffer.size() - 2, 0x12345678));

  buffer = "   der kuckuck und der Esel, die hatten einen Streit";
  EXPECT_TRUE(13782917465498480784ULL ==
              fasthash64(buffer.c_str() + 3, buffer.size() - 3, 0x12345678));

  buffer = "    der kuckuck und der Esel, die hatten einen Streit";
  EXPECT_TRUE(13782917465498480784ULL ==
              fasthash64(buffer.c_str() + 4, buffer.size() - 4, 0x12345678));

  buffer = "     der kuckuck und der Esel, die hatten einen Streit";
  EXPECT_TRUE(13782917465498480784ULL ==
              fasthash64(buffer.c_str() + 5, buffer.size() - 5, 0x12345678));

  buffer = "      der kuckuck und der Esel, die hatten einen Streit";
  EXPECT_TRUE(13782917465498480784ULL ==
              fasthash64(buffer.c_str() + 6, buffer.size() - 6, 0x12345678));

  buffer = "       der kuckuck und der Esel, die hatten einen Streit";
  EXPECT_TRUE(13782917465498480784ULL ==
              fasthash64(buffer.c_str() + 7, buffer.size() - 7, 0x12345678));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test fnv64 for simple strings
////////////////////////////////////////////////////////////////////////////////

TEST_F(CHashesTest, tst_fnv64_simple) {
  std::string buffer;

  buffer = "";
  EXPECT_TRUE(14695981039346656037ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(14695981039346656037ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(14695981039346656037ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = " ";
  EXPECT_TRUE(12638117931323064703ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(12638117931323064703ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(12638117931323064703ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = "  ";
  EXPECT_TRUE(560038479724991597ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(560038479724991597ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(560038479724991597ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = "a";
  EXPECT_TRUE(12638187200555641996ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(12638187200555641996ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(12638187200555641996ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = "A";
  EXPECT_TRUE(12638222384927744748ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(12638222384927744748ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(12638222384927744748ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = " a";
  EXPECT_TRUE(559967011469157882ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(559967011469157882ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(559967011469157882ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = " a ";
  EXPECT_TRUE(14038824050427892078ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(14038824050427892078ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(14038824050427892078ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = "a ";
  EXPECT_TRUE(620373080799520836ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(620373080799520836ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(620373080799520836ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = "A ";
  EXPECT_TRUE(650913115778654372ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(650913115778654372ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(650913115778654372ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = " A";
  EXPECT_TRUE(560002195841260634ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(560002195841260634ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(560002195841260634ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = " A ";
  EXPECT_TRUE(14069504822895436622ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(14069504822895436622ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(14069504822895436622ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = "0";
  EXPECT_TRUE(12638135523509116079ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(12638135523509116079ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(12638135523509116079ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = "1";
  EXPECT_TRUE(12638134423997487868ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(12638134423997487868ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(12638134423997487868ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = "11";
  EXPECT_TRUE(574370613795883607ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(574370613795883607ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(574370613795883607ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = "111";
  EXPECT_TRUE(5002439360283388754ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(5002439360283388754ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(5002439360283388754ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = "2";
  EXPECT_TRUE(12638137722532372501ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(12638137722532372501ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(12638137722532372501ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = "3";
  EXPECT_TRUE(12638136623020744290ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(12638136623020744290ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(12638136623020744290ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = "the quick brown fox jumped over the lazy dog";
  EXPECT_TRUE(5742411339260295416ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(5742411339260295416ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(5742411339260295416ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = "The Quick Brown Fox Jumped Over The Lazy Dog";
  EXPECT_TRUE(11643291398347681368ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(11643291398347681368ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(11643291398347681368ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test fnv64 for UTF-8 strings
////////////////////////////////////////////////////////////////////////////////

TEST_F(CHashesTest, tst_fnv64_utf8) {
  std::string buffer;

  buffer =
      "ジャパン は、イギリスのニュー・ウェーヴバンド。デヴィッド・ ... "
      "を構築していった。 "
      "日本では初来日でいきなり武道館での公演を行うなど、爆発的な人気を誇ったが"
      "、英国ではなかなか人気が出ず、初期は典型的な「ビッグ・イン・ジャパン」状"
      "態であった。日本最大級のポータルサイト。検索、オークション、ニュース、メ"
      "ール、コミュニティ、ショッピング、など80以上のサービスを展開。あなたの生"
      "活をより豊かにする「ライフ・エンジン」を目指していきます。デヴィッド・シ"
      "ルヴィアンとその弟スティーヴ・ジャンセン、デヴィッドの親友であったミック"
      "・カーンを中心に結成。ミック・カーンの兄の結婚式にバンドとして最初のお披"
      "露目をした。当初はミック・カーンをリードボーカルとして練習していたが、本"
      "番直前になって怖じ気づいたミックがデヴィッド・シルヴィアンに無理矢理頼み"
      "込んでボーカルを代わってもらい、以降デヴィッドがリードボーカルとなった。"
      "その後高校の同級であったリチャード・バルビエリを誘い、更にオーディション"
      "でロブ・ディーンを迎え入れ、デビュー当初のバンドの形態となった。デビュー"
      "当初はアイドルとして宣伝されたグループだったが、英国の音楽シーンではほと"
      "んど人気が無かった。初期のサウンドは主に黒人音楽やグラムロックをポスト・"
      "パンク的に再解釈したものであったが、作品を重ねるごとに耽美的な作風、退廃"
      "的な歌詞やシンセサイザーの利用など独自のスタイルを構築していった。日本で"
      "は初来日でいきなり武道館での公演を行うなど、爆発的な人気を誇ったが、英国"
      "ではなかなか人気が出ず、初期は典型的な「ビッグ・イン・ジャパン」状態であ"
      "った。";
  EXPECT_TRUE(211184911024797733ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(211184911024797733ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(211184911024797733ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer =
      "코리아닷컴 메일알리미 서비스 중단안내 [안내] 개인정보취급방침 변경 안내 "
      "회사소개 | 광고안내 | 제휴안내 | 개인정보취급방침 | 청소년보호정책 | "
      "스팸방지정책 | 사이버고객센터 | 약관안내 | 이메일 무단수집거부 | 서비스 "
      "전체보기";
  EXPECT_TRUE(270676307504294177ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(270676307504294177ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(270676307504294177ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer =
      "中华网以中国的市场为核心，致力为当地用户提供流动增值服务、网上娱乐及互联"
      "网服务。本公司亦推出网上游戏，及透过其门户网站提供包罗万有的网上产品及服"
      "务。";
  EXPECT_TRUE(14670566365397374664ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(14670566365397374664ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(14670566365397374664ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer =
      "כפי שסופיה קופולה היטיבה לבטא בסרטה אבודים בטוקיו, בתי מלון יוקרתיים "
      "בערים גדולות אמנם מציעים אינספור פינוקים, אבל הם גם עלולים לגרום לנו "
      "להרגיש בודדים ואומללים מאי פעם. לעומת זאת, B&B, בתים פרטיים שבהם אפשר "
      "לישון ולאכול ארוחת בוקר, הם דרך נהדרת להכיר עיר אירופאית כמו מקומיים "
      "ולפגוש אנשים מרתקים מרחבי העולם. לטובת מי שנוסע לממלכה בחודשים הקרובים, "
      "הגרדיאן הבריטי קיבץ את עשרת ה-B&B המומלצים ביותר בלונדון. כל שנותר הוא "
      "לבחור, ולהזמין מראש";
  EXPECT_TRUE(16145169633099782595ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(16145169633099782595ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(16145169633099782595ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer =
      "بان يأسف لمقتل لاجئين سوريين بتركيا المرزوقي يندد بعنف الأمن التونسي "
      "تنديد بقتل الجيش السوري مصورا تلفزيونيا 14 قتيلا وعشرات الجرحى بانفجار "
      "بالصومال";
  EXPECT_TRUE(7398242043026945788ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(7398242043026945788ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(7398242043026945788ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer =
      "Голкипер мадридского «Реала» Икер Касильяс призвал своих партнеров "
      "сосредоточить все мысли на предстоящем дерби с «Атлетико»";
  EXPECT_TRUE(10412552537249637418ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(10412552537249637418ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(10412552537249637418ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = "   ";
  EXPECT_TRUE(4095843978425089933ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(4095843978425089933ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(4095843978425089933ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer =
      "अ आ इ ई उ ऊ ऋ ॠ ऌ ॡ ए ऐ ओ औ क ख ग घ ङ च छ ज झ ञ ट ठ ड ढ ण त थ द ध न प फ "
      "ब भ म य र ल व श ष स ह";
  EXPECT_TRUE(2927729442665428350ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(2927729442665428350ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(2927729442665428350ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer =
      "tɜt kɐː mɔj ŋɨɜj siŋ za ɗew ɗɨɜk tɨɰ zɔ vɐː ɓiŋ ɗɐŋ vej ɲɜn fɜm vɐː "
      "kɨɜn. mɔj kɔn ŋɨɜj ɗeu ɗɨɜk tɐːw huɜ ɓɐːn cɔ li ci vɐː lɨɜŋ tɜm vɐː kɜn "
      "fɐːj ɗoj sɨ vɜj ɲɐw cɔŋ tiŋ ɓɐŋ hɨw.";
  EXPECT_TRUE(15359789603011345030ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(15359789603011345030ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(15359789603011345030ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));

  buffer = "äöüßÄÖÜ€µ";
  EXPECT_TRUE(2954195900047086928ULL == TRI_FnvHashString(buffer.c_str()));
  EXPECT_TRUE(2954195900047086928ULL ==
              TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(2954195900047086928ULL ==
              TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(),
                               strlen(buffer.c_str())));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test crc32 for simple strings
////////////////////////////////////////////////////////////////////////////////

TEST_F(CHashesTest, tst_crc32_simple) {
  std::string buffer;

  buffer = "";
  EXPECT_TRUE(0ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(0ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(0ULL ==
              TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(),
                                            strlen(buffer.c_str()))));

  buffer = " ";
  EXPECT_TRUE(1925242255ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(1925242255ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(1925242255ULL ==
              TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(),
                                            strlen(buffer.c_str()))));

  buffer = "  ";
  EXPECT_TRUE(2924943886ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(2924943886ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(2924943886ULL ==
              TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(),
                                            strlen(buffer.c_str()))));

  buffer = "a";
  EXPECT_TRUE(3251651376ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(3251651376ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(3251651376ULL ==
              TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(),
                                            strlen(buffer.c_str()))));

  buffer = "A";
  EXPECT_TRUE(3782069742ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(3782069742ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));

  buffer = " a";
  EXPECT_TRUE(491226289ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(491226289ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));

  buffer = " a ";
  EXPECT_TRUE(849570753ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(849570753ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));

  buffer = "a ";
  EXPECT_TRUE(1122124925ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(1122124925ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));

  buffer = "A ";
  EXPECT_TRUE(1030334335ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(1030334335ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));

  buffer = " A";
  EXPECT_TRUE(1039796847ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(1039796847ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));

  buffer = " A ";
  EXPECT_TRUE(1294502083ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(1294502083ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));

  buffer = "0";
  EXPECT_TRUE(1654528736ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(1654528736ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));

  buffer = "1";
  EXPECT_TRUE(2432014819ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(2432014819ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));

  buffer = "11";
  EXPECT_TRUE(1610954644ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(1610954644ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));

  buffer = "111";
  EXPECT_TRUE(3316119516ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(3316119516ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));

  buffer = "2";
  EXPECT_TRUE(2208655895ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(2208655895ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));

  buffer = "3";
  EXPECT_TRUE(1909385492ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(1909385492ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));

  buffer = "the quick brown fox jumped over the lazy dog";
  EXPECT_TRUE(3928504206ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(3928504206ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(3928504206ULL ==
              TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(),
                                            strlen(buffer.c_str()))));

  buffer = "The Quick Brown Fox Jumped Over The Lazy Dog";
  EXPECT_TRUE(4053635637ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(4053635637ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(4053635637ULL ==
              TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(),
                                            strlen(buffer.c_str()))));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test crc32 for UTF-8 strings
////////////////////////////////////////////////////////////////////////////////

TEST_F(CHashesTest, tst_crc32_utf8) {
  std::string buffer;

  buffer =
      "ジャパン は、イギリスのニュー・ウェーヴバンド。デヴィッド・ ... "
      "を構築していった。 "
      "日本では初来日でいきなり武道館での公演を行うなど、爆発的な人気を誇ったが"
      "、英国ではなかなか人気が出ず、初期は典型的な「ビッグ・イン・ジャパン」状"
      "態であった。日本最大級のポータルサイト。検索、オークション、ニュース、メ"
      "ール、コミュニティ、ショッピング、など80以上のサービスを展開。あなたの生"
      "活をより豊かにする「ライフ・エンジン」を目指していきます。デヴィッド・シ"
      "ルヴィアンとその弟スティーヴ・ジャンセン、デヴィッドの親友であったミック"
      "・カーンを中心に結成。ミック・カーンの兄の結婚式にバンドとして最初のお披"
      "露目をした。当初はミック・カーンをリードボーカルとして練習していたが、本"
      "番直前になって怖じ気づいたミックがデヴィッド・シルヴィアンに無理矢理頼み"
      "込んでボーカルを代わってもらい、以降デヴィッドがリードボーカルとなった。"
      "その後高校の同級であったリチャード・バルビエリを誘い、更にオーディション"
      "でロブ・ディーンを迎え入れ、デビュー当初のバンドの形態となった。デビュー"
      "当初はアイドルとして宣伝されたグループだったが、英国の音楽シーンではほと"
      "んど人気が無かった。初期のサウンドは主に黒人音楽やグラムロックをポスト・"
      "パンク的に再解釈したものであったが、作品を重ねるごとに耽美的な作風、退廃"
      "的な歌詞やシンセサイザーの利用など独自のスタイルを構築していった。日本で"
      "は初来日でいきなり武道館での公演を行うなど、爆発的な人気を誇ったが、英国"
      "ではなかなか人気が出ず、初期は典型的な「ビッグ・イン・ジャパン」状態であ"
      "った。";
  EXPECT_TRUE(4191893375ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(4191893375ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(4191893375ULL ==
              TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(),
                                            strlen(buffer.c_str()))));

  buffer =
      "코리아닷컴 메일알리미 서비스 중단안내 [안내] 개인정보취급방침 변경 안내 "
      "회사소개 | 광고안내 | 제휴안내 | 개인정보취급방침 | 청소년보호정책 | "
      "스팸방지정책 | 사이버고객센터 | 약관안내 | 이메일 무단수집거부 | 서비스 "
      "전체보기";
  EXPECT_TRUE(4065546148ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(4065546148ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(4065546148ULL ==
              TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(),
                                            strlen(buffer.c_str()))));

  buffer =
      "中华网以中国的市场为核心，致力为当地用户提供流动增值服务、网上娱乐及互联"
      "网服务。本公司亦推出网上游戏，及透过其门户网站提供包罗万有的网上产品及服"
      "务。";
  EXPECT_TRUE(1577296531ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(1577296531ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(1577296531ULL ==
              TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(),
                                            strlen(buffer.c_str()))));

  buffer =
      "כפי שסופיה קופולה היטיבה לבטא בסרטה אבודים בטוקיו, בתי מלון יוקרתיים "
      "בערים גדולות אמנם מציעים אינספור פינוקים, אבל הם גם עלולים לגרום לנו "
      "להרגיש בודדים ואומללים מאי פעם. לעומת זאת, B&B, בתים פרטיים שבהם אפשר "
      "לישון ולאכול ארוחת בוקר, הם דרך נהדרת להכיר עיר אירופאית כמו מקומיים "
      "ולפגוש אנשים מרתקים מרחבי העולם. לטובת מי שנוסע לממלכה בחודשים הקרובים, "
      "הגרדיאן הבריטי קיבץ את עשרת ה-B&B המומלצים ביותר בלונדון. כל שנותר הוא "
      "לבחור, ולהזמין מראש";
  EXPECT_TRUE(3810256208ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(3810256208ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(3810256208ULL ==
              TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(),
                                            strlen(buffer.c_str()))));

  buffer =
      "بان يأسف لمقتل لاجئين سوريين بتركيا المرزوقي يندد بعنف الأمن التونسي "
      "تنديد بقتل الجيش السوري مصورا تلفزيونيا 14 قتيلا وعشرات الجرحى بانفجار "
      "بالصومال";
  EXPECT_TRUE(2844487215ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(2844487215ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(2844487215ULL ==
              TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(),
                                            strlen(buffer.c_str()))));

  buffer =
      "Голкипер мадридского «Реала» Икер Касильяс призвал своих партнеров "
      "сосредоточить все мысли на предстоящем дерби с «Атлетико»";
  EXPECT_TRUE(1905918845ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(1905918845ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(1905918845ULL ==
              TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(),
                                            strlen(buffer.c_str()))));

  buffer = "   ";
  EXPECT_TRUE(1824561399ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(1824561399ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(1824561399ULL ==
              TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(),
                                            strlen(buffer.c_str()))));

  buffer =
      "अ आ इ ई उ ऊ ऋ ॠ ऌ ॡ ए ऐ ओ औ क ख ग घ ङ च छ ज झ ञ ट ठ ड ढ ण त थ द ध न प फ "
      "ब भ म य र ल व श ष स ह";
  EXPECT_TRUE(3232493769ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(3232493769ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(3232493769ULL ==
              TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(),
                                            strlen(buffer.c_str()))));

  buffer =
      "tɜt kɐː mɔj ŋɨɜj siŋ za ɗew ɗɨɜk tɨɰ zɔ vɐː ɓiŋ ɗɐŋ vej ɲɜn fɜm vɐː "
      "kɨɜn. mɔj kɔn ŋɨɜj ɗeu ɗɨɜk tɐːw huɜ ɓɐːn cɔ li ci vɐː lɨɜŋ tɜm vɐː kɜn "
      "fɐːj ɗoj sɨ vɜj ɲɐw cɔŋ tiŋ ɓɐŋ hɨw.";
  EXPECT_TRUE(193365419ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(193365419ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(193365419ULL ==
              TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(),
                                            strlen(buffer.c_str()))));

  buffer = "äöüßÄÖÜ€µ";
  EXPECT_TRUE(1426740181ULL == TRI_Crc32HashString(buffer.c_str()));
  EXPECT_TRUE(1426740181ULL ==
              TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  EXPECT_TRUE(1426740181ULL ==
              TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(),
                                            strlen(buffer.c_str()))));
}
