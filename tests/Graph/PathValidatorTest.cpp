////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Graph/PathManagement/PathStore.cpp"
#include "Graph/PathManagement/PathValidator.cpp"
#include "Graph/Providers/BaseStep.h"

#include "Graph/Types/UniquenessLevel.h"

#include <Basics/StringUtils.h>

using namespace arangodb;
using namespace arangodb::graph;

namespace arangodb {
namespace tests {
namespace graph_path_validator_test {

static_assert(GTEST_HAS_TYPED_TEST, "We need typed tests for the following:");

class Step : public arangodb::graph::BaseStep<Step> {
 public:
  using Vertex = size_t;
  using Edge = size_t;

  size_t _id;
  double _weight;
  bool _isLooseEnd;

  Step(size_t id, double weight, size_t previous, bool isLooseEnd)
      : arangodb::graph::BaseStep<Step>{previous} {
    _id = id;
    _weight = weight;
    _isLooseEnd = isLooseEnd;  // TODO: needed here?
  };

  ~Step() = default;

  Step(Step const& other) = default;
  Step& operator=(Step const& other) = delete;
  bool operator==(Step const& other) { return _id == other._id; }
  std::string toString() const {
    return "<Step> _id: " + basics::StringUtils::itoa(_id) +
           ", _weight: " + basics::StringUtils::ftoa(_weight) +
           ", _previous: " + basics::StringUtils::itoa(getPrevious());
  }

  bool isProcessable() const { return _isLooseEnd ? false : true; }
  size_t getVertex() const { return _id; }  // TODO: adjust
  size_t getEdge() const { return _id; }    // TODO: adjust
};

using TypesToTest =
    ::testing::Types<PathValidator<PathStore<Step>, VertexUniquenessLevel::NONE>,
                     PathValidator<PathStore<Step>, VertexUniquenessLevel::PATH>>;

template <class ValidatorType>
class PathValidatorTest : public ::testing::Test {
  arangodb::ResourceMonitor _resourceMonitor{};

  PathStore<Step> _pathStore{_resourceMonitor};

 protected:
  VertexUniquenessLevel getVertexUniquness() {
    if constexpr (std::is_same_v<ValidatorType, PathValidator<PathStore<Step>, VertexUniquenessLevel::NONE>>) {
      return VertexUniquenessLevel::NONE;
    } else if constexpr (std::is_same_v<ValidatorType, PathValidator<PathStore<Step>, VertexUniquenessLevel::PATH>>) {
      return VertexUniquenessLevel::PATH;
    } else {
      return VertexUniquenessLevel::GLOBAL;
    }
  }

  PathStore<Step>& store() { return _pathStore; }

  ValidatorType testee() { return ValidatorType{this->store()}; }
};

TYPED_TEST_CASE(PathValidatorTest, TypesToTest);

TYPED_TEST(PathValidatorTest, it_should_honor_uniqueness_on_single_path_first_duplicate) {
  auto ps = this->store();
  auto validator = this->testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  EXPECT_EQ(lastIndex, 0);
  // We add a loop that ends in the start vertex (0) again.
  {
    Step s{1, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s{2, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s{3, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }

  // Add duplicate vertex on Path
  {
    Step s{0, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    if (this->getVertexUniquness() == VertexUniquenessLevel::NONE) {
      // No uniqueness check, take the vertex
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    } else {
      // With PATH or GLOBAL uniqueness this vertex is illegal
      EXPECT_TRUE(res.isFiltered());
      EXPECT_TRUE(res.isPruned());
    }
  }
}

TYPED_TEST(PathValidatorTest, it_should_honor_uniqueness_on_single_path_last_duplicate) {
  auto ps = this->store();
  auto validator = this->testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  EXPECT_EQ(lastIndex, 0);
  // We add a loop on the last vertex of the path (3)
  {
    Step s{1, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s{2, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s{3, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }

  // Add duplicate vertex on Path
  {
    Step s{3, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    if (this->getVertexUniquness() == VertexUniquenessLevel::NONE) {
      // No uniqueness check, take the vertex
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    } else {
      // With PATH or GLOBAL uniqueness this vertex is illegal
      EXPECT_TRUE(res.isFiltered());
      EXPECT_TRUE(res.isPruned());
    }
  }
}

TYPED_TEST(PathValidatorTest, it_should_honor_uniqueness_on_single_path_interior_duplicate) {
  auto ps = this->store();
  auto validator = this->testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  EXPECT_EQ(lastIndex, 0);
  // We add a loop that ends in one interior vertex (2) again.
  {
    Step s{1, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s{2, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s{3, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }

  // Add duplicate vertex on Path
  {
    Step s{2, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    if (this->getVertexUniquness() == VertexUniquenessLevel::NONE) {
      // No uniqueness check, take the vertex
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    } else {
      // With PATH or GLOBAL uniqueness this vertex is illegal
      EXPECT_TRUE(res.isFiltered());
      EXPECT_TRUE(res.isPruned());
    }
  }
}

TYPED_TEST(PathValidatorTest, it_should_honor_uniqueness_on_global_paths_last_duplicate) {
  auto ps = this->store();
  auto validator = this->testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  EXPECT_EQ(lastIndex, 0);
  // We add two paths, each without a loop.
  // Both paths share a common vertex besides the start.

  // First path 0 -> 1 -> 2 -> 3
  {
    Step s{1, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s{2, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s{3, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }

  // First path 0 -> 4 -> 5
  lastIndex = 0;
  {
    Step s{4, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 4);
  }
  {
    Step s{5, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 5);
  }

  // Add duplicate vertex (3) which is the last on the first path
  {
    Step s{3, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    if (this->getVertexUniquness() != VertexUniquenessLevel::GLOBAL) {
      // The vertex is visited twice, but not on same path.
      // As long as we are not GLOBAL this is okay.
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    } else {
      // With GLOBAL uniqueness this vertex is illegal
      EXPECT_TRUE(res.isFiltered());
      EXPECT_TRUE(res.isPruned());
    }
  }
}

TYPED_TEST(PathValidatorTest, it_should_honor_uniqueness_on_global_paths_interior_duplicate) {
  auto ps = this->store();
  auto validator = this->testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  lastIndex = ps.append({0, 1, lastIndex, false});
  EXPECT_EQ(lastIndex, 0);
  // We add two paths, each without a loop.
  // Both paths share a common vertex besides the start.

  // First path 0 -> 1 -> 2 -> 3
  {
    Step s{1, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s{2, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s{3, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }

  // First path 0 -> 4 -> 5
  lastIndex = 0;
  {
    Step s{4, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 4);
  }
  {
    Step s{5, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 5);
  }

  // Add duplicate vertex (1) which is interior to the first path
  {
    Step s{1, 1, lastIndex, false};
    auto res = validator.validatePath(s);
    if (this->getVertexUniquness() != VertexUniquenessLevel::GLOBAL) {
      // The vertex is visited twice, but not on same path.
      // As long as we are not GLOBAL this is okay.
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    } else {
      // With GLOBAL uniqueness this vertex is illegal
      EXPECT_TRUE(res.isFiltered());
      EXPECT_TRUE(res.isPruned());
    }
  }
}

}  // namespace graph_path_validator_test
}  // namespace tests
}  // namespace arangodb