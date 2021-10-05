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

#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "tests-common.h"

static auto DoNothingCallback =
    [](Slice const&, ValueLength) -> bool { return false; };
static auto FailCallback = [](Slice const&, ValueLength) -> bool {
  EXPECT_TRUE(false);
  return false;
};

TEST(CollectionTest, KeysNonObject1) {
  std::string const value("null");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(Collection::keys(s), Exception::InvalidValueType);
}

TEST(CollectionTest, KeysNonObject2) {
  std::string const value("null");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> result;
  ASSERT_VELOCYPACK_EXCEPTION(Collection::keys(s, result),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, KeysNonObject3) {
  std::string const value("null");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::unordered_set<std::string> result;
  ASSERT_VELOCYPACK_EXCEPTION(Collection::keys(s, result),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, KeysNonObject4) {
  std::string const value("[]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(Collection::keys(s), Exception::InvalidValueType);
}

TEST(CollectionTest, KeysNonObject5) {
  std::string const value("[]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> result;
  ASSERT_VELOCYPACK_EXCEPTION(Collection::keys(s, result),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, KeysNonObject6) {
  std::string const value("[]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::unordered_set<std::string> result;
  ASSERT_VELOCYPACK_EXCEPTION(Collection::keys(s, result),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, ObjectKeys1) {
  std::string const value("{\"foo\":1,\"bar\":2,\"baz\":3}");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> keys = Collection::keys(s);
  ASSERT_EQ(3UL, keys.size());
  ASSERT_EQ("bar", keys[0]);
  ASSERT_EQ("baz", keys[1]);
  ASSERT_EQ("foo", keys[2]);
}

TEST(CollectionTest, ObjectKeys2) {
  std::string const value("{\"foo\":1,\"bar\":2,\"baz\":3}");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> keys;
  Collection::keys(s, keys);
  ASSERT_EQ(3UL, keys.size());
  ASSERT_EQ("bar", keys[0]);
  ASSERT_EQ("baz", keys[1]);
  ASSERT_EQ("foo", keys[2]);
}

TEST(CollectionTest, ObjectKeys3) {
  std::string const value("{\"foo\":1,\"bar\":2,\"baz\":3}");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::unordered_set<std::string> keys;
  Collection::keys(s, keys);
  ASSERT_EQ(3UL, keys.size());
  ASSERT_TRUE(keys.find("foo") != keys.end());
  ASSERT_TRUE(keys.find("bar") != keys.end());
  ASSERT_TRUE(keys.find("baz") != keys.end());
}

TEST(CollectionTest, KeysSetNonObject) {
  std::string const value("[]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::set<std::string> result;
  ASSERT_VELOCYPACK_EXCEPTION(Collection::keys(s, result),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, ObjectKeysSet) {
  Options options;

  std::string const value("{\"foo\":1,\"bar\":2,\"baz\":3}");
  Parser parser(&options);
  parser.parse(value);
  Slice s(parser.start());

  std::set<std::string> keys;
  Collection::keys(s, keys);
  ASSERT_EQ(3UL, keys.size());
  ASSERT_TRUE(keys.find("bar") != keys.end());
  ASSERT_TRUE(keys.find("baz") != keys.end());
  ASSERT_TRUE(keys.find("foo") != keys.end());
}

TEST(CollectionTest, ObjectKeysSetMerge) {
  Options options;

  std::set<std::string> keys;
  std::string const value("{\"foo\":1,\"bar\":2,\"baz\":3}");
  Parser parser(&options);
  parser.parse(value);
  Slice s(parser.start());

  Collection::keys(s, keys);
  ASSERT_EQ(3UL, keys.size());
  ASSERT_TRUE(keys.find("bar") != keys.end());
  ASSERT_TRUE(keys.find("baz") != keys.end());
  ASSERT_TRUE(keys.find("foo") != keys.end());
  
  std::string const value2("{\"foobar\":1,\"quux\":3,\"baz\":2,\"bark\":3}");
  Parser parser2(&options);
  parser2.parse(value2);
  Slice s2(parser2.start());
  
  Collection::keys(s2, keys);
  ASSERT_EQ(6UL, keys.size());
  ASSERT_TRUE(keys.find("bar") != keys.end());
  ASSERT_TRUE(keys.find("bark") != keys.end());
  ASSERT_TRUE(keys.find("baz") != keys.end());
  ASSERT_TRUE(keys.find("foo") != keys.end());
  ASSERT_TRUE(keys.find("foobar") != keys.end());
  ASSERT_TRUE(keys.find("quux") != keys.end());
}

TEST(CollectionTest, ObjectKeys) {
  std::string const value(
      "{\"1foo\":\"bar\",\"2baz\":\"quux\",\"3number\":1,\"4boolean\":true,"
      "\"5empty\":null}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> keys = Collection::keys(s);
  ASSERT_EQ(5U, keys.size());
  ASSERT_EQ("1foo", keys[0]);
  ASSERT_EQ("2baz", keys[1]);
  ASSERT_EQ("3number", keys[2]);
  ASSERT_EQ("4boolean", keys[3]);
  ASSERT_EQ("5empty", keys[4]);
}

TEST(CollectionsTest, ObjectKeysRef) {
  std::string const value(
      "{\"1foo\":\"bar\",\"2baz\":\"quux\",\"3number\":1,\"4boolean\":true,"
      "\"5empty\":null}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> keys;
  Collection::keys(s, keys);
  ASSERT_EQ(5U, keys.size());
  ASSERT_EQ("1foo", keys[0]);
  ASSERT_EQ("2baz", keys[1]);
  ASSERT_EQ("3number", keys[2]);
  ASSERT_EQ("4boolean", keys[3]);
  ASSERT_EQ("5empty", keys[4]);
}

TEST(CollectionTest, ObjectKeysCompact) {
  std::string const value(
      "{\"1foo\":\"bar\",\"2baz\":\"quux\",\"3number\":1,\"4boolean\":true,"
      "\"5empty\":null}");

  Options options;
  options.buildUnindexedArrays = true;

  Parser parser(&options);
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> keys = Collection::keys(s);
  ASSERT_EQ(5U, keys.size());
  ASSERT_EQ("1foo", keys[0]);
  ASSERT_EQ("2baz", keys[1]);
  ASSERT_EQ("3number", keys[2]);
  ASSERT_EQ("4boolean", keys[3]);
  ASSERT_EQ("5empty", keys[4]);
}

TEST(CollectionTest, ValuesNonObject1) {
  std::string const value("null");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(Collection::values(s),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, ValuesNonObject2) {
  std::string const value("\"foobar\"");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(Collection::values(s),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, ValuesNonObject3) {
  std::string const value("[]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(Collection::values(s),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, ObjectValues) {
  std::string const value(
      "{\"1foo\":\"bar\",\"2baz\":\"quux\",\"3number\":1,\"4boolean\":true,"
      "\"5empty\":null}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  Builder b = Collection::values(s);
  s = b.slice();
  ASSERT_TRUE(s.isArray());
  ASSERT_EQ(5U, s.length());

  ASSERT_TRUE(s.at(0).isString());
  ASSERT_EQ("bar", s.at(0).copyString());
  ASSERT_TRUE(s.at(1).isString());
  ASSERT_EQ("quux", s.at(1).copyString());
  ASSERT_TRUE(s.at(2).isNumber());
  ASSERT_EQ(1UL, s.at(2).getUInt());
  ASSERT_TRUE(s.at(3).isBoolean());
  ASSERT_TRUE(s.at(3).getBoolean());
  ASSERT_TRUE(s.at(4).isNull());
}

TEST(CollectionTest, ObjectValuesCompact) {
  std::string const value(
      "{\"1foo\":\"bar\",\"2baz\":\"quux\",\"3number\":1,\"4boolean\":true,"
      "\"5empty\":null}");

  Options options;
  options.buildUnindexedObjects = true;

  Parser parser(&options);
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_EQ(0x14, s.head());

  Builder b = Collection::values(s);
  s = b.slice();
  ASSERT_TRUE(s.isArray());
  ASSERT_EQ(5U, s.length());

  ASSERT_TRUE(s.at(0).isString());
  ASSERT_EQ("bar", s.at(0).copyString());
  ASSERT_TRUE(s.at(1).isString());
  ASSERT_EQ("quux", s.at(1).copyString());
  ASSERT_TRUE(s.at(2).isNumber());
  ASSERT_EQ(1UL, s.at(2).getUInt());
  ASSERT_TRUE(s.at(3).isBoolean());
  ASSERT_TRUE(s.at(3).getBoolean());
  ASSERT_TRUE(s.at(4).isNull());
}

TEST(CollectionTest, ForEachNonArray) {
  std::string const value("null");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(Collection::forEach(s, DoNothingCallback),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, ForEachEmptyArray) {
  std::string const value("[]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  Collection::forEach(s, FailCallback);
}

TEST(CollectionTest, ForEachArray) {
  std::string const value("[1,2,3,\"foo\",\"bar\"]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  Collection::forEach(s,
                      [&seen](Slice const& slice, ValueLength index) -> bool {
    EXPECT_EQ(seen, index);

    switch (seen) {
      case 0:
      case 1:
      case 2:
        EXPECT_TRUE(slice.isNumber());
        break;
      case 3:
      case 4:
        EXPECT_TRUE(slice.isString());
    }

    ++seen;
    return true;
  });

  ASSERT_EQ(5UL, seen);
}

TEST(CollectionTest, ForEachArrayAbort) {
  std::string const value("[1,2,3,\"foo\",\"bar\"]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  Collection::forEach(s, [&seen](Slice const&, ValueLength index) -> bool {
    EXPECT_EQ(seen, index);

    if (seen == 3) {
      return false;
    }
    ++seen;
    return true;
  });

  ASSERT_EQ(3UL, seen);
}

TEST(CollectionTest, IterateArrayValues) {
  std::string const value("[1,2,3,4,null,true,\"foo\",\"bar\"]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t state = 0;
  Collection::forEach(s, [&state](Slice const& value, ValueLength) -> bool {
    switch (state++) {
      case 0:
        EXPECT_TRUE(value.isNumber());
        EXPECT_EQ(1ULL, value.getUInt());
        break;
      case 1:
        EXPECT_TRUE(value.isNumber());
        EXPECT_EQ(2ULL, value.getUInt());
        break;
      case 2:
        EXPECT_TRUE(value.isNumber());
        EXPECT_EQ(3ULL, value.getUInt());
        break;
      case 3:
        EXPECT_TRUE(value.isNumber());
        EXPECT_EQ(4ULL, value.getUInt());
        break;
      case 4:
        EXPECT_TRUE(value.isNull());
        break;
      case 5:
        EXPECT_TRUE(value.isBoolean());
        EXPECT_TRUE(value.getBoolean());
        break;
      case 6:
        EXPECT_TRUE(value.isString());
        EXPECT_EQ("foo", value.copyString());
        break;
      case 7:
        EXPECT_TRUE(value.isString());
        EXPECT_EQ("bar", value.copyString());
        break;
    }
    return true;
  });
  ASSERT_EQ(8U, state);
}

TEST(CollectionTest, AppendArray) {
  std::string const value("[1,2,null,true,\"foo\"]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  Builder b;
  b.openArray();
  Collection::appendArray(b, s);
  b.close();

  ASSERT_EQ(b.toJson(), value);
}

TEST(CollectionTest, FilterNonArray) {
  std::string const value("null");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(Collection::filter(s, DoNothingCallback),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, FilterEmptyArray) {
  std::string const value("[]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  Builder b = Collection::filter(s, FailCallback);

  s = b.slice();
  ASSERT_TRUE(s.isArray());
  ASSERT_EQ(0UL, s.length());
}

TEST(CollectionTest, FilterAll) {
  std::string const value("[1,2,3,4,-42,19]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  Builder b = Collection::filter(s, DoNothingCallback);

  s = b.slice();
  ASSERT_TRUE(s.isArray());
  ASSERT_EQ(0UL, s.length());
}

TEST(CollectionTest, FilterArray) {
  std::string const value("[1,2,3,4,-42,19]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  Builder b = Collection::filter(
      s, [&seen](Slice const& slice, ValueLength index) -> bool {
        EXPECT_EQ(seen, index);
        EXPECT_TRUE(slice.isNumber());

        switch (seen) {
          case 0:
            EXPECT_EQ(1, slice.getInt());
            break;
          case 1:
            EXPECT_EQ(2, slice.getInt());
            break;
          case 2:
            EXPECT_EQ(3, slice.getInt());
            break;
          case 3:
            EXPECT_EQ(4, slice.getInt());
            break;
          case 4:
            EXPECT_EQ(-42, slice.getInt());
            break;
          case 5:
            EXPECT_EQ(19, slice.getInt());
            break;
        }
        ++seen;
        return (index != 4);
      });
  ASSERT_EQ(6UL, seen);

  s = b.slice();
  ASSERT_TRUE(s.isArray());
  ASSERT_EQ(5UL, s.length());

  ASSERT_TRUE(s.at(0).isNumber());
  ASSERT_EQ(1, s.at(0).getInt());

  ASSERT_TRUE(s.at(1).isNumber());
  ASSERT_EQ(2, s.at(1).getInt());

  ASSERT_TRUE(s.at(2).isNumber());
  ASSERT_EQ(3, s.at(2).getInt());

  ASSERT_TRUE(s.at(3).isNumber());
  ASSERT_EQ(4, s.at(3).getInt());

  ASSERT_TRUE(s.at(4).isNumber());
  ASSERT_EQ(19, s.at(4).getInt());
}

TEST(CollectionTest, FindNonArray) {
  std::string const value("null");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(Collection::find(s, DoNothingCallback),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, FindEmptyArray) {
  std::string const value("[]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  Slice found = Collection::find(s, FailCallback);
  ASSERT_TRUE(found.isNone());
}

TEST(CollectionTest, FindArrayFalse) {
  std::string const value("[1,2,3]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  Slice found = Collection::find(s, DoNothingCallback);
  ASSERT_TRUE(found.isNone());
}

TEST(CollectionTest, FindArrayFirst) {
  std::string const value("[1,2,3]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  Slice found = Collection::find(s, [&seen](Slice const&, ValueLength) {
    ++seen;
    return true;
  });
  ASSERT_EQ(1UL, seen);
  ASSERT_TRUE(found.isNumber());
  ASSERT_EQ(1UL, found.getUInt());
}

TEST(CollectionTest, FindArrayLast) {
  std::string const value("[1,2,3]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  Slice found = Collection::find(s, [&seen](Slice const&, ValueLength index) {
    ++seen;
    if (index == 2) {
      return true;
    }
    return false;
  });
  ASSERT_EQ(3UL, seen);
  ASSERT_TRUE(found.isNumber());
  ASSERT_EQ(3UL, found.getUInt());
}

TEST(CollectionTest, ContainsNonArray) {
  std::string const value("null");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(Collection::contains(s, DoNothingCallback),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, ContainsEmptyArray) {
  std::string const value("[]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_FALSE(Collection::contains(s, FailCallback));
}

TEST(CollectionTest, ContainsArrayFalse) {
  std::string const value("[1,2,3]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_FALSE(Collection::contains(s, DoNothingCallback));
}

TEST(CollectionTest, ContainsArrayFirst) {
  std::string const value("[1,2,3]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  ASSERT_TRUE(Collection::contains(s, [&seen](Slice const&, ValueLength) {
    ++seen;
    return true;
  }));
  ASSERT_EQ(1UL, seen);
}

TEST(CollectionTest, ContainsArrayLast) {
  std::string const value("[1,2,3]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  ASSERT_TRUE(Collection::contains(s, [&seen](Slice const&, ValueLength index) {
    ++seen;
    if (index == 2) {
      return true;
    }
    return false;
  }));
  ASSERT_EQ(3UL, seen);
}

TEST(CollectionTest, ContainsArrayUsingIsEqualPredicate) {
  std::string const value("[1,2,3,4,5,6,7,8,9,10,11,12,13,129,141]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::shared_ptr<Builder> b = Parser::fromJson("129");
  IsEqualPredicate predicate(Slice(b->start()));
  ASSERT_TRUE(Collection::contains(s, predicate));
}

TEST(CollectionTest, ContainsArrayUsingIsEqualPredicateNotFound) {
  std::string const value("[1,2,3,4,5,6,7,8,9,10,11,12,13,129,141]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::shared_ptr<Builder> b = Parser::fromJson("-2");
  IsEqualPredicate predicate(Slice(b->start()));
  ASSERT_FALSE(Collection::contains(s, predicate));
}

TEST(CollectionTest, ContainsArrayUsingSlice) {
  std::string const value(
      "[1,2,3,4,5,6,7,8,9,\"foobar\",10,11,12,13,129,\"bazz!!\",141]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::shared_ptr<Builder> b1 = Parser::fromJson("\"bazz!!\"");
  ASSERT_TRUE(Collection::contains(s, Slice(b1->start())));
  std::shared_ptr<Builder> b2 = Parser::fromJson("\"bark\"");
  ASSERT_FALSE(Collection::contains(s, Slice(b2->start())));
  std::shared_ptr<Builder> b3 = Parser::fromJson("141");
  ASSERT_TRUE(Collection::contains(s, Slice(b3->start())));
  ASSERT_FALSE(Collection::contains(s, Slice()));
}

TEST(CollectionTest, IndexOfArray) {
  std::string const value(
      "[1,2,3,4,5,6,7,8,9,\"foobar\",10,11,12,13,129,\"bazz!!\",141]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_EQ(0U, Collection::indexOf(s, Parser::fromJson("1")->slice()));
  ASSERT_EQ(1U, Collection::indexOf(s, Parser::fromJson("2")->slice()));
  ASSERT_EQ(2U, Collection::indexOf(s, Parser::fromJson("3")->slice()));
  ASSERT_EQ(8U, Collection::indexOf(s, Parser::fromJson("9")->slice()));
  ASSERT_EQ(9U, Collection::indexOf(s, Parser::fromJson("\"foobar\"")->slice()));
  ASSERT_EQ(13U, Collection::indexOf(s, Parser::fromJson("13")->slice()));
  ASSERT_EQ(14U, Collection::indexOf(s, Parser::fromJson("129")->slice()));
  ASSERT_EQ(15U,
            Collection::indexOf(s, Parser::fromJson("\"bazz!!\"")->slice()));
  ASSERT_EQ(16U, Collection::indexOf(s, Parser::fromJson("141")->slice()));

  ASSERT_EQ(Collection::NotFound,
            Collection::indexOf(s, Parser::fromJson("\"bazz\"")->slice()));
  ASSERT_EQ(Collection::NotFound,
            Collection::indexOf(s, Parser::fromJson("\"bazz!\"")->slice()));
  ASSERT_EQ(Collection::NotFound,
            Collection::indexOf(s, Parser::fromJson("\"bart\"")->slice()));
  ASSERT_EQ(Collection::NotFound,
            Collection::indexOf(s, Parser::fromJson("99")->slice()));
  ASSERT_EQ(Collection::NotFound,
            Collection::indexOf(s, Parser::fromJson("true")->slice()));
  ASSERT_EQ(Collection::NotFound,
            Collection::indexOf(s, Parser::fromJson("false")->slice()));
  ASSERT_EQ(Collection::NotFound,
            Collection::indexOf(s, Parser::fromJson("null")->slice()));
  ASSERT_EQ(Collection::NotFound,
            Collection::indexOf(s, Parser::fromJson("-1")->slice()));
  ASSERT_EQ(Collection::NotFound,
            Collection::indexOf(s, Parser::fromJson("[]")->slice()));
  ASSERT_EQ(Collection::NotFound,
            Collection::indexOf(s, Parser::fromJson("{}")->slice()));
}

TEST(CollectionTest, AllNonArray) {
  std::string const value("null");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(Collection::all(s, DoNothingCallback),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, AllEmptyArray) {
  std::string const value("[]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_TRUE(Collection::all(s, FailCallback));
}

TEST(CollectionTest, AllArrayFalse) {
  std::string const value("[1,2,3]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_FALSE(Collection::all(s, DoNothingCallback));
}

TEST(CollectionTest, AllArrayFirstFalse) {
  std::string const value("[1,2,3,4]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  ASSERT_FALSE(
      Collection::all(s, [&seen](Slice const&, ValueLength index) -> bool {
        EXPECT_EQ(seen, index);

        ++seen;
        return false;
      }));

  ASSERT_EQ(1UL, seen);
}

TEST(CollectionTest, AllArrayLastFalse) {
  std::string const value("[1,2,3,4]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  ASSERT_FALSE(
      Collection::all(s, [&seen](Slice const&, ValueLength index) -> bool {
        EXPECT_EQ(seen, index);

        ++seen;
        if (index == 2) {
          return false;
        }
        return true;
      }));

  ASSERT_EQ(3UL, seen);
}

TEST(CollectionTest, AllArrayTrue) {
  std::string const value("[1,2,3,4]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  ASSERT_TRUE(
      Collection::all(s, [&seen](Slice const&, ValueLength index) -> bool {
        EXPECT_EQ(seen, index);

        ++seen;
        return true;
      }));

  ASSERT_EQ(4UL, seen);
}

TEST(CollectionTest, AnyNonArray) {
  std::string const value("null");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(Collection::any(s, DoNothingCallback),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, AnyEmptyArray) {
  std::string const value("[]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_FALSE(Collection::any(s, FailCallback));
}

TEST(CollectionTest, AnyArrayFalse) {
  std::string const value("[1,2,3]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_FALSE(Collection::all(s, DoNothingCallback));
}

TEST(CollectionTest, AnyArrayLastTrue) {
  std::string const value("[1,2,3,4]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  ASSERT_TRUE(
      Collection::any(s, [&seen](Slice const&, ValueLength index) -> bool {
        EXPECT_EQ(seen, index);

        ++seen;
        if (index == 3) {
          return true;
        }
        return false;
      }));

  ASSERT_EQ(4UL, seen);
}

TEST(CollectionTest, AnyArrayFirstTrue) {
  std::string const value("[1,2,3,4]");
  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::size_t seen = 0;
  ASSERT_TRUE(
      Collection::any(s, [&seen](Slice const&, ValueLength index) -> bool {
        EXPECT_EQ(seen, index);

        ++seen;
        return true;
      }));

  ASSERT_EQ(1UL, seen);
}

TEST(CollectionTest, ConcatEmpty) {
  std::string const value("[]");

  Parser parser;
  parser.parse(value);
  Slice s1(parser.start());
  Slice s2(parser.start());

  Builder b = Collection::concat(s1, s2);
  Slice s = b.slice();

  ASSERT_TRUE(s.isArray()); 
  ASSERT_EQ(0UL, s.length()); 
}

TEST(CollectionTest, ConcatLeftEmpty) {
  std::string const value1("[]");
  std::string const value2("[1,2,3]");

  Parser parser1;
  parser1.parse(value1);
  Slice s1(parser1.start());

  Parser parser2;
  parser2.parse(value2);
  Slice s2(parser2.start());

  Builder b = Collection::concat(s1, s2);
  Slice s = b.slice();

  ASSERT_TRUE(s.isArray()); 
  ASSERT_EQ(3UL, s.length()); 
  ASSERT_EQ(1UL, s.at(0).getNumber<uint64_t>());
  ASSERT_EQ(2UL, s.at(1).getNumber<uint64_t>());
  ASSERT_EQ(3UL, s.at(2).getNumber<uint64_t>());
}

TEST(CollectionTest, ConcatRightEmpty) {
  std::string const value1("[1,2,3]");
  std::string const value2("[]");

  Parser parser1;
  parser1.parse(value1);
  Slice s1(parser1.start());

  Parser parser2;
  parser2.parse(value2);
  Slice s2(parser2.start());

  Builder b = Collection::concat(s1, s2);
  Slice s = b.slice();

  ASSERT_TRUE(s.isArray()); 
  ASSERT_EQ(3UL, s.length()); 
  ASSERT_EQ(1UL, s.at(0).getNumber<uint64_t>());
  ASSERT_EQ(2UL, s.at(1).getNumber<uint64_t>());
  ASSERT_EQ(3UL, s.at(2).getNumber<uint64_t>());
}

TEST(CollectionTest, ConcatArray) {
  std::string const value1("[1,2,3,4,5,10,42]");
  std::string const value2("[1,2,3]");

  Parser parser1;
  parser1.parse(value1);
  Slice s1(parser1.start());

  Parser parser2;
  parser2.parse(value2);
  Slice s2(parser2.start());

  Builder b = Collection::concat(s1, s2);
  Slice s = b.slice();

  ASSERT_TRUE(s.isArray()); 
  ASSERT_EQ(10UL, s.length()); 
  ASSERT_EQ(1UL, s.at(0).getNumber<uint64_t>());
  ASSERT_EQ(2UL, s.at(1).getNumber<uint64_t>());
  ASSERT_EQ(3UL, s.at(2).getNumber<uint64_t>());
  ASSERT_EQ(4UL, s.at(3).getNumber<uint64_t>());
  ASSERT_EQ(5UL, s.at(4).getNumber<uint64_t>());
  ASSERT_EQ(10UL, s.at(5).getNumber<uint64_t>());
  ASSERT_EQ(42UL, s.at(6).getNumber<uint64_t>());
  ASSERT_EQ(1UL, s.at(7).getNumber<uint64_t>());
  ASSERT_EQ(2UL, s.at(8).getNumber<uint64_t>());
  ASSERT_EQ(3UL, s.at(9).getNumber<uint64_t>());
}

TEST(CollectionTest, ExtractEmpty) {
  std::string const value("[]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  Builder b1 = Collection::extract(s, 0, 10);
  Slice s1 = b1.slice();
  ASSERT_TRUE(s1.isArray()); 
  ASSERT_EQ(0UL, s1.length()); 
  
  Builder b2 = Collection::extract(s, 0, 0);
  Slice s2 = b2.slice();
  ASSERT_TRUE(s2.isArray()); 
  ASSERT_EQ(0UL, s2.length()); 
  
  Builder b3 = Collection::extract(s, 2, -1);
  Slice s3 = b3.slice();
  ASSERT_TRUE(s3.isArray()); 
  ASSERT_EQ(0UL, s3.length()); 
  
  Builder b4 = Collection::extract(s, 4, 10);
  Slice s4 = b4.slice();
  ASSERT_TRUE(s4.isArray()); 
  ASSERT_EQ(0UL, s4.length()); 
}

TEST(CollectionTest, ExtractVarious) {
  std::string const value("[1,2,3,4,5,6]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  Builder b1 = Collection::extract(s, 0, 10);
  Slice s1 = b1.slice();
  ASSERT_TRUE(s1.isArray()); 
  ASSERT_EQ(6UL, s1.length()); 
  ASSERT_EQ(1UL, s1.at(0).getNumber<uint64_t>());
  ASSERT_EQ(2UL, s1.at(1).getNumber<uint64_t>());
  ASSERT_EQ(6UL, s1.at(5).getNumber<uint64_t>());
  
  Builder b2 = Collection::extract(s, 1, 1);
  Slice s2 = b2.slice();
  ASSERT_TRUE(s2.isArray()); 
  ASSERT_EQ(1UL, s2.length()); 
  ASSERT_EQ(2UL, s2.at(0).getNumber<uint64_t>());
  
  Builder b3 = Collection::extract(s, 1, -1);
  Slice s3 = b3.slice();
  ASSERT_TRUE(s3.isArray()); 
  ASSERT_EQ(4UL, s3.length()); 
  ASSERT_EQ(2UL, s3.at(0).getNumber<uint64_t>());
  ASSERT_EQ(5UL, s3.at(3).getNumber<uint64_t>());
  
  Builder b4 = Collection::extract(s, 1, 4);
  Slice s4 = b4.slice();
  ASSERT_TRUE(s4.isArray()); 
  ASSERT_EQ(4UL, s4.length()); 
  ASSERT_EQ(2UL, s4.at(0).getNumber<uint64_t>());
  ASSERT_EQ(5UL, s4.at(3).getNumber<uint64_t>());
  
  Builder b5 = Collection::extract(s, 1, 5);
  Slice s5 = b5.slice();
  ASSERT_TRUE(s5.isArray()); 
  ASSERT_EQ(5UL, s5.length()); 
  ASSERT_EQ(2UL, s5.at(0).getNumber<uint64_t>());
  ASSERT_EQ(6UL, s5.at(4).getNumber<uint64_t>());
  
  Builder b6 = Collection::extract(s, 1, -2);
  Slice s6 = b6.slice();
  ASSERT_TRUE(s6.isArray()); 
  ASSERT_EQ(3UL, s6.length()); 
  ASSERT_EQ(2UL, s6.at(0).getNumber<uint64_t>());
  ASSERT_EQ(4UL, s6.at(2).getNumber<uint64_t>());
}

TEST(CollectionTest, KeepNonObject) {
  std::string const value("[]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> const toKeep = {"foo", "bar"};
  ASSERT_VELOCYPACK_EXCEPTION(Collection::keep(s, toKeep),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, KeepEmptyObject) {
  std::string const value("{}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> const toKeep = {"foo", "bar"};
  Builder b = Collection::keep(s, toKeep);
  s = b.slice();
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(0U, s.length());
}

TEST(CollectionTest, KeepNoAttributes) {
  std::string const value(
      "{\"foo\":\"bar\",\"baz\":\"quux\",\"number\":1,\"boolean\":true,"
      "\"empty\":null}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> const toKeep = {};
  Builder b = Collection::keep(s, toKeep);
  s = b.slice();
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(0U, s.length());
}

TEST(CollectionTest, KeepSomeAttributes) {
  std::string const value(
      "{\"foo\":\"bar\",\"baz\":\"quux\",\"number\":1,\"boolean\":true,"
      "\"empty\":null}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> const toKeep = {"foo", "baz", "empty"};
  Builder b = Collection::keep(s, toKeep);
  s = b.slice();
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(3U, s.length());

  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_EQ("bar", s.get("foo").copyString());

  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_EQ("quux", s.get("baz").copyString());

  ASSERT_TRUE(s.hasKey("empty"));
  ASSERT_TRUE(s.get("empty").isNull());

  ASSERT_FALSE(s.hasKey("number"));
  ASSERT_FALSE(s.hasKey("boolean"));
  ASSERT_FALSE(s.hasKey("quetzalcoatl"));
}

TEST(CollectionTest, KeepSomeAttributesUsingSet) {
  std::string const value(
      "{\"foo\":\"bar\",\"baz\":\"quux\",\"number\":1,\"boolean\":true,"
      "\"empty\":null}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::unordered_set<std::string> const toKeep = {"foo", "baz", "empty"};
  Builder b = Collection::keep(s, toKeep);
  s = b.slice();
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(3U, s.length());

  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_EQ("bar", s.get("foo").copyString());

  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_EQ("quux", s.get("baz").copyString());

  ASSERT_TRUE(s.hasKey("empty"));
  ASSERT_TRUE(s.get("empty").isNull());

  ASSERT_FALSE(s.hasKey("number"));
  ASSERT_FALSE(s.hasKey("boolean"));
  ASSERT_FALSE(s.hasKey("quetzalcoatl"));
}

TEST(CollectionTest, KeepNonExistingAttributes) {
  std::string const value(
      "{\"foo\":\"bar\",\"baz\":\"quux\",\"number\":1,\"boolean\":true,"
      "\"empty\":null}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> const toKeep = {"boo", "far", "quetzalcoatl",
                                           "empty"};
  Builder b = Collection::keep(s, toKeep);
  s = b.slice();
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(1U, s.length());

  ASSERT_TRUE(s.hasKey("empty"));
  ASSERT_TRUE(s.get("empty").isNull());

  ASSERT_FALSE(s.hasKey("foo"));
  ASSERT_FALSE(s.hasKey("baz"));
  ASSERT_FALSE(s.hasKey("number"));
  ASSERT_FALSE(s.hasKey("boolean"));
  ASSERT_FALSE(s.hasKey("quetzalcoatl"));
}

TEST(CollectionTest, KeepNonExistingAttributesUsingSet) {
  std::string const value(
      "{\"foo\":\"bar\",\"baz\":\"quux\",\"number\":1,\"boolean\":true,"
      "\"empty\":null}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::unordered_set<std::string> const toKeep = {"boo", "far", "quetzalcoatl",
                                                  "empty"};
  Builder b = Collection::keep(s, toKeep);
  s = b.slice();
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(1U, s.length());

  ASSERT_TRUE(s.hasKey("empty"));
  ASSERT_TRUE(s.get("empty").isNull());

  ASSERT_FALSE(s.hasKey("foo"));
  ASSERT_FALSE(s.hasKey("baz"));
  ASSERT_FALSE(s.hasKey("number"));
  ASSERT_FALSE(s.hasKey("boolean"));
  ASSERT_FALSE(s.hasKey("quetzalcoatl"));
}

TEST(CollectionTest, KeepManyAttributes) {
  std::string value("{");
  for (std::size_t i = 0; i < 100; ++i) {
    if (i > 0) {
      value.push_back(',');
    }
    value.append("\"test");
    value.append(std::to_string(i));
    value.append("\":");
    value.append(std::to_string(i));
  }
  value.push_back('}');

  std::shared_ptr<Builder> b(Parser::fromJson(value));
  Slice s(b->start());

  std::vector<std::string> toKeep;
  for (std::size_t i = 0; i < 30; ++i) {
    std::string key = "test" + std::to_string(i);
    toKeep.push_back(key);
  }

  *b = Collection::keep(s, toKeep);
  s = b->slice();
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(30U, s.length());

  for (std::size_t i = 0; i < 100; ++i) {
    std::string key = "test" + std::to_string(i);
    if (i < 30) {
      ASSERT_TRUE(s.hasKey(key));
      ASSERT_TRUE(s.get(key).isNumber());
      ASSERT_EQ(i, s.get(key).getUInt());
    } else {
      ASSERT_FALSE(s.hasKey(key));
    }
  }
}

TEST(CollectionTest, RemoveNonObject) {
  std::string const value("[]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> const toRemove = {"foo", "bar"};
  ASSERT_VELOCYPACK_EXCEPTION(Collection::remove(s, toRemove),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, RemoveEmptyObject) {
  std::string const value("{}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> const toRemove = {"foo", "bar"};
  Builder b = Collection::remove(s, toRemove);
  s = b.slice();
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(0U, s.length());
}

TEST(CollectionTest, RemoveNoAttributes) {
  std::string const value(
      "{\"foo\":\"bar\",\"baz\":\"quux\",\"number\":1,\"boolean\":true,"
      "\"empty\":null}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> const toRemove = {};
  Builder b = Collection::remove(s, toRemove);
  s = b.slice();
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(5U, s.length());

  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_EQ("bar", s.get("foo").copyString());
  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_EQ("quux", s.get("baz").copyString());
  ASSERT_TRUE(s.hasKey("number"));
  ASSERT_EQ(1U, s.get("number").getUInt());
  ASSERT_TRUE(s.hasKey("boolean"));
  ASSERT_TRUE(s.get("boolean").getBoolean());
  ASSERT_TRUE(s.hasKey("empty"));
  ASSERT_TRUE(s.get("empty").isNull());
}

TEST(CollectionTest, RemoveSomeAttributes) {
  std::string const value(
      "{\"foo\":\"bar\",\"baz\":\"quux\",\"number\":1,\"boolean\":true,"
      "\"empty\":null}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> const toRemove = {"foo", "baz", "empty"};
  Builder b = Collection::remove(s, toRemove);
  s = b.slice();
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(2U, s.length());

  ASSERT_FALSE(s.hasKey("foo"));
  ASSERT_FALSE(s.hasKey("baz"));
  ASSERT_FALSE(s.hasKey("empty"));

  ASSERT_TRUE(s.hasKey("number"));
  ASSERT_EQ(1U, s.get("number").getUInt());
  ASSERT_TRUE(s.hasKey("boolean"));
  ASSERT_TRUE(s.get("boolean").getBoolean());
}

TEST(CollectionTest, RemoveSomeAttributesUsingSet) {
  std::string const value(
      "{\"foo\":\"bar\",\"baz\":\"quux\",\"number\":1,\"boolean\":true,"
      "\"empty\":null}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::unordered_set<std::string> const toRemove = {"foo", "baz", "empty"};
  Builder b = Collection::remove(s, toRemove);
  s = b.slice();
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(2U, s.length());

  ASSERT_FALSE(s.hasKey("foo"));
  ASSERT_FALSE(s.hasKey("baz"));
  ASSERT_FALSE(s.hasKey("empty"));

  ASSERT_TRUE(s.hasKey("number"));
  ASSERT_EQ(1U, s.get("number").getUInt());
  ASSERT_TRUE(s.hasKey("boolean"));
  ASSERT_TRUE(s.get("boolean").getBoolean());
}

TEST(CollectionTest, RemoveManyAttributes) {
  std::string value("{");
  for (std::size_t i = 0; i < 100; ++i) {
    if (i > 0) {
      value.push_back(',');
    }
    value.append("\"test");
    value.append(std::to_string(i));
    value.append("\":");
    value.append(std::to_string(i));
  }
  value.push_back('}');

  std::shared_ptr<Builder> b = Parser::fromJson(value);
  Slice s(b->start());

  std::vector<std::string> toRemove;
  for (std::size_t i = 0; i < 30; ++i) {
    std::string key = "test" + std::to_string(i);
    toRemove.push_back(key);
  }

  *b = Collection::remove(s, toRemove);
  s = b->slice();
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(70U, s.length());

  for (std::size_t i = 0; i < 100; ++i) {
    std::string key = "test" + std::to_string(i);
    if (i >= 30) {
      ASSERT_TRUE(s.hasKey(key));
      ASSERT_TRUE(s.get(key).isNumber());
      ASSERT_EQ(i, s.get(key).getUInt());
    } else {
      ASSERT_FALSE(s.hasKey(key));
    }
  }
}

TEST(CollectionTest, RemoveNonExistingAttributes) {
  std::string const value(
      "{\"foo\":\"bar\",\"baz\":\"quux\",\"number\":1,\"boolean\":true,"
      "\"empty\":null}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::vector<std::string> const toRemove = {"boo", "far", "quetzalcoatl",
                                             "empty"};
  Builder b = Collection::remove(s, toRemove);
  s = b.slice();
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(4U, s.length());

  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_EQ("bar", s.get("foo").copyString());
  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_EQ("quux", s.get("baz").copyString());
  ASSERT_TRUE(s.hasKey("number"));
  ASSERT_EQ(1UL, s.get("number").getUInt());
  ASSERT_TRUE(s.hasKey("boolean"));
  ASSERT_TRUE(s.get("boolean").getBoolean());
  ASSERT_FALSE(s.hasKey("empty"));
}

TEST(CollectionTest, RemoveNonExistingAttributesUsingSet) {
  std::string const value(
      "{\"foo\":\"bar\",\"baz\":\"quux\",\"number\":1,\"boolean\":true,"
      "\"empty\":null}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  std::unordered_set<std::string> const toRemove = {"boo", "far",
                                                    "quetzalcoatl", "empty"};
  Builder b = Collection::remove(s, toRemove);
  s = b.slice();
  ASSERT_TRUE(s.isObject());
  ASSERT_EQ(4U, s.length());

  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_EQ("bar", s.get("foo").copyString());
  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_EQ("quux", s.get("baz").copyString());
  ASSERT_TRUE(s.hasKey("number"));
  ASSERT_EQ(1UL, s.get("number").getUInt());
  ASSERT_TRUE(s.hasKey("boolean"));
  ASSERT_TRUE(s.get("boolean").getBoolean());
  ASSERT_FALSE(s.hasKey("empty"));
}

TEST(CollectionTest, MergeNonObject) {
  Builder b1;
  b1.add(Value(ValueType::Array));
  b1.close();

  Builder b2;
  b2.add(Value(ValueType::Object));
  b2.close();

  ASSERT_VELOCYPACK_EXCEPTION(Collection::merge(b1.slice(), b1.slice(), false, false),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(Collection::merge(b1.slice(), b2.slice(), false, false),
                              Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(Collection::merge(b2.slice(), b1.slice(), false, false),
                              Exception::InvalidValueType);
}

TEST(CollectionTest, MergeEmptyLeft) {
  std::string const l("{}");
  std::string const r("{\"bark\":1,\"qux\":2,\"bart\":3}");

  std::shared_ptr<Builder> p1 = Parser::fromJson(l);
  Slice s1(p1->start());

  std::shared_ptr<Builder> p2 = Parser::fromJson(r);
  Slice s2(p2->start());

  Builder b = Collection::merge(s1, s2, true, false);
  Slice s(b.start());
  ASSERT_TRUE(s.hasKey("bark"));
  ASSERT_EQ(1UL, s.get("bark").getUInt());
  ASSERT_TRUE(s.hasKey("qux"));
  ASSERT_EQ(2UL, s.get("qux").getUInt());
  ASSERT_TRUE(s.hasKey("bart"));
  ASSERT_EQ(3UL, s.get("bart").getUInt());
}

TEST(CollectionTest, MergeEmptyRight) {
  std::string const l("{\"bark\":1,\"qux\":2,\"bart\":3}");
  std::string const r("{}");

  std::shared_ptr<Builder> p1 = Parser::fromJson(l);
  Slice s1(p1->start());

  std::shared_ptr<Builder> p2 = Parser::fromJson(r);
  Slice s2(p2->start());

  Builder b = Collection::merge(s1, s2, true, false);
  Slice s(b.start());
  ASSERT_TRUE(s.hasKey("bark"));
  ASSERT_EQ(1UL, s.get("bark").getUInt());
  ASSERT_TRUE(s.hasKey("qux"));
  ASSERT_EQ(2UL, s.get("qux").getUInt());
  ASSERT_TRUE(s.hasKey("bart"));
  ASSERT_EQ(3UL, s.get("bart").getUInt());
}

TEST(CollectionTest, MergeDistinct) {
  std::string const l("{\"foo\":1,\"bar\":2,\"baz\":3}");
  std::string const r("{\"bark\":1,\"qux\":2,\"bart\":3}");

  std::shared_ptr<Builder> p1 = Parser::fromJson(l);
  Slice s1(p1->start());

  std::shared_ptr<Builder> p2 = Parser::fromJson(r);
  Slice s2(p2->start());

  Builder b = Collection::merge(s1, s2, true, false);
  Slice s(b.start());
  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_EQ(1UL, s.get("foo").getUInt());
  ASSERT_TRUE(s.hasKey("bar"));
  ASSERT_EQ(2UL, s.get("bar").getUInt());
  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_EQ(3UL, s.get("baz").getUInt());
  ASSERT_TRUE(s.hasKey("bark"));
  ASSERT_EQ(1UL, s.get("bark").getUInt());
  ASSERT_TRUE(s.hasKey("qux"));
  ASSERT_EQ(2UL, s.get("qux").getUInt());
  ASSERT_TRUE(s.hasKey("bart"));
  ASSERT_EQ(3UL, s.get("bart").getUInt());
}

TEST(CollectionTest, MergeOverlap) {
  std::string const l("{\"foo\":1,\"bar\":2,\"baz\":3}");
  std::string const r(
      "{\"baz\":19,\"bark\":1,\"qux\":2,\"bar\":42,\"test\":9,\"foo\":12}");

  std::shared_ptr<Builder> p1 = Parser::fromJson(l);
  Slice s1(p1->start());

  std::shared_ptr<Builder> p2 = Parser::fromJson(r);
  Slice s2(p2->start());

  Builder b = Collection::merge(s1, s2, true, false);
  Slice s(b.start());
  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_EQ(12UL, s.get("foo").getUInt());
  ASSERT_TRUE(s.hasKey("bar"));
  ASSERT_EQ(42UL, s.get("bar").getUInt());
  ASSERT_TRUE(s.hasKey("baz"));
  ASSERT_EQ(19UL, s.get("baz").getUInt());
  ASSERT_TRUE(s.hasKey("bark"));
  ASSERT_EQ(1UL, s.get("bark").getUInt());
  ASSERT_TRUE(s.hasKey("qux"));
  ASSERT_EQ(2UL, s.get("qux").getUInt());
  ASSERT_TRUE(s.hasKey("test"));
  ASSERT_EQ(9UL, s.get("test").getUInt());
}

TEST(CollectionTest, MergeSubAttributes) {
  std::string const l(
      "{\"foo\":1,\"bar\":{\"one\":1,\"two\":2,\"three\":3},\"baz\":{},"
      "\"test\":1}");
  std::string const r(
      "{\"foo\":2,\"bar\":{\"one\":23,\"two\":42,\"four\":99},\"baz\":{"
      "\"test\":1,\"bart\":2}}");

  std::shared_ptr<Builder> p1 = Parser::fromJson(l);
  Slice s1(p1->start());

  std::shared_ptr<Builder> p2 = Parser::fromJson(r);
  Slice s2(p2->start());

  Builder b = Collection::merge(s1, s2, true, false);
  Slice s(b.start());
  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_EQ(2UL, s.get("foo").getUInt());
  ASSERT_TRUE(s.hasKey("bar"));
  Slice sub = s.get("bar");
  ASSERT_TRUE(sub.hasKey("one"));
  ASSERT_EQ(23UL, sub.get("one").getUInt());
  ASSERT_TRUE(sub.hasKey("two"));
  ASSERT_EQ(42UL, sub.get("two").getUInt());
  ASSERT_TRUE(sub.hasKey("three"));
  ASSERT_EQ(3UL, sub.get("three").getUInt());
  ASSERT_TRUE(sub.hasKey("four"));
  ASSERT_EQ(99UL, sub.get("four").getUInt());
  ASSERT_TRUE(s.hasKey("test"));
  ASSERT_EQ(1UL, s.get("test").getUInt());
  ASSERT_TRUE(s.hasKey("baz"));
  sub = s.get("baz");
  ASSERT_EQ(2UL, sub.length());
  ASSERT_TRUE(sub.hasKey("test"));
  ASSERT_EQ(1UL, sub.get("test").getUInt());
  ASSERT_TRUE(sub.hasKey("bart"));
  ASSERT_EQ(2UL, sub.get("bart").getUInt());
}

TEST(CollectionTest, MergeOverwriteSubAttributes) {
  std::string const l(
      "{\"foo\":1,\"bar\":{\"one\":1,\"two\":2,\"three\":3},\"baz\":{\"bird\":"
      "9},\"test\":1}");
  std::string const r(
      "{\"foo\":2,\"bar\":{\"one\":23,\"two\":42,\"four\":99},\"baz\":{"
      "\"test\":1,\"bart\":2}}");

  std::shared_ptr<Builder> p1 = Parser::fromJson(l);
  Slice s1(p1->start());

  std::shared_ptr<Builder> p2 = Parser::fromJson(r);
  Slice s2(p2->start());

  Builder b = Collection::merge(s1, s2, false, false);
  Slice s(b.start());
  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_EQ(2UL, s.get("foo").getUInt());
  ASSERT_TRUE(s.hasKey("bar"));
  Slice sub = s.get("bar");
  ASSERT_TRUE(sub.hasKey("one"));
  ASSERT_EQ(23UL, sub.get("one").getUInt());
  ASSERT_TRUE(sub.hasKey("two"));
  ASSERT_EQ(42UL, sub.get("two").getUInt());
  ASSERT_FALSE(sub.hasKey("three"));
  ASSERT_TRUE(sub.hasKey("four"));
  ASSERT_EQ(99UL, sub.get("four").getUInt());
  ASSERT_TRUE(s.hasKey("test"));
  ASSERT_EQ(1UL, s.get("test").getUInt());
  ASSERT_TRUE(s.hasKey("baz"));
  sub = s.get("baz");
  ASSERT_EQ(2UL, sub.length());
  ASSERT_FALSE(sub.hasKey("bird"));
  ASSERT_TRUE(sub.hasKey("test"));
  ASSERT_EQ(1UL, sub.get("test").getUInt());
  ASSERT_TRUE(sub.hasKey("bart"));
  ASSERT_EQ(2UL, sub.get("bart").getUInt());
}

TEST(CollectionTest, MergeNullMeansRemove) {
  std::string const l("{\"foo\":1,\"bar\":2,\"baz\":3}");
  std::string const r(
      "{\"baz\":null,\"bark\":1,\"qux\":null,\"bar\":null,\"test\":9}");

  std::shared_ptr<Builder> p1 = Parser::fromJson(l);
  Slice s1(p1->start());

  std::shared_ptr<Builder> p2 = Parser::fromJson(r);
  Slice s2(p2->start());

  Builder b = Collection::merge(s1, s2, true, true);
  Slice s(b.start());
  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_EQ(1UL, s.get("foo").getUInt());
  ASSERT_FALSE(s.hasKey("bar"));
  ASSERT_FALSE(s.hasKey("baz"));
  ASSERT_TRUE(s.hasKey("bark"));
  ASSERT_EQ(1UL, s.get("bark").getUInt());
  ASSERT_FALSE(s.hasKey("qux"));
  ASSERT_TRUE(s.hasKey("test"));
  ASSERT_EQ(9UL, s.get("test").getUInt());
}

TEST(CollectionTest, MergeNullMeansRemoveRecursive) {
  std::string const l("{\"foo\":1,\"bar\":{\"bart\":null,\"barto\":4},\"baz\":3,\"baz\":{\"foo\":1}}");
  std::string const r(
      "{\"bar\":{\"barto\":null,\"barko\":7},\"baz\":null}");

  std::shared_ptr<Builder> p1 = Parser::fromJson(l);
  Slice s1(p1->start());

  std::shared_ptr<Builder> p2 = Parser::fromJson(r);
  Slice s2(p2->start());

  Builder b = Collection::merge(s1, s2, true, true);
  Slice s(b.start());
  ASSERT_TRUE(s.hasKey("foo"));
  ASSERT_EQ(1UL, s.get("foo").getUInt());
  ASSERT_TRUE(s.hasKey("bar"));
  ASSERT_TRUE(s.hasKey(std::vector<std::string>({ "bar", "bart" })));
  ASSERT_TRUE(s.get(std::vector<std::string>({ "bar", "bart" })).isNull());
  ASSERT_FALSE(s.hasKey(std::vector<std::string>({ "bar", "barto" })));
  ASSERT_TRUE(s.hasKey(std::vector<std::string>({ "bar", "barko" })));
  ASSERT_EQ(7UL, s.get(std::vector<std::string>({ "bar", "barko" })).getUInt());
  ASSERT_FALSE(s.hasKey("baz"));
}

TEST(CollectionTest, VisitRecursiveNonCompound) {
  std::string const value("[1,null,true,\"foo\"]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  ASSERT_VELOCYPACK_EXCEPTION(
      Collection::visitRecursive(
          s.at(0), Collection::PreOrder,
          [](Slice const&, Slice const&) -> bool { return true; }),
      Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(
      Collection::visitRecursive(
          s.at(1), Collection::PreOrder,
          [](Slice const&, Slice const&) -> bool { return true; }),
      Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(
      Collection::visitRecursive(
          s.at(2), Collection::PreOrder,
          [](Slice const&, Slice const&) -> bool { return true; }),
      Exception::InvalidValueType);
  ASSERT_VELOCYPACK_EXCEPTION(
      Collection::visitRecursive(
          s.at(3), Collection::PreOrder,
          [](Slice const&, Slice const&) -> bool { return true; }),
      Exception::InvalidValueType);
}

TEST(CollectionTest, VisitRecursiveArrayPreOrderAbort) {
  std::string const value("[true, false, 1]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  int seen = 0;
  Collection::visitRecursive(
      s, Collection::PreOrder,
      [&seen](Slice const& key, Slice const& value) -> bool {
        EXPECT_TRUE(key.isNone());
        switch (seen) {
          case 0:
            EXPECT_TRUE(value.isTrue());
            break;
          case 1:
            EXPECT_TRUE(value.isFalse());
            return false;
          default:
            throw "invalid state";
        }
        ++seen;
        return true;
      });

  ASSERT_EQ(1, seen);
}

TEST(CollectionTest, VisitRecursiveArrayPostOrderAbort) {
  std::string const value("[true, [null], false, 1]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  int seen = 0;
  Collection::visitRecursive(
      s, Collection::PostOrder,
      [&seen](Slice const& key, Slice const& value) -> bool {
        EXPECT_TRUE(key.isNone());
        switch (seen) {
          case 0:
            EXPECT_TRUE(value.isTrue());
            break;
          case 1:
            EXPECT_TRUE(value.isArray());
            EXPECT_EQ(1UL, value.length());
            break;
          case 2:
            EXPECT_TRUE(value.isNull());
            return false;
          default:
            throw "invalid state";
        }
        ++seen;
        return true;
      });

  ASSERT_EQ(2, seen);
}

TEST(CollectionTest, VisitRecursiveObjectPreOrderAbort) {
  std::string const value("{\"foo\":true,\"bar\":false,\"baz\":1}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  int seen = 0;
  Collection::visitRecursive(
      s, Collection::PreOrder,
      [&seen](Slice const& key, Slice const& value) -> bool {
        EXPECT_FALSE(key.isNone());
        switch (seen) {
          case 0:
            EXPECT_EQ("bar", key.copyString());
            EXPECT_TRUE(value.isFalse());
            break;
          case 1:
            EXPECT_EQ("baz", key.copyString());
            EXPECT_TRUE(value.isSmallInt() && value.getInt() == 1);
            return false;
          default:
            throw "invalid state";
        }
        ++seen;
        return true;
      });

  ASSERT_EQ(1, seen);
}

TEST(CollectionTest, VisitRecursiveObjectPostOrderAbort) {
  std::string const value("{\"foo\":{\"baz\":1,\"bar\":2},\"bark\":3}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  int seen = 0;
  Collection::visitRecursive(
      s, Collection::PostOrder,
      [&seen](Slice const& key, Slice const& value) -> bool {
        EXPECT_FALSE(key.isNone());
        switch (seen) {
          case 0:
            EXPECT_EQ("bark", key.copyString());
            EXPECT_TRUE(value.isNumber());
            EXPECT_EQ(3UL, value.getUInt());
            break;
          case 1:
            EXPECT_EQ("foo", key.copyString());
            EXPECT_TRUE(value.isObject());
            return false;
          default:
            throw "invalid state";
        }
        ++seen;
        return true;
      });

  ASSERT_EQ(1, seen);
}

TEST(CollectionTest, VisitRecursiveArrayPreOrder) {
  std::string const value("[1,null,true,\"foo\",[23,42],false,[]]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  int seen = 0;
  Collection::visitRecursive(
      s, Collection::PreOrder,
      [&seen](Slice const& key, Slice const& value) -> bool {
        EXPECT_TRUE(key.isNone());
        switch (seen) {
          case 0:
            EXPECT_TRUE(value.isNumber());
            EXPECT_EQ(1UL, value.getUInt());
            break;
          case 1:
            EXPECT_TRUE(value.isNull());
            break;
          case 2:
            EXPECT_TRUE(value.isTrue());
            break;
          case 3:
            EXPECT_TRUE(value.isString());
            EXPECT_EQ("foo", value.copyString());
            break;
          case 4:
            EXPECT_TRUE(value.isNumber());
            EXPECT_EQ(23UL, value.getUInt());
            break;
          case 5:
            EXPECT_TRUE(value.isNumber());
            EXPECT_EQ(42UL, value.getUInt());
            break;
          case 6:
            EXPECT_TRUE(value.isArray());
            EXPECT_EQ(2UL, value.length());
            break;
          case 7:
            EXPECT_TRUE(value.isFalse());
            break;
          case 8:
            EXPECT_TRUE(value.isArray());
            EXPECT_EQ(0UL, value.length());
            break;
        }
        ++seen;
        return true;
      });

  ASSERT_EQ(9, seen);
}

TEST(CollectionTest, VisitRecursiveArrayPostOrder) {
  std::string const value("[1,null,true,\"foo\",[23,42],false,[]]");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  int seen = 0;
  Collection::visitRecursive(
      s, Collection::PostOrder,
      [&seen](Slice const& key, Slice const& value) -> bool {
        EXPECT_TRUE(key.isNone());
        switch (seen) {
          case 0:
            EXPECT_TRUE(value.isNumber());
            EXPECT_EQ(1UL, value.getUInt());
            break;
          case 1:
            EXPECT_TRUE(value.isNull());
            break;
          case 2:
            EXPECT_TRUE(value.isTrue());
            break;
          case 3:
            EXPECT_TRUE(value.isString());
            EXPECT_EQ("foo", value.copyString());
            break;
          case 4:
            EXPECT_TRUE(value.isArray());
            EXPECT_EQ(2UL, value.length());
            break;
          case 5:
            EXPECT_TRUE(value.isNumber());
            EXPECT_EQ(23UL, value.getUInt());
            break;
          case 6:
            EXPECT_TRUE(value.isNumber());
            EXPECT_EQ(42UL, value.getUInt());
            break;
          case 7:
            EXPECT_TRUE(value.isFalse());
            break;
          case 8:
            EXPECT_TRUE(value.isArray());
            EXPECT_EQ(0UL, value.length());
            break;
        }
        ++seen;
        return true;
      });

  ASSERT_EQ(9, seen);
}

TEST(CollectionTest, VisitRecursiveObjectPreOrder) {
  std::string const value(
      "{\"foo\":1,\"bar\":null,\"baz\":true,\"bark\":{\"qux\":23,\"quetzal\":"
      "42},\"quux\":{}}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  int seen = 0;
  Collection::visitRecursive(
      s, Collection::PreOrder,
      [&seen](Slice const& key, Slice const& value) -> bool {
        EXPECT_FALSE(key.isNone());
        switch (seen) {
          case 0:
            EXPECT_EQ("bar", key.copyString());
            EXPECT_TRUE(value.isNull());
            break;
          case 1:
            EXPECT_EQ("quetzal", key.copyString());
            EXPECT_TRUE(value.isNumber());
            EXPECT_EQ(42UL, value.getUInt());
            break;
          case 2:
            EXPECT_EQ("qux", key.copyString());
            EXPECT_TRUE(value.isNumber());
            EXPECT_EQ(23UL, value.getUInt());
            break;
          case 3:
            EXPECT_EQ("bark", key.copyString());
            EXPECT_TRUE(value.isObject());
            EXPECT_EQ(2UL, value.length());
            break;
          case 4:
            EXPECT_EQ("baz", key.copyString());
            EXPECT_TRUE(value.isTrue());
            break;
          case 5:
            EXPECT_EQ("foo", key.copyString());
            EXPECT_TRUE(value.isNumber());
            EXPECT_EQ(1UL, value.getUInt());
            break;
          case 6:
            EXPECT_EQ("quux", key.copyString());
            EXPECT_TRUE(value.isObject());
            EXPECT_EQ(0UL, value.length());
            break;
        }
        ++seen;
        return true;
      });

  ASSERT_EQ(7, seen);
}

TEST(CollectionTest, VisitRecursiveObjectPostOrder) {
  std::string const value(
      "{\"foo\":1,\"bar\":null,\"baz\":true,\"bark\":{\"qux\":23,\"quetzal\":"
      "42},\"quux\":{}}");

  Parser parser;
  parser.parse(value);
  Slice s(parser.start());

  int seen = 0;
  Collection::visitRecursive(
      s, Collection::PostOrder,
      [&seen](Slice const& key, Slice const& value) -> bool {
        EXPECT_FALSE(key.isNone());
        switch (seen) {
          case 0:
            EXPECT_EQ("bar", key.copyString());
            EXPECT_TRUE(value.isNull());
            break;
          case 1:
            EXPECT_EQ("bark", key.copyString());
            EXPECT_TRUE(value.isObject());
            EXPECT_EQ(2UL, value.length());
            break;
          case 2:
            EXPECT_EQ("quetzal", key.copyString());
            EXPECT_TRUE(value.isNumber());
            EXPECT_EQ(42UL, value.getUInt());
            break;
          case 3:
            EXPECT_EQ("qux", key.copyString());
            EXPECT_TRUE(value.isNumber());
            EXPECT_EQ(23UL, value.getUInt());
            break;
          case 4:
            EXPECT_EQ("baz", key.copyString());
            EXPECT_TRUE(value.isTrue());
            break;
          case 5:
            EXPECT_EQ("foo", key.copyString());
            EXPECT_TRUE(value.isNumber());
            EXPECT_EQ(1UL, value.getUInt());
            break;
          case 6:
            EXPECT_EQ("quux", key.copyString());
            EXPECT_TRUE(value.isObject());
            EXPECT_EQ(0UL, value.length());
            break;
        }
        ++seen;
        return true;
      });

  ASSERT_EQ(7, seen);
}

static bool lt(Slice const a, Slice const b) {
  if (! a.isInteger() || ! b.isInteger()) {
    return false;
  }
  return a.getInt() < b.getInt();
}

TEST(CollectionTest, Sort) {
  int const NUMBER = 3;
  Builder b;
  b.openArray();
  for (int i = NUMBER - 1; i >= 0; --i) {
    b.add(Value(i));
  }
  b.close();
  Builder b2 = Collection::sort(b.slice(), &lt);
  Slice const s(b2.slice());
  ASSERT_TRUE(s.isArray());
  ASSERT_EQ(static_cast<uint64_t>(NUMBER), s.length());
  for (int i = 0; i < NUMBER; i++) {
    Slice const ss = s[i];
    ASSERT_TRUE(ss.isInteger());
    ASSERT_EQ(static_cast<int64_t>(i), ss.getInt());
  }
}

TEST(CollectionTest, SortNonArray) {
  Builder b;
  b.add(Value("foo"));
  
  ASSERT_VELOCYPACK_EXCEPTION(Collection::sort(b.slice(), &lt), Exception::InvalidValueType);
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
