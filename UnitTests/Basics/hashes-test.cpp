////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for hashes.c
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

#include "Basics/Common.h"

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include "Basics/fasthash.h"
#include "Basics/hashes.h"
#include "Basics/directories.h"
#include "Basics/Utf8Helper.h"

#if _WIN32
#include "Basics/win-utils.h"
#define FIX_ICU_ENV     TRI_FixIcuDataEnv(SBIN_DIRECTORY)
#else
#define FIX_ICU_ENV
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CHashesSetup {
  CHashesSetup () {
    FIX_ICU_ENV;
    if (!arangodb::basics::Utf8Helper::DefaultUtf8Helper.setCollatorLanguage("", SBIN_DIRECTORY)) {
      std::string msg =
        "cannot initialize ICU; please make sure ICU*dat is available; "
        "the variable ICU_DATA='";
      if (getenv("ICU_DATA") != nullptr) {
        msg += getenv("ICU_DATA");
      }
      msg += "' should point the directory containing the ICU*dat file.";
      BOOST_TEST_MESSAGE(msg);
      BOOST_CHECK_EQUAL(false, true);
    }
    BOOST_TEST_MESSAGE("setup hashes");
  }

  ~CHashesSetup () {
    BOOST_TEST_MESSAGE("tear-down hashes");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(CHashesTest, CHashesSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test fasthash64
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_fasthash64) {
  std::string buffer;

  buffer = "";
  BOOST_CHECK_EQUAL((uint64_t) 5555116246627715051ULL, fasthash64(buffer.c_str(), buffer.size(), 0x12345678));

  buffer = " ";
  BOOST_CHECK_EQUAL((uint64_t) 4304446254109062897ULL, fasthash64(buffer.c_str(), buffer.size(), 0x12345678));
  
  buffer = "abc";
  BOOST_CHECK_EQUAL((uint64_t) 14147965635343636579ULL, fasthash64(buffer.c_str(), buffer.size(), 0x12345678));
  
  buffer = "ABC";
  BOOST_CHECK_EQUAL((uint64_t) 3265783561331679725ULL, fasthash64(buffer.c_str(), buffer.size(), 0x12345678));
  
  buffer = "der kuckuck und der Esel, die hatten einen Streit";
  BOOST_CHECK_EQUAL((uint64_t) 13782917465498480784ULL, fasthash64(buffer.c_str(), buffer.size(), 0x12345678));
  
  buffer = "Fox you have stolen the goose, give she back again";
  BOOST_CHECK_EQUAL((uint64_t) 5079926258749101985ULL, fasthash64(buffer.c_str(), buffer.size(), 0x12345678));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test fasthash64 unaligned reads
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_fasthash64_unaligned) {
  std::string buffer;

  buffer = " der kuckuck und der Esel, die hatten einen Streit";
  BOOST_CHECK_EQUAL((uint64_t) 13782917465498480784ULL, fasthash64(buffer.c_str() + 1, buffer.size() - 1, 0x12345678));
  
  buffer = "  der kuckuck und der Esel, die hatten einen Streit";
  BOOST_CHECK_EQUAL((uint64_t) 13782917465498480784ULL, fasthash64(buffer.c_str() + 2, buffer.size() - 2, 0x12345678));
  
  buffer = "   der kuckuck und der Esel, die hatten einen Streit";
  BOOST_CHECK_EQUAL((uint64_t) 13782917465498480784ULL, fasthash64(buffer.c_str() + 3, buffer.size() - 3, 0x12345678));
  
  buffer = "    der kuckuck und der Esel, die hatten einen Streit";
  BOOST_CHECK_EQUAL((uint64_t) 13782917465498480784ULL, fasthash64(buffer.c_str() + 4, buffer.size() - 4, 0x12345678));
  
  buffer = "     der kuckuck und der Esel, die hatten einen Streit";
  BOOST_CHECK_EQUAL((uint64_t) 13782917465498480784ULL, fasthash64(buffer.c_str() + 5, buffer.size() - 5, 0x12345678));
  
  buffer = "      der kuckuck und der Esel, die hatten einen Streit";
  BOOST_CHECK_EQUAL((uint64_t) 13782917465498480784ULL, fasthash64(buffer.c_str() + 6, buffer.size() - 6, 0x12345678));
  
  buffer = "       der kuckuck und der Esel, die hatten einen Streit";
  BOOST_CHECK_EQUAL((uint64_t) 13782917465498480784ULL, fasthash64(buffer.c_str() + 7, buffer.size() - 7, 0x12345678));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test fnv64 for simple strings
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_fnv64_simple) {
  std::string buffer;

  buffer = "";
  BOOST_CHECK_EQUAL((uint64_t) 14695981039346656037ULL, TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 14695981039346656037ULL, TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 14695981039346656037ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = " ";
  BOOST_CHECK_EQUAL((uint64_t) 12638117931323064703ULL, TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 12638117931323064703ULL, TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 12638117931323064703ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "  ";
  BOOST_CHECK_EQUAL((uint64_t) 560038479724991597ULL,   TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 560038479724991597ULL,   TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 560038479724991597ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "a";
  BOOST_CHECK_EQUAL((uint64_t) 12638187200555641996ULL, TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 12638187200555641996ULL, TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 12638187200555641996ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "A";
  BOOST_CHECK_EQUAL((uint64_t) 12638222384927744748ULL, TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 12638222384927744748ULL, TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 12638222384927744748ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));

  buffer = " a";
  BOOST_CHECK_EQUAL((uint64_t) 559967011469157882ULL,   TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 559967011469157882ULL,   TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 559967011469157882ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = " a ";
  BOOST_CHECK_EQUAL((uint64_t) 14038824050427892078ULL, TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 14038824050427892078ULL, TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 14038824050427892078ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "a ";
  BOOST_CHECK_EQUAL((uint64_t) 620373080799520836ULL,   TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 620373080799520836ULL,   TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 620373080799520836ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "A ";
  BOOST_CHECK_EQUAL((uint64_t) 650913115778654372ULL,   TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 650913115778654372ULL,   TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 650913115778654372ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = " A";
  BOOST_CHECK_EQUAL((uint64_t) 560002195841260634ULL,   TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 560002195841260634ULL,   TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 560002195841260634ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = " A ";
  BOOST_CHECK_EQUAL((uint64_t) 14069504822895436622ULL, TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 14069504822895436622ULL, TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 14069504822895436622ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "0";
  BOOST_CHECK_EQUAL((uint64_t) 12638135523509116079ULL, TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 12638135523509116079ULL, TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 12638135523509116079ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "1";
  BOOST_CHECK_EQUAL((uint64_t) 12638134423997487868ULL, TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 12638134423997487868ULL, TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 12638134423997487868ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "11";
  BOOST_CHECK_EQUAL((uint64_t) 574370613795883607ULL,   TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 574370613795883607ULL,   TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 574370613795883607ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "111";
  BOOST_CHECK_EQUAL((uint64_t) 5002439360283388754ULL,  TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 5002439360283388754ULL,  TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 5002439360283388754ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "2";
  BOOST_CHECK_EQUAL((uint64_t) 12638137722532372501ULL, TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 12638137722532372501ULL, TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 12638137722532372501ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "3";
  BOOST_CHECK_EQUAL((uint64_t) 12638136623020744290ULL, TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 12638136623020744290ULL, TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 12638136623020744290ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "the quick brown fox jumped over the lazy dog";
  BOOST_CHECK_EQUAL((uint64_t) 5742411339260295416ULL,  TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 5742411339260295416ULL,  TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 5742411339260295416ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "The Quick Brown Fox Jumped Over The Lazy Dog";
  BOOST_CHECK_EQUAL((uint64_t) 11643291398347681368ULL, TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 11643291398347681368ULL, TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 11643291398347681368ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test fnv64 for UTF-8 strings
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_fnv64_utf8) {
  std::string buffer;

  buffer = "ジャパン は、イギリスのニュー・ウェーヴバンド。デヴィッド・ ... を構築していった。 日本では初来日でいきなり武道館での公演を行うなど、爆発的な人気を誇ったが、英国ではなかなか人気が出ず、初期は典型的な「ビッグ・イン・ジャパン」状態であった。日本最大級のポータルサイト。検索、オークション、ニュース、メール、コミュニティ、ショッピング、など80以上のサービスを展開。あなたの生活をより豊かにする「ライフ・エンジン」を目指していきます。デヴィッド・シルヴィアンとその弟スティーヴ・ジャンセン、デヴィッドの親友であったミック・カーンを中心に結成。ミック・カーンの兄の結婚式にバンドとして最初のお披露目をした。当初はミック・カーンをリードボーカルとして練習していたが、本番直前になって怖じ気づいたミックがデヴィッド・シルヴィアンに無理矢理頼み込んでボーカルを代わってもらい、以降デヴィッドがリードボーカルとなった。その後高校の同級であったリチャード・バルビエリを誘い、更にオーディションでロブ・ディーンを迎え入れ、デビュー当初のバンドの形態となった。デビュー当初はアイドルとして宣伝されたグループだったが、英国の音楽シーンではほとんど人気が無かった。初期のサウンドは主に黒人音楽やグラムロックをポスト・パンク的に再解釈したものであったが、作品を重ねるごとに耽美的な作風、退廃的な歌詞やシンセサイザーの利用など独自のスタイルを構築していった。日本では初来日でいきなり武道館での公演を行うなど、爆発的な人気を誇ったが、英国ではなかなか人気が出ず、初期は典型的な「ビッグ・イン・ジャパン」状態であった。";
  BOOST_CHECK_EQUAL((uint64_t) 211184911024797733ULL,   TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 211184911024797733ULL,   TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 211184911024797733ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));
 

  buffer = "코리아닷컴 메일알리미 서비스 중단안내 [안내] 개인정보취급방침 변경 안내 회사소개 | 광고안내 | 제휴안내 | 개인정보취급방침 | 청소년보호정책 | 스팸방지정책 | 사이버고객센터 | 약관안내 | 이메일 무단수집거부 | 서비스 전체보기";
  BOOST_CHECK_EQUAL((uint64_t) 270676307504294177ULL,   TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 270676307504294177ULL,   TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 270676307504294177ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "中华网以中国的市场为核心，致力为当地用户提供流动增值服务、网上娱乐及互联网服务。本公司亦推出网上游戏，及透过其门户网站提供包罗万有的网上产品及服务。";
  BOOST_CHECK_EQUAL((uint64_t) 14670566365397374664ULL,   TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 14670566365397374664ULL,   TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 14670566365397374664ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "כפי שסופיה קופולה היטיבה לבטא בסרטה אבודים בטוקיו, בתי מלון יוקרתיים בערים גדולות אמנם מציעים אינספור פינוקים, אבל הם גם עלולים לגרום לנו להרגיש בודדים ואומללים מאי פעם. לעומת זאת, B&B, בתים פרטיים שבהם אפשר לישון ולאכול ארוחת בוקר, הם דרך נהדרת להכיר עיר אירופאית כמו מקומיים ולפגוש אנשים מרתקים מרחבי העולם. לטובת מי שנוסע לממלכה בחודשים הקרובים, הגרדיאן הבריטי קיבץ את עשרת ה-B&B המומלצים ביותר בלונדון. כל שנותר הוא לבחור, ולהזמין מראש";
  BOOST_CHECK_EQUAL((uint64_t) 16145169633099782595ULL,   TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 16145169633099782595ULL,   TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 16145169633099782595ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "بان يأسف لمقتل لاجئين سوريين بتركيا المرزوقي يندد بعنف الأمن التونسي تنديد بقتل الجيش السوري مصورا تلفزيونيا 14 قتيلا وعشرات الجرحى بانفجار بالصومال";
  BOOST_CHECK_EQUAL((uint64_t) 7398242043026945788ULL,   TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 7398242043026945788ULL,   TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 7398242043026945788ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));
  

  buffer = "Голкипер мадридского «Реала» Икер Касильяс призвал своих партнеров сосредоточить все мысли на предстоящем дерби с «Атлетико»";
  BOOST_CHECK_EQUAL((uint64_t) 10412552537249637418ULL,   TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 10412552537249637418ULL,   TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 10412552537249637418ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));


  buffer = "   ";
  BOOST_CHECK_EQUAL((uint64_t) 4095843978425089933ULL,   TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 4095843978425089933ULL,   TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 4095843978425089933ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));
  

  buffer = "अ आ इ ई उ ऊ ऋ ॠ ऌ ॡ ए ऐ ओ औ क ख ग घ ङ च छ ज झ ञ ट ठ ड ढ ण त थ द ध न प फ ब भ म य र ल व श ष स ह";
  BOOST_CHECK_EQUAL((uint64_t) 2927729442665428350ULL,   TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 2927729442665428350ULL,   TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 2927729442665428350ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));
  

  buffer = "tɜt kɐː mɔj ŋɨɜj siŋ za ɗew ɗɨɜk tɨɰ zɔ vɐː ɓiŋ ɗɐŋ vej ɲɜn fɜm vɐː kɨɜn. mɔj kɔn ŋɨɜj ɗeu ɗɨɜk tɐːw huɜ ɓɐːn cɔ li ci vɐː lɨɜŋ tɜm vɐː kɜn fɐːj ɗoj sɨ vɜj ɲɐw cɔŋ tiŋ ɓɐŋ hɨw.";
  BOOST_CHECK_EQUAL((uint64_t) 15359789603011345030ULL,   TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 15359789603011345030ULL,   TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 15359789603011345030ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));
  

  buffer = "äöüßÄÖÜ€µ";
  BOOST_CHECK_EQUAL((uint64_t) 2954195900047086928ULL,   TRI_FnvHashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 2954195900047086928ULL,   TRI_FnvHashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 2954195900047086928ULL, TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), buffer.c_str(), strlen(buffer.c_str())));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test crc32 for simple strings
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_crc32_simple) {
  std::string buffer;

  buffer = "";
  BOOST_CHECK_EQUAL((uint64_t) 0ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 0ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 0ULL, TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(), strlen(buffer.c_str()))));


  buffer = " ";
  BOOST_CHECK_EQUAL((uint64_t) 1925242255ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 1925242255ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 1925242255ULL, TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(), strlen(buffer.c_str()))));


  buffer = "  ";
  BOOST_CHECK_EQUAL((uint64_t) 2924943886ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 2924943886ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 2924943886ULL, TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(), strlen(buffer.c_str()))));


  buffer = "a";
  BOOST_CHECK_EQUAL((uint64_t) 3251651376ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 3251651376ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 3251651376ULL, TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(), strlen(buffer.c_str()))));


  buffer = "A";
  BOOST_CHECK_EQUAL((uint64_t) 3782069742ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 3782069742ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));


  buffer = " a";
  BOOST_CHECK_EQUAL((uint64_t) 491226289ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 491226289ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));


  buffer = " a ";
  BOOST_CHECK_EQUAL((uint64_t) 849570753ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 849570753ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));


  buffer = "a ";
  BOOST_CHECK_EQUAL((uint64_t) 1122124925ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 1122124925ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));


  buffer = "A ";
  BOOST_CHECK_EQUAL((uint64_t) 1030334335ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 1030334335ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));


  buffer = " A";
  BOOST_CHECK_EQUAL((uint64_t) 1039796847ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 1039796847ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));


  buffer = " A ";
  BOOST_CHECK_EQUAL((uint64_t) 1294502083ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 1294502083ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));


  buffer = "0";
  BOOST_CHECK_EQUAL((uint64_t) 1654528736ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 1654528736ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));


  buffer = "1";
  BOOST_CHECK_EQUAL((uint64_t) 2432014819ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 2432014819ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));


  buffer = "11";
  BOOST_CHECK_EQUAL((uint64_t) 1610954644ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 1610954644ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));


  buffer = "111";
  BOOST_CHECK_EQUAL((uint64_t) 3316119516ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 3316119516ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));


  buffer = "2";
  BOOST_CHECK_EQUAL((uint64_t) 2208655895ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 2208655895ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));


  buffer = "3";
  BOOST_CHECK_EQUAL((uint64_t) 1909385492ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 1909385492ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));


  buffer = "the quick brown fox jumped over the lazy dog";
  BOOST_CHECK_EQUAL((uint64_t) 3928504206ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 3928504206ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 3928504206ULL, TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(), strlen(buffer.c_str()))));


  buffer = "The Quick Brown Fox Jumped Over The Lazy Dog";
  BOOST_CHECK_EQUAL((uint64_t) 4053635637ULL, TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 4053635637ULL, TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 4053635637ULL, TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(), strlen(buffer.c_str()))));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test crc32 for UTF-8 strings
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_crc32_utf8) {
  std::string buffer;

  buffer = "ジャパン は、イギリスのニュー・ウェーヴバンド。デヴィッド・ ... を構築していった。 日本では初来日でいきなり武道館での公演を行うなど、爆発的な人気を誇ったが、英国ではなかなか人気が出ず、初期は典型的な「ビッグ・イン・ジャパン」状態であった。日本最大級のポータルサイト。検索、オークション、ニュース、メール、コミュニティ、ショッピング、など80以上のサービスを展開。あなたの生活をより豊かにする「ライフ・エンジン」を目指していきます。デヴィッド・シルヴィアンとその弟スティーヴ・ジャンセン、デヴィッドの親友であったミック・カーンを中心に結成。ミック・カーンの兄の結婚式にバンドとして最初のお披露目をした。当初はミック・カーンをリードボーカルとして練習していたが、本番直前になって怖じ気づいたミックがデヴィッド・シルヴィアンに無理矢理頼み込んでボーカルを代わってもらい、以降デヴィッドがリードボーカルとなった。その後高校の同級であったリチャード・バルビエリを誘い、更にオーディションでロブ・ディーンを迎え入れ、デビュー当初のバンドの形態となった。デビュー当初はアイドルとして宣伝されたグループだったが、英国の音楽シーンではほとんど人気が無かった。初期のサウンドは主に黒人音楽やグラムロックをポスト・パンク的に再解釈したものであったが、作品を重ねるごとに耽美的な作風、退廃的な歌詞やシンセサイザーの利用など独自のスタイルを構築していった。日本では初来日でいきなり武道館での公演を行うなど、爆発的な人気を誇ったが、英国ではなかなか人気が出ず、初期は典型的な「ビッグ・イン・ジャパン」状態であった。";
  BOOST_CHECK_EQUAL((uint64_t) 4191893375ULL,   TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 4191893375ULL,   TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 4191893375ULL,   TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(), strlen(buffer.c_str()))));


  buffer = "코리아닷컴 메일알리미 서비스 중단안내 [안내] 개인정보취급방침 변경 안내 회사소개 | 광고안내 | 제휴안내 | 개인정보취급방침 | 청소년보호정책 | 스팸방지정책 | 사이버고객센터 | 약관안내 | 이메일 무단수집거부 | 서비스 전체보기";
  BOOST_CHECK_EQUAL((uint64_t) 4065546148ULL,   TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 4065546148ULL,   TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 4065546148ULL,   TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(), strlen(buffer.c_str()))));


  buffer = "中华网以中国的市场为核心，致力为当地用户提供流动增值服务、网上娱乐及互联网服务。本公司亦推出网上游戏，及透过其门户网站提供包罗万有的网上产品及服务。";
  BOOST_CHECK_EQUAL((uint64_t) 1577296531ULL,   TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 1577296531ULL,   TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 1577296531ULL,   TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(), strlen(buffer.c_str()))));


  buffer = "כפי שסופיה קופולה היטיבה לבטא בסרטה אבודים בטוקיו, בתי מלון יוקרתיים בערים גדולות אמנם מציעים אינספור פינוקים, אבל הם גם עלולים לגרום לנו להרגיש בודדים ואומללים מאי פעם. לעומת זאת, B&B, בתים פרטיים שבהם אפשר לישון ולאכול ארוחת בוקר, הם דרך נהדרת להכיר עיר אירופאית כמו מקומיים ולפגוש אנשים מרתקים מרחבי העולם. לטובת מי שנוסע לממלכה בחודשים הקרובים, הגרדיאן הבריטי קיבץ את עשרת ה-B&B המומלצים ביותר בלונדון. כל שנותר הוא לבחור, ולהזמין מראש";
  BOOST_CHECK_EQUAL((uint64_t) 3810256208ULL,   TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 3810256208ULL,   TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 3810256208ULL,   TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(), strlen(buffer.c_str()))));


  buffer = "بان يأسف لمقتل لاجئين سوريين بتركيا المرزوقي يندد بعنف الأمن التونسي تنديد بقتل الجيش السوري مصورا تلفزيونيا 14 قتيلا وعشرات الجرحى بانفجار بالصومال";
  BOOST_CHECK_EQUAL((uint64_t) 2844487215ULL,   TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 2844487215ULL,   TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 2844487215ULL,   TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(), strlen(buffer.c_str()))));
  

  buffer = "Голкипер мадридского «Реала» Икер Касильяс призвал своих партнеров сосредоточить все мысли на предстоящем дерби с «Атлетико»";
  BOOST_CHECK_EQUAL((uint64_t) 1905918845ULL,   TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 1905918845ULL,   TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 1905918845ULL,   TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(), strlen(buffer.c_str()))));


  buffer = "   ";
  BOOST_CHECK_EQUAL((uint64_t) 1824561399ULL,   TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 1824561399ULL,   TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 1824561399ULL,   TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(), strlen(buffer.c_str()))));
  

  buffer = "अ आ इ ई उ ऊ ऋ ॠ ऌ ॡ ए ऐ ओ औ क ख ग घ ङ च छ ज झ ञ ट ठ ड ढ ण त थ द ध न प फ ब भ म य र ल व श ष स ह";
  BOOST_CHECK_EQUAL((uint64_t) 3232493769ULL,   TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 3232493769ULL,   TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 3232493769ULL,   TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(), strlen(buffer.c_str()))));
  

  buffer = "tɜt kɐː mɔj ŋɨɜj siŋ za ɗew ɗɨɜk tɨɰ zɔ vɐː ɓiŋ ɗɐŋ vej ɲɜn fɜm vɐː kɨɜn. mɔj kɔn ŋɨɜj ɗeu ɗɨɜk tɐːw huɜ ɓɐːn cɔ li ci vɐː lɨɜŋ tɜm vɐː kɜn fɐːj ɗoj sɨ vɜj ɲɐw cɔŋ tiŋ ɓɐŋ hɨw.";
  BOOST_CHECK_EQUAL((uint64_t) 193365419ULL,   TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 193365419ULL,   TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 193365419ULL,   TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(), strlen(buffer.c_str()))));
  

  buffer = "äöüßÄÖÜ€µ";
  BOOST_CHECK_EQUAL((uint64_t) 1426740181ULL,   TRI_Crc32HashString(buffer.c_str()));
  BOOST_CHECK_EQUAL((uint64_t) 1426740181ULL,   TRI_Crc32HashPointer(buffer.c_str(), strlen(buffer.c_str())));
  BOOST_CHECK_EQUAL((uint64_t) 1426740181ULL,   TRI_FinalCrc32(TRI_BlockCrc32(TRI_InitialCrc32(), buffer.c_str(), strlen(buffer.c_str()))));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
