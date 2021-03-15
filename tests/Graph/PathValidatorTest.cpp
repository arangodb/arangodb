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

#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/StringHeap.h"
#include "Basics/StringUtils.h"
#include "Graph/PathManagement/PathStore.cpp"
#include "Graph/PathManagement/PathValidator.cpp"
#include "Graph/Providers/BaseStep.h"
#include "Graph/Types/UniquenessLevel.h"

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::velocypack;

namespace arangodb {
namespace tests {
namespace graph_path_validator_test {

static_assert(GTEST_HAS_TYPED_TEST, "We need typed tests for the following:");

class Step : public arangodb::graph::BaseStep<Step> {
 public:
  using Vertex = HashedStringRef;
  using Edge = HashedStringRef;

  HashedStringRef _id;

  Step(HashedStringRef id, size_t previous)
      : arangodb::graph::BaseStep<Step>{previous}, _id{id} {};

  ~Step() = default;

  bool operator==(Step const& other) { return _id == other._id; }

  std::string toString() const {
    return "<Step> _id: " + _id.toString() +
           ", _previous: " + basics::StringUtils::itoa(getPrevious());
  }

  bool isProcessable() const { return true; }
  HashedStringRef getVertex() const { return _id; }
  HashedStringRef getEdge() const { return _id; }

  arangodb::velocypack::HashedStringRef getVertexIdentifier() const {
    return getVertex();
  }
};

using TypesToTest =
    ::testing::Types<PathValidator<PathStore<Step>, VertexUniquenessLevel::NONE>,
                     PathValidator<PathStore<Step>, VertexUniquenessLevel::PATH>,
                     PathValidator<PathStore<Step>, VertexUniquenessLevel::GLOBAL>>;

template <class ValidatorType>
class PathValidatorTest : public ::testing::Test {
  arangodb::GlobalResourceMonitor _global{};
  arangodb::ResourceMonitor _resourceMonitor{_global};

  PathStore<Step> _pathStore{_resourceMonitor};
  StringHeap _heap{_resourceMonitor, 4096};

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
  Step makeStep(size_t id, size_t previous) {
    std::string idStr = basics::StringUtils::itoa(id);
    HashedStringRef hStr(idStr.data(), static_cast<uint32_t>(idStr.length()));
    return Step(_heap.registerString(hStr), previous);
  }
};

TYPED_TEST_CASE(PathValidatorTest, TypesToTest);

TYPED_TEST(PathValidatorTest, it_should_honor_uniqueness_on_single_path_first_duplicate) {
  auto&& ps = this->store();
  auto validator = this->testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  {
    Step s = this->makeStep(0, lastIndex);
    auto res = validator.validatePath(s);
    // The start vertex is always valid
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 0);
  }

  // We add a loop that ends in the start vertex (0) again.
  {
    Step s = this->makeStep(1, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s = this->makeStep(2, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s = this->makeStep(3, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }

  // Add duplicate vertex on Path
  {
    Step s = this->makeStep(0, lastIndex);
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
  auto&& ps = this->store();
  auto validator = this->testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  {
    Step s = this->makeStep(0, lastIndex);
    auto res = validator.validatePath(s);
    // The start vertex is always valid
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 0);
  }
  // We add a loop on the last vertex of the path (3)
  {
    Step s = this->makeStep(1, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s = this->makeStep(2, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s = this->makeStep(3, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }

  // Add duplicate vertex on Path
  {
    Step s = this->makeStep(3, lastIndex);
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
  auto&& ps = this->store();
  auto validator = this->testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  {
    Step s = this->makeStep(0, lastIndex);
    auto res = validator.validatePath(s);
    // The start vertex is always valid
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 0);
  }
  // We add a loop that ends in one interior vertex (2) again.
  {
    Step s = this->makeStep(1, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s = this->makeStep(2, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s = this->makeStep(3, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }

  // Add duplicate vertex on Path
  {
    Step s = this->makeStep(2, lastIndex);
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
  auto&& ps = this->store();
  auto validator = this->testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  {
    Step s = this->makeStep(0, lastIndex);
    auto res = validator.validatePath(s);
    // The start vertex is always valid
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 0);
  }
  // We add two paths, each without a loop.
  // Both paths share a common vertex besides the start.

  // First path 0 -> 1 -> 2 -> 3
  {
    Step s = this->makeStep(1, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s = this->makeStep(2, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s = this->makeStep(3, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }

  // First path 0 -> 4 -> 5
  lastIndex = 0;
  {
    Step s = this->makeStep(4, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 4);
  }
  {
    Step s = this->makeStep(5, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 5);
  }

  // Add duplicate vertex (3) which is the last on the first path
  {
    Step s = this->makeStep(3, lastIndex);
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
  auto&& ps = this->store();
  auto validator = this->testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  {
    Step s = this->makeStep(0, lastIndex);
    auto res = validator.validatePath(s);
    // The start vertex is always valid
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 0);
  }
  // We add two paths, each without a loop.
  // Both paths share a common vertex besides the start.

  // First path 0 -> 1 -> 2 -> 3
  {
    Step s = this->makeStep(1, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s = this->makeStep(2, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s = this->makeStep(3, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }

  // First path 0 -> 4 -> 5
  lastIndex = 0;
  {
    Step s = this->makeStep(4, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 4);
  }
  {
    Step s = this->makeStep(5, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 5);
  }

  // Add duplicate vertex (1) which is interior to the first path
  {
    Step s = this->makeStep(1, lastIndex);
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
