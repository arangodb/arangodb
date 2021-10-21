////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////
#include "common.h"
#include "gtest/gtest.h"
#include "IResearch/common.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Query.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchInvertedIndex.h"
#include "IResearch/ExpressionContextMock.h"
#include "IResearch/IResearchFilterFactory.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"
#include "RestServer/DatabaseFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/Methods/Collections.h"
#include "Basics/StaticStrings.h"

class IResearchInvertedIndexConditionTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;
  std::shared_ptr<arangodb::LogicalCollection> _collection;

 protected:
  IResearchInvertedIndexConditionTest() {
    arangodb::tests::init();
    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    dbFeature.createDatabase(testDBInfo(server.server()), _vocbase);

    arangodb::OperationOptions options(arangodb::ExecContext::current());
    arangodb::methods::Collections::createSystem(*_vocbase, options,
                                                 arangodb::tests::AnalyzerCollectionName,
                                                 false, _collection);
  }
  VPackBuilder getPropertiesSlice(arangodb::IndexId iid, std::vector<std::string> const& fields) {
    VPackBuilder vpack;
    {
      VPackObjectBuilder obj(&vpack);
      vpack.add(arangodb::StaticStrings::IndexId, VPackValue(iid.id()));
      vpack.add(arangodb::StaticStrings::IndexType, VPackValue("inverted"));

      //FIXME: maybe this should be set by index internally ?
      vpack.add(arangodb::StaticStrings::IndexUnique, VPackValue(false));
      vpack.add(arangodb::StaticStrings::IndexSparse, VPackValue(true));

      {
        VPackArrayBuilder arrayFields(&vpack, arangodb::StaticStrings::IndexFields);
        for (auto const& f : fields) {
          vpack.add(VPackValue(f));
        }
      }
    }
    return vpack;
  }

  ~IResearchInvertedIndexConditionTest() {
    _collection.reset();
  }

  void estimateFilterCondition(
      std::string const& queryString, std::vector<std::string> const& fields,
      arangodb::Index::FilterCosts const& expectedCosts,
      arangodb::aql::ExpressionContext* exprCtx = nullptr,
      std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
      std::string const& refName = "d") {
    SCOPED_TRACE(testing::Message("estimateFilterCondition failed for query:<")
      << queryString << "> Expected support:" << expectedCosts.supportsCondition);
    arangodb::IndexId id(1);
    arangodb::iresearch::InvertedIndexFieldMeta meta;
    std::string errorField;
    meta.init(server.server(), getPropertiesSlice(id, fields).slice(), false, errorField, _vocbase->name());
    auto indexFields =arangodb::iresearch::IResearchInvertedIndex::fields(meta);
    arangodb::iresearch::IResearchInvertedIndex Index(id, *_collection, std::move(meta));


    auto ctx = std::make_shared<arangodb::transaction::StandaloneContext>(vocbase());
    auto query = arangodb::aql::Query::create(ctx, arangodb::aql::QueryString(queryString),
                                              bindVars);

    ASSERT_NE(query.get(), nullptr);
    auto const parseResult = query->parse();
    ASSERT_TRUE(parseResult.result.ok());

    auto* ast = query->ast();
    ASSERT_TRUE(ast);

    auto* root = ast->root();
    ASSERT_TRUE(root);

    // find first FILTER node
    arangodb::aql::AstNode* filterNode = nullptr;
    for (size_t i = 0; i < root->numMembers(); ++i) {
      auto* node = root->getMemberUnchecked(i);
      ASSERT_TRUE(node);

      if (arangodb::aql::NODE_TYPE_FILTER == node->type) {
        filterNode = node;
        break;
      }
    }
    ASSERT_TRUE(filterNode);

    // find referenced variable
    auto* allVars = ast->variables();
    ASSERT_TRUE(allVars);
    arangodb::aql::Variable* ref = nullptr;
    for (auto entry : allVars->variables(true)) {
      if (entry.second == refName) {
        ref = allVars->getVariable(entry.first);
        break;
      }
    }
    ASSERT_TRUE(ref);

    // optimization time
    {
      arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase()),
                                          {}, {}, {}, arangodb::transaction::Options());
      auto* mockCtx = dynamic_cast<ExpressionContextMock*>(exprCtx);
      if (mockCtx) {
        mockCtx->setTrx(&trx);
      }

      arangodb::iresearch::QueryContext const ctx{&trx, nullptr, nullptr, mockCtx, nullptr, ref};
      auto costs = Index.supportsFilterCondition(id, indexFields, {}, filterNode, ref, 0);
      ASSERT_EQ(expectedCosts.supportsCondition, costs.supportsCondition);
    }
    // runtime is not intended - we must decide during optimize time!
  }

  arangodb::LogicalCollection& collection() {return *_collection;}
  TRI_vocbase_t& vocbase() { return *_vocbase; }
};  // IResearchFilterSetup

TEST_F(IResearchInvertedIndexConditionTest, test_with_equality) {
  std::string queryString = "FOR d IN test FILTER d.a == 'value' RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_not_mix_atr) {
  std::string queryString = "FOR c IN test FOR d IN test FILTER d.a == c.missing RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_equality_index) {
  std::string queryString = "FOR d IN test FILTER d.a[5] == 'value' RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_equality_index_attribute) {
  std::string queryString = "FOR d IN test FILTER d['a'] == 'value' RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_equality_index_attribute_chain) {
  std::string queryString = "FOR d IN test FILTER d.a['b'] == 'value' RETURN d ";
  std::vector<std::string> fields = {"a.b"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_equality_index_attribute_chain_missing) {
  std::string queryString = "FOR d IN test FILTER d['a']['c'] == 'value' RETURN d ";
  std::vector<std::string> fields = {"a.b"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_equality_index_attribute_missing) {
  std::string queryString = "FOR d IN test FILTER d['a'] == 'value' RETURN d ";
  std::vector<std::string> fields = {"b"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}


TEST_F(IResearchInvertedIndexConditionTest, test_with_equality_expansion) {
  std::string queryString = "FOR d IN test FILTER d.a[*] == 'value' RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_simple_expression) {
  std::string queryString = "FOR d IN test FILTER d.a == 'value' AND (1==1) RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_simple_expression_normalization) {
  std::string queryString = "FOR d IN test FILTER d.a == 'value' AND (1==d.a) RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_equality_many_fields) {
  std::string queryString = "FOR d IN test FILTER d.a == 'value' OR d.b == 'value2' AND d.c == 'value3' RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_fcalls) {
  std::string queryString = "FOR d IN test FILTER d.a == 'value' OR d.b == 'value2' AND d.c == UPPER('value3') RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_fcalls_on_ref) {
  std::string queryString = "FOR d IN test FILTER d.a == 'value' OR d.b == 'value2' AND UPPER(d.c) == UPPER('value3') RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_array_comparison) {
  std::string queryString = "FOR d IN test FILTER [1,2,3] ALL IN d.a  RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_array_comparison_ref) {
  std::string queryString = "FOR d IN test FILTER ['A', 'B', 'C', UPPER(d.a)] ANY IN d.a  RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_array_as_nodeterm_var_comparison) {
  std::string queryString = "LET arr = [1,2, NOOPT(3)] FOR d IN test FILTER arr ALL IN d.a  RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true; // we can support as NOOPT will be evaluated out of our scope
  estimateFilterCondition(queryString, fields, expected);
}


TEST_F(IResearchInvertedIndexConditionTest, test_with_array_as_var_comparison) {
  std::string queryString = "LET arr = [1,2, 3] FOR d IN test FILTER arr ALL IN d.a  RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_in_array) {
  std::string queryString = "LET arr = [1,2,3] FOR d IN test FILTER d.a IN arr RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_in_nondeterm_array) {
  std::string queryString = "LET arr = [1,2,NOOPT(3)] FOR d IN test FILTER d.a IN arr RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true; // NOOPT is evaluated out of our loop - so we support this
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_in_nondeterm_array_ref) {
  std::string queryString = "FOR d IN test FILTER d.a IN [1,2, d.c] RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_range) {
  std::string queryString = "FOR d IN test FILTER d.a IN 1..10 RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_nondet_var_range) {
  std::string queryString = "LET lim = NOOPT(10) FOR d IN test FILTER d.a IN 1..lim RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_nondet_range) {
  std::string queryString = "FOR d IN test FILTER d.a IN 1..NOOPT(10) RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_range_as_var) {
  std::string queryString = "LET r = 1..10 FOR d IN test FILTER d.a IN r RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_nondet_range_as_var) {
  std::string queryString = "LET r = 1..NOOPT(10) FOR d IN test FILTER d.a IN r RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_negation) {
  std::string queryString = "FOR d IN test FILTER NOT(d.a == 'c') RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_nondet_negation) {
  std::string queryString = "FOR d IN test FILTER NOT(d.a == d.b) RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_boost) {
  std::string queryString = "FOR d IN test FILTER BOOST(d.a == 10, 10) RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_nondet_boost) {
  std::string queryString = "FOR d IN test FILTER BOOST(d.a == d.b, 10) RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_nondet_analyzer) {
  ExpressionContextMock ctx; // need this for trx for analyzer pool
  std::string queryString = "FOR d IN test FILTER ANALYZER(d.a == d.b, 'text_en') RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected, &ctx);
}

TEST_F(IResearchInvertedIndexConditionTest, test_analyzer) {
  ExpressionContextMock ctx; // need this for trx for analyzer pool
  std::string queryString = "FOR d IN test FILTER ANALYZER(d.a == '10', 'text_en') RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected, &ctx);
}

TEST_F(IResearchInvertedIndexConditionTest, test_exists) {
  std::string queryString = "FOR d IN test FILTER EXISTS(d.a, 'string') RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_no_fields) {
  std::string queryString = "FOR d IN test FILTER d.a == 'value' RETURN d ";
  std::vector<std::string> fields = {"b"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_sub_fields_no_nested) {
  std::string queryString = "FOR d IN test FILTER d.b == 'value' RETURN d ";
  std::vector<std::string> fields = {"b.a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_sub_fields_wrong_nested) {
  std::string queryString = "FOR d IN test FILTER d.b.a == 'value' RETURN d ";
  std::vector<std::string> fields = {"b.c"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_sub_fields_covered) {
  std::string queryString = "FOR d IN test FILTER d.b.a == 'value' RETURN d ";
  std::vector<std::string> fields = {"b.a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_no_fields_one_missing) {
  std::string queryString = "FOR d IN test FILTER d.a == 'value' OR d.b == 'c' RETURN d ";
  std::vector<std::string> fields = {"b"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_nondeterm_expression) {
  std::string queryString = "FOR d IN test FILTER d.a == NOOPT('value') RETURN d ";
  std::vector<std::string> fields = {"a"};
  estimateFilterCondition(queryString, fields, arangodb::Index::FilterCosts::defaultCosts(0));
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_subquery_same_atr) {
  std::string queryString = "FOR a IN test FOR d IN test FILTER d.a == a.a RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_subquery_not_same_atr) {
  std::string queryString = "FOR a IN test FOR d IN test FILTER d.a == a.b RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_subquery_fcall) {
  std::string queryString = "FOR a IN test FOR d IN test FILTER d.a == UPPER(a.b) RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_subquery_non_determ_fcall) {
  std::string queryString = "FOR a IN test2 FOR d IN test FILTER d.a == NOOPT(a.b) RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_range_func) {
  std::string queryString = "LET a  = 10  FOR d IN test FILTER IN_RANGE(d.a, a, 20, true, true) RETURN d ";
  ExpressionContextMock ctx;
  auto obj = VPackParser::fromJson("10");
  ctx.vars.emplace("a", arangodb::aql::AqlValue(obj->slice()));
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected, &ctx);
}


TEST_F(IResearchInvertedIndexConditionTest, test_with_range_func_bind) {
  auto obj = VPackParser::fromJson("10");
  ExpressionContextMock ctx;
  ctx.vars.emplace("x", arangodb::aql::AqlValue(obj->slice()));
  auto obj2 = VPackParser::fromJson("20");
  ctx.vars.emplace("a", arangodb::aql::AqlValue(obj2->slice()));
  std::string queryString = "LET a  = 20 LET x = 10  FOR d IN test FILTER IN_RANGE(d.a, x, a, true, true) RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected, &ctx);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_levenshtein_nondet) {
  ExpressionContextMock ctx;
  auto obj2 = VPackParser::fromJson("2");
  ctx.vars.emplace("a", arangodb::aql::AqlValue(obj2->slice()));
  std::string queryString = "LET a  = 2 FOR d IN test FILTER LEVENSHTEIN_MATCH(d.a, 'sometext', NOOPT(a), true, 5) RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected, &ctx);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_levenshtein) {
  ExpressionContextMock ctx;
  auto obj2 = VPackParser::fromJson("2");
  ctx.vars.emplace("a", arangodb::aql::AqlValue(obj2->slice()));
  std::string queryString = "LET a  = 2 FOR d IN test FILTER LEVENSHTEIN_MATCH(d.a, 'sometext', a, true, 5) RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected, &ctx);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_levenshtein_longdist) {
  ExpressionContextMock ctx;
  std::string queryString = "FOR d IN test FILTER LEVENSHTEIN_MATCH(d.a, 'sometext', 10, true, 5) RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected, &ctx);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_object_equal) {
  ExpressionContextMock ctx;
  std::string queryString = "FOR d IN test FILTER  d.a == {a:1, b:2} RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected, &ctx);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_array_comparison_righthand) {
  ExpressionContextMock ctx;
  std::string queryString = "FOR d IN test FILTER  d.a ANY IN [1,2,3] RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected, &ctx);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_array_comparison_equality) {
  ExpressionContextMock ctx;
  std::string queryString = "FOR d IN test FILTER [1,2,3] ANY == d.a RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  estimateFilterCondition(queryString, fields, expected, &ctx);
}
