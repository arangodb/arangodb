////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <ostream>
#include <string>

#include "tests-common.h"

static unsigned char LocalBuffer[4096];

TEST(DumperTest, CreateWithoutOptions) {
  ASSERT_VELOCYPACK_EXCEPTION(new Dumper(nullptr), Exception::InternalError);

  std::string result;
  StringSink sink(&result);
  ASSERT_VELOCYPACK_EXCEPTION(new Dumper(&sink, nullptr),
                              Exception::InternalError);

  ASSERT_VELOCYPACK_EXCEPTION(new Dumper(nullptr, nullptr),
                              Exception::InternalError);
}

TEST(DumperTest, InvokeOnSlice) {
  LocalBuffer[0] = 0x18;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("null"), buffer);
}

TEST(DumperTest, InvokeOnSlicePointer) {
  LocalBuffer[0] = 0x18;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(&slice);
  ASSERT_EQ(std::string("null"), buffer);
}

TEST(SinkTest, CharBufferAppenders) {
  Buffer<char> buffer;
  CharBufferSink sink(&buffer);
  sink.push_back('1');
  ASSERT_EQ(1UL, buffer.length());
  ASSERT_EQ(0, memcmp("1", buffer.data(), buffer.length()));

  sink.append(std::string("abcdef"));
  ASSERT_EQ(7UL, buffer.length());
  ASSERT_EQ(0, memcmp("1abcdef", buffer.data(), buffer.length()));

  sink.append("foobar", strlen("foobar"));
  ASSERT_EQ(13UL, buffer.length());
  ASSERT_EQ(0, memcmp("1abcdeffoobar", buffer.data(), buffer.length()));

  sink.append("quetzalcoatl");
  ASSERT_EQ(25UL, buffer.length());
  ASSERT_EQ(
      0, memcmp("1abcdeffoobarquetzalcoatl", buffer.data(), buffer.length()));

  sink.push_back('*');
  ASSERT_EQ(26UL, buffer.length());
  ASSERT_EQ(
      0, memcmp("1abcdeffoobarquetzalcoatl*", buffer.data(), buffer.length()));
}

TEST(SinkTest, StringAppenders) {
  std::string buffer;
  StringSink sink(&buffer);
  sink.push_back('1');
  ASSERT_EQ("1", buffer);

  sink.append(std::string("abcdef"));
  ASSERT_EQ("1abcdef", buffer);

  sink.append("foobar", strlen("foobar"));
  ASSERT_EQ("1abcdeffoobar", buffer);

  sink.append("quetzalcoatl");
  ASSERT_EQ("1abcdeffoobarquetzalcoatl", buffer);

  sink.push_back('*');
  ASSERT_EQ("1abcdeffoobarquetzalcoatl*", buffer);
}

TEST(SinkTest, OStreamAppenders) {
  std::ostringstream result;

  StringStreamSink sink(&result);
  sink.push_back('1');
  ASSERT_EQ("1", result.str());

  sink.append(std::string("abcdef"));
  ASSERT_EQ("1abcdef", result.str());

  sink.append("foobar", strlen("foobar"));
  ASSERT_EQ("1abcdeffoobar", result.str());

  sink.append("quetzalcoatl");
  ASSERT_EQ("1abcdeffoobarquetzalcoatl", result.str());

  sink.push_back('*');
  ASSERT_EQ("1abcdeffoobarquetzalcoatl*", result.str());
}

TEST(OutStreamTest, StringifyComplexObject) {
  std::string const value(
      "{\"foo\":\"bar\",\"baz\":[1,2,3,[4]],\"bark\":[{\"troet\\nmann\":1,"
      "\"mötör\":[2,3.4,-42.5,true,false,null,\"some\\nstring\"]}]}");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  std::ostringstream result;
  result << s;

  ASSERT_EQ("[Slice object (0x0b), byteSize: 107]", result.str());

  Options dumperOptions;
  dumperOptions.prettyPrint = true;
  std::string prettyResult = Dumper::toString(s, &dumperOptions);
  ASSERT_EQ(std::string(
        "{\n  \"bark\" : [\n    {\n      \"mötör\" : [\n        2,\n        "
        "3.4,\n        -42.5,\n        true,\n        false,\n        null,\n"
        "        \"some\\nstring\"\n      ],\n      \"troet\\nmann\" : 1\n    "
        "}\n  ],"
        "\n  \"baz\" : [\n    1,\n    2,\n    3,\n    [\n      4\n    ]\n  ],"
        "\n  \"foo\" : \"bar\"\n}"), prettyResult);
}

TEST(PrettyDumperTest, SimpleObject) {
  std::string const value("{\"foo\":\"bar\"}");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  std::ostringstream result;
  result << s;

  ASSERT_EQ("[Slice object (0x14), byteSize: 11]", result.str());

  Options dumperOptions;
  dumperOptions.prettyPrint = true;
  std::string prettyResult = Dumper::toString(s, &dumperOptions);
  ASSERT_EQ(std::string("{\n  \"foo\" : \"bar\"\n}"), prettyResult);
}

TEST(PrettyDumperTest, ComplexObject) {
  std::string const value(
      "{\"foo\":\"bar\",\"baz\":[1,2,3,[4]],\"bark\":[{\"troet\\nmann\":1,"
      "\"mötör\":[2,3.4,-42.5,true,false,null,\"some\\nstring\"]}]}");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Options dumperOptions;
  dumperOptions.prettyPrint = true;
  std::string result = Dumper::toString(s, &dumperOptions);
  ASSERT_EQ(std::string("{\n  \"bark\" : [\n    {\n      \"mötör\" : [\n"
        "        2,\n        3.4,\n        -42.5,\n        true,\n        "
        "false,\n        null,\n        \"some\\nstring\"\n      ],\n      "
        "\"troet\\nmann\" : 1\n    }\n  ],\n  \"baz\" : [\n    1,\n    "
        "2,\n    3,\n    [\n      4\n    ]\n  ],\n  \"foo\" : \"bar\"\n}"),
      result);
}

TEST(PrettyDumperTest, ComplexObjectSingleLine) {
  std::string const value(
      "{\"foo\":\"bar\",\"baz\":[1,2,3,[4]],\"bark\":[{\"troet\\nmann\":1,"
      "\"mötör\":[2,3.4,-42.5,true,false,null,\"some\\nstring\"]}]}");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Options dumperOptions;
  dumperOptions.singleLinePrettyPrint = true;
  std::string result = Dumper::toString(s, &dumperOptions);
  ASSERT_EQ(std::string("{\"bark\": [{\"mötör\": [2, 3.4, -42.5, true, false, "
		  "null, \"some\\nstring\"], \"troet\\nmann\": 1}], \"baz\": [1, 2, 3,"
		  " [4]], \"foo\": \"bar\"}"),
      result);
}

TEST(StreamDumperTest, SimpleObject) {
  std::string const value("{\"foo\":\"bar\"}");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Options options;
  options.prettyPrint = true;
  std::ostringstream result;
  StringStreamSink sink(&result);
  Dumper dumper(&sink, &options);
  dumper.dump(s);
  ASSERT_EQ(std::string("{\n  \"foo\" : \"bar\"\n}"), result.str());
}

TEST(StreamDumperTest, UseStringStreamTypedef) {
  std::string const value("{\"foo\":\"bar\"}");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Options options;
  options.prettyPrint = true;
  std::ostringstream result;
  StringStreamSink sink(&result);
  Dumper dumper(&sink, &options);
  dumper.dump(s);
  ASSERT_EQ(std::string("{\n  \"foo\" : \"bar\"\n}"), result.str());
}

TEST(StreamDumperTest, DumpAttributesInIndexOrder) {
  std::string const value(
      "{\"foo\":\"bar\",\"baz\":[1,2,3,[4]],\"bark\":[{\"troet\\nmann\":1,"
      "\"mötör\":[2,3.4,-42.5,true,false,null,\"some\\nstring\"]}]}");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Options dumperOptions;
  dumperOptions.dumpAttributesInIndexOrder = true;
  dumperOptions.prettyPrint = false;
  std::ostringstream result;
  StringStreamSink sink(&result);
  Dumper dumper(&sink, &dumperOptions);
  dumper.dump(s);
  ASSERT_EQ(std::string("{\"bark\":[{\"m\xC3\xB6t\xC3\xB6r\":[2,3.4,-42.5,true,"
        "false,null,\"some\\nstring\"],\"troet\\nmann\":1}],\"baz\":[1,2,3,[4]],\"foo\":\"bar\"}"),
        result.str());
}

TEST(StreamDumperTest, DontDumpAttributesInIndexOrder) {
  std::string const value(
      "{\"foo\":\"bar\",\"baz\":[1,2,3,[4]],\"bark\":[{\"troet\\nmann\":1,"
      "\"mötör\":[2,3.4,-42.5,true,false,null,\"some\\nstring\"]}]}");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Options dumperOptions;
  dumperOptions.dumpAttributesInIndexOrder = false;
  dumperOptions.prettyPrint = false;
  std::ostringstream result;
  StringStreamSink sink(&result);
  Dumper dumper(&sink, &dumperOptions);
  dumper.dump(s);
  ASSERT_EQ(std::string("{\"foo\":\"bar\",\"baz\":[1,2,3,[4]],\"bark\":[{\"troet\\nmann\":1,"
            "\"m\xC3\xB6t\xC3\xB6r\":[2,3.4,-42.5,true,false,null,\"some\\nstring\"]}]}"),
            result.str());
}

TEST(StreamDumperTest, ComplexObject) {
  std::string const value(
      "{\"foo\":\"bar\",\"baz\":[1,2,3,[4]],\"bark\":[{\"troet\\nmann\":1,"
      "\"mötör\":[2,3.4,-42.5,true,false,null,\"some\\nstring\"]}]}");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Options dumperOptions;
  dumperOptions.prettyPrint = true;
  std::ostringstream result;
  StringStreamSink sink(&result);
  Dumper dumper(&sink, &dumperOptions);
  dumper.dump(s);
  ASSERT_EQ(std::string("{\n  \"bark\" : [\n    {\n      \"m\xC3\xB6t\xC3\xB6r"
        "\" : [\n        2,\n        3.4,\n        -42.5,\n        true,"
        "\n        false,\n        null,\n        \"some\\nstring\"\n      ],"
        "\n      \"troet\\nmann\" : 1\n    }\n  ],\n  \"baz\" : [\n    1,\n    "
        "2,\n    3,\n    [\n      4\n    ]\n  ],\n  \"foo\" : \"bar\"\n}"),
            result.str());
}

TEST(BufferDumperTest, Null) {
  LocalBuffer[0] = 0x18;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  Buffer<char> buffer;
  CharBufferSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("null"), std::string(buffer.data(), buffer.size()));
}

TEST(StringDumperTest, Null) {
  LocalBuffer[0] = 0x18;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("null"), buffer);
}

TEST(StringDumperTest, Numbers) {
  int64_t pp = 2;
  for (int p = 1; p <= 61; p++) {
    int64_t i;

    auto check = [&]() -> void {
      Builder b;
      b.add(Value(i));
      Slice s(b.start());

      std::string buffer;
      StringSink sink(&buffer);
      Dumper dumper(&sink);
      dumper.dump(s);
      ASSERT_EQ(std::to_string(i), buffer);
    };

    i = pp;
    check();
    i = pp + 1;
    check();
    i = pp - 1;
    check();
    i = -pp;
    check();
    i = -pp + 1;
    check();
    i = -pp - 1;
    check();

    pp *= 2;
  }
}

TEST(BufferDumperTest, False) {
  LocalBuffer[0] = 0x19;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  Buffer<char> buffer;
  CharBufferSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("false"), std::string(buffer.data(), buffer.size()));
}

TEST(StringDumperTest, False) {
  LocalBuffer[0] = 0x19;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("false"), buffer);
}

TEST(BufferDumperTest, True) {
  LocalBuffer[0] = 0x1a;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  Buffer<char> buffer;
  CharBufferSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("true"), std::string(buffer.data(), buffer.size()));
}

TEST(StringDumperTest, True) {
  LocalBuffer[0] = 0x1a;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("true"), buffer);
}

TEST(StringDumperTest, StringSimple) {
  Builder b;
  b.add(Value("foobar"));

  Slice slice = b.slice();
  ASSERT_EQ(std::string("\"foobar\""), Dumper::toString(slice));
}

TEST(StringDumperTest, StringSpecialChars) {
  Builder b;
  b.add(Value("\"fo\r \n \\to''\\ \\bar\""));

  Slice slice = b.slice();
  ASSERT_EQ(std::string("\"\\\"fo\\r \\n \\\\to''\\\\ \\\\bar\\\"\""),
            Dumper::toString(slice));
}

TEST(StringDumperTest, StringControlChars) {
  Builder b;
  b.add(Value(std::string("\x00\x01\x02 baz \x03", 9)));

  Slice slice = b.slice();
  ASSERT_EQ(std::string("\"\\u0000\\u0001\\u0002 baz \\u0003\""),
            Dumper::toString(slice));
}

TEST(StringDumperTest, StringUTF8) {
  Builder b;
  b.add(Value("mötör"));

  Slice slice = b.slice();
  ASSERT_EQ(std::string("\"mötör\""), Dumper::toString(slice));
}

TEST(StringDumperTest, StringUTF8Escaped) {
  Builder b;
  b.add(Value("mötör"));

  Options options;
  options.escapeUnicode = true;
  ASSERT_EQ(std::string("\"m\\u00F6t\\u00F6r\""), Dumper::toString(b.slice(), &options));
}

TEST(StringDumperTest, StringTwoByteUTF8) {
  Builder b;
  b.add(Value("\xc2\xa2"));

  Slice slice = b.slice();
  ASSERT_EQ(std::string("\"\xc2\xa2\""), Dumper::toString(slice));
}

TEST(StringDumperTest, StringTwoByteUTF8Escaped) {
  Builder b;
  b.add(Value("\xc2\xa2"));

  Options options;
  options.escapeUnicode = true;
  ASSERT_EQ(std::string("\"\\u00A2\""), Dumper::toString(b.slice(), &options));
}

TEST(StringDumperTest, StringThreeByteUTF8) {
  Builder b;
  b.add(Value("\xe2\x82\xac"));

  Slice slice = b.slice();
  ASSERT_EQ(std::string("\"\xe2\x82\xac\""), Dumper::toString(slice));
}

TEST(StringDumperTest, StringThreeByteUTF8Escaped) {
  Builder b;
  b.add(Value("\xe2\x82\xac"));

  Options options;
  options.escapeUnicode = true;
  ASSERT_EQ(std::string("\"\\u20AC\""), Dumper::toString(b.slice(), &options));
}

TEST(StringDumperTest, StringFourByteUTF8) {
  Builder b;
  b.add(Value("\xf0\xa4\xad\xa2"));

  Slice slice = b.slice();
  ASSERT_EQ(std::string("\"\xf0\xa4\xad\xa2\""), Dumper::toString(slice));
}

TEST(StringDumperTest, StringFourByteUTF8Escaped) {
  Builder b;
  b.add(Value("\xf0\xa4\xad\xa2"));

  Options options;
  options.escapeUnicode = true;
  ASSERT_EQ(std::string("\"\\uD852\\uDF62\""), Dumper::toString(b.slice(), &options));
}

TEST(StringDumperTest, StringMultibytes) {
  std::vector<std::string> expected;
  expected.emplace_back("Lorem ipsum dolor sit amet, te enim mandamus consequat ius, cu eos timeam bonorum, in nec eruditi tibique. At nec malorum saperet vivendo. Qui delectus moderatius in. Vivendo expetendis ullamcorper ut mei.");
  expected.emplace_back("Мёнём пауло пытынтёюм ад ыам. Но эрож рыпудяары вим, пожтэа эюрйпйдяч ентырпрытаряш ад хёз. Мыа дектаж дёжкэрэ котёдиэквюэ ан. Ведят брутэ мэдиокретатым йн прё");
  expected.emplace_back("Μει ει παρτεμ μολλις δελισατα, σιφιβυς σονσυλατυ ραθιονιβυς συ φις, φερι μυνερε μεα ετ. Ειρμωδ απεριρι δισενθιετ εα υσυ, κυο θωτα φευγαιθ δισενθιετ νο");
  expected.emplace_back("供覧必責同界要努新少時購止上際英連動信。同売宗島載団報音改浅研壊趣全。並嗅整日放横旅関書文転方。天名他賞川日拠隊散境行尚島自模交最葉駒到");
  expected.emplace_back("舞ばい支小ぜ館応ヌエマ得6備ルあ煮社義ゃフおづ報載通ナチセ東帯あスフず案務革た証急をのだ毎点十はぞじド。1芸キテ成新53験モワサセ断団ニカ働給相づらべさ境著ラさ映権護ミオヲ但半モ橋同タ価法ナカネ仙説時オコワ気社オ");
  expected.emplace_back("أي جنوب بداية السبب بلا. تمهيد التكاليف العمليات إذ دول, عن كلّ أراضي اعتداء, بال الأوروبي الإقتصادية و. دخول تحرّكت بـ حين. أي شاسعة لليابان استطاعوا مكن. الأخذ الصينية والنرويج هو أخذ.");
  expected.emplace_back("זכר דפים בדפים מה, צילום מדינות היא או, ארץ צרפתית העברית אירועים ב. שונה קולנוע מתן אם, את אחד הארץ ציור וכמקובל. ויש העיר שימושי מדויקים בה, היא ויקי ברוכים תאולוגיה או. את זכר קהילה חבריכם ליצירתה, ערכים התפתחות חפש גם.");

  for (auto& it : expected) {
    Builder b;
    b.add(Value(it));

    std::string dumped = Dumper::toString(b.slice());
    ASSERT_EQ(std::string("\"") + it + "\"", dumped);
  
    Parser parser;
    parser.parse(dumped); 
  
    std::shared_ptr<Builder> builder = parser.steal();
    Slice s(builder->start());
    ASSERT_EQ(it, s.copyString()); 
  }
}

TEST(StringDumperTest, StringMultibytesEscaped) {
  std::vector<std::pair<std::string, std::string>> expected;
  expected.emplace_back(std::make_pair("Мёнём пауло пытынтёюм ад ыам. Но эрож рыпудяары вим, пожтэа эюрйпйдяч ентырпрытаряш ад хёз. Мыа дектаж дёжкэрэ котёдиэквюэ ан. Ведят брутэ мэдиокретатым йн прё", "\\u041C\\u0451\\u043D\\u0451\\u043C \\u043F\\u0430\\u0443\\u043B\\u043E \\u043F\\u044B\\u0442\\u044B\\u043D\\u0442\\u0451\\u044E\\u043C \\u0430\\u0434 \\u044B\\u0430\\u043C. \\u041D\\u043E \\u044D\\u0440\\u043E\\u0436 \\u0440\\u044B\\u043F\\u0443\\u0434\\u044F\\u0430\\u0440\\u044B \\u0432\\u0438\\u043C, \\u043F\\u043E\\u0436\\u0442\\u044D\\u0430 \\u044D\\u044E\\u0440\\u0439\\u043F\\u0439\\u0434\\u044F\\u0447 \\u0435\\u043D\\u0442\\u044B\\u0440\\u043F\\u0440\\u044B\\u0442\\u0430\\u0440\\u044F\\u0448 \\u0430\\u0434 \\u0445\\u0451\\u0437. \\u041C\\u044B\\u0430 \\u0434\\u0435\\u043A\\u0442\\u0430\\u0436 \\u0434\\u0451\\u0436\\u043A\\u044D\\u0440\\u044D \\u043A\\u043E\\u0442\\u0451\\u0434\\u0438\\u044D\\u043A\\u0432\\u044E\\u044D \\u0430\\u043D. \\u0412\\u0435\\u0434\\u044F\\u0442 \\u0431\\u0440\\u0443\\u0442\\u044D \\u043C\\u044D\\u0434\\u0438\\u043E\\u043A\\u0440\\u0435\\u0442\\u0430\\u0442\\u044B\\u043C \\u0439\\u043D \\u043F\\u0440\\u0451")); 
  expected.emplace_back(std::make_pair("Μει ει παρτεμ μολλις δελισατα, σιφιβυς σονσυλατυ ραθιονιβυς συ φις, φερι μυνερε μεα ετ. Ειρμωδ απεριρι δισενθιετ εα υσυ, κυο θωτα φευγαιθ δισενθιετ νο", "\\u039C\\u03B5\\u03B9 \\u03B5\\u03B9 \\u03C0\\u03B1\\u03C1\\u03C4\\u03B5\\u03BC \\u03BC\\u03BF\\u03BB\\u03BB\\u03B9\\u03C2 \\u03B4\\u03B5\\u03BB\\u03B9\\u03C3\\u03B1\\u03C4\\u03B1, \\u03C3\\u03B9\\u03C6\\u03B9\\u03B2\\u03C5\\u03C2 \\u03C3\\u03BF\\u03BD\\u03C3\\u03C5\\u03BB\\u03B1\\u03C4\\u03C5 \\u03C1\\u03B1\\u03B8\\u03B9\\u03BF\\u03BD\\u03B9\\u03B2\\u03C5\\u03C2 \\u03C3\\u03C5 \\u03C6\\u03B9\\u03C2, \\u03C6\\u03B5\\u03C1\\u03B9 \\u03BC\\u03C5\\u03BD\\u03B5\\u03C1\\u03B5 \\u03BC\\u03B5\\u03B1 \\u03B5\\u03C4. \\u0395\\u03B9\\u03C1\\u03BC\\u03C9\\u03B4 \\u03B1\\u03C0\\u03B5\\u03C1\\u03B9\\u03C1\\u03B9 \\u03B4\\u03B9\\u03C3\\u03B5\\u03BD\\u03B8\\u03B9\\u03B5\\u03C4 \\u03B5\\u03B1 \\u03C5\\u03C3\\u03C5, \\u03BA\\u03C5\\u03BF \\u03B8\\u03C9\\u03C4\\u03B1 \\u03C6\\u03B5\\u03C5\\u03B3\\u03B1\\u03B9\\u03B8 \\u03B4\\u03B9\\u03C3\\u03B5\\u03BD\\u03B8\\u03B9\\u03B5\\u03C4 \\u03BD\\u03BF")); 
  expected.emplace_back(std::make_pair("供覧必責同界要努新少時購止上際英連動信。同売宗島載団報音改浅研壊趣全。並嗅整日放横旅関書文転方。天名他賞川日拠隊散境行尚島自模交最葉駒到", "\\u4F9B\\u89A7\\u5FC5\\u8CAC\\u540C\\u754C\\u8981\\u52AA\\u65B0\\u5C11\\u6642\\u8CFC\\u6B62\\u4E0A\\u969B\\u82F1\\u9023\\u52D5\\u4FE1\\u3002\\u540C\\u58F2\\u5B97\\u5CF6\\u8F09\\u56E3\\u5831\\u97F3\\u6539\\u6D45\\u7814\\u58CA\\u8DA3\\u5168\\u3002\\u4E26\\u55C5\\u6574\\u65E5\\u653E\\u6A2A\\u65C5\\u95A2\\u66F8\\u6587\\u8EE2\\u65B9\\u3002\\u5929\\u540D\\u4ED6\\u8CDE\\u5DDD\\u65E5\\u62E0\\u968A\\u6563\\u5883\\u884C\\u5C1A\\u5CF6\\u81EA\\u6A21\\u4EA4\\u6700\\u8449\\u99D2\\u5230"));
  expected.emplace_back(std::make_pair("舞ばい支小ぜ館応ヌエマ得6備ルあ煮社義ゃフおづ報載通ナチセ東帯あスフず案務革た証急をのだ毎点十はぞじド。1芸キテ成新53験モワサセ断団ニカ働給相づらべさ境著ラさ映権護ミオヲ但半モ橋同タ価法ナカネ仙説時オコワ気社オ", "\\u821E\\u3070\\u3044\\u652F\\u5C0F\\u305C\\u9928\\u5FDC\\u30CC\\u30A8\\u30DE\\u5F976\\u5099\\u30EB\\u3042\\u716E\\u793E\\u7FA9\\u3083\\u30D5\\u304A\\u3065\\u5831\\u8F09\\u901A\\u30CA\\u30C1\\u30BB\\u6771\\u5E2F\\u3042\\u30B9\\u30D5\\u305A\\u6848\\u52D9\\u9769\\u305F\\u8A3C\\u6025\\u3092\\u306E\\u3060\\u6BCE\\u70B9\\u5341\\u306F\\u305E\\u3058\\u30C9\\u30021\\u82B8\\u30AD\\u30C6\\u6210\\u65B053\\u9A13\\u30E2\\u30EF\\u30B5\\u30BB\\u65AD\\u56E3\\u30CB\\u30AB\\u50CD\\u7D66\\u76F8\\u3065\\u3089\\u3079\\u3055\\u5883\\u8457\\u30E9\\u3055\\u6620\\u6A29\\u8B77\\u30DF\\u30AA\\u30F2\\u4F46\\u534A\\u30E2\\u6A4B\\u540C\\u30BF\\u4FA1\\u6CD5\\u30CA\\u30AB\\u30CD\\u4ED9\\u8AAC\\u6642\\u30AA\\u30B3\\u30EF\\u6C17\\u793E\\u30AA"));
  expected.emplace_back(std::make_pair("أي جنوب بداية السبب بلا. تمهيد التكاليف العمليات إذ دول, عن كلّ أراضي اعتداء, بال الأوروبي الإقتصادية و. دخول تحرّكت بـ حين. أي شاسعة لليابان استطاعوا مكن. الأخذ الصينية والنرويج هو أخذ.", "\\u0623\\u064A \\u062C\\u0646\\u0648\\u0628 \\u0628\\u062F\\u0627\\u064A\\u0629 \\u0627\\u0644\\u0633\\u0628\\u0628 \\u0628\\u0644\\u0627. \\u062A\\u0645\\u0647\\u064A\\u062F \\u0627\\u0644\\u062A\\u0643\\u0627\\u0644\\u064A\\u0641 \\u0627\\u0644\\u0639\\u0645\\u0644\\u064A\\u0627\\u062A \\u0625\\u0630 \\u062F\\u0648\\u0644, \\u0639\\u0646 \\u0643\\u0644\\u0651 \\u0623\\u0631\\u0627\\u0636\\u064A \\u0627\\u0639\\u062A\\u062F\\u0627\\u0621, \\u0628\\u0627\\u0644 \\u0627\\u0644\\u0623\\u0648\\u0631\\u0648\\u0628\\u064A \\u0627\\u0644\\u0625\\u0642\\u062A\\u0635\\u0627\\u062F\\u064A\\u0629 \\u0648. \\u062F\\u062E\\u0648\\u0644 \\u062A\\u062D\\u0631\\u0651\\u0643\\u062A \\u0628\\u0640 \\u062D\\u064A\\u0646. \\u0623\\u064A \\u0634\\u0627\\u0633\\u0639\\u0629 \\u0644\\u0644\\u064A\\u0627\\u0628\\u0627\\u0646 \\u0627\\u0633\\u062A\\u0637\\u0627\\u0639\\u0648\\u0627 \\u0645\\u0643\\u0646. \\u0627\\u0644\\u0623\\u062E\\u0630 \\u0627\\u0644\\u0635\\u064A\\u0646\\u064A\\u0629 \\u0648\\u0627\\u0644\\u0646\\u0631\\u0648\\u064A\\u062C \\u0647\\u0648 \\u0623\\u062E\\u0630."));
  expected.emplace_back(std::make_pair("זכר דפים בדפים מה, צילום מדינות היא או, ארץ צרפתית העברית אירועים ב. שונה קולנוע מתן אם, את אחד הארץ ציור וכמקובל. ויש העיר שימושי מדויקים בה, היא ויקי ברוכים תאולוגיה או. את זכר קהילה חבריכם ליצירתה, ערכים התפתחות חפש גם.", "\\u05D6\\u05DB\\u05E8 \\u05D3\\u05E4\\u05D9\\u05DD \\u05D1\\u05D3\\u05E4\\u05D9\\u05DD \\u05DE\\u05D4, \\u05E6\\u05D9\\u05DC\\u05D5\\u05DD \\u05DE\\u05D3\\u05D9\\u05E0\\u05D5\\u05EA \\u05D4\\u05D9\\u05D0 \\u05D0\\u05D5, \\u05D0\\u05E8\\u05E5 \\u05E6\\u05E8\\u05E4\\u05EA\\u05D9\\u05EA \\u05D4\\u05E2\\u05D1\\u05E8\\u05D9\\u05EA \\u05D0\\u05D9\\u05E8\\u05D5\\u05E2\\u05D9\\u05DD \\u05D1. \\u05E9\\u05D5\\u05E0\\u05D4 \\u05E7\\u05D5\\u05DC\\u05E0\\u05D5\\u05E2 \\u05DE\\u05EA\\u05DF \\u05D0\\u05DD, \\u05D0\\u05EA \\u05D0\\u05D7\\u05D3 \\u05D4\\u05D0\\u05E8\\u05E5 \\u05E6\\u05D9\\u05D5\\u05E8 \\u05D5\\u05DB\\u05DE\\u05E7\\u05D5\\u05D1\\u05DC. \\u05D5\\u05D9\\u05E9 \\u05D4\\u05E2\\u05D9\\u05E8 \\u05E9\\u05D9\\u05DE\\u05D5\\u05E9\\u05D9 \\u05DE\\u05D3\\u05D5\\u05D9\\u05E7\\u05D9\\u05DD \\u05D1\\u05D4, \\u05D4\\u05D9\\u05D0 \\u05D5\\u05D9\\u05E7\\u05D9 \\u05D1\\u05E8\\u05D5\\u05DB\\u05D9\\u05DD \\u05EA\\u05D0\\u05D5\\u05DC\\u05D5\\u05D2\\u05D9\\u05D4 \\u05D0\\u05D5. \\u05D0\\u05EA \\u05D6\\u05DB\\u05E8 \\u05E7\\u05D4\\u05D9\\u05DC\\u05D4 \\u05D7\\u05D1\\u05E8\\u05D9\\u05DB\\u05DD \\u05DC\\u05D9\\u05E6\\u05D9\\u05E8\\u05EA\\u05D4, \\u05E2\\u05E8\\u05DB\\u05D9\\u05DD \\u05D4\\u05EA\\u05E4\\u05EA\\u05D7\\u05D5\\u05EA \\u05D7\\u05E4\\u05E9 \\u05D2\\u05DD."));

  for (auto& it : expected) {
    Builder b;
    b.add(Value(it.first));

    Options options;
    options.escapeUnicode = true;

    std::string dumped = Dumper::toString(b.slice(), &options);
    ASSERT_EQ(std::string("\"") + it.second + "\"", dumped);
  
    Parser parser;
    parser.parse(dumped); 
  
    std::shared_ptr<Builder> builder = parser.steal();
    Slice s(builder->start());
    ASSERT_EQ(it.first, s.copyString()); 
  }
}

TEST(StringDumperTest, NumberDoubleZero) {
  Builder b;
  b.add(Value(0.0));
  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("0"), buffer);
}

TEST(StringDumperTest, NumberDouble1) {
  Builder b;
  b.add(Value(123456.67));
  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("123456.67"), buffer);
}

TEST(StringDumperTest, NumberDouble2) {
  Builder b;
  b.add(Value(-123456.67));
  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("-123456.67"), buffer);
}

TEST(StringDumperTest, NumberDouble3) {
  Builder b;
  b.add(Value(-0.000442));
  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("-0.000442"), buffer);
}

TEST(StringDumperTest, NumberDouble4) {
  Builder b;
  b.add(Value(0.1));
  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("0.1"), buffer);
}

TEST(StringDumperTest, NumberDoubleScientific1) {
  Builder b;
  b.add(Value(2.41e-109));
  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("2.41e-109"), buffer);
}

TEST(StringDumperTest, NumberDoubleScientific2) {
  Builder b;
  b.add(Value(-3.423e78));
  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("-3.423e+78"), buffer);
}

TEST(StringDumperTest, NumberDoubleScientific3) {
  Builder b;
  b.add(Value(3.423e123));
  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("3.423e+123"), buffer);
}

TEST(StringDumperTest, NumberDoubleScientific4) {
  Builder b;
  b.add(Value(3.4239493e104));
  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("3.4239493e+104"), buffer);
}

TEST(StringDumperTest, NumberInt1) {
  Builder b;
  b.add(Value(static_cast<int64_t>(123456789)));
  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("123456789"), buffer);
}

TEST(StringDumperTest, NumberInt2) {
  Builder b;
  b.add(Value(static_cast<int64_t>(-123456789)));
  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("-123456789"), buffer);
}

TEST(StringDumperTest, NumberZero) {
  Builder b;
  b.add(Value(static_cast<int64_t>(0)));
  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(slice);
  ASSERT_EQ(std::string("0"), buffer);
}

TEST(StringDumperTest, External) {
  Builder b1;
  b1.add(Value("this is a test string"));
  Slice slice1 = b1.slice();

  // create an external pointer to the string
  Builder b2;
  b2.add(Value(static_cast<void const*>(slice1.start())));
  Slice slice2 = b2.slice();

  ASSERT_EQ(std::string("\"this is a test string\""), Dumper::toString(slice2));
}

TEST(StringDumperTest, CustomWithoutHandler) {
  LocalBuffer[0] = 0xf0;
  LocalBuffer[1] = 0x00;

  Slice slice(reinterpret_cast<uint8_t const*>(&LocalBuffer[0]));

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  ASSERT_VELOCYPACK_EXCEPTION(dumper.dump(slice),
                              Exception::NeedCustomTypeHandler);
}

TEST(StringDumperTest, CustomWithCallbackDefaultHandler) {
  Builder b;
  b.openObject();
  uint8_t* p = b.add("_id", ValuePair(9ULL, ValueType::Custom));
  *p = 0xf3;
  for (std::size_t i = 1; i <= 8; i++) {
    p[i] = uint8_t(i + '@');
  }
  b.close();

  struct MyCustomTypeHandler : public CustomTypeHandler {};
  
  MyCustomTypeHandler handler;
  std::string buffer;
  StringSink sink(&buffer);
  Options options;
  options.customTypeHandler = &handler;
  Dumper dumper(&sink, &options);
  ASSERT_VELOCYPACK_EXCEPTION(dumper.dump(b.slice()), Exception::NotImplemented);
  
  ASSERT_VELOCYPACK_EXCEPTION(handler.dump(b.slice(), &dumper, b.slice()), Exception::NotImplemented);
  ASSERT_VELOCYPACK_EXCEPTION(handler.toString(b.slice(), nullptr, b.slice()), Exception::NotImplemented);
}

TEST(StringDumperTest, CustomWithHeapCallbackDefaultHandler) {
  Builder b;
  b.openObject();
  uint8_t* p = b.add("_id", ValuePair(9ULL, ValueType::Custom));
  *p = 0xf3;
  for (std::size_t i = 1; i <= 8; i++) {
    p[i] = uint8_t(i + '@');
  }
  b.close();

  struct MyCustomTypeHandler : public CustomTypeHandler {};
  
  auto handler = std::make_unique<MyCustomTypeHandler>();
  std::string buffer;
  StringSink sink(&buffer);
  Options options;
  options.customTypeHandler = handler.get();
  Dumper dumper(&sink, &options);
  ASSERT_VELOCYPACK_EXCEPTION(dumper.dump(b.slice()), Exception::NotImplemented);
  
  ASSERT_VELOCYPACK_EXCEPTION(handler->dump(b.slice(), &dumper, b.slice()), Exception::NotImplemented);
  ASSERT_VELOCYPACK_EXCEPTION(handler->toString(b.slice(), nullptr, b.slice()), Exception::NotImplemented);
}

TEST(StringDumperTest, CustomWithCallback) {
  Builder b;
  b.openObject();
  uint8_t* p = b.add("_id", ValuePair(9ULL, ValueType::Custom));
  *p = 0xf3;
  for (std::size_t i = 1; i <= 8; i++) {
    p[i] = uint8_t(i + '@');
  }
  b.close();

  struct MyCustomTypeHandler : public CustomTypeHandler {
    void dump(Slice const& value, Dumper* dumper, Slice const&) override {
      ASSERT_EQ(ValueType::Custom, value.type());
      ASSERT_EQ(0xf3UL, value.head());
      sawCustom = true;
      dumper->sink()->push_back('"');
      for (std::size_t i = 1; i <= 8; i++) {
        dumper->sink()->push_back(value.start()[i]);
      }
      dumper->sink()->push_back('"');
    }
    bool sawCustom = false;
  };

  MyCustomTypeHandler handler;
  std::string buffer;
  StringSink sink(&buffer);
  Options options;
  options.customTypeHandler = &handler;
  Dumper dumper(&sink, &options);
  dumper.dump(b.slice());
  ASSERT_TRUE(handler.sawCustom);
  ASSERT_EQ(R"({"_id":"ABCDEFGH"})", buffer);
}

TEST(StringDumperTest, CustomStringWithCallback) {
  Builder b;
  b.add(Value(ValueType::Object));
  uint8_t* p = b.add("foo", ValuePair(5ULL, ValueType::Custom));
  *p++ = 0xf5;
  *p++ = 0x03;
  *p++ = 'b';
  *p++ = 'a';
  *p++ = 'r';
  b.close();

  struct MyCustomTypeHandler : public CustomTypeHandler {
    void dump(Slice const& value, Dumper* dumper, Slice const&) {
      Sink* sink = dumper->sink();
      ASSERT_EQ(ValueType::Custom, value.type());
      ASSERT_EQ(0xf5UL, value.head());
      ValueLength length = static_cast<ValueLength>(*(value.start() + 1));
      sink->push_back('"');
      sink->append(value.startAs<char const>() + 2, length);
      sink->push_back('"');
      sawCustom = true;
    }
    bool sawCustom = false;
  };

  MyCustomTypeHandler handler;
  std::string buffer;
  StringSink sink(&buffer);
  Options options;
  options.customTypeHandler = &handler;
  Dumper dumper(&sink, &options);
  dumper.dump(b.slice());
  ASSERT_TRUE(handler.sawCustom);

  ASSERT_EQ(std::string("{\"foo\":\"bar\"}"), buffer);
}

TEST(StringDumperTest, CustomWithCallbackWithContent) {
  struct MyCustomTypeHandler : public CustomTypeHandler {
    void dump(Slice const& value, Dumper* dumper, Slice const& base) override {
      Sink* sink = dumper->sink();
      ASSERT_EQ(ValueType::Custom, value.type());

      EXPECT_TRUE(base.isObject());
      auto key = base.get("_key");
      EXPECT_EQ(ValueType::String, key.type());
      sink->append("\"foobar/");
      sink->append(key.copyString());
      sink->push_back('"');
    }
  };

  MyCustomTypeHandler handler;
  Options options;
  options.customTypeHandler = &handler;

  Builder b(&options);
  b.add(Value(ValueType::Object));
  uint8_t* p = b.add("_id", ValuePair(2ULL, ValueType::Custom));
  *p++ = 0xf0;
  *p = 0x12;
  b.add("_key", Value("this is a key"));
  b.close();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.dump(b.slice());

  ASSERT_EQ(
      std::string(
          "{\"_id\":\"foobar/this is a key\",\"_key\":\"this is a key\"}"),
      buffer);
}

TEST(StringDumperTest, ArrayWithCustom) {
  struct MyCustomTypeHandler : public CustomTypeHandler {
    void dump(Slice const& value, Dumper* dumper, Slice const& base) {
      Sink* sink = dumper->sink();
      ASSERT_EQ(ValueType::Custom, value.type());

      EXPECT_TRUE(base.isArray());
      if (value.head() == 0xf0 && value.start()[1] == 0x01) {
        sink->append("\"foobar\"");
      } else if (value.head() == 0xf0 && value.start()[1] == 0x02) {
        sink->append("1234");
      } else if (value.head() == 0xf0 && value.start()[1] == 0x03) {
        sink->append("[]");
      } else if (value.head() == 0xf0 && value.start()[1] == 0x04) {
        sink->append("{\"qux\":2}");
      } else {
        EXPECT_TRUE(false);
      }
    }
  };

  MyCustomTypeHandler handler;
  Options options;
  options.customTypeHandler = &handler;

  uint8_t* p;

  Builder b(&options);
  b.add(Value(ValueType::Array));
  p = b.add(ValuePair(2ULL, ValueType::Custom));
  *p++ = 0xf0;
  *p = 1;
  p = b.add(ValuePair(2ULL, ValueType::Custom));
  *p++ = 0xf0;
  *p = 2;
  p = b.add(ValuePair(2ULL, ValueType::Custom));
  *p++ = 0xf0;
  *p = 3;
  p = b.add(ValuePair(2ULL, ValueType::Custom));
  *p++ = 0xf0;
  *p = 4;
  b.close();

  // array with same sizes
  ASSERT_EQ(0x02, b.slice().head());

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.dump(b.slice());

  ASSERT_EQ(std::string("[\"foobar\",1234,[],{\"qux\":2}]"), buffer);
}

TEST(StringDumperTest, AppendCharTest) {
  char const* p = "this is a simple string";
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.appendString(p, strlen(p));

  ASSERT_EQ(std::string("\"this is a simple string\""), buffer);
}

TEST(StringDumperTest, AppendStringTest) {
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.appendString("this is a simple string");

  ASSERT_EQ(std::string("\"this is a simple string\""), buffer);
}

TEST(StringDumperTest, AppendCharTestSpecialChars1) {
  Options options;
  options.escapeForwardSlashes = true;

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.appendString(std::string(
      "this is a string with special chars / \" \\ ' foo\n\r\t baz"));

  ASSERT_EQ(std::string(
                "\"this is a string with special chars \\/ \\\" \\\\ ' "
                "foo\\n\\r\\t baz\""),
            buffer);
}

TEST(StringDumperTest, AppendCharTestSpecialChars2) {
  Options options;
  options.escapeForwardSlashes = false;

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.appendString(std::string(
      "this is a string with special chars / \" \\ ' foo\n\r\t baz"));

  ASSERT_EQ(std::string(
                "\"this is a string with special chars / \\\" \\\\ ' "
                "foo\\n\\r\\t baz\""),
            buffer);
}

TEST(StringDumperTest, AppendStringTestSpecialChars1) {
  Options options;
  options.escapeForwardSlashes = true;

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.appendString(
      "this is a string with special chars / \" \\ ' foo\n\r\t baz");

  ASSERT_EQ(std::string(
                "\"this is a string with special chars \\/ \\\" \\\\ ' "
                "foo\\n\\r\\t baz\""),
            buffer);
}

TEST(StringDumperTest, AppendStringTestSpecialChars2) {
  Options options;
  options.escapeForwardSlashes = false;

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.appendString(
      "this is a string with special chars / \" \\ ' foo\n\r\t baz");

  ASSERT_EQ(std::string(
                "\"this is a string with special chars / \\\" \\\\ ' "
                "foo\\n\\r\\t baz\""),
            buffer);
}

TEST(StringDumperTest, AppendStringTestTruncatedTwoByteUtf8) {
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  ASSERT_VELOCYPACK_EXCEPTION(dumper.appendString("\xc2"),
                              Exception::InvalidUtf8Sequence);
}

TEST(StringDumperTest, AppendStringTestTruncatedThreeByteUtf8) {
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  ASSERT_VELOCYPACK_EXCEPTION(dumper.appendString("\xe2\x82"),
                              Exception::InvalidUtf8Sequence);
}

TEST(StringDumperTest, AppendStringTestTruncatedFourByteUtf8) {
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  ASSERT_VELOCYPACK_EXCEPTION(dumper.appendString("\xf0\xa4\xad"),
                              Exception::InvalidUtf8Sequence);
}

TEST(StringDumperTest, AppendStringSlice1) {
  Options options;
  options.escapeForwardSlashes = true;

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);

  std::string const s =
      "this is a string with special chars / \" \\ ' foo\n\r\t baz";
  Builder b;
  b.add(Value(s));
  Slice slice(b.start());
  dumper.append(slice);

  ASSERT_EQ(std::string(
                "\"this is a string with special chars \\/ \\\" \\\\ ' "
                "foo\\n\\r\\t baz\""),
            buffer);
}

TEST(StringDumperTest, AppendStringSlice2) {
  Options options;
  options.escapeForwardSlashes = false;

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);

  std::string const s =
      "this is a string with special chars / \" \\ ' foo\n\r\t baz";
  Builder b;
  b.add(Value(s));
  Slice slice(b.start());

  dumper.append(slice);
  ASSERT_EQ(std::string(
                "\"this is a string with special chars / \\\" \\\\ ' "
                "foo\\n\\r\\t baz\""),
            buffer);
}

TEST(StringDumperTest, AppendStringSliceRef1) {
  Options options;
  options.escapeForwardSlashes = true;

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);

  std::string const s =
      "this is a string with special chars / \" \\ ' foo\n\r\t baz";
  Builder b;
  b.add(Value(s));
  Slice slice(b.start());
  dumper.append(&slice);

  ASSERT_EQ(std::string(
                "\"this is a string with special chars \\/ \\\" \\\\ ' "
                "foo\\n\\r\\t baz\""),
            buffer);
}

TEST(StringDumperTest, AppendStringSliceRef2) {
  Options options;
  options.escapeForwardSlashes = false;

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);

  std::string const s =
      "this is a string with special chars / \" \\ ' foo\n\r\t baz";
  Builder b;
  b.add(Value(s));
  Slice slice(b.start());
  dumper.append(&slice);
  ASSERT_EQ(std::string(
                "\"this is a string with special chars / \\\" \\\\ ' "
                "foo\\n\\r\\t baz\""),
            buffer);
}

TEST(StringDumperTest, AppendDoubleNan) {
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.appendDouble(std::nan("1"));
  ASSERT_EQ(std::string("NaN"), buffer);
}

TEST(StringDumperTest, AppendDoubleMinusInf) {
  double v = -3.33e307;
  // go to -inf
  v *= 3.1e90;

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.appendDouble(v);
  ASSERT_EQ(std::string("-inf"), buffer);
}

TEST(StringDumperTest, AppendDoublePlusInf) {
  double v = 3.33e307;
  // go to +inf
  v *= v;

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.appendDouble(v);
  ASSERT_EQ(std::string("inf"), buffer);
}

TEST(StringDumperTest, UnsupportedTypeDoubleMinusInf) {
  double v = -3.33e307;
  // go to -inf
  v *= 3.1e90;
  Builder b;
  b.add(Value(v));

  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  ASSERT_VELOCYPACK_EXCEPTION(dumper.dump(slice), Exception::NoJsonEquivalent);
}

TEST(StringDumperTest, ConvertTypeDoubleMinusInf) {
  double v = -3.33e307;
  // go to -inf
  v *= 3.1e90;
  Builder b;
  b.add(Value(v));

  Slice slice = b.slice();

  Options options;
  options.unsupportedTypeBehavior = Options::NullifyUnsupportedType;
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.dump(slice);
  ASSERT_EQ(std::string("null"), buffer);
}

TEST(StringDumperTest, UnsupportedTypeDoublePlusInf) {
  double v = 3.33e307;
  // go to +inf
  v *= v;
  Builder b;
  b.add(Value(v));

  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  ASSERT_VELOCYPACK_EXCEPTION(dumper.dump(slice), Exception::NoJsonEquivalent);
}

TEST(StringDumperTest, ConvertTypeDoublePlusInf) {
  double v = 3.33e307;
  // go to +inf
  v *= v;
  Builder b;
  b.add(Value(v));

  Slice slice = b.slice();

  Options options;
  options.unsupportedTypeBehavior = Options::NullifyUnsupportedType;
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.dump(slice);
  ASSERT_EQ(std::string("null"), buffer);
}

TEST(StringDumperTest, UnsupportedTypeDoubleNan) {
  double v = std::nan("1");
  ASSERT_TRUE(std::isnan(v));
  Builder b;
  b.add(Value(v));

  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  ASSERT_VELOCYPACK_EXCEPTION(dumper.dump(slice), Exception::NoJsonEquivalent);
}

TEST(StringDumperTest, DoubleNanAsString) {
  Options options;
  options.unsupportedDoublesAsString = true;

  double v = std::nan("1");
  ASSERT_TRUE(std::isnan(v));
  Builder b;
  b.add(Value(v));

  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.dump(slice);
  ASSERT_EQ(std::string("\"NaN\""), buffer);
}

TEST(StringDumperTest, DoubleInfinityAsString) {
  Options options;
  options.unsupportedDoublesAsString = true;

  double v = INFINITY;
  ASSERT_TRUE(std::isinf(v));
  Builder b;
  b.add(Value(v));

  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.dump(slice);
  ASSERT_EQ(std::string("\"Infinity\""), buffer);
}

TEST(StringDumperTest, DoubleMinusInfinityAsString) {
  Options options;
  options.unsupportedDoublesAsString = true;

  double v = -INFINITY;
  ASSERT_TRUE(std::isinf(v));
  Builder b;
  b.add(Value(v));

  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.dump(slice);
  ASSERT_EQ(std::string("\"-Infinity\""), buffer);
}

TEST(StringDumperTest, ConvertTypeDoubleNan) {
  double v = std::nan("1");
  ASSERT_TRUE(std::isnan(v));
  Builder b;
  b.add(Value(v));

  Slice slice = b.slice();

  Options options;
  options.unsupportedTypeBehavior = Options::NullifyUnsupportedType;
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.dump(slice);
  ASSERT_EQ(std::string("null"), buffer);
}

TEST(StringDumperTest, UnsupportedTypeBinary) {
  Builder b;
  b.add(Value(std::string("der fuchs"), ValueType::Binary));

  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  ASSERT_VELOCYPACK_EXCEPTION(dumper.dump(slice), Exception::NoJsonEquivalent);
}

TEST(StringDumperTest, ConvertTypeBinary) {
  Builder b;
  b.add(Value(std::string("der fuchs"), ValueType::Binary));

  Slice slice = b.slice();

  Options options;
  options.unsupportedTypeBehavior = Options::NullifyUnsupportedType;
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.dump(slice);
  ASSERT_EQ(std::string("null"), buffer);
}

TEST(StringDumperTest, UnsupportedTypeUTCDate) {
  int64_t v = 0;
  Builder b;
  b.add(Value(v, ValueType::UTCDate));

  Slice slice = b.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  ASSERT_VELOCYPACK_EXCEPTION(dumper.dump(slice), Exception::NoJsonEquivalent);
}

TEST(StringDumperTest, ConvertTypeUTCDate) {
  int64_t v = 0;
  Builder b;
  b.add(Value(v, ValueType::UTCDate));

  Slice slice = b.slice();

  Options options;
  options.unsupportedTypeBehavior = Options::NullifyUnsupportedType;
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.dump(slice);
  ASSERT_EQ(std::string("null"), buffer);
}

TEST(StringDumperTest, ConvertUnsupportedTypeUTCDate) {
  int64_t v = 0;
  Builder b;
  b.add(Value(v, ValueType::UTCDate));

  Slice slice = b.slice();

  Options options;
  options.unsupportedTypeBehavior = Options::ConvertUnsupportedType;
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.dump(slice);
  ASSERT_EQ(std::string("\"(non-representable type utc-date)\""), buffer);
}

TEST(StringDumperTest, UnsupportedTypeNone) {
  static uint8_t const b[] = {0x00};
  Slice slice(&b[0]);

  ASSERT_VELOCYPACK_EXCEPTION(Dumper::toString(slice),
                              Exception::NoJsonEquivalent);
}

TEST(StringDumperTest, ConvertTypeNone) {
  static uint8_t const b[] = {0x00};
  Slice slice(&b[0]);

  Options options;
  options.unsupportedTypeBehavior = Options::NullifyUnsupportedType;
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.dump(slice);
  ASSERT_EQ(std::string("null"), buffer);
}

TEST(StringDumperTest, UnsupportedTypeIllegal) {
  static uint8_t const b[] = {0x17};
  Slice slice(&b[0]);

  ASSERT_VELOCYPACK_EXCEPTION(Dumper::toString(slice),
                              Exception::NoJsonEquivalent);
}

TEST(StringDumperTest, ConvertTypeIllegal) {
  static uint8_t const b[] = {0x17};
  Slice slice(&b[0]);

  Options options;
  options.unsupportedTypeBehavior = Options::NullifyUnsupportedType;
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.dump(slice);
  ASSERT_EQ(std::string("null"), buffer);
}

TEST(StringDumperTest, ConvertUnsupportedTypeIllegal) {
  static uint8_t const b[] = {0x17};
  Slice slice(&b[0]);

  Options options;
  options.unsupportedTypeBehavior = Options::ConvertUnsupportedType;
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.dump(slice);
  ASSERT_EQ(std::string("\"(non-representable type illegal)\""), buffer);
}

TEST(StringDumperTest, UnsupportedTypeMinKey) {
  static uint8_t const b[] = {0x1e};
  Slice slice(&b[0]);

  ASSERT_VELOCYPACK_EXCEPTION(Dumper::toString(slice),
                              Exception::NoJsonEquivalent);
}

TEST(StringDumperTest, ConvertTypeMinKey) {
  static uint8_t const b[] = {0x1e};
  Slice slice(&b[0]);

  Options options;
  options.unsupportedTypeBehavior = Options::NullifyUnsupportedType;
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.dump(slice);
  ASSERT_EQ(std::string("null"), buffer);
}

TEST(StringDumperTest, UnsupportedTypeMaxKey) {
  static uint8_t const b[] = {0x1f};
  Slice slice(&b[0]);

  ASSERT_VELOCYPACK_EXCEPTION(Dumper::toString(slice),
                              Exception::NoJsonEquivalent);
}

TEST(StringDumperTest, ConvertTypeMaxKey) {
  static uint8_t const b[] = {0x1f};
  Slice slice(&b[0]);

  Options options;
  options.unsupportedTypeBehavior = Options::NullifyUnsupportedType;
  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink, &options);
  dumper.dump(slice);
  ASSERT_EQ(std::string("null"), buffer);
}

TEST(StringDumperTest, BCD) {
  static uint8_t const b[] = {0xc8, 0x00, 0x00, 0x00};  // fake BCD value
  Slice slice(&b[0]);

  ASSERT_VELOCYPACK_EXCEPTION(Dumper::toString(slice),
                              Exception::NotImplemented);
}

TEST(StringDumperTest, BCDNeg) {
  static uint8_t const b[] = {0xd0, 0x00, 0x00, 0x00};  // fake BCD value
  Slice slice(&b[0]);

  ASSERT_VELOCYPACK_EXCEPTION(Dumper::toString(slice),
                              Exception::NotImplemented);
}

TEST(StringDumperTest, AttributeTranslationsNotSet) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);
  // intentionally don't add any translations
  translator->seal();

  AttributeTranslatorScope scope(translator.get());
  
  Options options;
  options.attributeTranslator = translator.get();

  std::string const value("{\"test\":\"bar\"}");

  Parser parser(&options);
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  std::string result = Dumper::toString(s, &options);
  ASSERT_EQ(value, result);
}

TEST(StringDumperTest, AttributeTranslations) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);

  translator->add("foo", 1);
  translator->add("bar", 2);
  translator->add("baz", 3);
  translator->add("bark", 4);
  translator->add("mötör", 5);
  translator->add("quetzalcoatl", 6);
  translator->seal();

  AttributeTranslatorScope scope(translator.get());

  Options options;
  options.attributeTranslator = translator.get();

  std::string const value(
      "{\"foo\":\"bar\",\"baz\":[1,2,3,[4]],\"bark\":[{\"troet\\nmann\":1,"
      "\"mötör\":[2,3.4,-42.5,true,false,null,\"some\\nstring\"]}]}");

  Parser parser(&options);
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  std::string result = Dumper::toString(s, &options);
  ASSERT_EQ(std::string(
        "{\"bark\":[{\"m\xC3\xB6t\xC3\xB6r\":[2,3.4,-42.5,true,false,null,"
        "\"some\\nstring\"],\"troet\\nmann\":1}],\"baz\":[1,2,3,[4]],"
        "\"foo\":\"bar\"}"), result);
}

TEST(StringDumperTest, AttributeTranslationsInSubObjects) {
  std::unique_ptr<AttributeTranslator> translator(new AttributeTranslator);

  translator->add("foo", 1);
  translator->add("bar", 2);
  translator->add("baz", 3);
  translator->add("bark", 4);
  translator->seal();

  AttributeTranslatorScope scope(translator.get());

  Options options;
  options.attributeTranslator = translator.get();

  std::string const value(
      "{\"foo\":{\"bar\":{\"baz\":\"baz\"},\"bark\":3,\"foo\":true},\"bar\":"
      "1}");

  Parser parser(&options);
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  std::string result = Dumper::toString(s, &options);
  ASSERT_EQ(std::string("{\"bar\":1,\"foo\":{\"bar\":{\"baz\":\"baz\"},"
        "\"bark\":3,\"foo\":true}}"), result);
}

TEST(DumperTest, EmptyAttributeName) {
  Builder builder;
  Parser parser(builder);
  parser.parse(R"({"":123,"a":"abc"})");
  Slice slice = builder.slice();

  std::string buffer;
  StringSink sink(&buffer);
  Dumper dumper(&sink);
  dumper.dump(&slice);
  ASSERT_EQ(std::string(R"({"":123,"a":"abc"})"), buffer);
}

TEST(DumperLengthTest, Null) {
  std::string const value("null");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Options options;
  options.prettyPrint = false;
  StringLengthSink sink;
  Dumper dumper(&sink, &options);
  dumper.dump(s);
  ASSERT_EQ(strlen("null"), sink.length);
}

TEST(DumperLengthTest, True) {
  std::string const value("  true ");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Options options;
  options.prettyPrint = false;
  StringLengthSink sink;
  Dumper dumper(&sink, &options);
  dumper.dump(s);
  ASSERT_EQ(strlen("true"), sink.length);
}

TEST(DumperLengthTest, False) {
  std::string const value("false ");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Options options;
  options.prettyPrint = false;
  StringLengthSink sink;
  Dumper dumper(&sink, &options);
  dumper.dump(s);
  ASSERT_EQ(strlen("false"), sink.length);
}

TEST(DumperLengthTest, String) {
  std::string const value("   \"abcdefgjfjhhgh\"  ");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Options options;
  options.prettyPrint = false;
  StringLengthSink sink;
  Dumper dumper(&sink, &options);
  dumper.dump(s);
  ASSERT_EQ(strlen("\"abcdefgjfjhhgh\""), sink.length);;
}

TEST(DumperLengthTest, EmptyObject) {
  std::string const value("{}");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Options options;
  options.prettyPrint = false;
  StringLengthSink sink;
  Dumper dumper(&sink, &options);
  dumper.dump(s);
  ASSERT_EQ(strlen("{}"), sink.length);
}

TEST(DumperLengthTest, SimpleObject) {
  std::string const value("{\"foo\":\"bar\"}");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Options options;
  options.prettyPrint = false;
  StringLengthSink sink;
  Dumper dumper(&sink, &options);
  dumper.dump(s);
  ASSERT_EQ(strlen("{\"foo\":\"bar\"}"), sink.length);
}

TEST(DumperLengthTest, SimpleArray) {
  std::string const value("[1, 2, 3, 4, 5, 6, 7, \"abcdef\"]");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Options options;
  options.prettyPrint = false;
  StringLengthSink sink;
  Dumper dumper(&sink, &options);
  dumper.dump(s);
  ASSERT_EQ(strlen("[1,2,3,4,5,6,7,\"abcdef\"]"), sink.length);
}

TEST(DumperLengthTest, EscapeUnicodeOn) {
  std::string const value("\"mötör\"");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Options options;
  options.prettyPrint = false;
  options.escapeUnicode = true;
  StringLengthSink sink;
  Dumper dumper(&sink, &options);
  dumper.dump(s);
  ASSERT_EQ(strlen("\"m\\uxxxxt\\uxxxxr\""), sink.length);
}

TEST(DumperLengthTest, EscapeUnicodeOff) {
  std::string const value("\"mötör\"");

  Parser parser;
  parser.parse(value);

  std::shared_ptr<Builder> builder = parser.steal();
  Slice s(builder->start());

  Options options;
  options.prettyPrint = false;
  options.escapeUnicode = false;
  StringLengthSink sink;
  Dumper dumper(&sink, &options);
  dumper.dump(s);
  ASSERT_EQ(strlen("\"mötör\""), sink.length);
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
