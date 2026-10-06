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

  SpyDefinition const& definition() const { return _definition; }
};

}  // namespace

class DelegatingIndexFactoryTest : public ::testing::Test {
 protected:
  tests::mocks::MockRestServer mockServer;
};

TEST_F(DelegatingIndexFactoryTest, referencesTheGivenDefinition) {
  SpyDefinition definition;
  SpyIndexFactory factory(mockServer.server(), definition);
  EXPECT_EQ(&factory.definition(), &definition);
}

TEST_F(DelegatingIndexFactoryTest, equalDelegatesToDefinition) {
  SpyDefinition definition;
  SpyIndexFactory factory(mockServer.server(), definition);
  definition.equalResult = false;

  EXPECT_FALSE(factory.equal(velocypack::Slice(), velocypack::Slice(), "test"));
  EXPECT_TRUE(definition.equalCalled);
}

TEST_F(DelegatingIndexFactoryTest, normalizeDelegatesToDefinition) {
  SpyDefinition definition;
  SpyIndexFactory factory(mockServer.server(), definition);
  velocypack::Builder builder;

  factory.normalize(builder, velocypack::Slice(), true,
                    mockServer.getSystemDatabase());

  EXPECT_TRUE(definition.normalizeCalled);
}

TEST_F(DelegatingIndexFactoryTest, attributeOrderMattersDelegatesToDefinition) {
  SpyDefinition definition;
  SpyIndexFactory factory(mockServer.server(), definition);
  definition.attributeOrderMattersResult = false;

  EXPECT_FALSE(factory.attributeOrderMatters());
  EXPECT_TRUE(definition.attributeOrderMattersCalled);
}

TEST_F(DelegatingIndexFactoryTest, dispatchesThroughIndexTypeFactoryInterface) {
  SpyDefinition definition;
  SpyIndexFactory factory(mockServer.server(), definition);
  definition.equalResult = false;
  IndexTypeFactory const& base = factory;

  EXPECT_FALSE(base.equal(velocypack::Slice(), velocypack::Slice(), "test"));
}
