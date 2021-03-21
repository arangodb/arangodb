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
#include <fstream>
#include <string>

#include "tests-common.h"

static std::string tryReadFile(std::string const& filename) {
  std::string s;
  std::ifstream ifs(filename.c_str(), std::ifstream::in);

  if (!ifs.is_open()) {
    throw "cannot open input file";
  }

  char buffer[4096];
  while (ifs.good()) {
    ifs.read(&buffer[0], sizeof(buffer));
    s.append(buffer, ifs.gcount());
  }
  ifs.close();

  return s;
}

static std::string readFile(std::string filename) {
#ifdef _WIN32
  std::string const separator("\\");
#else
  std::string const separator("/");
#endif
  filename = "tests" + separator + "jsonSample" + separator + filename;

  for (std::size_t i = 0; i < 3; ++i) {
    try {
      return tryReadFile(filename);
    } catch (...) {
      filename = ".." + separator + filename;
    }
  }
  throw "cannot open input file";
}

static bool parseFile(std::string const& filename) {
  std::string const data = readFile(filename);

  Parser parser;
  try {
    parser.parse(data);
    auto builder = parser.steal();
    Slice slice = builder->slice();

    Validator validator;
    validator.validate(slice.start(), slice.byteSize(), false);
    return true;
  } catch (...) {
    return false;
  }
}

TEST(StaticFilesTest, CommitsJson) { ASSERT_TRUE(parseFile("commits.json")); }

TEST(StaticFilesTest, SampleJson) { ASSERT_TRUE(parseFile("sample.json")); }

TEST(StaticFilesTest, SampleNoWhiteJson) {
  ASSERT_TRUE(parseFile("sampleNoWhite.json"));
}

TEST(StaticFilesTest, SmallJson) { ASSERT_TRUE(parseFile("small.json")); }

TEST(StaticFilesTest, Fail2Json) { ASSERT_FALSE(parseFile("fail2.json")); }

TEST(StaticFilesTest, Fail3Json) { ASSERT_FALSE(parseFile("fail3.json")); }

TEST(StaticFilesTest, Fail4Json) { ASSERT_FALSE(parseFile("fail4.json")); }

TEST(StaticFilesTest, Fail5Json) { ASSERT_FALSE(parseFile("fail5.json")); }

TEST(StaticFilesTest, Fail6Json) { ASSERT_FALSE(parseFile("fail6.json")); }

TEST(StaticFilesTest, Fail7Json) { ASSERT_FALSE(parseFile("fail7.json")); }

TEST(StaticFilesTest, Fail8Json) { ASSERT_FALSE(parseFile("fail8.json")); }

TEST(StaticFilesTest, Fail9Json) { ASSERT_FALSE(parseFile("fail9.json")); }

TEST(StaticFilesTest, Fail10Json) { ASSERT_FALSE(parseFile("fail10.json")); }

TEST(StaticFilesTest, Fail11Json) { ASSERT_FALSE(parseFile("fail11.json")); }

TEST(StaticFilesTest, Fail12Json) { ASSERT_FALSE(parseFile("fail12.json")); }

TEST(StaticFilesTest, Fail13Json) { ASSERT_FALSE(parseFile("fail13.json")); }

TEST(StaticFilesTest, Fail14Json) { ASSERT_FALSE(parseFile("fail14.json")); }

TEST(StaticFilesTest, Fail15Json) { ASSERT_FALSE(parseFile("fail15.json")); }

TEST(StaticFilesTest, Fail16Json) { ASSERT_FALSE(parseFile("fail16.json")); }

TEST(StaticFilesTest, Fail17Json) { ASSERT_FALSE(parseFile("fail17.json")); }

TEST(StaticFilesTest, Fail19Json) { ASSERT_FALSE(parseFile("fail19.json")); }

TEST(StaticFilesTest, Fail20Json) { ASSERT_FALSE(parseFile("fail20.json")); }

TEST(StaticFilesTest, Fail21Json) { ASSERT_FALSE(parseFile("fail21.json")); }

TEST(StaticFilesTest, Fail22Json) { ASSERT_FALSE(parseFile("fail22.json")); }

TEST(StaticFilesTest, Fail23Json) { ASSERT_FALSE(parseFile("fail23.json")); }

TEST(StaticFilesTest, Fail24Json) { ASSERT_FALSE(parseFile("fail24.json")); }

TEST(StaticFilesTest, Fail25Json) { ASSERT_FALSE(parseFile("fail25.json")); }

TEST(StaticFilesTest, Fail26Json) { ASSERT_FALSE(parseFile("fail26.json")); }

TEST(StaticFilesTest, Fail27Json) { ASSERT_FALSE(parseFile("fail27.json")); }

TEST(StaticFilesTest, Fail28Json) { ASSERT_FALSE(parseFile("fail28.json")); }

TEST(StaticFilesTest, Fail29Json) { ASSERT_FALSE(parseFile("fail29.json")); }

TEST(StaticFilesTest, Fail30Json) { ASSERT_FALSE(parseFile("fail30.json")); }

TEST(StaticFilesTest, Fail31Json) { ASSERT_FALSE(parseFile("fail31.json")); }

TEST(StaticFilesTest, Fail32Json) { ASSERT_FALSE(parseFile("fail32.json")); }

TEST(StaticFilesTest, Fail33Json) { ASSERT_FALSE(parseFile("fail33.json")); }

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
