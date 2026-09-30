////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Indexes/IndexFactory.h"
#include "Mocks/Servers.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;

namespace {

struct SpyDefinition {
  explicit SpyDefinition(int tag = 0) : tag(tag) {}

  bool equal(velocypack::Slice, velocypack::Slice, std::string const&) const {
    equalCalled = true;
    return equalResult;
  }

  Result normalize(velocypack::Builder&, velocypack::Slice, bool,
                   Database const&) const {
    normalizeCalled = true;
    return {};
  }

  bool attributeOrderMatters() const {
    attributeOrderMattersCalled = true;
    return attributeOrderMattersResult;
  }

  int tag;
  bool equalResult = true;
  bool attributeOrderMattersResult = true;
  mutable bool equalCalled = false;
  mutable bool normalizeCalled = false;
  mutable bool attributeOrderMattersCalled = false;
};

struct SpyIndexFactory : public DelegatingIndexFactory<SpyDefinition> {
  using DelegatingIndexFactory::DelegatingIndexFactory;

  std::shared_ptr<Index> instantiate(LogicalCollection&, velocypack::Slice,
                                     IndexId, bool) const override {
    return nullptr;
  }

  SpyDefinition& definition() { return _definition; }
};

}  // namespace

class DelegatingIndexFactoryTest : public ::testing::Test {
 protected:
  tests::mocks::MockRestServer mockServer;
};

TEST_F(DelegatingIndexFactoryTest, forwardsConstructorArgsToDefinition) {
  SpyIndexFactory factory(mockServer.server(), 42);
  EXPECT_EQ(factory.definition().tag, 42);
}

TEST_F(DelegatingIndexFactoryTest, equalDelegatesToDefinition) {
  SpyIndexFactory factory(mockServer.server());
  factory.definition().equalResult = false;

  EXPECT_FALSE(factory.equal(velocypack::Slice(), velocypack::Slice(), "test"));
  EXPECT_TRUE(factory.definition().equalCalled);
}

TEST_F(DelegatingIndexFactoryTest, normalizeDelegatesToDefinition) {
  SpyIndexFactory factory(mockServer.server());
  velocypack::Builder builder;

  factory.normalize(builder, velocypack::Slice(), true,
                    mockServer.getSystemDatabase());

  EXPECT_TRUE(factory.definition().normalizeCalled);
}

TEST_F(DelegatingIndexFactoryTest, attributeOrderMattersDelegatesToDefinition) {
  SpyIndexFactory factory(mockServer.server());
  factory.definition().attributeOrderMattersResult = false;

  EXPECT_FALSE(factory.attributeOrderMatters());
  EXPECT_TRUE(factory.definition().attributeOrderMattersCalled);
}

TEST_F(DelegatingIndexFactoryTest, dispatchesThroughIndexTypeFactoryInterface) {
  SpyIndexFactory factory(mockServer.server());
  factory.definition().equalResult = false;
  IndexTypeFactory const& base = factory;

  EXPECT_FALSE(base.equal(velocypack::Slice(), velocypack::Slice(), "test"));
}
