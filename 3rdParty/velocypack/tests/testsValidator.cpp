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

#include <ostream>
#include <string>

#include "tests-common.h"

TEST(ValidatorTest, NoOptions) {
  ASSERT_VELOCYPACK_EXCEPTION(Validator(nullptr), Exception::InternalError);
}

TEST(ValidatorTest, ReservedValue1) {
  std::string const value("\x15", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidType);
}

TEST(ValidatorTest, ReservedValue2) {
  std::string const value("\x16", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidType);
}

TEST(ValidatorTest, ReservedValue3) {
  std::string const value("\xd8", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidType);
}

TEST(ValidatorTest, NoneValue) {
  std::string const value("\x00", 1);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, NullValue) {
  std::string const value("\x18", 1);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, NullValueWithExtra) {
  std::string const value("\x18\x41", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, FalseValue) {
  std::string const value("\x19", 1);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, FalseValueWithExtra) {
  std::string const value("\x19\x41", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, TrueValue) {
  std::string const value("\x1a", 1);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, TrueValueWithExtra) {
  std::string const value("\x1a\x41", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, Illegal) {
  std::string const value("\x17", 1);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, IllegalWithExtra) {
  std::string const value("\x17\x41", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, MinKey) {
  std::string const value("\x1e", 1);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, MinKeyWithExtra) {
  std::string const value("\x1e\x41", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, MaxKey) {
  std::string const value("\x1f", 1);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, MaxKeyWithExtra) {
  std::string const value("\x1f\x41", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, DoubleValue) {
  std::string const value("\x1b\x00\x00\x00\x00\x00\x00\x00\x00", 9);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, DoubleValueTruncated) {
  std::string const value("\x1b", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, DoubleValueTooShort) {
  std::string const value("\x1b\x00\x00\x00\x00\x00\x00\x00", 8);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, DoubleValueTooLong) {
  std::string const value("\x1b\x00\x00\x00\x00\x00\x00\x00\x00\x00", 10);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, UTCDate) {
  std::string const value("\x1c\x00\x00\x00\x00\x00\x00\x00\x00", 9);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, UTCDateTruncated) {
  std::string const value("\x1c", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, UTCDateTooShort) {
  std::string const value("\x1c\x00\x00\x00\x00\x00\x00\x00", 8);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, UTCDateTooLong) {
  std::string const value("\x1c\x00\x00\x00\x00\x00\x00\x00\x00\x00", 10);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, SmallInt) {
  for (uint8_t i = 0; i <= 9; ++i) {
    std::string value;
    value.push_back(0x30 + i);

    Validator validator;
    ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
  }
}

TEST(ValidatorTest, SmallIntWithExtra) {
  std::string const value("\x30\x41", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, SmallIntNegative) {
  for (uint8_t i = 0; i <= 5; ++i) {
    std::string value;
    value.push_back(0x3a + i);

    Validator validator;
    ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
  }
}

TEST(ValidatorTest, SmallIntNegativeWithExtra) {
  std::string const value("\x3a\x41", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, IntPositiveOneByte) {
  std::string const value("\x20\x00", 2);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, IntPositiveOneByteTooShort) {
  std::string const value("\x20", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, IntPositiveOneByteWithExtra) {
  std::string const value("\x20\x00\x41", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, IntPositiveTwoBytes) {
  std::string const value("\x21\x00\x00", 3);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, IntPositiveTwoBytesTooShort) {
  std::string const value("\x21", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, IntPositiveTwoBytesWithExtra) {
  std::string const value("\x21\x00\x00\x41", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, IntPositiveEightBytes) {
  std::string const value("\x27\x00\x00\x00\x00\x00\x00\x00\x00", 9);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, IntPositiveEightBytesTooShort) {
  std::string const value("\x27", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, IntPositiveEightBytesWithExtra) {
  std::string const value("\x27\x00\x00\x00\x00\x00\x00\x00\x00\x41", 10);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, UIntPositiveOneByte) {
  std::string const value("\x28\x00", 2);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, UIntPositiveOneByteTooShort) {
  std::string const value("\x28", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, UIntPositiveOneByteWithExtra) {
  std::string const value("\x28\x00\x41", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, UIntPositiveTwoBytes) {
  std::string const value("\x29\x00\x00", 3);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, UIntPositiveTwoBytesTooShort) {
  std::string const value("\x29", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, UIntPositiveTwoBytesWithExtra) {
  std::string const value("\x29\x00\x00\x41", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, UIntPositiveEightBytes) {
  std::string const value("\x2f\x00\x00\x00\x00\x00\x00\x00\x00", 9);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, UIntPositiveEightBytesTooShort) {
  std::string const value("\x2f", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, UIntPositiveEightBytesWithExtra) {
  std::string const value("\x2f\x00\x00\x00\x00\x00\x00\x00\x00\x41", 10);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, StringEmpty) {
  std::string const value("\x40", 1);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, StringEmptyWithExtra) {
  std::string const value("\x40\x41", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, StringValidLength) {
  std::string const value("\x43\x41\x42\x43", 4);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, StringLongerThanSpecified) {
  std::string const value("\x42\x41\x42\x43", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, StringShorterThanSpecified) {
  std::string const value("\x43\x41\x42", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, StringValidUtf8Empty) {
  std::string const value("\x40", 1);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, StringValidUtf8OneByte) {
  std::string const value("\x41\x0a", 2);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, StringValidUtf8TwoBytes) {
  std::string const value("\x42\xc2\xa2", 3);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, StringValidUtf8ThreeBytes) {
  std::string const value("\x43\xe2\x82\xac", 4);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, StringValidUtf8FourBytes) {
  std::string const value("\x44\xf0\xa4\xad\xa2", 5);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, StringValidUtf8Long) {
  std::string const value("\xbf\x04\x00\x00\x00\x00\x00\x00\x00\x40\x41\x42\x43", 13);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, StringInvalidUtf8NoValidation) {
  std::string const value("\x41\xff", 2);

  Options options;
  options.validateUtf8Strings = false;
  Validator validator(&options);
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, StringInvalidUtf8WithValidation1) {
  std::string const value("\x41\x80", 2);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::InvalidUtf8Sequence);
}

TEST(ValidatorTest, StringInvalidUtf8WithValidation2) {
  std::string const value("\x41\xff", 2);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::InvalidUtf8Sequence);
}

TEST(ValidatorTest, StringInvalidUtf8WithValidation3) {
  std::string const value("\x42\xff\x70", 3);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::InvalidUtf8Sequence);
}

TEST(ValidatorTest, StringInvalidUtf8WithValidation4) {
  std::string const value("\x43\xff\xff\x07", 4);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::InvalidUtf8Sequence);
}

TEST(ValidatorTest, StringInvalidUtf8WithValidation5) {
  std::string const value("\x44\xff\xff\xff\x07", 5);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::InvalidUtf8Sequence);
}

TEST(ValidatorTest, StringInvalidUtf8Long) {
  std::string const value("\xbf\x04\x00\x00\x00\x00\x00\x00\x00\xff\xff\xff\x07", 13);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::InvalidUtf8Sequence);
}

TEST(ValidatorTest, StringValidUtf8ObjectWithValidation) {
  std::string const value("\x0b\x08\x01\x41\x41\x41\x41\x03", 8);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, StringInvalidUtf8ObjectKeyWithValidation) {
  std::string const value("\x0b\x08\x01\x41\x80\x41\x41\x03", 8);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::InvalidUtf8Sequence);
}

TEST(ValidatorTest, StringInvalidUtf8ObjectValueWithValidation) {
  std::string const value("\x0b\x08\x01\x41\x41\x41\x80\x03", 8);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::InvalidUtf8Sequence);
}

TEST(ValidatorTest, StringInvalidUtf8ObjectLongKeyWithValidation) {
  std::string const value("\x0b\x10\x01\xbf\x01\x00\x00\x00\x00\x00\x00\x00\x80\x41\x41\x03", 16);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::InvalidUtf8Sequence);
}

TEST(ValidatorTest, StringInvalidUtf8ObjectLongValueWithValidation) {
  std::string const value("\x0b\x10\x01\x41\x41\xbf\x01\x00\x00\x00\x00\x00\x00\x00\x80\x03", 16);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::InvalidUtf8Sequence);
}

TEST(ValidatorTest, StringValidUtf8CompactObjectWithValidation) {
  std::string const value("\x14\x07\x41\x41\x41\x41\x01", 7);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, StringInvalidUtf8CompactObjectKeyWithValidation) {
  std::string const value("\x14\x07\x41\x80\x41\x41\x01", 7);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::InvalidUtf8Sequence);
}

TEST(ValidatorTest, StringInvalidUtf8CompactObjectLongKeyWithValidation) {
  std::string const value("\x14\x0f\xbf\x01\x00\x00\x00\x00\x00\x00\x00\x80\x41\x41\x01", 15);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::InvalidUtf8Sequence);
}

TEST(ValidatorTest, StringInvalidUtf8CompactObjectValueWithValidation) {
  std::string const value("\x14\x07\x41\x41\x41\x80\x01", 7);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::InvalidUtf8Sequence);
}

TEST(ValidatorTest, StringInvalidUtf8CompactLongObjectValueWithValidation) {
  std::string const value("\x14\x0f\x41\x41\xbf\x01\x00\x00\x00\x00\x00\x00\x00\x80\x01", 15);

  Options options;
  options.validateUtf8Strings = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::InvalidUtf8Sequence);
}

TEST(ValidatorTest, LongStringEmpty) {
  std::string const value("\xbf\x00\x00\x00\x00\x00\x00\x00\x00", 9);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, LongStringNonEmpty) {
  std::string const value("\xbf\x01\x00\x00\x00\x00\x00\x00\x00\x41", 10);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, LongStringTooShort) {
  std::string const value("\xbf", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, LongStringShorterThanSpecified1) {
  std::string const value("\xbf\x01\x00\x00\x00\x00\x00\x00\x00", 9);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, LongStringShorterThanSpecified2) {
  std::string const value("\xbf\x03\x00\x00\x00\x00\x00\x00\x00\x41\x42", 11);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, LongStringLongerThanSpecified1) {
  std::string const value("\xbf\x00\x00\x00\x00\x00\x00\x00\x00\x41", 10);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, LongStringLongerThanSpecified2) {
  std::string const value("\xbf\x01\x00\x00\x00\x00\x00\x00\x00\x41\x42", 11);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, TagsAllowed) {
  std::string value("\xee\x01\x0a", 3);

  Options options;
  options.disallowTags = false;
  Validator validator(&options);
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));

  Slice s(reinterpret_cast<uint8_t const*>(value.data()));
  ASSERT_TRUE(s.isTagged());
  
  value.pop_back(); // make the value invalid
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);

  value.append("\x42" "ab", 3);
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
  
  value.pop_back(); // make the value invalid
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, TagsAllowed8Bytes) {
  std::string value("\xef\x00\x00\x00\x00\x00\x00\x00\x01\x0a", 10);

  Options options;
  options.disallowTags = false;
  Validator validator(&options);
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
  
  value.pop_back(); // make the value invalid
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
  
  value.append("\x42" "ab", 3);
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
  
  value.pop_back(); // make the value invalid
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, TagsDisallowed) {
  std::string value("\xee\x01\x0a", 3);

  Options options;
  options.disallowTags = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::BuilderTagsDisallowed);
  
  value.pop_back(); // make the value invalid
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::BuilderTagsDisallowed);
}

TEST(ValidatorTest, TagsDisallowed8Bytes) {
  std::string value("\xef\x00\x00\x00\x00\x00\x00\x00\x01\x0a", 10);

  Options options;
  options.disallowTags = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::BuilderTagsDisallowed);
  
  value.pop_back(); // make the value invalid
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::BuilderTagsDisallowed);
}

TEST(ValidatorTest, ExternalAllowed) {
  std::string const value("\x1d\x00\x00\x00\x00\x00\x00\x00\x00", 9);

  Options options;
  options.disallowExternals = false;
  Validator validator(&options);
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ExternalDisallowed) {
  std::string const value("\x1d\x00\x00\x00\x00\x00\x00\x00\x00", 9);

  Options options;
  options.disallowExternals = true;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::BuilderExternalsDisallowed);
}

TEST(ValidatorTest, ExternalWithExtra) {
  std::string const value("\x1d\x00\x00\x00\x00\x00\x00\x00\x00\x41", 10);

  Options options;
  options.disallowExternals = false;
  Validator validator(&options);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByte) {
  std::string const value("\xf0\xff", 2);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, CustomOneByteTooShort) {
  std::string const value("\xf0", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteWithExtra) {
  std::string const value("\xf0\xff\x41", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomTwoBytes) {
  std::string const value("\xf1\xff\xff", 3);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, CustomTwoBytesTooShort) {
  std::string const value("\xf1", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomTwoBytesWithExtra) {
  std::string const value("\xf1\xff\xff\x41", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomFourBytes) {
  std::string const value("\xf2\xff\xff\xff\xff", 5);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, CustomFourBytesTooShort) {
  std::string const value("\xf2", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomFourBytesWithExtra) {
  std::string const value("\xf2\xff\xff\xff\xff\x41", 6);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomEightBytes) {
  std::string const value("\xf3\xff\xff\xff\xff\xff\xff\xff\xff", 9);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, CustomEightBytesTooShort) {
  std::string const value("\xf3", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomEightBytesWithExtra) {
  std::string const value("\xf3\xff\xff\xff\xff\xff\xff\xff\xff\x41", 10);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteF4) {
  std::string const value("\xf4\x01\xff", 3);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, CustomOneByteF4ZeroLength) {
  std::string const value("\xf4\x00", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteF4TooShort) {
  std::string const value("\xf4\x01", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteF4WithExtra) {
  std::string const value("\xf4\x01\xff\x41", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteF7) {
  std::string const value("\xf7\x01\x00\xff", 4);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, CustomOneByteF7ZeroLength) {
  std::string const value("\xf7\x00\x00", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteF7TooShort1) {
  std::string const value("\xf7\x01", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteF7TooShort2) {
  std::string const value("\xf7\x01\x00", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteF7WithExtra) {
  std::string const value("\xf7\x01\x00\xff\x41", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteFA) {
  std::string const value("\xfa\x01\x00\x00\x00\xff", 6);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, CustomOneByteFAZeroLength) {
  std::string const value("\xfa\x00\x00\x00\x00", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteFATooShort1) {
  std::string const value("\xfa\x01", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteFATooShort2) {
  std::string const value("\xfa\x01\x00", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteFATooShort3) {
  std::string const value("\xfa\x01\x00\x00", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteFATooShort4) {
  std::string const value("\xfa\x01\x00\x00\x00", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteFAWithExtra) {
  std::string const value("\xfa\x01\x00\x00\x00\xff\x41", 7);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteFD) {
  std::string const value("\xfd\x01\x00\x00\x00\x00\x00\x00\x00\xff", 10);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, CustomOneByteFDZeroLength) {
  std::string const value("\xfd\x00\x00\x00\x00\x00\x00\x00\x00", 9);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteFDTooShort1) {
  std::string const value("\xfd\x01", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteFDTooShort2) {
  std::string const value("\xfd\x01\x00", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteFDTooShort3) {
  std::string const value("\xfd\x01\x00\x00", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteFDTooShort4) {
  std::string const value("\xfd\x01\x00\x00\x00\x00\x00\x00", 7);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteFDTooShort5) {
  std::string const value("\xfd\x01\x00\x00\x00\x00\x00\x00\x00", 8);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, CustomOneByteFDWithExtra) {
  std::string const value("\xfd\x01\x00\x00\x00\x00\x00\x00\x00\xff\x41", 11);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, BCD) {
  std::string const value("\xd0", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::NotImplemented);
}

TEST(ValidatorTest, EmptyArray) {
  std::string const value("\x01", 1);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, EmptyArrayWithExtra) {
  std::string const value("\x01\x02", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayOneByte) {
  std::string const value("\x02\x03\x18", 3);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayOneBytePadded) {
  std::string const value("\x02\x0a\x00\x00\x00\x00\x00\x00\x00\x18", 10);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayOneByteInvalidPaddings) {
  std::string const value1("\x02\x09\x00\x00\x00\x00\x00\x00\x18", 9);
  std::string const value2("\x02\x08\x00\x00\x00\x00\x00\x18", 8);
  std::string const value3("\x02\x07\x00\x00\x00\x00\x18", 7);
  std::string const value4("\x02\x06\x00\x00\x00\x18", 6);
  std::string const value5("\x02\x05\x00\x00\x18", 5);
  std::string const value6("\x02\x04\x00\x18", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value1.c_str(), value1.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value2.c_str(), value2.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value3.c_str(), value3.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value4.c_str(), value4.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value5.c_str(), value5.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value6.c_str(), value6.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayOneByteTooShort) {
  std::string const value("\x02\x04\x18", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayOneByteTooShortBytesize0) {
  std::string const value("\x02", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayOneByteTooShortBytesize1) {
  std::string const value("\x02\x05", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayOneByteMultipleMembers) {
  std::string const value("\x02\x05\x18\x18\x18", 5);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayOneByteTooFewMembers1) {
  std::string const value("\x02\x05\x18", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayOneByteTooFewMembers2) {
  std::string const value("\x02\x05\x18\x18", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayOneByteMultipleMembersDifferentSizes) {
  std::string const value("\x02\x05\x18\x28\x00", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoBytes) {
  std::string const value("\x03\x04\x00\x18", 4);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayTwoBytesPadded) {
  std::string const value("\x03\x0a\x00\x00\x00\x00\x00\x00\x00\x18", 10);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayTwoBytesInvalidPaddings) {
  std::string const value1("\x03\x09\x00\x00\x00\x00\x00\x00\x18", 9);
  std::string const value2("\x03\x08\x00\x00\x00\x00\x00\x18", 8);
  std::string const value3("\x03\x07\x00\x00\x00\x00\x18", 7);
  std::string const value4("\x03\x06\x00\x00\x00\x18", 6);
  std::string const value5("\x03\x05\x00\x00\x18", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value1.c_str(), value1.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value2.c_str(), value2.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value3.c_str(), value3.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value4.c_str(), value4.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value5.c_str(), value5.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoBytesTooShort) {
  std::string const value("\x03\x05\x00\x18", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoBytesTooShortBytesize0) {
  std::string const value("\x03", 1);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoBytesTooShortBytesize1) {
  std::string const value("\x03\x05", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoBytesTooShortBytesize2) {
  std::string const value("\x03\x05\x00", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoBytesMultipleMembers) {
  std::string const value("\x03\x06\x00\x18\x18\x18", 6);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayTwoBytesTooFewMembers1) {
  std::string const value("\x03\x05\x00\x18", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoBytesTooFewMembers2) {
  std::string const value("\x03\x06\x00\x18\x18", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoBytesMultipleMembersDifferentSizes) {
  std::string const value("\x03\x06\x00\x18\x28\x00", 6);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayFourBytes) {
  std::string const value("\x04\x06\x00\x00\x00\x18", 6);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayFourBytesPadded) {
  std::string const value("\x04\x0a\x00\x00\x00\x00\x00\x00\x00\x18", 10);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayFourBytesInvalidPaddings) {
  std::string const value1("\x04\x09\x00\x00\x00\x00\x00\x00\x18", 9);
  std::string const value2("\x04\x08\x00\x00\x00\x00\x00\x18", 8);
  std::string const value3("\x04\x07\x00\x00\x00\x00\x18", 7);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value1.c_str(), value1.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value2.c_str(), value2.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value3.c_str(), value3.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayFourBytesTooShort) {
  std::string const value("\x04\x05\x00\x00\x00\x18", 6);
  std::string temp;

  for (std::size_t i = 0; i < value.size() - 1; ++i) {
    temp.push_back(value.at(i));

    Validator validator;
    ASSERT_VELOCYPACK_EXCEPTION(validator.validate(temp.c_str(), temp.size()), Exception::ValidatorInvalidLength);
  }
}

TEST(ValidatorTest, ArrayFourBytesMultipleMembers) {
  std::string const value("\x04\x08\x00\x00\x00\x18\x18\x18", 8);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayFourBytesTooFewMembers1) {
  std::string const value("\x04\x07\x00\x00\x00\x18", 6);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayFourBytesTooFewMembers2) {
  std::string const value("\x04\x08\x00\x00\x00\x18\x18", 7);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayFourBytesMultipleMembersDifferentSizes) {
  std::string const value("\x04\x08\x00\x00\x00\x18\x28\x00", 8);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayEightBytes) {
  std::string const value("\x05\x0a\x00\x00\x00\x00\x00\x00\x00\x18", 10);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayEightBytesTooShort) {
  std::string const value("\x05\x09\x00\x00\x00\x00\x00\x00\x00\x18", 10);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayEightBytesTooShortBytesize) {
  std::string const value("\x05\x0a\x00\x00\x00\x00\x00\x00\x00\x00", 10);
  std::string temp;

  for (std::size_t i = 0; i < value.size() - 1; ++i) {
    temp.push_back(value.at(i));

    Validator validator;
    ASSERT_VELOCYPACK_EXCEPTION(validator.validate(temp.c_str(), temp.size()), Exception::ValidatorInvalidLength);
  }
}

TEST(ValidatorTest, ArrayEightBytesMultipleMembers) {
  std::string const value("\x05\x0c\x00\x00\x00\x00\x00\x00\x00\x18\x18\x18", 12);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayEightBytesTooFewMembers1) {
  std::string const value("\x05\x0b\x00\x00\x00\x00\x00\x00\x00\x18", 10);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayEightBytesTooFewMembers2) {
  std::string const value("\x05\x0c\x00\x00\x00\x00\x00\x00\x00\x18\x18", 11);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayEightBytesMultipleMembersDifferentSizes) {
  std::string const value("\x05\x0c\x00\x00\x00\x00\x00\x00\x00\x18\x28\x00", 12);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayOneByteIndexed) {
  std::string const value("\x06\x05\x01\x18\x03", 5);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayOneByteIndexedEmpty) {
  std::string const value("\x06\x03\x00", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayOneByteIndexedPadding) {
  std::string const value("\x06\x0b\x01\x00\x00\x00\x00\x00\x00\x18\x09", 11);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayOneByteIndexedInvalidPaddings) {
  std::string const value1("\x06\x0a\x01\x00\x00\x00\x00\x00\x18\x08", 10);
  std::string const value2("\x06\x09\x01\x00\x00\x00\x00\x18\x07", 9);
  std::string const value3("\x06\x08\x01\x00\x00\x00\x18\x06", 8);
  std::string const value4("\x06\x07\x01\x00\x00\x18\x05", 7);
  std::string const value5("\x06\x06\x01\x00\x18\x04", 6);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value1.c_str(), value1.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value2.c_str(), value2.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value3.c_str(), value3.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value4.c_str(), value4.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value5.c_str(), value5.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayOneByteIndexedTooShort) {
  std::string const value("\x06\x05\x01\x18\x03", 5);
  std::string temp;

  for (std::size_t i = 0; i < value.size() - 1; ++i) {
    temp.push_back(value.at(i));

    Validator validator;
    ASSERT_VELOCYPACK_EXCEPTION(validator.validate(temp.c_str(), temp.size()), Exception::ValidatorInvalidLength);
  }
}

TEST(ValidatorTest, ArrayOneByteIndexedIndexOutOfBounds) {
  std::string value("\x06\x05\x01\x18\x00", 5);
  
  for (std::size_t i = 0; i < value.size() + 10; ++i) {
    if (i == 3) {
      continue;
    }
    value.at(value.size() - 1) = (char) i;

    Validator validator;
    ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
  }
}

TEST(ValidatorTest, ArrayOneByteIndexedTooManyMembers1) {
  std::string const value("\x06\x09\x02\x18\x18\x18\x03\x04\x05", 9);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayOneByteIndexedTooManyMembers2) {
  std::string const value("\x06\x08\x02\x18\x18\x03\x04\x05", 8);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayOneByteIndexedTooManyMembers3) {
  std::string const value("\x06\x08\x03\x18\x18\x18\x03\x04", 8);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayOneByteIndexedTooManyMembers4) {
  std::string const value("\x06\x08\x03\x18\x18\x03\x04\x05", 8);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayOneByteIndexedRepeatedValues) {
  std::string const value("\x06\x07\x02\x18\x18\x03\x03", 7);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoByteIndexed) {
  std::string const value("\x07\x08\x00\x01\x00\x18\x05\x00", 8);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayTwoByteIndexedEmpty) {
  std::string const value("\x07\x05\x00\x00\x00", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoBytesIndexedPadded) {
  std::string const value("\x07\x0c\x00\x01\x00\x00\x00\x00\x00\x18\x09\x00", 12);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayTwoBytesIndexedInvalidPaddings) {
  std::string const value1("\x07\x0b\x00\x01\x00\x00\x00\x00\x18\x08\x00", 11);
  std::string const value2("\x07\x0a\x00\x01\x00\x00\x00\x18\x07\x00", 10);
  std::string const value3("\x07\x09\x00\x01\x00\x00\x18\x06\x00", 9);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value1.c_str(), value1.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value2.c_str(), value2.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value3.c_str(), value3.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoByteIndexedTooShort) {
  std::string const value("\x07\x08\x00\x01\x00\x18\x05\x00", 8);
  std::string temp;

  for (std::size_t i = 0; i < value.size() - 1; ++i) {
    temp.push_back(value.at(i));

    Validator validator;
    ASSERT_VELOCYPACK_EXCEPTION(validator.validate(temp.c_str(), temp.size()), Exception::ValidatorInvalidLength);
  }
}

TEST(ValidatorTest, ArrayTwoByteIndexedInvalidLength1) {
  std::string const value("\x07\x00", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoByteIndexedInvalidLength2) {
  std::string const value("\x07\x00\x00", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoByteIndexedInvalidLength3) {
  std::string const value("\x07\x00\x00\x00", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoByteIndexedInvalidLength5) {
  std::string const value("\x07\x00\x00\x00\x00", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoByteIndexedInvalidLength6) {
  std::string const value("\x07\x05\x00\x00\x00", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoByteIndexedInvalidLength7) {
  std::string const value("\x07\x05\x00\x01\x00", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoByteIndexedIndexOutOfBounds) {
  std::string value("\x07\x08\x00\x01\x00\x18\x00\x00", 8);
  
  for (std::size_t i = 0; i < value.size() + 10; ++i) {
    if (i == 5) {
      continue;
    }
    value.at(value.size() - 2) = (char) i;

    Validator validator;
    ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
  }
}

TEST(ValidatorTest, ArrayTwoByteIndexedTooManyMembers1) {
  std::string const value("\x07\x0e\x00\x02\x00\x18\x18\x18\x05\x00\x06\x00\x07\x00", 14);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoByteIndexedTooManyMembers2) {
  std::string const value("\x07\x0d\x00\x02\x00\x18\x18\x05\x00\x06\x00\x07\x00", 13);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoByteIndexedTooManyMembers3) {
  std::string const value("\x07\x0c\x00\x03\x00\x18\x18\x18\x05\x00\x06\x00", 12);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoByteIndexedTooManyMembers4) {
  std::string const value("\x07\x0b\x00\x03\x00\x18\x18\x05\x00\x06\x00", 11);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayTwoByteIndexedRepeatedValues) {
  std::string const value("\x07\x09\x00\x02\x00\x18\x18\x05\x05", 9);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayFourByteIndexed) {
  std::string const value("\x08\x0e\x00\x00\x00\x01\x00\x00\x00\x18\x09\x00\x00\x00", 14);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayFourByteIndexedEmpty) {
  std::string const value("\x08\x09\x00\x00\x00\x00\x00\x00\x00", 9);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayFourByteIndexedTooShort) {
  std::string const value("\x08\x0e\x00\x00\x00\x01\x00\x00\x00\x18\x09\x00\x00\x00", 14);
  std::string temp;
  
  for (std::size_t i = 0; i < value.size() - 1; ++i) {
    temp.push_back(value.at(i));

    Validator validator;
    ASSERT_VELOCYPACK_EXCEPTION(validator.validate(temp.c_str(), temp.size()), Exception::ValidatorInvalidLength);
  }
}

TEST(ValidatorTest, ArrayFourByteIndexedInvalidLength) {
  std::string const value("\x08\x00\x00\x00\x00\x00\x00\x00\x00", 9);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayFourByteIndexedIndexOutOfBounds) {
  std::string value("\x08\x0e\x00\x00\x00\x01\x00\x00\x00\x18\x00\x00\x00\x00", 14);
  
  for (std::size_t i = 0; i < value.size() + 10; ++i) {
    if (i == 9) {
      continue;
    }
    value.at(value.size() - 4) = (char) i;

    Validator validator;
    ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
  }
}

TEST(ValidatorTest, ArrayFourByteIndexedTooManyMembers1) {
  std::string const value("\x08\x18\x00\x00\x00\x02\x00\x00\x00\x18\x18\x18\x09\x00\x00\x00\x0a\x00\x00\x00\x0b\x00\x00\x00", 24);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayFourByteIndexedTooManyMembers2) {
  std::string const value("\x08\x17\x00\x00\x00\x02\x00\x00\x00\x18\x18\x09\x00\x00\x00\x0a\x00\x00\x00\x0b\x00\x00\x00", 23);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayFourByteIndexedTooManyMembers3) {
  std::string const value("\x08\x14\x00\x00\x00\x03\x00\x00\x00\x18\x18\x18\x09\x00\x00\x00\x0a\x00\x00\x00", 20);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayFourByteIndexedTooManyMembers4) {
  std::string const value("\x08\x14\x00\x00\x00\x03\x00\x00\x00\x18\x18\x09\x00\x00\x00\x0a\x00\x00\x00", 20);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayFourByteIndexedRepeatedValues) {
  std::string const value("\x08\x13\x00\x00\x00\x02\x00\x00\x00\x18\x18\x09\x00\x00\x00\x09\x00\x00\x00", 19);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayEightByteIndexed) {
  std::string const value("\x09\x1a\x00\x00\x00\x00\x00\x00\x00\x18\x09\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00", 26);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayEightByteIndexedEmpty) {
  std::string const value("\x09\x11\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 17);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayEightByteIndexedTooShort) {
  std::string const value("\x09\x1a\x00\x00\x00\x00\x00\x00\x00\x18\x09\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00", 26);
  std::string temp;

  for (std::size_t i = 0; i < value.size() - 1; ++i) {
    temp.push_back(value.at(i));

    Validator validator;
    ASSERT_VELOCYPACK_EXCEPTION(validator.validate(temp.c_str(), temp.size()), Exception::ValidatorInvalidLength);
  }
}

TEST(ValidatorTest, ArrayEightByteIndexedInvalidLength) {
  std::string const value("\x09\x00\x00\x00\x00\x00\x00\x00\x00", 9);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayEightByteIndexedIndexOutOfBounds) {
  std::string value("\x09\x1a\x00\x00\x00\x00\x00\x00\x00\x18\x09\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00", 26);
  
  for (std::size_t i = 0; i < value.size() + 10; ++i) {
    if (i == 9) {
      continue;
    }
    value.at(value.size() - 16) = (char) i;

    Validator validator;
    ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
  }
}

TEST(ValidatorTest, ArrayEightByteIndexedTooManyMembers1) {
  std::string const value("\x09\x2c\x00\x00\x00\x00\x00\x00\x00\x18\x18\x18\x09\x00\x00\x00\x00\x00\x00\x00\x0a\x00\x00\x00\x00\x00\x00\x00\x0b\x00\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00", 44);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayEightByteIndexedTooManyMembers2) {
  std::string const value("\x09\x2b\x00\x00\x00\x00\x00\x00\x00\x18\x18\x09\x00\x00\x00\x00\x00\x00\x00\x0a\x00\x00\x00\x00\x00\x00\x00\x0b\x00\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00", 43);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayEightByteIndexedTooManyMembers3) {
  std::string const value("\x09\x24\x00\x00\x00\x00\x00\x00\x00\x18\x18\x18\x09\x00\x00\x00\x00\x00\x00\x00\x0a\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00", 36);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayEightByteIndexedTooManyMembers4) {
  std::string const value("\x09\x23\x00\x00\x00\x00\x00\x00\x00\x18\x18\x09\x00\x00\x00\x00\x00\x00\x00\x0a\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00", 35);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayEightByteIndexedRepeatedValues) {
  std::string const value("\x09\x23\x00\x00\x00\x00\x00\x00\x00\x18\x18\x09\x00\x00\x00\x00\x00\x00\x00\x09\x00\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00", 35);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayOneByteWithExtraPadding) {
  Options options;
  options.paddingBehavior = Options::PaddingBehavior::UsePadding;
  Builder b(&options);
  b.openArray(false);
  b.add(Value(1));
  b.add(Value(2));
  b.add(Value(3));
  b.close();

  Slice s = b.slice();
  uint8_t const* data = s.start();

  ASSERT_EQ(0x05, data[0]);
  ASSERT_EQ(0x0c, data[1]);
  ASSERT_EQ(0x00, data[2]);
  ASSERT_EQ(0x00, data[3]);
  ASSERT_EQ(0x00, data[4]);
  ASSERT_EQ(0x00, data[5]);
  ASSERT_EQ(0x00, data[6]);
  ASSERT_EQ(0x00, data[7]);
  ASSERT_EQ(0x00, data[8]);
  ASSERT_EQ(0x31, data[9]);
  ASSERT_EQ(0x32, data[10]);
  ASSERT_EQ(0x33, data[11]);
  
  Validator validator;
  ASSERT_TRUE(validator.validate(data, s.byteSize()));
}

TEST(ValidatorTest, ArrayCompact) {
  std::string const value("\x13\x04\x18\x01", 4);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayCompactWithExtra) {
  std::string const value("\x13\x04\x18\x01\x41", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayCompactTooShort1) {
  std::string const value("\x13\x04", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayCompactTooShort2) {
  std::string const value("\x13\x04\x18", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayCompactTooShort3) {
  std::string const value("\x13\x80", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayCompactTooShort4) {
  std::string const value("\x13\x80\x80", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayCompactTooShort5) {
  std::string const value("\x13\x80\x05\x18", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayCompactTooShort6) {
  std::string const value("\x13\x04\x18\x02", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayCompactTooShort7) {
  std::string const value("\x13\x04\x18\xff", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayCompactTooShort8) {
  std::string const value("\x13\x04\x06\x01", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayCompactTooShort9) {
  std::string const value("\x13\x81", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayCompactEmpty) {
  std::string const value("\x13\x04\x18\x00", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayCompactNrItemsWrong1) {
  std::string const value("\x13\x04\x18\x81", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayCompactNrItemsWrong2) {
  std::string const value("\x13\x05\x18\x81\x81", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayCompactNrItemsWrong3) {
  std::string const value("\x13\x05\x18\x01\x80", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayCompactManyEntries) {
  Builder b;
  b.openArray(true);
  for (std::size_t i = 0; i < 2048; ++i) {
    b.add(Value(i));
  }
  b.close();

  ASSERT_EQ(b.slice().head(), '\x13'); 

  Validator validator;
  ASSERT_TRUE(validator.validate(b.slice().start(), b.slice().byteSize()));
}

TEST(ValidatorTest, ArrayEqualSize) {
  std::string const value("\x02\x04\x01\x18", 4);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayEqualSizeMultiple) {
  std::string const value("\x02\x04\x18\x18", 4);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ArrayEqualSizeMultipleWithExtra) {
  std::string const value("\x02\x04\x18\x18\x41", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayEqualSizeTooShort) {
  std::string const value("\x02\x05\x18\x18", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayEqualSizeContainingNone) {
  std::string const value("\x02\x03\x00", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayEqualSizeUnequalElements) {
  std::string const value("\x02\x05\x18\x41\x40", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, EmptyObject) {
  std::string const value("\x0a", 1);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, EmptyObjectWithExtra) {
  std::string const value("\x0a\x02", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ArrayMoreItemsThanIndex) {
  std::string const value("\x06\x06\x01\xBE\x30\x04", 6);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectMoreItemsThanIndex) {
  std::string const value("\x0B\x08\x01\xBE\x41\x61\x31\x04", 8);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectValueLeakIndex) {
  std::string const value("\x0B\x06\x01\x41\x61\x03", 6);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectCompact) {
  std::string const value("\x14\x05\x40\x18\x01", 5);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ObjectCompactEmpty) {
  std::string const value("\x14\x03\x00", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectCompactWithExtra) {
  std::string const value("\x14\x06\x40\x18\x01\x41", 6);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectCompactTooShort1) {
  std::string const value("\x14\x05", 2);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectCompactTooShort2) {
  std::string const value("\x14\x05\x40", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectCompactTooShort3) {
  std::string const value("\x14\x05\x40\x18", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectCompactNrItemsWrong1) {
  std::string const value("\x14\x05\x40\x18\x02", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectCompactNrItemsWrong2) {
  std::string const value("\x14\x07\x40\x18\x40\x18\x01", 7);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectCompactDifferentAttributes) {
  std::string const value("\x14\x09\x41\x41\x18\x41\x42\x18\x02", 9);

  Options options;
  options.checkAttributeUniqueness = true;
  Validator validator(&options);
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ObjectCompactOnlyKey) {
  std::string const value("\x14\x04\x40\x01", 4);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectCompactStopInKey) {
  std::string const value("\x14\x05\x45\x41\x01", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectCompactManyEntries) {
  Builder b;
  b.openObject(true);
  for (std::size_t i = 0; i < 2048; ++i) {
    std::string key = "test" + std::to_string(i);
    b.add(key, Value(i));
  }
  b.close();

  ASSERT_EQ(b.slice().head(), '\x14'); 

  Validator validator;
  ASSERT_TRUE(validator.validate(b.slice().start(), b.slice().byteSize()));
}

TEST(ValidatorTest, ObjectNegativeKeySmallInt) {
  std::string const value("\x0b\x06\x01\x3a\x18\x03", 6);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectKeySignedInt) {
  std::string const value("\x0b\x07\x01\x20\x01\x18\x03", 7);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectOneByte) {
  std::string const value("\x0b\x06\x01\x40\x18\x03", 6);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ObjectOneByteSingleByteKey) {
  std::string const value("\x0b\x07\x01\x41\x41\x18\x03", 7);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ObjectOneBytePadding) {
  std::string const value("\x0b\x0c\x01\x00\x00\x00\x00\x00\x00\x40\x18\x09", 12);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ObjectOneByteInvalidPaddings) {
  std::string const value1("\x0b\x0b\x01\x00\x00\x00\x00\x00\x40\x18\x08", 11);
  std::string const value2("\x0b\x0a\x01\x00\x00\x00\x00\x40\x18\x07", 10);
  std::string const value3("\x0b\x09\x01\x00\x00\x00\x40\x18\x06", 9);
  std::string const value4("\x0b\x08\x01\x00\x00\x40\x18\x05", 8);
  std::string const value5("\x0b\x07\x01\x00\x40\x18\x04", 7);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value1.c_str(), value1.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value2.c_str(), value2.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value3.c_str(), value3.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value4.c_str(), value4.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value5.c_str(), value5.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectOneByteEmpty) {
  std::string const value("\x0b\x03\x00", 3);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectOneByteWithExtra) {
  std::string const value("\x0b\x07\x40\x18\x02\x01\x41", 7);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectOneByteTooShort) {
  std::string const value("\x0b\x06\x01\x40\x18\x03", 6);
  std::string temp;
  
  for (std::size_t i = 0; i < value.size() - 1; ++i) {
    temp.push_back(value.at(i));

    Validator validator;
    ASSERT_VELOCYPACK_EXCEPTION(validator.validate(temp.c_str(), temp.size()), Exception::ValidatorInvalidLength);
  }
}

TEST(ValidatorTest, ObjectOneByteNrItemsWrong1) {
  std::string const value("\x0b\x06\x02\x40\x18\x03", 6);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectOneByteNrItemsWrong2) {
  std::string const value("\x0b\x08\x01\x40\x18\x40\x18\x03", 8);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectOneByteNrItemsWrong3) {
  std::string const value("\x0b\x08\x02\x40\x18\x40\x18\x03", 8);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectOneByteNrItemsMatch) {
  std::string const value("\x0b\x09\x02\x40\x18\x40\x18\x03\x05", 9);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ObjectOneByteOnlyKey) {
  std::string const value("\x0b\x05\x01\x40\x03", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectOneByteStopInKey) {
  std::string const value("\x0b\x06\x01\x45\x41\x03", 6);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectOneByteSomeEntries) {
  Builder b;
  b.openObject();
  for (std::size_t i = 0; i < 8; ++i) {
    std::string key = "t" + std::to_string(i);
    b.add(key, Value(i));
  }
  b.close();

  ASSERT_EQ(b.slice().head(), '\x0b'); 

  Validator validator;
  ASSERT_TRUE(validator.validate(b.slice().start(), b.slice().byteSize()));
}

TEST(ValidatorTest, ObjectOneByteMoreEntries) {
  Builder b;
  b.openObject();
  for (std::size_t i = 0; i < 17; ++i) {
    std::string key = "t" + std::to_string(i);
    b.add(key, Value(i));
  }
  b.close();

  ASSERT_EQ(b.slice().head(), '\x0b'); 

  Validator validator;
  ASSERT_TRUE(validator.validate(b.slice().start(), b.slice().byteSize()));
}

TEST(ValidatorTest, ObjectTwoByte) {
  std::string const value("\x0c\x09\x00\x01\x00\x40\x18\x05\x00", 9);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ObjectTwoBytePadding) {
  std::string const value("\x0c\x0d\x00\x01\x00\x00\x00\x00\x00\x40\x18\x09\x00", 13);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ObjectTwoByteInvalidPaddings) {
  std::string const value1("\x0c\x0c\x00\x01\x00\x00\x00\x00\x40\x18\x08\x00", 12);
  std::string const value2("\x0c\x0b\x00\x01\x00\x00\x00\x40\x18\x07\x00", 11);
  std::string const value3("\x0c\x0a\x00\x01\x00\x00\x40\x18\x06\x00", 10);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value1.c_str(), value1.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value2.c_str(), value2.size()), Exception::ValidatorInvalidLength);
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value3.c_str(), value3.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectTwoByteEmpty) {
  std::string const value("\x0c\x05\x00\x00\x00", 5);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectTwoByteWithExtra) {
  std::string const value("\x0c\x0a\x00\x01\x00\x40\x18\x05\x00\x41", 10);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectTwoByteTooShort) {
  std::string const value("\x0c\x06\x00\x01\x00\x40\x18\x05", 8);
  std::string temp;
  
  for (std::size_t i = 0; i < value.size() - 1; ++i) {
    temp.push_back(value.at(i));

    Validator validator;
    ASSERT_VELOCYPACK_EXCEPTION(validator.validate(temp.c_str(), temp.size()), Exception::ValidatorInvalidLength);
  }
}

TEST(ValidatorTest, ObjectTwoByteNrItemsWrong1) {
  std::string const value("\x0c\x09\x00\x02\x00\x40\x18\x05\x00", 9);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectTwoByteNrItemsWrong2) {
  std::string const value("\x0c\x0b\x00\x01\x00\x40\x18\x40\x18\x05\x00", 11);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectTwoByteNrItemsWrong3) {
  std::string const value("\x0c\x0b\x00\x02\x00\x40\x18\x40\x18\x05\x00", 11);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectTwoByteNrItemsMatch) {
  std::string const value("\x0c\x0d\x00\x02\x00\x40\x18\x40\x18\x05\x00\x07\x00", 13);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ObjectTwoByteOnlyKey) {
  std::string const value("\x0c\x08\x00\x01\x00\x40\x05\x00", 8);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectTwoByteStopInKey) {
  std::string const value("\x0c\x09\x00\x01\x00\x45\x41\x05\x00", 9);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectTwoByteSomeEntries) {
  std::string prefix;
  for (std::size_t i = 0; i < 100; ++i) {
    prefix.push_back('x');
  }
  Builder b;
  b.openObject();
  for (std::size_t i = 0; i < 8; ++i) {
    std::string key = prefix + std::to_string(i);
    b.add(key, Value(i));
  }
  b.close();

  ASSERT_EQ(b.slice().head(), '\x0c'); 

  Validator validator;
  ASSERT_TRUE(validator.validate(b.slice().start(), b.slice().byteSize()));
}

TEST(ValidatorTest, ObjectTwoByteMoreEntries) {
  Builder b;
  b.openObject();
  for (std::size_t i = 0; i < 256; ++i) {
    std::string key = "test" + std::to_string(i);
    b.add(key, Value(i));
  }
  b.close();

  ASSERT_EQ(b.slice().head(), '\x0c'); 

  Validator validator;
  ASSERT_TRUE(validator.validate(b.slice().start(), b.slice().byteSize()));
}

TEST(ValidatorTest, ObjectFourByte) {
  std::string const value("\x0d\x0f\x00\x00\x00\x01\x00\x00\x00\x40\x18\x09\x00\x00\x00", 15);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ObjectFourByteEmpty) {
  std::string const value("\x0d\x09\x00\x00\x00\x00\x00\x00\x00", 9);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectFourByteWithExtra) {
  std::string const value("\x0d\x10\x00\x00\x00\x01\x00\x00\x00\x40\x18\x09\x00\x00\x00\x41", 16);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectFourByteTooShort) {
  std::string const value("\x0d\x0f\x00\x00\x00\x01\x00\x00\x00\x40\x18\x09\x00\x00\x00", 15);
  std::string temp;
  
  for (std::size_t i = 0; i < value.size() - 1; ++i) {
    temp.push_back(value.at(i));

    Validator validator;
    ASSERT_VELOCYPACK_EXCEPTION(validator.validate(temp.c_str(), temp.size()), Exception::ValidatorInvalidLength);
  }
}

TEST(ValidatorTest, ObjectFourByteNrItemsWrong1) {
  std::string const value("\x0d\x0f\x00\x00\x00\x02\x00\x00\x00\x40\x18\x09\x00\x00\x00", 15);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectFourByteNrItemsWrong2) {
  std::string const value("\x0d\x11\x00\x00\x00\x01\x00\x00\x00\x40\x18\x40\x18\x09\x00\x00\x00", 17);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectFourByteNrItemsWrong3) {
  std::string const value("\x0d\x11\x00\x00\x00\x02\x00\x00\x00\x40\x18\x40\x18\x09\x00\x00\x00", 17);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectFourByteNrItemsMatch) {
  std::string const value("\x0d\x15\x00\x00\x00\x02\x00\x00\x00\x40\x18\x40\x18\x09\x00\x00\x00\x0b\x00\x00\x00", 21);

  Validator validator;
  ASSERT_TRUE(validator.validate(value.c_str(), value.size()));
}

TEST(ValidatorTest, ObjectFourByteOnlyKey) {
  std::string const value("\x0d\x0e\x00\x00\x00\x01\x00\x00\x00\x40\x09\x00\x00\x00", 14);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectFourByteStopInKey) {
  std::string const value("\x0d\x0f\x00\x00\x00\x01\x00\x00\x00\x45\x41\x09\x00\x00\x00", 15);

  Validator validator;
  ASSERT_VELOCYPACK_EXCEPTION(validator.validate(value.c_str(), value.size()), Exception::ValidatorInvalidLength);
}

TEST(ValidatorTest, ObjectFourByteManyEntries) {
  Builder b;
  b.openObject();
  for (std::size_t i = 0; i < 65536; ++i) {
    std::string key = "test" + std::to_string(i);
    b.add(key, Value(i));
  }
  b.close();

  ASSERT_EQ(b.slice().head(), '\x0d'); 

  Validator validator;
  ASSERT_TRUE(validator.validate(b.slice().start(), b.slice().byteSize()));
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
