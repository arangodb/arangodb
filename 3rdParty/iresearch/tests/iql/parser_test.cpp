////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "iql/parser.hh"
#include "iql/parser_context.hpp"

namespace tests {
  class test_context: public irs::iql::parser_context {
  public:
    test_context(std::string& sData): parser_context(sData) {}
    test_context(std::string& sData, irs::iql::functions const& functions): parser_context(sData, functions) {}
    template<typename T>
    typename T::contextual_function_t const& function(T const& fn) const { return irs::iql::parser_context::function(fn); }
    query_node const& node(size_t const& id) const { return find_node(id); }
    query_state state() { return current_state(); }
  };

  class IqlParserTestSuite: public ::testing::Test {
    virtual void SetUp() {
      // Code here will be called immediately after the constructor (right before each test).
      // use the following code to enble parser debug outut
      //::irs::iql::debug(parser, [true|false]);
    }

    virtual void TearDown() {
      // Code here will be called immediately after each test (right before the destructor).
    }
  };
}

using namespace tests;
using namespace irs::iql;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IqlParserTestSuite, test_optional_space) {
  {
    std::string sData = "b==a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("b", pNode->sValue);
  }

  {
    std::string sData = " b==a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("b", pNode->sValue);
  }

  {
    std::string sData = "b == a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("b", pNode->sValue);
  }

  {
    std::string sData = "b == a ";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("b", pNode->sValue);
  }

  // comments as whitespace
  {
    std::string sData = " /*456*/b/*789*/ ==/*comment1 d==e */ a/*123*/ ";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("b", pNode->sValue);
  }
}

TEST_F(IqlParserTestSuite, test_sequence) {
  {
    std::string sData = "123==a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("123", pNode->sValue);
  }

  {
    std::string sData = "123.45==a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("123.45", pNode->sValue);
  }

  {
    std::string sData = "'abc'==a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("abc", pNode->sValue);
  }

  {
    std::string sData = "'abc def ghi'==a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("abc def ghi", pNode->sValue);
  }

  {
    std::string sData = "'abc''def''ghi'==a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("abc'def'ghi", pNode->sValue);
  }

  {
    std::string sData = "\"abc\"==a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("abc", pNode->sValue);
  }

  {
    std::string sData = "\"abc def ghi\"==a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("abc def ghi", pNode->sValue);
  }

  {
    std::string sData = "\"abc\"\"def\"\"ghi\"==a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("abc\"def\"ghi", pNode->sValue);
  }

  {
    std::string sData = u8"\u041F\u043E==a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    std::basic_string<wchar_t> actual;
    ASSERT_EQ(u8"\u041F\u043E", pNode->sValue);
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  {
    std::string sData = "a==abc'def'";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(7, state.nOffset);
    ASSERT_EQ(6, state.pError->nStart);
  }

  {
    std::string sData = "a=='abc'def";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(11, state.nOffset);
    ASSERT_EQ(8, state.pError->nStart);
  }

  {
    std::string sData = "a==abc\"def\"";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(7, state.nOffset);
    ASSERT_EQ(6, state.pError->nStart);
  }

  {
    std::string sData = "a==\"abc\"def";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(11, state.nOffset);
    ASSERT_EQ(8, state.pError->nStart);
  }


  {
    std::string sData = "a=='abc'\"def\"";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(9, state.nOffset);
    ASSERT_EQ(8, state.pError->nStart);
  }
}

TEST_F(IqlParserTestSuite, test_bool_function) {
  std::string sFnResult;
  irs::iql::boolean_function::deterministic_function_t fnFail = [](
    irs::iql::boolean_function::deterministic_buffer_t&,
    const irs::iql::boolean_function::deterministic_function_args_t&
  )->bool {
    std::cerr << "File: " << __FILE__ << " Line: " << __LINE__ << " Failed" << std::endl;
    throw "Fail";
  };
  irs::iql::boolean_function::contextual_function_t fnFailDyn = [](
    irs::iql::boolean_function::contextual_buffer_t&,
    const irs::string_ref&,
    void*,
    const irs::iql::boolean_function::contextual_function_args_t&
  )->bool {
    std::cerr << "File: " << __FILE__ << " Line: " << __LINE__ << " Failed" << std::endl;
    throw "Fail";
  };
  irs::iql::sequence_function::contextual_function_t fnArgFailDyn = [](
    irs::iql::sequence_function::contextual_buffer_t&,
    const irs::string_ref&,
    void*,
    const irs::iql::sequence_function::contextual_function_args_t&
  )->bool {
    std::cerr << "File: " << __FILE__ << " Line: " << __LINE__ << " Failed" << std::endl;
    throw "Fail";
  };
  irs::iql::sequence_function::deterministic_function_t fnTestArg = [](
    irs::iql::sequence_function::deterministic_buffer_t& buf,
    const irs::iql::sequence_function::deterministic_function_args_t& args
  )->bool {
    std::string sDelim = "";

    buf.append("[");

    for (auto& arg: args) {
      buf.append(sDelim).append("'").append(arg).append("'");
      sDelim = ",";
    }

    buf.append("]");

    return true;
  };
  irs::iql::boolean_function::deterministic_function_t fnTestFalse = [&sFnResult](
    irs::iql::boolean_function::deterministic_buffer_t& buf,
    const irs::iql::boolean_function::deterministic_function_args_t& args
  )->bool {
    std::string sDelim = "";

    sFnResult.append("[");

    for (auto& arg: args) {
      sFnResult.append(sDelim).append("'").append(arg.c_str(), arg.size()).append("'");
      sDelim = ",";
    }

    sFnResult.append("]");
    buf = false;

    return true;
  };
  irs::iql::boolean_function::contextual_function_t fnTestTrueDyn = [](
    irs::iql::boolean_function::contextual_buffer_t&,
    const irs::string_ref&,
    void*,
    const irs::iql::boolean_function::contextual_function_args_t&
  )->bool {
    return true;
  };
  irs::iql::boolean_function::deterministic_function_t fnTestTrue = [&sFnResult](
    irs::iql::boolean_function::deterministic_buffer_t& buf,
    const irs::iql::boolean_function::deterministic_function_args_t& args
  )->bool {
      std::string sDelim = "";

      sFnResult.append("[");

      for (auto& arg: args) {
        sFnResult.append(sDelim).append("'").append(arg.c_str(), arg.size()).append("'");
        sDelim = ",";
      }

    sFnResult.append("]");
    buf = true;

    return true;
  };

  {
    std::string sData = "fun()";
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("fun", irs::iql::boolean_function(fnTestTrue));
    sFnResult.clear();
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->BOOL_TRUE, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);

    ASSERT_EQ("[]", sFnResult);
  }

  {
    std::string sData = "fun()";
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("fun", irs::iql::boolean_function(fnTestFalse));
    sFnResult.clear();
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->BOOL_TRUE, pQuery->type);
    ASSERT_TRUE(pQuery->bNegated);

    ASSERT_EQ("[]", sFnResult);
  }

  {
    std::string sData = "'fun cti.on'()";
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("fun cti.on", irs::iql::boolean_function(fnTestTrue));
    sFnResult.clear();
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->BOOL_TRUE, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);

    ASSERT_EQ("[]", sFnResult);
  }

  {
    std::string sData = "f(1)";
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f", irs::iql::boolean_function(fnTestTrue, 1));
    sFnResult.clear();
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->BOOL_TRUE, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);

    ASSERT_EQ("['1']", sFnResult);
  }

  {
    std::string sData = "f(a, b, c)";
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f", irs::iql::boolean_function(fnTestTrue));
    sFnResult.clear();
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->BOOL_TRUE, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);

    ASSERT_EQ("['a','b','c']", sFnResult);
  }

  {
    std::string sData = "f(g(b), h(c))";
    irs::iql::boolean_functions boolFns;
    irs::iql::sequence_functions seqFns;
    irs::iql::functions fns(boolFns, seqFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f", irs::iql::boolean_function(fnTestTrue));
    seqFns.emplace("g", irs::iql::sequence_function(fnTestArg));
    seqFns.emplace("h", irs::iql::sequence_function(fnTestArg));
    sFnResult.clear();
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->BOOL_TRUE, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);

    ASSERT_EQ("['['b']','['c']']", sFnResult);
  }

  {
    std::string sData = "f(1)";
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f", irs::iql::boolean_function(fnTestTrue, 0, true)); // #arg < nFixedArg
    sFnResult.clear();
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->BOOL_TRUE, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);

    ASSERT_EQ("['1']", sFnResult);
  }

  {
    std::string sData = "f(1)";
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f", irs::iql::boolean_function(fnTestTrue, 1, true)); // #arg == nFixedArg
    sFnResult.clear();
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->BOOL_TRUE, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);

    ASSERT_EQ("['1']", sFnResult);
  }

  {
    std::string sData = "f(1, 2)";
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f", irs::iql::boolean_function(fnFail, 0, true));
    boolFns.emplace("f", irs::iql::boolean_function(fnFail, 1, true));
    boolFns.emplace("f", irs::iql::boolean_function(fnTestTrue, 2, true)); // best match
    boolFns.emplace("f", irs::iql::boolean_function(fnFail, 3, true));
    sFnResult.clear();
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->BOOL_TRUE, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);

    ASSERT_EQ("['1','2']", sFnResult);
  }

  {
    std::string sData = "f_dyn(a==b)";
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f_dyn", irs::iql::boolean_function(fnTestTrueDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->FUNCTION, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(1, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "f_dyn(1)";
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f_dyn", irs::iql::boolean_function(fnFailDyn, 1));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->FUNCTION, pQuery->type);
    ASSERT_EQ(&fnFailDyn, &(ctx.function(*(pQuery->pFnBoolean))));
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(1, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("1", pNode->sValue);
  }

  {
    std::string sData = "f_st(f_dyn(b))";
    irs::iql::boolean_functions boolFns;
    irs::iql::sequence_functions seqFns;
    irs::iql::functions fns(boolFns, seqFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f_st", irs::iql::boolean_function(fnTestTrue, fnTestTrueDyn, 1));
    seqFns.emplace("f_dyn", irs::iql::sequence_function(fnArgFailDyn, 1));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->FUNCTION, pQuery->type);
    ASSERT_EQ(&fnTestTrueDyn, &(ctx.function(*(pQuery->pFnBoolean))));
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(1, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnArgFailDyn, &(ctx.function(*(pNode->pFnSequence))));
    ASSERT_EQ(1, pNode->children.size());
    pNode = &ctx.node(pNode->children[0]);
    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("b", pNode->sValue);
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  {
    std::string sData = "f()";
    irs::iql::functions fns;
    test_context ctx(sData, fns);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(3, state.nOffset);
    //ASSERT_EQ(3, state.pError->nStart); Bison GLR parser doesn't invoke yyerror(...) if all GLR states collapse to fail
  }

  {
    std::string sData = "f(1)";
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f", irs::iql::boolean_function(fnFail, 1)); // signature collision
    boolFns.emplace("f", irs::iql::boolean_function(fnFail, 1, true)); // signature collision
    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(4, state.nOffset);
    //ASSERT_EQ(4, state.pError->nStart); Bison GLR parser doesn't invoke yyerror(...) if all GLR states collapse to fail
  }

  {
    irs::iql::order_function::deterministic_function_t fnFailOrder = [](
      irs::iql::order_function::deterministic_buffer_t&,
      const irs::iql::order_function::deterministic_function_args_t&
    )->bool {
      std::cerr << "File: " << __FILE__ << " Line: " << __LINE__ << " Failed" << std::endl;
      throw "Fail";
    };
    std::string sData = "f(g())";
    irs::iql::boolean_functions boolFns;
    irs::iql::order_functions orderFns;
    irs::iql::functions fns(boolFns, orderFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f", irs::iql::boolean_function(fnFail, 1));
    orderFns.emplace("g", irs::iql::order_function(fnFailOrder)); // will not match function<std::string>
    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(6, state.nOffset);
    //ASSERT_EQ(2, state.pError->nStart); Bison GLR parser doesn't invoke yyerror(...) if all GLR states collapse to fail
  }

  {
    std::string sData = "f(,)";
    irs::iql::functions fns;
    test_context ctx(sData, fns);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(3, state.nOffset);
    ASSERT_EQ(2, state.pError->nStart);
  }
}

TEST_F(IqlParserTestSuite, test_seq_function) {
  irs::iql::sequence_function::contextual_function_t fnFailDyn = [](
    irs::iql::sequence_function::contextual_buffer_t&,
    const irs::string_ref&,
    void*,
    const irs::iql::sequence_function::contextual_function_args_t&
  )->bool {
    std::cerr << "File: " << __FILE__ << " Line: " << __LINE__ << " Failed" << std::endl;
    throw "Fail";
  };
  irs::iql::sequence_function::deterministic_function_t fnFail = [](
    irs::iql::sequence_function::deterministic_buffer_t&,
    const irs::iql::sequence_function::deterministic_function_args_t&
  )->bool {
    std::cerr << "File: " << __FILE__ << " Line: " << __LINE__ << " Failed" << std::endl;
    throw "Fail";
  };
  irs::iql::sequence_function::contextual_function_t fnTestDyn = [](
    irs::iql::sequence_function::contextual_buffer_t&,
    const irs::string_ref&,
    void*,
    const irs::iql::sequence_function::contextual_function_args_t&
  )->bool {
    return true;
  };
  irs::iql::sequence_function::deterministic_function_t fnTest = [](
    irs::iql::sequence_function::deterministic_buffer_t& buf,
    const irs::iql::sequence_function::deterministic_function_args_t& args
  )->bool {
    std::string sDelim = "";

    buf.append("[");

    for (auto& arg: args) {
      buf.append(sDelim).append("'").append(arg).append("'");
      sDelim = ",";
    }

    buf.append("]");

    return true;
  };

  {
    std::string sData = "fun()==a";
    irs::iql::sequence_functions seqFns;
    irs::iql::functions fns(seqFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    seqFns.emplace("fun", irs::iql::sequence_function(fnTest));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("[]", pNode->sValue);
  }

  {
    std::string sData = "'fun cti.on'()==a";
    irs::iql::sequence_functions seqFns;
    irs::iql::functions fns(seqFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    seqFns.emplace("fun cti.on", irs::iql::sequence_function(fnTest));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("[]", pNode->sValue);
  }

  {
    std::string sData = "f(1)==a";
    irs::iql::sequence_functions seqFns;
    irs::iql::functions fns(seqFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    seqFns.emplace("f", irs::iql::sequence_function(fnTest, 1));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("['1']", pNode->sValue);
  }

  {
    std::string sData = "f(a, b, c)==a";
    irs::iql::sequence_functions seqFns;
    irs::iql::functions fns(seqFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    seqFns.emplace("f", irs::iql::sequence_function(fnTest));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("['a','b','c']", pNode->sValue);
  }

  {
    std::string sData = "f(g(b), h(c))==a";
    irs::iql::sequence_functions seqFns;
    irs::iql::functions fns(seqFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    seqFns.emplace("f", irs::iql::sequence_function(fnTest));
    seqFns.emplace("g", irs::iql::sequence_function(fnTest));
    seqFns.emplace("h", irs::iql::sequence_function(fnTest));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("['['b']','['c']']", pNode->sValue);
  }

  {
    std::string sData = "f(1)==a";
    irs::iql::sequence_functions seqFns;
    irs::iql::functions fns(seqFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    seqFns.emplace("f", irs::iql::sequence_function(fnTest, 0, true)); // #arg < nFixedArg
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("['1']", pNode->sValue);
  }

  {
    std::string sData = "f(1)==a";
    irs::iql::sequence_functions seqFns;
    irs::iql::functions fns(seqFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    seqFns.emplace("f", irs::iql::sequence_function(fnTest, 1, true)); // #arg == nFixedArg
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("['1']", pNode->sValue);
  }

  {
    std::string sData = "f(1, 2)==a";
    irs::iql::sequence_functions seqFns;
    irs::iql::functions fns(seqFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    seqFns.emplace("f", irs::iql::sequence_function(fnFail, 0, true));
    seqFns.emplace("f", irs::iql::sequence_function(fnFail, 1, true));
    seqFns.emplace("f", irs::iql::sequence_function(fnTest, 2, true)); // best match
    seqFns.emplace("f", irs::iql::sequence_function(fnFail, 3, true));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("['1','2']", pNode->sValue);
  }

  {
    std::string sData = "f_dyn(1)==a";
    irs::iql::sequence_functions seqFns;
    irs::iql::functions fns(seqFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    seqFns.emplace("f_dyn", irs::iql::sequence_function(fnFailDyn, 1));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnFailDyn, &(ctx.function(*(pNode->pFnSequence))));
    ASSERT_EQ(1, pNode->children.size());
    pNode = &ctx.node(pNode->children[0]);
    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("1", pNode->sValue);
  }

  {
    std::string sData = "f_st(f_dyn(b))==a";
    irs::iql::sequence_functions seqFns;
    irs::iql::functions fns(seqFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    seqFns.emplace("f_dyn", irs::iql::sequence_function(fnFailDyn, 1));
    seqFns.emplace("f_st", irs::iql::sequence_function(fnTest, fnTestDyn, 1));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnTestDyn, &(ctx.function(*(pNode->pFnSequence))));
    ASSERT_EQ(1, pNode->children.size());
    pNode = &ctx.node(pNode->children[0]);
    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnFailDyn, &(ctx.function(*(pNode->pFnSequence))));
    ASSERT_EQ(1, pNode->children.size());
    pNode = &ctx.node(pNode->children[0]);
    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("b", pNode->sValue);
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  {
    std::string sData = "a==f()";
    irs::iql::functions fns;
    test_context ctx(sData, fns);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(6, state.nOffset);
    //ASSERT_EQ(6, state.pError->nStart); Bison GLR parser doesn't invoke yyerror(...) if all GLR states collapse to fail
  }

  {
    std::string sData = "a==f(1)";
    irs::iql::sequence_functions seqFns;
    irs::iql::functions fns(seqFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    seqFns.emplace("f", irs::iql::sequence_function(fnFail, 1)); // signature collision
    seqFns.emplace("f", irs::iql::sequence_function(fnFail, 1, true)); // signature collision
    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(7, state.nOffset);
    //ASSERT_EQ(7, state.pError->nStart); Bison GLR parser doesn't invoke yyerror(...) if all GLR states collapse to fail
  }

  {
    std::string sData = "a==f(,)";
    irs::iql::functions fns;
    test_context ctx(sData, fns);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(6, state.nOffset);
    ASSERT_EQ(5, state.pError->nStart);
  }
}

TEST_F(IqlParserTestSuite, test_term) {
  {
    std::string sData = "123==a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("123", pNode->sValue);
  }

  {
    std::string sData = "fun()==a";
    irs::iql::sequence_function::deterministic_function_t fnTest = [](
      irs::iql::sequence_function::deterministic_buffer_t& buf,
      const irs::iql::sequence_function::deterministic_function_args_t&
    )->bool {
      buf.append("abc");
      return true;
    };
    irs::iql::sequence_functions seqFns;
    irs::iql::functions fns(seqFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    seqFns.emplace("fun", irs::iql::sequence_function(fnTest));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("abc", pNode->sValue);
  }
}

TEST_F(IqlParserTestSuite, test_range) {
  {
    std::string sData = "a==[123,456]";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("123", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("456", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a==[123 , 456)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_FALSE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("123", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("456", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a==( 123 , 456 )";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_FALSE(pNode->bBeginInclusive);
    ASSERT_FALSE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("123", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("456", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a==(123,456]";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_FALSE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("123", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("456", ctx.node(pNode->children[1]).sValue);
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  {
    std::string sData = "a==[123]";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(8, state.nOffset);
    ASSERT_EQ(7, state.pError->nStart);
  }

  {
    std::string sData = "a==[,]";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(5, state.nOffset);
    ASSERT_EQ(4, state.pError->nStart);
  }

  {
    std::string sData = "a==[,b]";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(5, state.nOffset);
    ASSERT_EQ(4, state.pError->nStart);
  }

  {
    std::string sData = "a==[a,]";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(7, state.nOffset);
    ASSERT_EQ(6, state.pError->nStart);
  }

  {
    std::string sData = "a==[a,b,c]";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(8, state.nOffset);
    ASSERT_EQ(7, state.pError->nStart);
  }
}

TEST_F(IqlParserTestSuite, test_compare) {
  {
    std::string sData = "a~=bcd";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->LIKE, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());
    ASSERT_EQ(pQuery->SEQUENCE, ctx.node(pQuery->children[0]).type);
    ASSERT_EQ("a", ctx.node(pQuery->children[0]).sValue);
    ASSERT_EQ(pQuery->SEQUENCE, ctx.node(pQuery->children[1]).type);
    ASSERT_EQ("bcd", ctx.node(pQuery->children[1]).sValue);
  }

  {
    std::string sData = "a!=bcd";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_TRUE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("bcd", ctx.node(pNode->children[1]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("bcd", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a<bcd";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_FALSE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->UNKNOWN, ctx.node(pNode->children[0]).type);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("bcd", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a<=bcd";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->UNKNOWN, ctx.node(pNode->children[0]).type);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("bcd", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a==bcd";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("bcd", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("bcd", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a>=bcd";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("bcd", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->UNKNOWN, ctx.node(pNode->children[1]).type);
  }

  {
    std::string sData = "a>bcd";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_FALSE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("bcd", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->UNKNOWN, ctx.node(pNode->children[1]).type);
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  {
    std::string sData = "~=a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(2, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = "!=a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(2, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = "< a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(1, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = "<=a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(2, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = "==a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(2, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = ">=a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(2, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = ">a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(1, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = "[1,10]==5";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(1, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }
}

TEST_F(IqlParserTestSuite, test_subexpression) {
  {
    std::string sData = "(a==b)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "f_dyn()";
    irs::iql::boolean_function::contextual_function_t fnFailDyn = [](
      irs::iql::boolean_function::contextual_buffer_t&,
      const irs::string_ref&,
      void*,
      const irs::iql::boolean_function::contextual_function_args_t&
    )->bool {
      throw "Fail";
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f_dyn", irs::iql::boolean_function(fnFailDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->FUNCTION, pQuery->type);
    ASSERT_EQ(&fnFailDyn, &(ctx.function(*(pQuery->pFnBoolean))));
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_TRUE(pQuery->children.empty());
  }

  {
    std::string sData = "( a==b )";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a==b && (c==d)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->INTERSECTION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a==b || (c==d)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a==b && (c==d && e==f)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->INTERSECTION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(3, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[2]); // value (child order is not important)

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[0]); // value (child order is not important)
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value (child order is not important)
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("e", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("f", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("f", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a==b && (c==d || e==f)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->INTERSECTION, pNode->type);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    pNode = &ctx.node(pNode->children[1]); // value (child order is not important)
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pQuery->children[0]); // value
    pNode = &ctx.node(pNode->children[0]); // value (child order is not important)
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->INTERSECTION, pNode->type);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    pNode = &ctx.node(pNode->children[1]); // value (child order is not important)
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pQuery->children[1]); // value
    pNode = &ctx.node(pNode->children[0]); // value (child order is not important)
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("e", ctx.node(pNode->children[0]).sValue);
  }

  {
    std::string sData = "a==b || (c==d && e==f)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->INTERSECTION, pNode->type);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());

    pNode = &ctx.node(pNode->children[0]); // value (child order is not important)
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pQuery->children[1]); // value
    pNode = &ctx.node(pNode->children[1]); // value (child order is not important)
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("e", ctx.node(pNode->children[0]).sValue);
  }

  {
    std::string sData = "(a==b && c==d) && (e==f && g==h)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->INTERSECTION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(4, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);

    pNode = &ctx.node(pQuery->children[2]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("e", ctx.node(pNode->children[0]).sValue);

    pNode = &ctx.node(pQuery->children[3]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("g", ctx.node(pNode->children[0]).sValue);
  }

  {
    std::string sData = "(a==b || c==d) || (e==f || g==h)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(4, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);

    pNode = &ctx.node(pQuery->children[2]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("e", ctx.node(pNode->children[0]).sValue);

    pNode = &ctx.node(pQuery->children[3]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("g", ctx.node(pNode->children[0]).sValue);
  }

  {
    std::string sData = "(a==b || c==d) && (e==f || g==h)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(4, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value (child order is not important)

    ASSERT_EQ(pNode->INTERSECTION, pNode->type);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    pNode = &ctx.node(pNode->children[0]);
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pQuery->children[0]); // value
    pNode = &ctx.node(pNode->children[1]);
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("e", ctx.node(pNode->children[0]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value (child order is not important)
    ASSERT_EQ(pNode->INTERSECTION, pNode->type);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    pNode = &ctx.node(pNode->children[0]);
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pQuery->children[1]); // value
    pNode = &ctx.node(pNode->children[1]);
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("g", ctx.node(pNode->children[0]).sValue);

    pNode = &ctx.node(pQuery->children[2]); // value (child order is not important)
    ASSERT_EQ(pNode->INTERSECTION, pNode->type);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    pNode = &ctx.node(pNode->children[0]);
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pQuery->children[2]); // value
    pNode = &ctx.node(pNode->children[1]);
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("e", ctx.node(pNode->children[0]).sValue);

    pNode = &ctx.node(pQuery->children[3]); // value (child order is not important)
    ASSERT_EQ(pNode->INTERSECTION, pNode->type);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    pNode = &ctx.node(pNode->children[0]);
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pQuery->children[3]); // value
    pNode = &ctx.node(pNode->children[1]);
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("g", ctx.node(pNode->children[0]).sValue);
  }

  {
    std::string sData = "(a==b && c==d) || (e==f && g==h)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value (child order is not important)

    ASSERT_EQ(pNode->INTERSECTION, pNode->type);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    pNode = &ctx.node(pNode->children[0]);
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pQuery->children[0]); // value
    pNode = &ctx.node(pNode->children[1]);
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value (child order is not important)
    ASSERT_EQ(pNode->INTERSECTION, pNode->type);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    pNode = &ctx.node(pNode->children[0]);
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("e", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pQuery->children[1]); // value
    pNode = &ctx.node(pNode->children[1]);
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("g", ctx.node(pNode->children[0]).sValue);
  }
}

TEST_F(IqlParserTestSuite, test_negation) {
  {
    std::string sData = "!(a==b)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_TRUE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "! (a==b)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_TRUE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "NOT(a==b)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_TRUE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "NOT (a==b)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_TRUE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "!f_dyn()";
    irs::iql::boolean_function::contextual_function_t fnFailDyn = [](
      irs::iql::boolean_function::contextual_buffer_t&,
      const irs::string_ref&,
      void*,
      const irs::iql::boolean_function::contextual_function_args_t&
    )->bool {
      throw "Fail";
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f_dyn", irs::iql::boolean_function(fnFailDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->FUNCTION, pQuery->type);
    ASSERT_EQ(&fnFailDyn, &(ctx.function(*(pQuery->pFnBoolean))));
    ASSERT_TRUE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_TRUE(pQuery->children.empty());
  }

  {
    std::string sData = "! f_dyn()";
    irs::iql::boolean_function::contextual_function_t fnFailDyn = [](
      irs::iql::boolean_function::contextual_buffer_t&,
      const irs::string_ref&,
      void*,
      const irs::iql::boolean_function::contextual_function_args_t&
    )->bool {
      throw "Fail";
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f_dyn", irs::iql::boolean_function(fnFailDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->FUNCTION, pQuery->type);
    ASSERT_EQ(&fnFailDyn, &(ctx.function(*(pQuery->pFnBoolean))));
    ASSERT_TRUE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_TRUE(pQuery->children.empty());
  }

  {
    std::string sData = "NOT f_dyn()";
    irs::iql::boolean_function::contextual_function_t fnFailDyn = [](
      irs::iql::boolean_function::contextual_buffer_t&,
      const irs::string_ref&,
      void*,
      const irs::iql::boolean_function::contextual_function_args_t&
    )->bool {
      throw "Fail";
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f_dyn", irs::iql::boolean_function(fnFailDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->FUNCTION, pQuery->type);
    ASSERT_EQ(&fnFailDyn, &(ctx.function(*(pQuery->pFnBoolean))));
    ASSERT_TRUE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_TRUE(pQuery->children.empty());
  }

  {
    std::string sData = "!(a==b && c==d)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_TRUE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_TRUE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "!(a==b AND c!=d)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_TRUE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "!(a==b || c==d)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->INTERSECTION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_TRUE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_TRUE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "!(a==b OR c!=d)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->INTERSECTION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_TRUE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "!(a==b && (c==d || e==f))";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value (child order is not important)

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_TRUE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[0]); // value (child order is not important)
    ASSERT_EQ(pNode->INTERSECTION, pNode->type);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    pNode = &ctx.node(pNode->children[0]);
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_TRUE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pQuery->children[0]); // value (child order is not important)
    pNode = &ctx.node(pNode->children[1]);
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_TRUE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("e", ctx.node(pNode->children[0]).sValue);
  }

  {
    std::string sData = "!(a==b || (c==d && e==f))";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value (child order is not important)

    ASSERT_EQ(pNode->INTERSECTION, pNode->type);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    pNode = &ctx.node(pNode->children[1]); // (child order is not important)
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_TRUE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pQuery->children[0]); // value (child order is not important)
    pNode = &ctx.node(pNode->children[0]); // (child order is not important)
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_TRUE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value (child order is not important)
    ASSERT_EQ(pNode->INTERSECTION, pNode->type);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    pNode = &ctx.node(pNode->children[1]); // (child order is not important)
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_TRUE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pQuery->children[1]); // value (child order is not important)
    pNode = &ctx.node(pNode->children[0]); // (child order is not important)
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_TRUE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("e", ctx.node(pNode->children[0]).sValue);
  }
}

TEST_F(IqlParserTestSuite, test_boost) {
  {
    std::string sData = "42*(a==b)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(42., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "3.14 * (a==b)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(3.14f, pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "(a==b)*2.71828";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(2.71828f, pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "(a==b) * 123";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(123, pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "42*f_dyn()";
    irs::iql::boolean_function::contextual_function_t fnFailDyn = [](
      irs::iql::boolean_function::contextual_buffer_t&,
      const irs::string_ref&,
      void*,
      const irs::iql::boolean_function::contextual_function_args_t&
    )->bool {
      throw "Fail";
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f_dyn", irs::iql::boolean_function(fnFailDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->FUNCTION, pQuery->type);
    ASSERT_EQ(&fnFailDyn, &(ctx.function(*(pQuery->pFnBoolean))));
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(42., pQuery->fBoost);
    ASSERT_TRUE(pQuery->children.empty());
  }

  {
    std::string sData = "3.14 * f_dyn()";
    irs::iql::boolean_function::contextual_function_t fnFailDyn = [](
      irs::iql::boolean_function::contextual_buffer_t&,
      const irs::string_ref&,
      void*,
      const irs::iql::boolean_function::contextual_function_args_t&
    )->bool {
      throw "Fail";
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f_dyn", irs::iql::boolean_function(fnFailDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->FUNCTION, pQuery->type);
    ASSERT_EQ(&fnFailDyn, &(ctx.function(*(pQuery->pFnBoolean))));
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(3.14f, pQuery->fBoost);
    ASSERT_TRUE(pQuery->children.empty());
  }

  {
    std::string sData = "f_dyn()*2.71828";
    irs::iql::boolean_function::contextual_function_t fnFailDyn = [](
      irs::iql::boolean_function::contextual_buffer_t&,
      const irs::string_ref&,
      void*,
      const irs::iql::boolean_function::contextual_function_args_t&
    )->bool {
      throw "Fail";
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f_dyn", irs::iql::boolean_function(fnFailDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->FUNCTION, pQuery->type);
    ASSERT_EQ(&fnFailDyn, &(ctx.function(*(pQuery->pFnBoolean))));
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(2.71828f, pQuery->fBoost);
    ASSERT_TRUE(pQuery->children.empty());
  }

  {
    std::string sData = "f_dyn() * 123";
    irs::iql::boolean_function::contextual_function_t fnFailDyn = [](
      irs::iql::boolean_function::contextual_buffer_t&,
      const irs::string_ref&,
      void*,
      const irs::iql::boolean_function::contextual_function_args_t&
    )->bool {
      throw "Fail";
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f_dyn", irs::iql::boolean_function(fnFailDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->FUNCTION, pQuery->type);
    ASSERT_EQ(&fnFailDyn, &(ctx.function(*(pQuery->pFnBoolean))));
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(123., pQuery->fBoost);
    ASSERT_TRUE(pQuery->children.empty());
  }

  {
    std::string sData = "(a==b && c==d) * 123";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->INTERSECTION, pQuery->type);
    ASSERT_FLOAT_EQ(123, pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  {
    std::string sData = "*(a==b)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(1, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = "abc*(a==b)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(10, state.nOffset);
    //ASSERT_EQ(0, state.pError->nStart); Bison GLR parser doesn't invoke yyerror(...) if all GLR states collapse to fail
  }

  {
    std::string sData = "(a==b)*abc";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(10, state.nOffset);
    //ASSERT_EQ(0, state.pError->nStart); Bison GLR parser doesn't invoke yyerror(...) if all GLR states collapse to fail
  }

  {
    std::string sData = "42*a";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(4, state.nOffset);
    ASSERT_EQ(4, state.pError->nStart);
  }

  {
    std::string sData = "3.14*(a==b)*42";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(12, state.nOffset);
    ASSERT_EQ(11, state.pError->nStart);
  }
}

TEST_F(IqlParserTestSuite, test_expression) {
  {
    std::string sData = "1.2*(a==b)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1.2f, pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("a", pNode->sValue);
  }

  {
    std::string sData = "a==b";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "!(a==b)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_TRUE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "(a==b)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[1]); // value

    ASSERT_EQ(pNode->RANGE, pNode->type);
    ASSERT_TRUE(pNode->bBeginInclusive);
    ASSERT_TRUE(pNode->bEndInclusive);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }
}

TEST_F(IqlParserTestSuite, test_intersection) {
  {
    std::string sData = "a==b&&c==d";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->INTERSECTION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a==b AND c==d";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->INTERSECTION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a==b&&false()";
    irs::iql::boolean_function::deterministic_function_t fnFalse = [](
      irs::iql::boolean_function::deterministic_buffer_t& buf,
      const irs::iql::boolean_function::deterministic_function_args_t&
    )->bool {
      buf = false;
      return true;
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("false", irs::iql::boolean_function(fnFalse));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->BOOL_TRUE, pQuery->type);
    ASSERT_TRUE(pQuery->bNegated);
  }

  {
    std::string sData = "false()&&a==b";
    irs::iql::boolean_function::deterministic_function_t fnFalse = [](
      irs::iql::boolean_function::deterministic_buffer_t& buf,
      const irs::iql::boolean_function::deterministic_function_args_t&
    )->bool {
      buf = false;
      return true;
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("false", irs::iql::boolean_function(fnFalse));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->BOOL_TRUE, pQuery->type);
    ASSERT_TRUE(pQuery->bNegated);
  }

  {
    std::string sData = "a==b&&true()";
    irs::iql::boolean_function::deterministic_function_t fnTrue = [](
      irs::iql::boolean_function::deterministic_buffer_t& buf,
      const irs::iql::boolean_function::deterministic_function_args_t&
    )->bool {
      buf = true;
      return true;
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("true", irs::iql::boolean_function(fnTrue));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("a", pNode->sValue);
    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "true()&&a==b";
    irs::iql::boolean_function::deterministic_function_t fnTrue = [](
      irs::iql::boolean_function::deterministic_buffer_t& buf,
      const irs::iql::boolean_function::deterministic_function_args_t&
    )->bool {
      buf = true;
      return true;
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("true", irs::iql::boolean_function(fnTrue));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("a", pNode->sValue);
    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a==b&&f_dyn()";
    irs::iql::boolean_function::contextual_function_t fnFailDyn = [](
      irs::iql::boolean_function::contextual_buffer_t&,
      const irs::string_ref&,
      void*,
      const irs::iql::boolean_function::contextual_function_args_t&
    )->bool {
      throw "Fail";
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f_dyn", irs::iql::boolean_function(fnFailDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->INTERSECTION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnFailDyn, &(ctx.function(*(pNode->pFnBoolean))));
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_TRUE(pNode->children.empty());
  }

  {
    std::string sData = "f_dyn() AND c==d";
    irs::iql::boolean_function::contextual_function_t fnFailDyn = [](
      irs::iql::boolean_function::contextual_buffer_t&,
      const irs::string_ref&,
      void*,
      const irs::iql::boolean_function::contextual_function_args_t&
    )->bool {
      throw "Fail";
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f_dyn", irs::iql::boolean_function(fnFailDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->INTERSECTION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnFailDyn, &(ctx.function(*(pNode->pFnBoolean))));
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_TRUE(pNode->children.empty());

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a==b AND c==d&&e==f";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->INTERSECTION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(3, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[2]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("e", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("f", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("f", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "(a==b && c==d) AND e==f";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->INTERSECTION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(3, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[2]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("e", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("f", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("f", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "(a==b && c==d)&&(e==f AND g==h)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->INTERSECTION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(4, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[2]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("e", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("f", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("f", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[3]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("g", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("h", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("h", ctx.node(pNode->children[1]).sValue);
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  {
    std::string sData = "&&a==b";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(2, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = "a==b&&";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(6, state.nOffset);
    ASSERT_EQ(6, state.pError->nStart);
  }

  {
    std::string sData = "a==b&&c";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(7, state.nOffset);
    ASSERT_EQ(7, state.pError->nStart);
  }

  {
    std::string sData = "AND (a==b)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(3, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = "(a==b) AND";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(10, state.nOffset);
    ASSERT_EQ(10, state.pError->nStart);
  }
}

TEST_F(IqlParserTestSuite, test_union) {
  {
    std::string sData = "a==b||c==d";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);
  }

    {
    std::string sData = "a==b OR c==d";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a==b||false()";
    irs::iql::boolean_function::deterministic_function_t fnFalse = [](
      irs::iql::boolean_function::deterministic_buffer_t& buf,
      const irs::iql::boolean_function::deterministic_function_args_t&
    )->bool {
      buf = false;
      return true;
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("false", irs::iql::boolean_function(fnFalse));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("a", pNode->sValue);
    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "false()||a==b";
    irs::iql::boolean_function::deterministic_function_t fnFalse = [](
      irs::iql::boolean_function::deterministic_buffer_t& buf,
      const irs::iql::boolean_function::deterministic_function_args_t&
    )->bool {
      buf = false;
      return true;
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("false", irs::iql::boolean_function(fnFalse));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("a", pNode->sValue);
    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a==b||true()";
    irs::iql::boolean_function::deterministic_function_t fnTrue = [](
      irs::iql::boolean_function::deterministic_buffer_t& buf,
      const irs::iql::boolean_function::deterministic_function_args_t&
    )->bool {
      buf = true;
      return true;
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("true", irs::iql::boolean_function(fnTrue));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->BOOL_TRUE, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
  }

  {
    std::string sData = "true()||a==b";
    irs::iql::boolean_function::deterministic_function_t fnTrue = [](
      irs::iql::boolean_function::deterministic_buffer_t& buf,
      const irs::iql::boolean_function::deterministic_function_args_t&
    )->bool {
      buf = true;
      return true;
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("true", irs::iql::boolean_function(fnTrue));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->BOOL_TRUE, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
  }

  {
    std::string sData = "a==b||f_dyn()";
    irs::iql::boolean_function::contextual_function_t fnFailDyn = [](
      irs::iql::boolean_function::contextual_buffer_t&,
      const irs::string_ref&,
      void*,
      const irs::iql::boolean_function::contextual_function_args_t&
    )->bool {
      throw "Fail";
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f_dyn", irs::iql::boolean_function(fnFailDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnFailDyn, &(ctx.function(*(pNode->pFnBoolean))));
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_TRUE(pNode->children.empty());
  }

  {
    std::string sData = "f_dyn() OR c==d";
    irs::iql::boolean_function::contextual_function_t fnFailDyn = [](
      irs::iql::boolean_function::contextual_buffer_t&,
      const irs::string_ref&,
      void*,
      const irs::iql::boolean_function::contextual_function_args_t&
    )->bool {
      throw "Fail";
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f_dyn", irs::iql::boolean_function(fnFailDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnFailDyn, &(ctx.function(*(pNode->pFnBoolean))));
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_TRUE(pNode->children.empty());

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "a==b OR c==d||e==f";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(3, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[2]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("e", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("f", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("f", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "(a==b || c==d) OR e==f";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(3, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[2]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("e", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("f", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("f", ctx.node(pNode->children[1]).sValue);
  }

  {
    std::string sData = "(a==b || c==d)||(e==f OR g==h)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->UNION, pQuery->type);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(4, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("a", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("b", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[1]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("c", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("d", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[2]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("e", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("f", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("f", ctx.node(pNode->children[1]).sValue);

    pNode = &ctx.node(pQuery->children[3]); // value
    ASSERT_EQ(pNode->EQUAL, pNode->type);
    ASSERT_FALSE(pNode->bNegated);
    ASSERT_FLOAT_EQ(1., pNode->fBoost);
    ASSERT_EQ(2, pNode->children.size());
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("g", ctx.node(pNode->children[0]).sValue);
    pNode = &ctx.node(pNode->children[1]); // value
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[0]).type);
    ASSERT_EQ("h", ctx.node(pNode->children[0]).sValue);
    ASSERT_EQ(pNode->SEQUENCE, ctx.node(pNode->children[1]).type);
    ASSERT_EQ("h", ctx.node(pNode->children[1]).sValue);
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  {
    std::string sData = "||a==b";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(2, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = "a==b||";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(6, state.nOffset);
    ASSERT_EQ(6, state.pError->nStart);
  }

  {
    std::string sData = "a==b||c";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(7, state.nOffset);
    ASSERT_EQ(7, state.pError->nStart);
  }

  {
    std::string sData = "OR (a==b)";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(2, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = "(a==b) OR";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(9, state.nOffset);
    ASSERT_EQ(9, state.pError->nStart);
  }
}

TEST_F(IqlParserTestSuite, test_input) {
  irs::iql::order_function::contextual_function_t fnTestDyn = [](
    irs::iql::order_function::contextual_buffer_t&,
    const irs::string_ref&,
    void*,
    const bool&,
    const irs::iql::order_function::contextual_function_args_t&
  )->bool {
    return true;
  };

  {
    std::string sData = "a==b";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("a", pNode->sValue);

    ASSERT_TRUE(state.order.empty());
    ASSERT_EQ(nullptr, state.pnLimit);
  }

  {
    std::string sData = "f_dyn()";
    irs::iql::boolean_function::contextual_function_t fnFailDyn = [](
      irs::iql::boolean_function::contextual_buffer_t&,
      const irs::string_ref&,
      void*,
      const irs::iql::boolean_function::contextual_function_args_t&
    )->bool {
      throw "Fail";
    };
    irs::iql::boolean_functions boolFns;
    irs::iql::functions fns(boolFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    boolFns.emplace("f_dyn", irs::iql::boolean_function(fnFailDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->FUNCTION, pQuery->type);
    ASSERT_EQ(&fnFailDyn, &(ctx.function(*(pQuery->pFnBoolean))));
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_TRUE(pQuery->children.empty());
  }

  {
    std::string sData = "a==b order c()";
    irs::iql::order_functions orderFns;
    irs::iql::functions fns(orderFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    orderFns.emplace("c", irs::iql::order_function(fnTestDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("a", pNode->sValue);

    ASSERT_EQ(1, state.order.size());
    pNode = &ctx.node(state.order[0].first);
    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnTestDyn, &(ctx.function(*(pNode->pFnOrder))));

    ASSERT_EQ(nullptr, state.pnLimit);
  }

  {
    std::string sData = "a==b limit 42";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("a", pNode->sValue);

    ASSERT_TRUE(state.order.empty());
    ASSERT_NE(nullptr, state.pnLimit);
    ASSERT_EQ(42, *(state.pnLimit));
  }

  {
    std::string sData = "a==b order c() limit 42";
    irs::iql::order_functions orderFns;
    irs::iql::functions fns(orderFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    orderFns.emplace("c", irs::iql::order_function(fnTestDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnFilter);

    auto* pQuery = &ctx.node(*(state.pnFilter));

    ASSERT_EQ(pQuery->EQUAL, pQuery->type);
    ASSERT_FALSE(pQuery->bNegated);
    ASSERT_FLOAT_EQ(1., pQuery->fBoost);
    ASSERT_EQ(2, pQuery->children.size());

    auto* pNode = &ctx.node(pQuery->children[0]); // value

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("a", pNode->sValue);

    ASSERT_EQ(1, state.order.size());
    pNode = &ctx.node(state.order[0].first);
    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnTestDyn, &(ctx.function(*(pNode->pFnOrder))));

    ASSERT_NE(nullptr, state.pnLimit);
    ASSERT_EQ(42, *(state.pnLimit));
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  {
    std::string sData = "";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(0, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = "order c()";
    irs::iql::order_functions orderFns;
    irs::iql::functions fns(orderFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    orderFns.emplace("c", irs::iql::order_function(fnTestDyn));
    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(5, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = "order c() order c()";
    irs::iql::order_functions orderFns;
    irs::iql::functions fns(orderFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    orderFns.emplace("c", irs::iql::order_function(fnTestDyn));
    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(5, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = "limit 42";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(5, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = "a==b limit 42 limit 43";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(19, state.nOffset);
    ASSERT_EQ(14, state.pError->nStart);
  }

  {
    std::string sData = "order c() limit 42";
    irs::iql::order_functions orderFns;
    irs::iql::functions fns(orderFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    orderFns.emplace("c", irs::iql::order_function(fnTestDyn));
    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(5, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }
}

TEST_F(IqlParserTestSuite, test_limit) {
  {
    std::string sData = "a==b limit 42";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnLimit);
    ASSERT_EQ(42, *(state.pnLimit));
  }

  {
    std::string sData = "a==b limit c()";
    irs::iql::sequence_function::deterministic_function_t fnTest = [](
      irs::iql::sequence_function::deterministic_buffer_t& buf,
      const irs::iql::sequence_function::deterministic_function_args_t&
    )->bool {
      buf.append("123");
      return true;
    };
    irs::iql::sequence_functions seqFns;
    irs::iql::functions fns(seqFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    seqFns.emplace("c", irs::iql::sequence_function(fnTest));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_NE(nullptr, state.pnLimit);
    ASSERT_EQ(123, *(state.pnLimit));
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  {
    std::string sData = "a==b limit -42";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(14, state.nOffset);
    //ASSERT_EQ(11, state.pError->nStart); Bison GLR parser doesn't invoke yyerror(...) if all GLR states collapse to fail
  }

  {
    std::string sData = "a==b limit 42.1";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(15, state.nOffset);
    //ASSERT_EQ(11, state.pError->nStart); Bison GLR parser doesn't invoke yyerror(...) if all GLR states collapse to fail
  }

  {
    std::string sData = "a==b limit c";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(12, state.nOffset);
    //ASSERT_EQ(0, state.pError->nStart); Bison GLR parser doesn't invoke yyerror(...) if all GLR states collapse to fail
  }
}

TEST_F(IqlParserTestSuite, test_order) {
  irs::iql::order_function::contextual_function_t fnTestDyn = [](
    irs::iql::order_function::contextual_buffer_t&,
    const irs::string_ref&,
    void*,
    const bool&,
    const irs::iql::order_function::contextual_function_args_t&
  )->bool {
    return true;
  };
  irs::iql::order_function::deterministic_function_t fnTest = [](
    irs::iql::order_function::deterministic_buffer_t& buf,
    const irs::iql::order_function::deterministic_function_args_t&
  )->bool {
    buf.append("abcdef");
    return true;
  };

  {
    std::string sData = "a==b order c";
    test_context ctx(sData);
    parser parser(ctx);

    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(1, state.order.size());

    auto* pNode = &ctx.node(state.order[0].first);

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("c", pNode->sValue);
    ASSERT_TRUE(state.order[0].second);
  }

  {
    std::string sData = "a==b order c()";
    irs::iql::order_functions orderFns;
    irs::iql::functions fns(orderFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    orderFns.emplace("c", irs::iql::order_function(fnTest));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(1, state.order.size());

    auto* pNode = &ctx.node(state.order[0].first);

    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("abcdef", pNode->sValue);
    ASSERT_TRUE(state.order[0].second);
  }

  {
    std::string sData = "a==b order c()";
    irs::iql::order_functions orderFns;
    irs::iql::functions fns(orderFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    orderFns.emplace("c", irs::iql::order_function(fnTestDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(1, state.order.size());

    auto* pNode = &ctx.node(state.order[0].first);

    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnTestDyn, &(ctx.function(*(pNode->pFnOrder))));
    ASSERT_TRUE(state.order[0].second);
  }

  {
    std::string sData = "a==b order c() asc";
    irs::iql::order_functions orderFns;
    irs::iql::functions fns(orderFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    orderFns.emplace("c", irs::iql::order_function(fnTestDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(1, state.order.size());

    auto* pNode = &ctx.node(state.order[0].first);

    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnTestDyn, &(ctx.function(*(pNode->pFnOrder))));
    ASSERT_TRUE(state.order[0].second);
  }

  {
    std::string sData = "a==b order c() desc";
    irs::iql::order_functions orderFns;
    irs::iql::functions fns(orderFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    orderFns.emplace("c", irs::iql::order_function(fnTestDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(1, state.order.size());

    auto* pNode = &ctx.node(state.order[0].first);

    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnTestDyn, &(ctx.function(*(pNode->pFnOrder))));
    ASSERT_FALSE(state.order[0].second);
  }

  {
    std::string sData = "a==b order c(), d(), e()";
    irs::iql::order_functions orderFns;
    irs::iql::functions fns(orderFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    orderFns.emplace("c", irs::iql::order_function(fnTestDyn));
    orderFns.emplace("d", irs::iql::order_function(fnTestDyn));
    orderFns.emplace("e", irs::iql::order_function(fnTestDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(3, state.order.size());

    auto* pNode = &ctx.node(state.order[0].first);

    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnTestDyn, &(ctx.function(*(pNode->pFnOrder))));
    ASSERT_TRUE(state.order[0].second);
    pNode = &ctx.node(state.order[1].first);
    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnTestDyn, &(ctx.function(*(pNode->pFnOrder))));
    ASSERT_TRUE(state.order[1].second);
    pNode = &ctx.node(state.order[2].first);
    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnTestDyn, &(ctx.function(*(pNode->pFnOrder))));
    ASSERT_TRUE(state.order[2].second);
  }

  {
    std::string sData = "a==b order c(), d(), e";
    irs::iql::order_functions orderFns;
    irs::iql::functions fns(orderFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    orderFns.emplace("c", irs::iql::order_function(fnTestDyn));
    orderFns.emplace("d", irs::iql::order_function(fnTestDyn));
    ASSERT_EQ(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(3, state.order.size());

    auto* pNode = &ctx.node(state.order[0].first);

    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnTestDyn, &(ctx.function(*(pNode->pFnOrder))));
    ASSERT_TRUE(state.order[0].second);
    pNode = &ctx.node(state.order[1].first);
    ASSERT_EQ(pNode->FUNCTION, pNode->type);
    ASSERT_EQ(&fnTestDyn, &(ctx.function(*(pNode->pFnOrder))));
    ASSERT_TRUE(state.order[1].second);
    pNode = &ctx.node(state.order[2].first);
    ASSERT_EQ(pNode->SEQUENCE, pNode->type);
    ASSERT_EQ("e", pNode->sValue);
    ASSERT_TRUE(state.order[2].second);
  }

  // ...........................................................................
  // invalid
  // ...........................................................................

  {
    std::string sData = "order c() abc";
    irs::iql::order_functions orderFns;
    irs::iql::functions fns(orderFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    orderFns.emplace("c", irs::iql::order_function(fnTestDyn));
    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(5, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }

  {
    std::string sData = "order c(),";
    irs::iql::order_functions orderFns;
    irs::iql::functions fns(orderFns);
    test_context ctx(sData, fns);
    parser parser(ctx);

    orderFns.emplace("c", irs::iql::order_function(fnTestDyn));
    ASSERT_NE(0, parser.parse());

    auto state = ctx.state();

    ASSERT_EQ(5, state.nOffset);
    ASSERT_EQ(0, state.pError->nStart);
  }
}
