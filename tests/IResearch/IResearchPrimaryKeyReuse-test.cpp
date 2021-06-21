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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchQueryCommon.h"

#include "IResearch/IResearchView.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>

extern const char* ARGV0;  // defined in main.cpp

namespace {

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchPrimaryKeyReuse : public IResearchQueryTest {};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_F(IResearchPrimaryKeyReuse, test_multiple_transactions_sequential) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;
  std::shared_ptr<arangodb::LogicalCollection> collection;

  // create collection0
  {
    auto createJson = VPackParser::fromJson(
        "{ \"name\": \"testCollection0\","
        "  \"usesRevisionsAsDocumentIds\": true }");
    collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);

  }

  // create view
  {
    auto createJson = VPackParser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_FALSE(!logicalView);

    view = logicalView.get();
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    ASSERT_FALSE(!impl);

    auto updateJson = VPackParser::fromJson(
        "{ \"links\": {"
        "\"testCollection0\": { \"analyzers\": [ \"test_analyzer\", "
        "\"identity\" ], \"includeAllFields\": true, \"trackListPositions\": "
        "true, \"storeValues\":\"id\" }"
        "}}");
    EXPECT_TRUE(impl->properties(updateJson->slice(), true).ok());
    std::set<arangodb::DataSourceId> cids;
    impl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_EQ(1, cids.size());
    EXPECT_EQ(TRI_ERROR_NO_ERROR, arangodb::tests::executeQuery(
                                      vocbase,
                                      "FOR d IN testView SEARCH 1 == 1 "
                                      "OPTIONS { waitForSync: true } RETURN d")
                                      .result.errorNumber());  // commit
  }

  // insert initial document
  {
      std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
        VPackParser::fromJson(
            "{ \"value\": true }")
    };

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE(trx.begin().ok());

    for (auto& entry : docs) {
      auto res = trx.insert(collection->name(), entry->slice(), options);
      EXPECT_TRUE(res.ok());
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE(trx.commit().ok());
  }

  // remove-insert loop
  for (std::size_t i = 0; i < 5; ++i) {
    // remove
    {
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                                *collection,
                                                arangodb::AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (auto& entry : insertedDocs) {
        auto res = trx.remove(collection->name(), entry.slice(), options);
        EXPECT_TRUE(res.ok());
      }

      EXPECT_TRUE(trx.commit().ok());
    }

    EXPECT_EQ(TRI_ERROR_NO_ERROR,
              arangodb::tests::executeQuery(
                  vocbase,
                  "FOR d IN testView SEARCH d.value == true "
                  "OPTIONS { waitForSync: true } RETURN d")
                  .result.errorNumber());  // commit

    // reinsert with _rev and isRestore == true
    {
      arangodb::OperationOptions options;
      options.returnNew = true;
      options.isRestore = true;
      arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                                *collection,
                                                arangodb::AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (auto& entry : insertedDocs) {
        auto res = trx.insert(collection->name(), entry.slice(), options);
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(res.slice().get("new").get(arangodb::StaticStrings::RevString).copyString(),
                  entry.slice().get(arangodb::StaticStrings::RevString).copyString());
      }

      EXPECT_TRUE(trx.commit().ok());
    }

    EXPECT_EQ(TRI_ERROR_NO_ERROR,
              arangodb::tests::executeQuery(
                  vocbase,
                  "FOR d IN testView SEARCH d.value == true "
                  "OPTIONS { waitForSync: true } RETURN d")
                  .result.errorNumber());  // commit
  }
}

TEST_F(IResearchPrimaryKeyReuse, test_multiple_transactions_interleaved) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  std::vector<arangodb::velocypack::Builder> extraDocs;
  arangodb::LogicalView* view;
  std::shared_ptr<arangodb::LogicalCollection> collection;

  // create collection0
  {
    auto createJson = VPackParser::fromJson(
        "{ \"name\": \"testCollection0\","
        "  \"usesRevisionsAsDocumentIds\": true }");
    collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);
  }

  // create view
  {
    auto createJson = VPackParser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_FALSE(!logicalView);

    view = logicalView.get();
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    ASSERT_FALSE(!impl);

    auto updateJson = VPackParser::fromJson(
        "{ \"links\": {"
        "\"testCollection0\": { \"analyzers\": [ \"test_analyzer\", "
        "\"identity\" ], \"includeAllFields\": true, \"trackListPositions\": "
        "true, \"storeValues\":\"id\" }"
        "}}");
    EXPECT_TRUE(impl->properties(updateJson->slice(), true).ok());
    std::set<arangodb::DataSourceId> cids;
    impl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_EQ(1, cids.size());
    EXPECT_EQ(TRI_ERROR_NO_ERROR, arangodb::tests::executeQuery(
                                      vocbase,
                                      "FOR d IN testView SEARCH 1 == 1 "
                                      "OPTIONS { waitForSync: true } RETURN d")
                                      .result.errorNumber());  // commit
  }

  std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
      VPackParser::fromJson("{ \"value\": true }")};

  // insert initial document
  {
    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE(trx.begin().ok());

    for (auto& entry : docs) {
      auto res = trx.insert(collection->name(), entry->slice(), options);
      EXPECT_TRUE(res.ok());
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE(trx.commit().ok());
  }

  // remove-insert loop
  for (std::size_t i = 0; i < 5; ++i) {
    // remove
    {
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(vocbase),
          *collection, arangodb::AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (auto& entry : insertedDocs) {
        auto res = trx.remove(collection->name(), entry.slice(), options);
        EXPECT_TRUE(res.ok());
      }

      EXPECT_TRUE(trx.commit().ok());
    }

    EXPECT_EQ(TRI_ERROR_NO_ERROR,
              arangodb::tests::executeQuery(
                  vocbase,
                  "FOR d IN testView SEARCH d.value == true "
                  "OPTIONS { waitForSync: true } RETURN d")
                  .result.errorNumber());  // commit

    // insert some extra doc
    {
      arangodb::OperationOptions options;
      options.returnNew = true;
      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(vocbase),
          *collection, arangodb::AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (auto& entry : docs) {
        auto res = trx.insert(collection->name(), entry->slice(), options);
        EXPECT_TRUE(res.ok());
        extraDocs.emplace_back(res.slice().get("new"));
      }

      EXPECT_TRUE(trx.commit().ok());
    }

    // remove the extra
    {
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(vocbase),
          *collection, arangodb::AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (auto& entry : extraDocs) {
        auto res = trx.remove(collection->name(), entry.slice(), options);
        EXPECT_TRUE(res.ok());
      }

      EXPECT_TRUE(trx.commit().ok());
      extraDocs.clear();
    }

    // reinsert with _rev and isRestore == true
    {
      arangodb::OperationOptions options;
      options.returnNew = true;
      options.isRestore = true;
      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(vocbase),
          *collection, arangodb::AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (auto& entry : insertedDocs) {
        auto res = trx.insert(collection->name(), entry.slice(), options);
        EXPECT_TRUE(res.ok());
        EXPECT_EQ(res.slice().get("new").get(arangodb::StaticStrings::RevString).copyString(),
                  entry.slice().get(arangodb::StaticStrings::RevString).copyString());
      }

      EXPECT_TRUE(trx.commit().ok());
    }

    EXPECT_EQ(TRI_ERROR_NO_ERROR,
              arangodb::tests::executeQuery(
                  vocbase,
                  "FOR d IN testView SEARCH d.value == true "
                  "OPTIONS { waitForSync: true } RETURN d")
                  .result.errorNumber());  // commit
  }
}

TEST_F(IResearchPrimaryKeyReuse, test_single_transaction) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;
  std::shared_ptr<arangodb::LogicalCollection> collection;

  // create collection0
  {
    auto createJson = VPackParser::fromJson(
        "{ \"name\": \"testCollection0\","
        "  \"usesRevisionsAsDocumentIds\": true }");
    collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);
  }

  // create view
  {
    auto createJson = VPackParser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_FALSE(!logicalView);

    view = logicalView.get();
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    ASSERT_FALSE(!impl);

    auto updateJson = VPackParser::fromJson(
        "{ \"links\": {"
        "\"testCollection0\": { \"analyzers\": [ \"test_analyzer\", "
        "\"identity\" ], \"includeAllFields\": true, \"trackListPositions\": "
        "true, \"storeValues\":\"id\" }"
        "}}");
    EXPECT_TRUE(impl->properties(updateJson->slice(), true).ok());
    std::set<arangodb::DataSourceId> cids;
    impl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_EQ(1, cids.size());
    EXPECT_EQ(TRI_ERROR_NO_ERROR, arangodb::tests::executeQuery(
                                      vocbase,
                                      "FOR d IN testView SEARCH 1 == 1 "
                                      "OPTIONS { waitForSync: true } RETURN d")
                                      .result.errorNumber());  // commit
  }

  // insert initial document
  {
    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
        VPackParser::fromJson("{ \"value\": true }")};

    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE(trx.begin().ok());

    for (auto& entry : docs) {
      arangodb::OperationOptions options;
      options.returnNew = true;
      auto res = trx.insert(collection->name(), entry->slice(), options);
      EXPECT_TRUE(res.ok());
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    // remove-insert loop
    for (std::size_t i = 0; i < 5; ++i) {
      // remove
      {
        arangodb::OperationOptions options;
        for (auto& entry : insertedDocs) {
          auto res = trx.remove(collection->name(), entry.slice(), options);
          EXPECT_TRUE(res.ok());
        }
      }

      // reinsert with _rev and isRestore == true
      {
        arangodb::OperationOptions options;
        options.returnNew = true;
        options.isRestore = true;
        for (auto& entry : insertedDocs) {
          auto res = trx.insert(collection->name(), entry.slice(), options);
          EXPECT_TRUE(res.ok());
          EXPECT_EQ(
              res.slice().get("new").get(arangodb::StaticStrings::RevString).copyString(),
              entry.slice().get(arangodb::StaticStrings::RevString).copyString());
        }
      }
    }

    EXPECT_TRUE(trx.commit().ok());

    EXPECT_EQ(TRI_ERROR_NO_ERROR,
              arangodb::tests::executeQuery(
                  vocbase,
                  "FOR d IN testView SEARCH d.value == true "
                  "OPTIONS { waitForSync: true } RETURN d")
                  .result.errorNumber());  // commit
  }
}
