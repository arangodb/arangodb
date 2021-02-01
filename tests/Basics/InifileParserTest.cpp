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
/// @author Dr. Frank Celler
/// @author Copyright 2007-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "ProgramOptions/IniFileParser.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb;
using namespace arangodb::basics;

TEST(InifileParserTest, test_options) {
  using namespace arangodb::options;

  uint64_t writeBufferSize = UINT64_MAX;
  uint64_t totalWriteBufferSize = UINT64_MAX;
  uint64_t maxWriteBufferNumber = UINT64_MAX;
  uint64_t maxTotalWalSize = UINT64_MAX;
  uint64_t blockCacheSize = UINT64_MAX;
  bool enforceBlockCacheSizeLimit = false;
  uint64_t cacheSize = UINT64_MAX;
  uint64_t nonoSetOption = UINT64_MAX;
  uint64_t someValueUsingSuffixes = UINT64_MAX;
  uint64_t someOtherValueUsingSuffixes = UINT64_MAX;
  uint64_t yetSomeOtherValueUsingSuffixes = UINT64_MAX;
  uint64_t andAnotherValueUsingSuffixes = UINT64_MAX;
  uint64_t andFinallySomeGb = UINT64_MAX;
  uint64_t aValueWithAnInlineComment = UINT64_MAX;
  bool aBoolean = false;
  bool aBooleanTrue = false;
  bool aBooleanFalse = true;
  bool aBooleanNotSet = false;
  double aDouble = -2.0;
  double aDoubleWithAComment = -2.0;
  double aDoubleNotSet = -2.0;
  std::string aStringValueEmpty = "snort";
  std::string aStringValue = "purr";
  std::string aStringValueWithAnInlineComment = "gaw";
  std::string anotherStringValueWithAnInlineComment = "gaw";
  std::string aStringValueNotSet = "meow";

  std::unordered_set<std::string> soundsPorksMake = { "foo", "bar", "blub", "snuggles", "slurp", "oink" };
  std::vector<std::string> porkSounds = {"slurp"};
  std::vector<std::string> strangePorkSounds = {"slurp", "snuggles"};
  
  ProgramOptions options("testi", "testi [options]", "bla", "/tmp/bla");
  options.addSection("rocksdb", "bla");
  options.addOption("--rocksdb.write-buffer-size", "bla", new UInt64Parameter(&writeBufferSize));
  options.addOption("--rocksdb.total-write-buffer-size", "bla", new UInt64Parameter(&totalWriteBufferSize));
  options.addOption("--rocksdb.max-write-buffer-number", "bla", new UInt64Parameter(&maxWriteBufferNumber));
  options.addOption("--rocksdb.max-total-wal-size", "bla", new UInt64Parameter(&maxTotalWalSize));
  options.addOption("--rocksdb.block-cache-size", "bla", new UInt64Parameter(&blockCacheSize));
  options.addOption("--rocksdb.enforce-block-cache-size-limit", "bla", new BooleanParameter(&enforceBlockCacheSizeLimit));
  
  options.addSection("cache", "bla");
  options.addOption("--cache.size", "bla", new UInt64Parameter(&cacheSize));
  options.addOption("--cache.nono-set-option", "bla", new UInt64Parameter(&nonoSetOption));
  
  options.addSection("pork", "bla");
  options.addOption("--pork.a-boolean", "bla", new BooleanParameter(&aBoolean, true));
  options.addOption("--pork.a-boolean-true", "bla", new BooleanParameter(&aBooleanTrue, true));
  options.addOption("--pork.a-boolean-false", "bla", new BooleanParameter(&aBooleanFalse, true));
  options.addOption("--pork.a-boolean-not-set", "bla", new BooleanParameter(&aBooleanNotSet, true));
  options.addOption("--pork.some-value-using-suffixes", "bla", new UInt64Parameter(&someValueUsingSuffixes));
  options.addOption("--pork.some-other-value-using-suffixes", "bla", new UInt64Parameter(&someOtherValueUsingSuffixes));
  options.addOption("--pork.yet-some-other-value-using-suffixes", "bla", new UInt64Parameter(&yetSomeOtherValueUsingSuffixes));
  options.addOption("--pork.and-another-value-using-suffixes", "bla", new UInt64Parameter(&andAnotherValueUsingSuffixes));
  options.addOption("--pork.and-finally-some-gb", "bla", new UInt64Parameter(&andFinallySomeGb));
  options.addOption("--pork.a-value-with-an-inline-comment", "bla", new UInt64Parameter(&aValueWithAnInlineComment));
  options.addOption("--pork.a-double", "bla", new DoubleParameter(&aDouble));
  options.addOption("--pork.a-double-with-a-comment", "bla", new DoubleParameter(&aDoubleWithAComment));
  options.addOption("--pork.a-double-not-set", "bla", new DoubleParameter(&aDoubleNotSet));
  options.addOption("--pork.a-string-value-empty", "bla", new StringParameter(&aStringValueEmpty));
  options.addOption("--pork.a-string-value", "bla", new StringParameter(&aStringValue));
  options.addOption("--pork.a-string-value-with-an-inline-comment", "bla", new StringParameter(&aStringValueWithAnInlineComment));
  options.addOption("--pork.another-string-value-with-an-inline-comment", "bla", new StringParameter(&anotherStringValueWithAnInlineComment));
  options.addOption("--pork.a-string-value-not-set", "bla", new StringParameter(&aStringValueNotSet));
  options.addOption("--pork.sounds", "which sounds do pigs make?",
                    new DiscreteValuesVectorParameter<StringParameter>(&porkSounds, soundsPorksMake),
                    arangodb::options::makeDefaultFlags(options::Flags::FlushOnFirst));

  options.addOption("--pork.strange-sounds", "which strange sounds do pigs make?",
                    new DiscreteValuesVectorParameter<StringParameter>(&strangePorkSounds, soundsPorksMake),
                    arangodb::options::makeDefaultFlags(options::Flags::FlushOnFirst));


  auto contents = R"data(
[rocksdb]
# Write buffers
write-buffer-size = 2048000 # 2M
total-write-buffer-size = 536870912
max-write-buffer-number = 4
max-total-wal-size = 1024000 # 1M

# Read buffers 
block-cache-size = 268435456
enforce-block-cache-size-limit = true

[cache]
size = 268435456 # 256M

[pork]
a-boolean = true
a-boolean-true = true
a-boolean-false = false
some-value-using-suffixes = 1M
some-other-value-using-suffixes = 1MiB
yet-some-other-value-using-suffixes = 12MB  
   and-another-value-using-suffixes = 256kb  
   and-finally-some-gb = 256GB
a-value-with-an-inline-comment = 12345#1234M
a-double = 335.25
a-double-with-a-comment = 2948.434#343
a-string-value-empty =      
a-string-value = 486hbsbq,r
a-string-value-with-an-inline-comment = abc#def h
another-string-value-with-an-inline-comment = abc  #def h
sounds = foo
sounds = oink
sounds = snuggles
)data";
  
  IniFileParser parser(&options);

  bool result = parser.parseContent("arangod.conf", contents, true);
  ASSERT_TRUE(result);
  ASSERT_EQ(2048000U, writeBufferSize);
  ASSERT_EQ(536870912U, totalWriteBufferSize);
  ASSERT_EQ(4U, maxWriteBufferNumber);
  ASSERT_EQ(1024000U, maxTotalWalSize);
  ASSERT_EQ(268435456U, blockCacheSize);
  ASSERT_TRUE(enforceBlockCacheSizeLimit);
  
  ASSERT_EQ(268435456U, cacheSize);
  ASSERT_EQ(UINT64_MAX, nonoSetOption);

  ASSERT_TRUE(aBoolean);
  ASSERT_TRUE(aBooleanTrue);
  ASSERT_FALSE(aBooleanFalse);
  ASSERT_FALSE(aBooleanNotSet);
  
  ASSERT_EQ(1000000U, someValueUsingSuffixes);
  ASSERT_EQ(1048576U, someOtherValueUsingSuffixes);
  ASSERT_EQ(12000000U, yetSomeOtherValueUsingSuffixes);
  ASSERT_EQ(256000U, andAnotherValueUsingSuffixes);
  ASSERT_EQ(256000000000U, andFinallySomeGb);
  ASSERT_EQ(12345U, aValueWithAnInlineComment);

  ASSERT_DOUBLE_EQ(335.25, aDouble);
  ASSERT_DOUBLE_EQ(2948.434, aDoubleWithAComment);
  ASSERT_DOUBLE_EQ(-2.0, aDoubleNotSet);
  
  ASSERT_EQ("", aStringValueEmpty);
  ASSERT_EQ("486hbsbq,r", aStringValue);
  ASSERT_EQ("abc#def h", aStringValueWithAnInlineComment);
  ASSERT_EQ("abc  #def h", anotherStringValueWithAnInlineComment);
  ASSERT_EQ("meow", aStringValueNotSet);
  auto findPorkSound = [&porkSounds](std::string sound) {
                         return std::find(porkSounds.begin(), porkSounds.end(), sound);
                           };
  ASSERT_EQ(porkSounds.size(), 3);
  ASSERT_EQ(findPorkSound("meow"), porkSounds.end());
  // the default value should have been removed:
  ASSERT_EQ(findPorkSound("slurp"), porkSounds.end());
  auto it = porkSounds.begin();
  ASSERT_EQ(findPorkSound("foo"), it);
  it++;
  ASSERT_EQ(findPorkSound("oink"), it);
  it++;
  ASSERT_EQ(findPorkSound("snuggles"), it);

  auto findStrangePorkSound = [&strangePorkSounds](std::string sound) {
                         return std::find(strangePorkSounds.begin(), strangePorkSounds.end(), sound);
                           };
  ASSERT_EQ(strangePorkSounds.size(), 2);
  it = strangePorkSounds.begin();
  ASSERT_EQ(findStrangePorkSound("slurp"), it);
  it++;
  ASSERT_EQ(findStrangePorkSound("snuggles"), it);
  ASSERT_EQ(findStrangePorkSound("blub"), strangePorkSounds.end());
}
