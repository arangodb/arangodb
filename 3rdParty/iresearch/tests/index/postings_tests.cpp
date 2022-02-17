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

#include "index/postings.hpp"
#include "store/store_utils.hpp"

#include <iostream>
#include <vector>
#include <set>
#include <memory>

using namespace iresearch;

namespace tests {
namespace detail {

bytes_ref to_bytes_ref(const std::string& s) {
  return bytes_ref(reinterpret_cast<const byte_type*>(s.c_str()), s.size());
}

} // detail

void insert_find_core(const std::vector<std::string>& src) {
  const size_t block_size = 32768;
  block_pool<byte_type, block_size> pool;
  block_pool<byte_type, block_size>::inserter writer(pool.begin());
  postings bh(writer);

  for (size_t i = 0; i < src.size(); ++i) {
    const std::string& s = src[i];
    const bytes_ref b = detail::to_bytes_ref(s);
    auto res = bh.emplace(b);
    ASSERT_NE(nullptr, res.first);
    ASSERT_TRUE(res.second);

    res = bh.emplace(b);
    ASSERT_NE(nullptr, res.first);
    ASSERT_FALSE(res.second);
  }

  ASSERT_EQ(src.size(), bh.size());
  ASSERT_FALSE(bh.empty());

  // insert long key
  {
    const std::string long_str(block_size - irs::bytes_io<size_t>::vsize(32767), 'c');
    auto res = bh.emplace(detail::to_bytes_ref(long_str));
    ASSERT_TRUE(res.second);
  }

  // insert long key
  {
    const std::string too_long_str(block_size, 'c');
    auto res = bh.emplace(detail::to_bytes_ref(too_long_str));
    ASSERT_TRUE(res.second);
  }
  
  // insert too long key
  {
    const std::string too_long_str(1 + block_size, 'c');
    auto res = bh.emplace(detail::to_bytes_ref(too_long_str));
    ASSERT_FALSE(res.second);
  }
}

} // tests

TEST(postings_tests, ctor) {
  const uint32_t block_size = 32768;
  block_pool<byte_type, block_size> pool;
  block_pool<byte_type, block_size>::inserter writer(pool.begin());
  postings bh(writer);
  ASSERT_TRUE(bh.empty());
  ASSERT_EQ(0, bh.size());
}

TEST(postings_tests, insert_find) {
  std::vector<std::string> data = {
    "rolEJjSxju8cbAa","PH65lKVFRBcDaZu","xBrSIkS6uCJkZzx","QcVj0vRPahp7KMWF","axqMCw0VDUOaDbk",
    "MWVyqKLiRhZ4uQC","vR6YLQBnPhk3RPL","V3yq1ucCfS70F2s","4bOXOCLCSG5LGLF","C23ynHQJAg6mLGN",
    "7uwbkgwmEShJL1j","cnwleZRgLr3xJhC","xgWbJxZqAILBlSE","jljmF7vUss0iD1e","PQ4q86bMEKeFn5z",
    "NRlpWVBImv174Gh","7kikVWvtfTsvEiN","h6YsibP5T7cxCEj","97EEzcZ09528x2z","XemAOyAs5VZc3kq",
    "TH7XGVuekbzLYWc","m4PzU2buUZxIQ3H","JVaaQa5Bano7KKi","sZWv5hrKEvI9bSe","V1lvlgOtx67w0mM",
    "rNJMUbOLazRCYVO","4emSc4OjGAfeDhX","A8OK9oIpaOyVcYg","mqaw5z0uAxl5cZT","QmogMgt2bakxu4I",
    "cNva9D9hwxcTE2n","4SOR5LCI08OiAgL","A20n4yjnHKbQ0fI","4C8nufAo4HwkS90","c8VE2f78PfPgWCQ",
    "3s447kLK5npMuvK","hyaaHIXaiNskxMa","rTL3WJj3WA5XVa9","y8gnHvCxoatWvbG","oDcZDYC7cfb06Sv",
    "QZ3bkD89gUA38qJ","5fwpaP72xxEM3ja","XFklmugWt3FBIiH","FODovG2E5jSXTGI","oKAgzwSgkqWwLqr",
    "oDIWSHYFRS1J1vW","VflrE0ssUcUiELQ","oPE4XvqkfJOkc6F","UcKsrYFRFAn7ngW","aELGflOPpRIQrvu",
    "z42S8CQ8jDrMyUU","5ahSJJlOjAQzNyD","0hL7CCU4yBe8965","mgRifEnoDu4nl4W","pXeFgr2SlqbbPO6",
    "3EsUF9XgBtn5Ye6","sBR6KrJ0Fha6WaV","zJvuhoIGOSEsG6H","vvwzAoVMXGJExWG","j5wvjcaQQWv5XzM",
    "N08m3zIe45KMOzJ","gfsiK692teE8Lpr","IsMt2QqXR15zNnw","tSDasEJWNs65B1E","fnb9gkxl3g9DLJp",
    "4tQrLW42wucGhjL","pJiZ8o9qPH0HWTl","K4pakVwEpRIuBhA","9OrIIPOc6URpiL3","3VHlT0Ke4tHrt4U",
    "BTb91ykMx4ilffj","p4tRGcJHjvYmUz8","eP7GVO278XRLYsg","LKeGXQUfepjTUzC","zfcWMlBQqFlJpwt",
    "s7wroI9GMomlAQV","ffLpVLAO2l12TXC","PlT7YbjvAniOx5B","9fn3NFUCNOwSNhp","bewV8rDVJsyKofg",
    "LkDbZhMF8hCDBkc","ggRfiUpyECZw9Qe","ng5BxAVZw3lLFKP","teEUk4H9PsDytp5","zWpOKy15TSEUVEu",
    "tM9iMXnlYuX8n23","RHVR6knT8A6CDHt","UEFUFeoMA9T02GK","QnVRpBAsL5UJOrR","E94oxIykDCn46F3",
    "AzFBybsTU2jMpSL","79sTvcsfV3Curw6","ODGrjtk5MCvEHoe","n9iRIHGG35FrUn3","bQnoJgjtFQVtkwQ",
    "3tmvYtofGUMvDlM","cs7Eyg6GBkTpoN8","AawWAUvr2FLXni0","x3eni0MpCNUxTAS","6xlDnUX89pZ7Q9T",
    "eK9Yg6hefsfqIVH","s6vxMxTC5y9aLHH","jNIKWvq81YPE81k","zv3S7XKjA0PEqOj","KH94ENzE4KrIbXJ",
    "oGh6TU9jWGYjepK","e4HJP42h1vSN5WL","58qZPYINvJ7sWEe","yiC1m4uooLJzsKP","z8TqIrbR9nTWRmo",
    "p9I9rrVNka4cuSL","WaTv7N1Y72mZjW4","60UyJ8YrDw0gywP","tSMMUin8opDi4pg","cq8HOfZ7Dcj6cbJ",
    "48Njtnvpf2WcEaV","nTwyCsElg3Mrr6o","BFUnQARm6YA1kOX","fSxLks7s3D2Op71","DcyK3UaIHOrK7yY",
    "Ow6PTZLVBHMp6lJ","pcY0AZOHkXsSnF5","9DrJkIDYXH82nyC","gJ8EnyejEe2pBsr","aywRuDoX2LOCkhU",
    "vkX86SukKO1NgNQ","SngzOg7iva6jKRm","ncEySn3xWVULcbc","eo44mqnwaYibYNA","U2jibUcasQj8RRA",
    "VTWY3P9NQVOclfI","BSvXivK7xzy3SDH","GgbwfoqWCWb9eic","2FxbQSXjHhogX3t","yQfvbKT1vAVWmXu",
    "GZOBJV1huYfWb2s","HYlTMCCeBY58b1y","pGGAnzScca2wVPw","XzPsW413lGNDUSX","40D7rADVHCg8Cwq",
    "NePA71FieGPoGsT","C7L5CIV5G9s60KP","Xu35kKCIcmZbH3g","HGZD9rykMqnqwlN","DYa4vkAR4Ae2Kpt",
    "YjrIOSenje8Y2Gf","z3aq5Wwk9eOP47x","0GMKakiE3rWQmcX","KjErJ5fmk4yp3lo","Vu2NF9bKnFi3Nfo",
    "RBt4lSnX7tUpxOo","9E5xe1uaibr4Loy","qfWJqOUMG76VwSb","51OHnqDHoJaueNN","4i4QbEgbnAY4To5",
    "c0EXiplap2Qu3AZ","5DXBx5yZ1CzySWg","BC9PiPsDDXtP49N","2ntRAGgCpSfyXzD","NUWZCSEm8hO2VVF",
    "lYzi9AnThDcwCNk","wsx7Jl12iSQET4F","kep04N5Q1KwEvN5","e1fgXimtNQxZI3b","SSL88XZwbkMoeBS",
    "iqWy6nWztTATrPb","6yuRNR0nVxe3fvf","JjiPYip6iNtXNvm","4ZihvwvFPAqz3Eo","9FV4Cwn8Mc5fFm9",
    "8TWJNhcw1OcxP73","f62yyGQ6w9DO8Fh","Ztn4x3IyH27TMgl","zVIFfjNxgjifEiT","CKehnOFTiAKgBFI",
    "rHpYMbKJVfPRH9T","GJVBKckqirVVRR9","VeFFEDcQ1YXUxA7","nHxcUiLTwtaHSZJ","i2a5EAB7k2QxbRb",
    "rKSAWsicKE50mmB","Kuf6x9JxJoAFCRQ","IA0ibBX4QLqCz5y","OOwFlLH03qretHE","beKGZqy3svr58MP",
    "5871YtHuJM1zSmK","UBs9rLccQzaoo9w","i18qioi7ghMgTh3","UOxtTnr0BsaWLrF","GsXSULTKR9xOYXE",
    "NwlT7AFBppVniZT","X76N6j4SDWKLJAk","jLhKSFrWQAXqNse","2RDYUmD9uR78QQO","rzmMkqjCAhvPAlQ",
    "uYAIft5HhW2l1UC","I6YpCqBRa7tsTAz","410EKVmzsAmSku0","prbnNurvwpv5VTW","nvan2Ynz2L2C6TW",
    "hgCzfz026OA5Utm","KWu4NiJgUBGvpt7","aYA7SpLcQJUO09V","E2YxqDmsnVVPgn9","MiCi1DWQXeEsonK",
    "bzm0xZMLf84CkUm","DxCeRGj4oLUqNe3","VZ1sf5k6TD8EaEx","OSvUoEsjASDIzzS","aNfp3W7RgvyWZqL",
    "LfZen1aw6nNR3Qn","mBMf5yrF4B8hYel","7bYAvSqt7VymKQJ","Ucvs2XMlLwoG985","8DC0EeFKz4wr9VB",
    "yPvgb4gR3Sya3Ss","Dj8hINvV1V6L65m","mttN2iCyCM9TMcN","oy7P15hwq4TmaoR","lgF8owVpEjvX1Xz",
    "S36gfK7N5cmuDGz","Oi7uWM6VKyzCHMn","biccxOpt2QTbWL9","YQXG44TZyZsTAGn","Y1IrgzayYCMog0X",
    "RckxuINCUSvVVtp","0KnxSTXk61T5DDC","Si05i70ZJGkmXc9","IytubsEqVfzOhEn","UKCvEaj14YKxbrA",
    "6uLeEHUkaLfA9Ce","ngnkpaCj5n6xvoe","egJW9srWpc7rkOq","JtNIATcEQqeaaeb","IfVWcE3BA4TlJyo",
    "UVgIKLnNXOWqou4","BXwGcZUmQmNSYCg","PyyAAxGZSIus8sc","NHUsupIZJeWx71J","lmGJOlpZBz5zL7m",
    "lylU8u8HWRV5YZ7","7XZCqjAjzKfkrEe","aMqHCIov9yrifi2","BgfZmM8M2rM1wEg","Jz4V7qQqVujehGV",
    "fn4vzT4fV9Gmbvo","lJ7ikcry5SCgzb8","iyvE6E4lIvjm3jJ","AOi4GqCptLeqMxI","FaxUP5f5AJn0u7c",
    "6hXjkk7LI5YiHa1","PcQcHNZt9eNKJGs","FJBCJUMVVPbpna7","kSpXcrnbz6AP3Ry","lyeqS3y11rWwrWl",
    "BAoYfimmInQgifg","ECHCoEQDxJJEzWf","5FHb6elrSBcTvI2","bRCDoKqvDcYCwhk","fLnWjknMEUP4XC2",
    "ou7Ee5jEOf0cSaw","sWa1k1WQpTjs0oG","9jYXZnZVGrDVPif","jkURWbNK9gaDUca","RPH9BtnDi1JV0tA",
    "Oj8ilj5Dmwm398J","l2vsGTP8ziHFVRj","yetgTsEPN0Ouho0","QJZDg0HIc3668nH","kUYbSDGhyv9Q4Cb",
    "Wtuq0cMOYWfOZEN","GAlqemEIQqkUoBy","YlqMpyWyOjKwlSj","kVIciNAX5rvBbDB","DDet1F0PCI0nc9a",
    "mM2nPX80quIAWU9","WXwnG3ctbcCexs2","EbCE3pAzTj5mASU","vmwYFtsMtOeWYI5","VfBLlEVE5khTtju",
    "ZBsNrf3NwLA5Uyc","0Q7RaXoOwYqHWfs","EboejWe92pnjjt3","U5kfEAK2jCWYnMu","9oxSDpU7e9RcrQV",
    "tAYPcARqtpYDIpM","XU506k9Q4kfTK8v","saVAzF5DGG9EZ3C","rUfvT0qszBOQGQ0","L5uumOx1XQw1kKG",
    "yhBDmrFg9GBV9yR","915UFunIDmYVouL","rYExofcTxCOhTcW","jED8jf3HSGgLxbm","VVJvx66OK4GAYIl",
    "nFu0HSZNmhryfHc","FqK198kT2OhEHlF","TC5X9FNqS2arFaO","t0f8qeNNKDPpc4O","46s9ZcHRCoYD5Np",
    "P8eUs8XkQVHa5pJ","YHNbxy6bfJQ9SxY","pvAQhFYSbWsTRqM","X6o33I0aprxZwKf","o4fCmVDXQhnUeH2",
    "wjBsYvpiuH0xMEq","rVKOlo4iM2C9y42","AvzR1YXBPRH9PAg","QtURrop3DL6J13S","rE8OljAYKUx7CgA",
    "3PRXjfmkBS3xKlc","jnS8jiIwTKM1efO","t8SgC28YriWAMmT","KFPGGx50UgHHsyg","QyAp3KT0bQFCV4I",
    "ViXHYBHfmZhylrO","OePxrwRbGHzfARQ","VuHeENCtCvM6RBx","Fnsu8VH0FILytt8","TZLCUpr020hsNoZ",
    "CDZ2jKGnSsB3RXs","iczWZqvr8Dv9jzI","51fmalqbngWGBMv","pAwGJWpMM3oNQzx","L7yj5ilq2ffnNzg",
    "u2GePhxcyNSN6yX","p5rnDmjzuXC9imo","03ucLsm7intihmB","wvIQnKBLJvSFA0s","PSu4U5nZjks0Mnj",
    "iOjHvHAG6v7mrCj","ub5FTbiJIFRRDHf","IWbQDY7UfBDvPxt","hT0T9nBbPV4496M","9vhcmgR9Da8oAwS",
    "8kBAUGDyro90FNv","u0XlytW5TRTjcSE","vw4jhi5F1KRvvVc","pmFQgXLBr1sjtEP","F86OJx7FywDYhbS",
    "VGwI5ApUNyuvvJX","55v3m8qjGeFgruX","GzRGixEjeyDK736","cg6xWGgH8IU2q1w","vlA3PRbV9qNsXFY",
    "xUf6IgVBfDDkl1q","yKtpfIklJVoBu2q","Qurf7PUwNnOqCE9","pDSNPpgWrcNy2XO","9e3pvYDoOnO8acb",
    "jquFriGUnBpiAiu","t6Jn3hzuKgQiMLm","LNH7tsaIrpxPLUi","j3R2GqQ2xiP7Mp3","71yENaDYH5Vc1Xs",
    "oub8RnD1Ah2BU2o","x5fHPtOiaAciIig","spHSEAZ9vKM5WIz","zsqkZujfK3zkm6k","qmzORZeD03k6x51",
    "wxtfyLni6rWo7sL","WvtKm5FRgW7fZVK","nAWYteAIsxDrgJn","gGkmaxVv14KyPNO","HPhGbjRTtCL3o3L",
    "K0yE1EboeZ4rncy","kaXfZ38H4JNxLV2","WhDo2N5QJTRfNwa","IDrTwfWUtjA9QAf","EkhsLcrYcbzO25Y",
    "DMrhOvfws6VTckQ","lyV2SpK1W0hayAi","CusejtcoibkDivO","YDXlF2GUvPl8VmT","YzNa8bjinMn7zmc",
    "OazFjXFLj0qM05p","z2iJrxl46xLEotK","QmbRovaRCWFmmTr","Jnk8xlA7PZS1HwW","wqAVvTDJRz1aCRR",
    "7ZSTVRntIjzIIuy","pOCpw1N2tgrV6n2","7WOoeMJBJn4w5mz","vfZvY2VgQNzcgFj","7gMkE092KRlU07a",
    "H0sWme34EvMIWYR","WXAz7TirCXrtRCX","KZUx7ImKLimHpqG","bPnDaRXCOZyzpkZ","WoDeCcRWHrCMtKa",
    "NIR4SH9ZD6JBTxW","SCvKabAG7Ou6U9f","YLIrP8r71flAuMZ","RRycJDCw0uJvffs","tU13uFSM7iKrt7V",
    "2KSuHaz0fynSpD5","U4ULR5z6EU4I6Ig","FX8MrENJFi46snM","Uvvl88GCKYD8usT","nyVq6MVOU8NURhh",
    "EmG5hHWKXyzRybi","ySclpSinbsMkZpM","T1m37NFHrlG45cJ","XO7NaVQC1mc7m8I","mxvijvJQv0ayxje",
    "jo3MXqRkIWFJuEP","TWQblhfQ1TU7a58","C3hvPkXhLhaI7Q8","3kRe3yoRKpoRtS3","8SCO87cAJ6EbRB0",
    "nJLTgWiiOXaPcE6","pGO5ynmLI3bA9WZ","2CUatutsqhXvhbg","Tjeq5wpZj5jbZU4","VfPz5CwI6cUx3TB",
    "OG5SvPWwBfJinI8","YUUt96MSGXqoJp6","v66y3YN3qiqCy05","QWO74aVaYlMDf0G","OfSBUyAisoD1OBH",
    "7WB1yjyyFyxJMQm","h8UwkSqRQ01ZYHb","psk7Y72AQuvLjcC","Dmx3c45pIrHlIn8","rI32zGknYl0PPUT",
    "mWDcWnKmMs7H4xp","XrJapRpk58f0Q4E","YWqGjxyJSlojlf7","gkkMPwgMtUX3wAB","JvI3S0y0ufT0ZKM",
    "6AwZ6OiyDViSqmz","AZ0EyefAzHM3ca9","z0xMj62YTlyw4fx","jYXTm4TiSxqnleD","BtTswhmNWhQpITf",
    "D6Utl8wqNMgb26b","AbiB6KU7GoGoyW2","rANjYkUl7AXhEca","xtFLIPLjguQ9tNX","bO0KvKmsg3BqzPZ",
    "vMzvQwDRm59eFFN","C65Mft4LjAyeNYm","FXmRyIhfT5Z9KiD","Wph8hh30qMR22uO","D8qqsRPEzgAceZl",
    "yhXNshZKShm8BOj","CR5LCuRNznlKWoI","M0KQD7kniJQP2gY","O2OGpk6mf8M01SU","6zULmHAtxj2LLaQ",
    "vZ4OboWuO42nyZU","CK8MPkeLRWwjkr2","y2o7MzfW4PXm9oM","uhfKPsjqVqulpnV","LXUosTsA7k10NL3",
    "XqSIcyZ8xUeO4go","AopKNOjzjsfqKYH","uDxncGIKhVN8Q5i","Zmxp3j0GEceb0b4","YeTmzTwzX3tjYtX",
    "fAv3FnJymVX9aQ4","0LMlzBnj4FQaXGV","DzosZlgANaD9Y0u","6LB4xZ1jogj000O","cSMyr3QKRw6DLCk",
    "OQp6z3jgEhLYpfK","th3LckyQKMQrKPC","IoVY9zfUwor8Ma9","cDrNLSVnXvTnBCs","7O9gPrUPBEwgKA3",
    "IoIQJDMYelHkZy7","pDXJYMyetOsZJcG","a4SfyDcAjRo3qGs","5XlBCzUmQDwFkUA","rrZl7juO7mE6FaB",
    "0DF3Ugi3DD8TUqj","yDRlCuuC5gSpsUq","neNy2qhmnKoeJR2","LQughgOK8XxvjAN","xHkySaIEHICHJc0",
    "f4C8FfzD2MiiXry","PqRyvYjqHY8OPuL","bDpWg3AaSrbrRbt","Mfc79P1Rony4zEI","1lmAF9v5KNyMUDL",
    "gtRY6ioGRkcjBcg","4XZDul58sZ4rMLb","DNyUyLAawJ2qVMR","hFhBGzihY0mAyMQ","oqJrgMGnBS6bzRx",
    "uLkypMtWxFcSsbY","JvD0AthMvCIDxW9","KivuxlDGtFFuO3G","2NOeUZSKokHyi3j","AGMmBbLwgkmPvNx",
    "ZKO3LrfJITkjHv8","uQ2PjgFQnDX3ATw","cSFWw9BZQyANY12","Q7eAxl9lo9QeL3q","sQ3NJLBDckl7t84",
    "AZg2ywjxXxXZS76","C6sAYqCUbPZjxMn","qaxJKzpoLXnCxRt","PBaGkCt8ZfzyYtl","hN86GIFZm7oFHIO","8Yjelxgoas4KQxT","EwFhYqJIaDrPGcx","VOU0zHWPFvixGZi","PbtV6pGOHftaHF7","g5VMrBJWvy3Nz9q","6L6VSKJG1vXA62I","X1H57ckAu5Whor5","qY6XQtkJgPbnTgh","yBuE1IshC3VexoA","RNoOsIJ66Qs542J","ujDheJc2GUqvvYA","hroOnpq0YeilwQt","Ro3ubrjxTE9g02o","lORM1F79cYmiWK8","We7lbSp3IAcKlgV","9xNmg9D4ahwvUkK","nx7hLFTcvtyjyOb","uJzCuXJEBnv2MRu","yu4W07aJxGbgQRW","RTk1XCgSZ1PHkoH","BHPJHx5NtPYkuXT","OgCw1ppCZvKtK99","8pjBV2klNeeRapV","fXSIvQasv6hYRZL","IKfH09jS8xOcVSD","PbebHxY1z6CRwUB","b1XMsYt3DXYLUHI","9cVrnTN6AQymFLD","N4vJBoAvVoFO3A5","AotIhK8sfvI8Xco","Z9h4Qi7auMGkano","vtkfs8v6h3QlpD0","3FexYqDFoNvpxTq","CC5rUm1j9oRmfXs","1GrvO9wOCO8Lvb5","0S2NzEb8WUhSV4v","qWv5DZtPNaRfOMn","1e1mhFxrcMsQw7x","H8pWskMskfMYIrn","Eojz6FogULu7FbU","FvDEDfgHaIGaSjc","NEswRk4gzZAH7hZ","yvPFysq1b7Euxnx","n1DPEQbT6tOsPzF","1oeNrGK1j1RVoMz","erqplBfWvyvm3rG",
    "gJ4hhnJp134x3Ag","pbSxhmqk3nfJGqE","hCNyIUgNkeW4V4k","tTYtHuGiGuhCFn2","q3iJUCB8bl1Tzss",
    "SiFjoDEfevlGE2E","zEhtu94XO27xkI1","GTJkzUM1mxk707a","OHfLFT8FQHkED6H","OczBWBgY2HNgtfu",
    "k0JFULXALDYNSYf","3ucR8NcFaZCyus7","lCgsaayk9kGKT5F","TvGZcsQwi0OKwTE","m5iD6kaxs1vo5rZ",
    "sFik9cyn4n3JaXp","RWLNU5TkOqBNl9s","1gluwmu4xfnzAQo","kOkuQQnV5T0IzXB","UwL1nl1PXUZTwMv",
    "bStsBFYNpMv1Fcr","XupNP58OjXt7XCP","72Z0beDOMPVRSPY","Jgkayfb6VzWImlP","knmucORgjVpPhgz","zjhZQ36qrkA7PhJ","5HhuYXxOviPhFXE","hYCEueoPDNQVQHB","Coe6A1h4JaeA4Hx","cj2boAGVzcRAQcQ","Cw1t7SclBhWvgoj","WRL4jBQtRc8Fa2W","EnioDt7QDOzEsHS","EEvrDefTkMD0PLs","X5KiA9J4au7WKNV","trG56maTiFHDI4x","Q2eCla4cut797Rn","5DojgcPYah4xfNU","s8FGMF2JPFTIPcn","XDWSVgZByHVNNCs","1BW5fneVvjOTkax","Wzy0XZaCUwonHlz","kZLTbVV2tjHOloM","xZZQjU2J3ct4lN9","KkcwZVaFbGk1MGi","xeCJaFEA8VUxInI","WSsNATOzsUXRXn3","12XnRjUVjDrtoT6","sRLEqGQ4qpcR85k","w2w50tx5w9PUVlb","AQVy7ZMofO3qc5L","HDPBg4YFiOFaxSi","4kvM9r4PX2DwfLv","7WeHk1xxWlWSC32","aIqe9Sg9HGtC0nU","vb0nqcmOIN9xDPJ","JQDJmHxA3yU9PUU","VR2HUiyL7Ia7vTj","Lzkqu7vbcVPniBq","NJvHtgipH6hJLNf","OpogU1BGm9T7mw4","740APSUGr6c3gxR","Z6ICCGOgatBGP1X","H4BsIqNInT77q85","fIHvFjy1YIcFAhO","gIoxb22ikDl2iYB","L8MXRAve5WHSRxK","0YIkOI4k1iAmu8I","wBJFCyIQiDWDUgV","ns3545hohRR40lc","qTzzkS7MlW8cujQ","z2sM3wrEx100LSF","bWu3KlA7zaIhBp9","u4PsqvGRmw1QKiL","1BhyO2vMrvuwo6L","Bkoqo4gH6Y0nwMJ","zp2ANeJuyaokQQn","2WuUYvvng3Ic2Hp","BybNo1ezx96LUGV","Ug7YlsRO7U4RlPM","6mbf4BYyx8TWA4Q","87nAGNFNPMca4U1","JjI79rbhuwsrYKT","3bnuRb3DuoLUfST","jjJ4lv5L7kitcjW","wOPfeEGMulHbKNq","wCU93YkhNEKD4fj","IpqBbI9YGit4YGK","wLr8RXGOTyWLswt","pT1n9c2MnJUfHDy","cDevx6oZsXl3lSo","197jJlXlBGtc7wJ","Ljn9v4iuh9qsxLR","klU2gT7q3N2R6yD","URL46HI7PQQjeL2","LgGOMPL2xyDoCNQ","0Qb7wTFnPptCxa0","S13nYiTUjmj3N7J","BtSqvXFYn7T8reU","zVpqoVYDLy7Bxev","j8ETKeUC9BbVPNw","cxaZX8Z0t3mh6rY","PfrtKy9yfCR6neg","zDYVfkHlAogA4ch","giG8lhctY4clh0g","4at9ovvCbcxtFDO","SlqFLUWWtEn37M2","1LRjjzotuN5iijI","ff9WoopmPA1e9v5","fGDPg5GvcWi1tq1","kSpaYgkK1GZbKtG","p6uFbuSXYcvuOKB","S5Nr0fXftJHMfex","oIxySycsEQmwb2B","5lvPkrt1R2i8wk4","mkxUk94aAPb5Fj5","vro8V9o2EwYIuCx","yuTpBLqciI96su0","2jlzKD2HR3gVB9J","f91czEl1P6h3DsR","pYngrraa8AzJmKN","TT2XCVYbscxfIhf","I1U5PenqPBgqtBk","snlj0UcHSiY91RF","7NHxsT3IUuuqQNA","pzGu5L1zbe4FxMk","TGK9gayHMmzUiIQ","2mSN8mobWFXcZt3","cGsgqKssXX442mH","MviTCmlzGhAE3Zb","j2tG9gWAj6QOzHi","8UrihuOj7zb3FEZ","hcl4VEDaWK4D3au","h69rm0YZbKMrmux","W5Rxjyv0ZWqN9M9","u65D37FSahxv9ln","7XKETwwrUOHF6cO","LJwtlee0VAmiF0y","xomDTwV02Vak6ZJ","plFmuVwODC0pQhc","hDlTmSfmeOeOEto","pk7R78ymVn1aQOJ","ETk3s5Gk1ALuIGn","TLElDq6JWC3Zhth","re3eQTpt8so0TjZ","oUKQ8bARUOC0WMb","Qs39CEOF44CII9a","PJ9y95AgZkHefun","8rLkEWaOGuy8wVJ","h2iYM1KemxFfNZu","iFVFRBD31qinh9G","xDlJ8VfYFnDcP7R","S73QLn9eFhWBgnr","wgfuGVZELO1ogNn","iVsJNY6Ibhu394p","CtsNjRgvQTrqJCS","Lv06cincRhEFxto","HqQW60YLIJfoLoK","A6jACNNKyNKYbA0","vAz651MgF0ebjkN","RuvOAxLh3rwUKs5","hE1yCpTQUH3Tl8J","MTqfOfZuu8BvqDj","W16t7ou6QGegk4y","GDJv3oLW1CnIxkO","9gAJnS5vRYvVEuw","jWyHySO7Gz6ItDD","Cn9nVV8XV5BNMzt","BcDQSWVrTbRvtQB","DliPPUXlJ8vZThE","poRgEvv2mAQsIJo","6HzLZB3HmvoIM8k","EjucrOhqknzIOvr","6KvoyLwoY7hXvpo","1yhzOSl9kPMk6Mq","6wh21uzherzDnmi","3tHjGxtmTBFfgz2","mhl8euC07asuok1","E9jUXhUcI9R8K4O","VS7oUWHNjStjF7n","7UEAK1cBvFxM50v","JhJMOFZGK0oelwR","ugzskkEQofrc6jx","XsbYB4ybR2HO8EA","ElNy6vNaRfCLETu","STPxabCmD9ECjxD","l1A2HXL9eDwiclP","eBaASWVNFKkUICD","JstPCA5Wjv03JwO","kPsL2WniSs98MbZ","FkZZv2br8BbYQYN","UyRfMiEuNKNnMVn","KBmbupyL5Z5Q5Je","7wAZpPlAh8cGUSP","8w5rDS9b31VvrnK","VwkLyzp3Z4oTaoL","8VwSjta7jlYgZkZ","sB0Kmjk8Sw6XQDf","IlTvzYBDgtsT2mt","tPpwMVIDqUpljBF","A8BYV0FPEXHnRpC","fDFSmIKBetGfZ7I","D43nlniYm7pPSL0","nqt86rJFXn8gVLn","IVbzVQmQVUaDK27","qyhDRCartZW6MLw","Y4O4FlStmTp3R3q","OnvYCOV6Eo1oBag","RvpywFjjpsYLeFu","bfKmfIGphjgKcpr","Vsua7pP0BAFeOk6","k0w69pYjXaC6CvR","xu7oksQxv1R3vyl","2lf6YWZ3SxabSV2","9reWERJGJq9KKG8","TFLCet9t0wox4TP","G8lr5Bqs20mkywn","we5v13U4JhmV0KV","weZ5FWLOMMGj4fM","NhUK0nF8WCykvRj","kFNj51LTHOzOAnl","8SVDV2yMN93yusn","EqyrruGCXSJiSD4","q70c8gVBYvXprke","EtjAiOKiI4cJPul","yJSeQEicRzC5sg6","rhoSkWlbrSjnrOr","FKWcxPPoEoyUvbK","h3AaxxwTm4MoOwO","MPe81waFzZUTyaL","OVY7N19vuPQ4KD8","LQr7YselMHEDejp","D4EYZ9gcNKc2yYH","JLKJHMitHUn1Hmj","H2pe4VMrSWtLWmx","oRv89uy1M1JzUos","O1Ble1UmzhvAnp7","zw6yDl3AxWAfNcC","URv0Czf9GLMtynu","pmOVagLkWTYvKpO","tXKzzHszCshHfuf","eYVyYOGsXhYBIRT","prOD3ZVZZx4rLjG","mnrcLt5fZtX5gh5","jD7h0ncHxIMhhko","M3SgZ4G2WHCfvXL","0nVyn4egrNTWUQj","aYWsn3DG5Q2zS6v","UXx4lY8xpuqRPq5","E4I6wlQJbHNbbWv","6f6msWM3leupvgF","kuSsjc8na6cNbUD","poaXyabYSvCKMzC","834a0fgsOHpB5qE","MeX97chChPQ9b7D","rOQEUm1S97HWPpR","iKNRcvvQtzD85Wc","PL8QqFO4VIsB7gq","T7jtJn56zXiLUeH","6pQKrVSco1gfNPJ","uVDXoGtDjqkQH4N","TIbVmws3qT8cV4E","oQOy8wU9CL4b5Qr","bLb3UczyESYSxKY","Awm6XK2o5VhIKmn","VTtOfCSP8p4aQR7","G2wXO8lmzyVSj9B","YkXoIryvlBzh0pX","YJZSr0H7RsQHA0k","toAJFfwsBDNaHyS","VNePXnamr5yH4mx","1quiBahp3Cgfu3n","AKq2XFzqrNZgQcg","V9lqutzQ1hTkebp","FJVKWcLEP7aJ6Qh","KNwaO6N7pSTkq2Y","8go2wy0UvxnpC9N","coFHLcRg5iw1XrF","g7ktE61nDEUMR7Z","YesqnAa9g821XQC","905CmktoojT7uv6","b3A5EOH63yIHhpz","c411eD98XrPZPWj","8yI1Uk0D45TaJ3W","M2kg12fKFRRMHe5","CNxIPG6LCheuEj8","1MEJwTsL10UTlfa","KrG8yTh0YK5mMGn","nfNUaNHJINKt2JB","zZcH4zrxZxafBTQ","NB0VriPp51pVwZZ","jKHsOxGboD6GN8W","SY9reGPMsYsERSF","TsTXNjoVPh4AR75","M6lo59ajlE9AEBb","Nhz2SKb9f2Rijvu","qPYLCqXNXXQKKph","HZmL8rQ0l1UFwGK","AFMXbGv77ipsznE","aWeRY57BfwRryQl","4ebol8i6LC1ow6Q","iYVUiWB1qY6kUaM","6z0FIFjjfGI8L1z","o9ruAor6YHfvlUw","lbsYFU40yApIVt2","jsNmZx0U0pRMInq","ufGRaaWTTmnYfw1","9hehR0yXLrArGCs","WRMq2lfW4zfYALV","a262SXIucUBjaqF","HlZRxEhQfyFTQfw","GR6Uxoaicnui7O7","SFeyp4GzfpT6NUT","tV65aYGXp9maWbE","DRiFgcr4Iy5BD9Q","vKegQSHMwBQkNPt","piSP6waVfxJbrXU","8SK22h1vuYXYLei","YqVicUknJHz1JaS","ixlT1fwcknraPnC","PeNxaHEWsUMOQUK","SjtznRs6r9ytIV2","99oartTfzxvojJM","u8IkHbK1SRnH6cT","sV4qcO9yA2egeRy","U5jBh7AOqhoVirD","pgM6gQf8H5SQsra","CHO8aqIBxxi4BBC","vogaWVjQv0M5er3","qsiLwygYG7S3IhO","McEfWQDzokph9sj","ysuB6lypfIy2vXj","LKStnwlDGwraW4T","E9wHFk0Dhhv9NER","eys4qz6Dn66EHS7","JiFBnHn3X9XRXkW","reOh8xHj1vyNFKn","COmMFi67wofmvF1","Ftit5ZLhWhWgtMz","5x6I4K7ML8Pq4xa","rLFtaXjSq9bAXc7","zt10h0R5le7e4pW","hLL3pADGXJMq8iv","tKhCgK3XAe5v1Ur","VPZ3KTep9twMmOV","uSFRlb1yU14KrqK","Phmn7nzDw6oBms7","IDFHJH5IRZqTfyn","XyGSNfvKW2WzFm6","U4WhhW5B95ztt04","7MiUkVGUITg7aQt","TxksYSeztr8gHZX","gaBKxGjsuMTqjOK","oINsfMvr5az8eUq","cRcE4NUMzqASfm9","ZoU8iDHEAnbQavr","8w4F947AzIwmGhi","a6EKCSXx1E90NRw","yc6XbYObuU57930","5ORM7gGEOKcPCnm","YHeYaXWfzN4GSWR","u9scxgQ0jxb55uu","rZNMvr87mT6aDGm","t657s6QO5Zs6Af9","oUKJhF8JnwhL3Au","snSnZ7tUONuW1Pw","ilUrN89PgcLEgPR","8pqmZqEeT46k8vb","qt4KWSUMMESi3ZF","9O1t7FntaDoEXno","5ND2Wo7yy9PoWRo","hx6lsIrbBzIQEjq","PAhtCBroTVXMOab","wkmCgkCmhyPxy4E","AJ5vC73MMJacspI","hXJ27YLe3X3St3p","4x4iWNhiRyinQzo","7BGajc3Feg1gouf","0F5ZhT2ZoSlOH1x","IgIDTpILxDro5KD","FNy60QXkRRIBDf0","FMVZpv6jFwmCyo0","LnlXwT88JxxY1V7","oSEhGRM1ZXO78Bt","hX5R7lwDHKHTD2P","vk7AkI61aTjFXpF","tkBw1COaF1xsMiI","GGWiASNLsaJfw8a","wonjxjcBKswUwyk","2MBYYAa72zL70pt","BVO2yTHqAh6Dt9X","QbuDDq9p4JiQMDi","tWmR76IB490CxYn","Vg7yNHPuL0MsefO","BCLBAjeYs8hbj9D","I7DEWy2YHTpWvZf","3akgOKGFX4bebTR","l278noA6JDFJYiA","W75uoE8AmEJUzSX","0BpAPEcvNF4AUW0","jTRFN6uK1oWZLU4","Yv5y4LpqP7UZysp","qzOs9MO4LDFIbxF","vTBZazPgzD0a384","I7SLPuRJom7SXCC","2VmRfa7KojmGupV","qTzrQKXoPnNfHGG","T83QGGgHqP3sL71","WnQxuJsVMqUa4ll","wHJVAPjlzUsIbVJ","7Pjrwv1PIUniIVv","MijWTBhNFTePM3G","QW2qJYkiqmszFOX","Si1BZw1bWg0rxX3","6tyjzs5CliGvTYU","qZ0Bh1trHD3aO3P","NP65ZIsPSZKJXE6","nNTSmH3kpbFnjnf","vulNOsUEPOvsyru","SYWTOS8eS1Zas8q","reTZLpT9HE7pR2x","XNSBDR9Tz9OlGfk","pxQrajPOrWRtXEl","QoglN237jCK16rr","YsF4wV5nKEKBU7j","YtvlJN79I1rMKil","2EHw7VizAAD5j0A","BrWJgjP7QAxJDwl","XuCOHjnEEMDq27Q","ntUAGYV4OI6chis","PeGBk1C6XSAv1KV","eyQ7eFQtmFBXRqh","zLGZ8o78xCxzxtj","PIL65MKAtWXZCEw","HUtFPNPlqMFzgxG","yflSLDgvqUAgz7K","AL3D5pB73kJYrzv","9hbmpDDsJZPvGEF","QJ4bn3QTV5W8ti0","4T3KwmPeMFVM0RO","RNlDqtPu8Ff8XxP","7HO7yE4L8l5fkFR","WU1201h53JUO6bg","2jXKsvme5ues6lu","YkGWFABJjeuDlTN","r83PmsOk6Bgjacm","Wg2VknFWhqW825E","735N2j6115tXKMh","TmrlCl31PJjmyVB" };
  tests::insert_find_core(data);
}

TEST(postings_tests, clear) {
  const uint32_t block_size = 32768;
  block_pool<byte_type, block_size> pool;
  block_pool<byte_type, block_size>::inserter writer(pool.begin());
  postings bh(writer);
  ASSERT_TRUE(bh.empty());
  ASSERT_EQ(0, bh.size());

  auto res = bh.emplace(tests::detail::to_bytes_ref("string"));
  ASSERT_FALSE(bh.empty());
  ASSERT_EQ(1, bh.size());

  bh.clear();
  ASSERT_TRUE(bh.empty());
  ASSERT_EQ(0, bh.size());
}

TEST(postings_tests, slice_alignment) {
  const uint32_t block_size = 32768;
  block_pool<byte_type, block_size> pool;
  std::vector<const posting*> sorted_postings;

  // add initial block
  pool.alloc_buffer();

  // read_write on new block pool
  {
    // seek to the 1 item before the end of the first block
    auto begin = pool.seek(block_size - 1); // begin of the slice chain
    block_pool<byte_type, block_size>::inserter writer(begin);
    postings bh(writer);

    ASSERT_TRUE(bh.empty());
    ASSERT_EQ(0, bh.size());

    auto res = bh.emplace(tests::detail::to_bytes_ref("string0"));
    ASSERT_FALSE(bh.empty());
    ASSERT_EQ(1, bh.size());
    bh.get_sorted_postings(sorted_postings);
    ASSERT_EQ(1, sorted_postings.size());
    ASSERT_EQ(tests::detail::to_bytes_ref("string0"), (*sorted_postings.begin())->term);
  }

  // read_write on reused pool
  {
    // seek to the 1 item before the end of the first block
    auto begin = pool.seek(block_size - 1); // begin of the slice chain
    block_pool<byte_type, block_size>::inserter writer(begin);
    postings bh(writer);

    ASSERT_TRUE(bh.empty());
    ASSERT_EQ(0, bh.size());

    auto res = bh.emplace(tests::detail::to_bytes_ref("string1"));
    ASSERT_FALSE(bh.empty());
    ASSERT_EQ(1, bh.size());
    bh.get_sorted_postings(sorted_postings);
    ASSERT_EQ(1, sorted_postings.size());
    ASSERT_EQ(tests::detail::to_bytes_ref("string1"), (*sorted_postings.begin())->term);
  }
}
