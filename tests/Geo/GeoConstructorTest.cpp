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
/// @author Heiko Kernbach
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "Aql/AstNode.h"
#include "Aql/AqlValue.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Aql/Query.h"
#include "Containers/SmallVector.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::containers;

namespace arangodb {
namespace tests {
namespace geo_constructors_aql {

class GeoConstructorTest : public ::testing::Test {
 protected:
  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext;

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx;
  fakeit::Mock<transaction::Context> contextMock;
  transaction::Context& context;

  SmallVector<AqlValue>::allocator_type::arena_type arena;
  SmallVector<AqlValue> params;

  GeoConstructorTest()
      : expressionContext(expressionContextMock.get()),
        trx(trxMock.get()),
        context(contextMock.get()),
        params{arena} {
    static VPackOptions options = velocypack::Options::Defaults;
    fakeit::When(Method(trxMock, transactionContextPtr)).AlwaysReturn(&context);
    fakeit::When(Method(trxMock, vpackOptions)).AlwaysReturn(options);
    fakeit::When(Method(contextMock, getVPackOptions)).AlwaysReturn(&options);
    fakeit::When(Method(contextMock, leaseBuilder)).AlwaysDo([]() { return new arangodb::velocypack::Builder(); });
    fakeit::When(Method(contextMock, returnBuilder)).AlwaysDo([](arangodb::velocypack::Builder* b) { delete b; });
    fakeit::When(Method(expressionContextMock, trx)).AlwaysDo([&]() -> transaction::Methods& { return this->trx; });
  }
};

namespace geo_point {
class GeoPointTest : public GeoConstructorTest {
 protected:
  GeoPointTest() 
      : GeoConstructorTest(),
        fun("GEO_POINT", &Functions::GeoPoint),
        funNode(NODE_TYPE_FCALL) {

    funNode.setData(static_cast<void const*>(&fun));

    fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
      ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    });
  }
    
  arangodb::aql::Function fun;
  arangodb::aql::AstNode funNode;
};

TEST_F(GeoPointTest, checking_two_positive_integer_values) {
  VPackBuilder foo;
  foo.openArray();
  foo.add(VPackValue(1));
  foo.add(VPackValue(2));
  foo.close();
  params.emplace_back(foo.slice().at(0));
  params.emplace_back(foo.slice().at(1));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_EQ(res.slice().get("coordinates").length(), 2);
  EXPECT_EQ(res.slice().get("coordinates").at(0).getDouble(), 1);
  EXPECT_EQ(res.slice().get("coordinates").at(1).getDouble(), 2);
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "Point");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_two_negative_integer_values) {
  VPackBuilder foo;
  foo.openArray();
  foo.add(VPackValue(-1));
  foo.add(VPackValue(-2));
  foo.close();
  params.emplace_back(foo.slice().at(0));
  params.emplace_back(foo.slice().at(1));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_EQ(res.slice().get("coordinates").length(), 2);
  EXPECT_EQ(res.slice().get("coordinates").at(0).getDouble(), -1);
  EXPECT_EQ(res.slice().get("coordinates").at(1).getDouble(), -2);
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "Point");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_two_positive_double_values) {
  VPackBuilder foo;
  foo.openArray();
  foo.add(VPackValue(1.1));
  foo.add(VPackValue(2.2));
  foo.close();
  params.emplace_back(foo.slice().at(0));
  params.emplace_back(foo.slice().at(1));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_EQ(res.slice().get("coordinates").length(), 2);
  EXPECT_EQ(res.slice().get("coordinates").at(0).getDouble(), 1.1);
  EXPECT_EQ(res.slice().get("coordinates").at(1).getDouble(), 2.2);
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "Point");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_two_negative_double_values) {
  VPackBuilder foo;
  foo.openArray();
  foo.add(VPackValue(-1.1));
  foo.add(VPackValue(-2.2));
  foo.close();
  params.emplace_back(foo.slice().at(0));
  params.emplace_back(foo.slice().at(1));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_EQ(res.slice().get("coordinates").length(), 2);
  EXPECT_EQ(res.slice().get("coordinates").at(0).getDouble(), -1.1);
  EXPECT_EQ(res.slice().get("coordinates").at(1).getDouble(), -2.2);
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "Point");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_two_postive_integer_and_positive_double_values) {
  VPackBuilder foo;
  foo.openArray();
  foo.add(VPackValue(1));
  foo.add(VPackValue(2.2));
  foo.close();
  params.emplace_back(foo.slice().at(0));
  params.emplace_back(foo.slice().at(1));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_EQ(res.slice().get("coordinates").length(), 2);
  EXPECT_EQ(res.slice().get("coordinates").at(0).getDouble(), 1);
  EXPECT_EQ(res.slice().get("coordinates").at(1).getDouble(), 2.2);
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "Point");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_two_negative_integer_and_positive_double_values) {
  VPackBuilder foo;
  foo.openArray();
  foo.add(VPackValue(-1));
  foo.add(VPackValue(2.2));
  foo.close();
  params.emplace_back(foo.slice().at(0));
  params.emplace_back(foo.slice().at(1));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_EQ(res.slice().get("coordinates").length(), 2);
  EXPECT_EQ(res.slice().get("coordinates").at(0).getDouble(), -1);
  EXPECT_EQ(res.slice().get("coordinates").at(1).getDouble(), 2.2);
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "Point");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_two_positive_integer_and_negative_double_values) {
  VPackBuilder foo;
  foo.openArray();
  foo.add(VPackValue(1));
  foo.add(VPackValue(-2.2));
  foo.close();
  params.emplace_back(foo.slice().at(0));
  params.emplace_back(foo.slice().at(1));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_EQ(res.slice().get("coordinates").length(), 2);
  EXPECT_EQ(res.slice().get("coordinates").at(0).getDouble(), 1);
  EXPECT_EQ(res.slice().get("coordinates").at(1).getDouble(), -2.2);
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "Point");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_bool_and_positive_double) {
  VPackBuilder foo;
  foo(VPackValue(VPackValueType::Object))("boolean", VPackValue(true))(
      "coords", VPackValue(VPackValueType::Array))(VPackValue(2.2))()();

  params.emplace_back(foo.slice().get("boolean"));
  params.emplace_back(foo.slice().get("coords").at(0));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_null) {
  char const* p = "null";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);
  params.emplace_back(json);

  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_string) {
  char const* p = "\"hallowelt\"";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);
  params.emplace_back(json);

  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_positive_int_and_bool) {
  VPackBuilder foo;
  foo(VPackValue(VPackValueType::Object))("boolean", VPackValue(true))(
      "coords", VPackValue(VPackValueType::Array))(VPackValue(2))()();

  params.emplace_back(foo.slice().get("coords").at(0));
  params.emplace_back(foo.slice().get("boolean"));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_bool_and_negative_double) {
  VPackBuilder foo;
  foo(VPackValue(VPackValueType::Object))("boolean", VPackValue(false))(
      "coords", VPackValue(VPackValueType::Array))(VPackValue(-2.2))()();

  params.emplace_back(foo.slice().get("boolean"));
  params.emplace_back(foo.slice().get("coords").at(0));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_array_and_positive_double) {
  VPackBuilder foo;
  foo(VPackValue(VPackValueType::Object))("array", VPackValue(VPackValueType::Array))(
      VPackValue(1.0))(VPackValue(2))(VPackValue(
      -3.3))()("coords", VPackValue(VPackValueType::Array))(VPackValue(2.2))()();

  params.emplace_back(foo.slice().get("array"));
  params.emplace_back(foo.slice().get("coords").at(0));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_negative_double_and_array) {
  VPackBuilder foo;
  foo(VPackValue(VPackValueType::Object))("array", VPackValue(VPackValueType::Array))(
      VPackValue(1.0))(VPackValue(2))(VPackValue(
      -3.3))()("coords", VPackValue(VPackValueType::Array))(VPackValue(-2.2))()();

  params.emplace_back(foo.slice().get("coords").at(0));
  params.emplace_back(foo.slice().get("array"));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_object_and_positive_double) {
  VPackBuilder b;
  b.openObject();
  b.add("object", VPackValue(VPackValueType::Object));
  b.add("a", VPackValue(123));
  b.add("b", VPackValue(true));
  b.close();
  b.add("coords", VPackValue(VPackValueType::Array));
  b.add(VPackValue(1.0));
  b.close();
  b.close();

  params.emplace_back(b.slice().get("object"));
  params.emplace_back(b.slice().get("coords").at(0));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_object_and_negative_double) {
  VPackBuilder b;
  b.openObject();
  b.add("object", VPackValue(VPackValueType::Object));
  b.add("a", VPackValue(123));
  b.add("b", VPackValue(true));
  b.close();
  b.add("coords", VPackValue(VPackValueType::Array));
  b.add(VPackValue(-2.2));
  b.close();
  b.close();

  params.emplace_back(b.slice().get("coords").at(0));
  params.emplace_back(b.slice().get("object"));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_object_and_array) {
  VPackBuilder b;
  b.openObject();
  b.add("object", VPackValue(VPackValueType::Object));
  b.add("a", VPackValue(123));
  b.add("b", VPackValue(true));
  b.close();
  b.add("coords", VPackValue(VPackValueType::Array));
  b.add(VPackValue(-2.2));
  b.add(VPackValue(3.2));
  b.add(VPackValue(-4.2));
  b.close();
  b.close();

  params.emplace_back(b.slice().get("object"));
  params.emplace_back(b.slice().get("coords"));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_array_and_object) {
  VPackBuilder b;
  b.openObject();
  b.add("object", VPackValue(VPackValueType::Object));
  b.add("a", VPackValue(123));
  b.add("b", VPackValue(true));
  b.close();
  b.add("coords", VPackValue(VPackValueType::Array));
  b.add(VPackValue(-2.2));
  b.add(VPackValue(3.2));
  b.add(VPackValue(-4.2));
  b.close();
  b.close();

  params.emplace_back(b.slice().get("coords"));
  params.emplace_back(b.slice().get("object"));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_bool_and_bool) {
  VPackBuilder foo;
  foo(VPackValue(VPackValueType::Object))("boolone",
                                          VPackValue(true))("booltwo",
                                                            VPackValue(false))();

  params.emplace_back(foo.slice().get("boolone"));
  params.emplace_back(foo.slice().get("booltwo"));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_array_and_array) {
  VPackBuilder b;
  b.openObject();
  b.add("arrone", VPackValue(VPackValueType::Array));
  b.add(VPackValue(-2.2));
  b.add(VPackValue(3.2));
  b.add(VPackValue(-4.2));
  b.close();
  b.add("arrtwo", VPackValue(VPackValueType::Array));
  b.add(VPackValue(-2.2));
  b.add(VPackValue(3.2));
  b.add(VPackValue(-4.2));
  b.close();
  b.close();

  params.emplace_back(b.slice().get("arrone"));
  params.emplace_back(b.slice().get("arrtwo"));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPointTest, checking_object_and_object) {
  VPackBuilder b;
  b.openObject();
  b.add("object", VPackValue(VPackValueType::Object));
  b.add("a", VPackValue(123));
  b.add("b", VPackValue(true));
  b.close();
  b.add("object", VPackValue(VPackValueType::Object));
  b.add("a", VPackValue(123));
  b.add("b", VPackValue(true));
  b.close();
  b.close();

  params.emplace_back(b.slice().get("coords"));
  params.emplace_back(b.slice().get("object"));
  AqlValue res = Functions::GeoPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}
}  // namespace geo_point

namespace geo_multipoint {
struct GeoMultipointTest : public GeoConstructorTest {
  GeoMultipointTest() 
      : GeoConstructorTest(),
        fun("GEO_MULTIPOINT", &Functions::GeoMultiPoint),
        funNode(NODE_TYPE_FCALL) {

    funNode.setData(static_cast<void const*>(&fun));
  }

  arangodb::aql::Function fun;
  arangodb::aql::AstNode funNode;
};

TEST_F(GeoMultipointTest, checking_multipoint_with_2_positions) {
  char const* p = "[[1.0, 2.0], [3.0, 4.0]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_EQ(res.slice().get("coordinates").length(), 2);
  EXPECT_TRUE(res.slice().get("coordinates").at(0).isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(1).isArray());
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(0).getDouble(), 1.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(1).getDouble(), 2.0);
  EXPECT_EQ(res.slice().get("coordinates").at(1).at(0).getDouble(), 3.0);
  EXPECT_EQ(res.slice().get("coordinates").at(1).at(1).getDouble(), 4.0);
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "MultiPoint");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoMultipointTest, checking_points_representing_points_in_cologne) {
  char const* p =
      "[[6.888427734375,50.91602169392645],[6.9632720947265625,50."
      "87921050161489],[7.013397216796875,50.89480467658874],[7."
      "0731353759765625,50.92424609910128],[7.093048095703125,50."
      "94804539355076],[7.03948974609375,50.9709677364145],[6.985244750976562,"
      "51.000360974529464],[6.8891143798828125,50.996471761616284],[6."
      "867828369140624,50.95669666276118],[6.888427734375,50.91602169392645]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_EQ(res.slice().get("coordinates").length(), 10);
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "MultiPoint");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}
TEST_F(GeoMultipointTest, checking_array_with_1_position) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[[1.0, 2.0]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoMultipointTest, checking_array_with_positions_and_invalid_bool) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[[1.0, 2.0], [1.0, 2.0], [1.0, 2.0], [1.0, 2.0], false]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoMultipointTest, checking_array_with_positions_and_invalid_bool_2) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[true, [1.0, 2.0], [1.0, 2.0], [1.0, 2.0], [1.0, 2.0]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoMultipointTest, checking_array_with_0_positions_nested) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[[]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoMultipointTest, checking_array_with_0_positions) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoMultipointTest, checking_bool) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_ARRAY_EXPECTED);
  });

  char const* p = "true";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoMultipointTest, checking_number) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_ARRAY_EXPECTED);
  });

  char const* p = "123";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoMultipointTest, checking_object) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_ARRAY_EXPECTED);
  });

  char const* p = "{\"Hello\": true, \"Hellox\": 123}";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiPoint(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}
}  // namespace geo_multipoint

namespace geo_polygon {
class GeoPolygonTest : public GeoConstructorTest {
 protected:
  GeoPolygonTest() 
      : GeoConstructorTest(),
        fun("GEO_POLYGON", &Functions::GeoPolygon),
        funNode(NODE_TYPE_FCALL) {
    funNode.setData(static_cast<void const*>(&fun));
  }

  arangodb::aql::Function fun;
  arangodb::aql::AstNode funNode;
};

TEST_F(GeoPolygonTest, checking_polygon_with_3_positive_tuples) {
  char const* p = "[[1.0, 2.0], [3.0, 4.0], [5.0, 6.0]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_EQ(res.slice().get("coordinates").at(0).length(), 3);
  EXPECT_TRUE(res.slice().get("coordinates").at(0).at(0).isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(0).at(1).isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(0).at(2).isArray());
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(0).at(0).getDouble(), 1.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(0).at(1).getDouble(), 2.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(1).at(0).getDouble(), 3.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(1).at(1).getDouble(), 4.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(2).at(0).getDouble(), 5.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(2).at(1).getDouble(), 6.0);
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "Polygon");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPolygonTest, checking_polygon_representing_cologne) {
  char const* p =
      "[[6.888427734375,50.91602169392645],[6.9632720947265625,50."
      "87921050161489],[7.013397216796875,50.89480467658874],[7."
      "0731353759765625,50.92424609910128],[7.093048095703125,50."
      "94804539355076],[7.03948974609375,50.9709677364145],[6.985244750976562,"
      "51.000360974529464],[6.8891143798828125,50.996471761616284],[6."
      "867828369140624,50.95669666276118],[6.888427734375,50.91602169392645]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(0).isArray());
  EXPECT_EQ(res.slice().get("coordinates").at(0).length(), 10);
  EXPECT_TRUE(res.slice().get("coordinates").at(0).at(0).isArray());
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "Polygon");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPolygonTest, checking_polygon_with_3_negative_positions) {
  char const* p = "[[-1.0, -2.0], [-3.0, -4.0], [-5.0, -6.0]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(0).isArray());
  EXPECT_EQ(res.slice().get("coordinates").at(0).length(), 3);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(0).length(), 2);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(1).length(), 2);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(2).length(), 2);
  EXPECT_TRUE(res.slice().get("coordinates").at(0).at(0).isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(0).at(1).isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(0).at(2).isArray());
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(0).at(0).getDouble(), -1.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(0).at(1).getDouble(), -2.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(1).at(0).getDouble(), -3.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(1).at(1).getDouble(), -4.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(2).at(0).getDouble(), -5.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(2).at(1).getDouble(), -6.0);
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "Polygon");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPolygonTest, checking_polygons_with_2x3_negative_positions) {
  char const* p =
      "[ [[-1.0, -2.0], [-3.0, -4.0], [-5.0, -6.0]], [[-1.0, -2.0], [-3.0, "
      "-4.0], [-5.0, -6.0]] ]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  // TODO check also at 1 position
  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(0).isArray());
  EXPECT_EQ(res.slice().get("coordinates").at(0).length(), 3);
  EXPECT_TRUE(res.slice().get("coordinates").at(1).isArray());
  EXPECT_EQ(res.slice().get("coordinates").at(1).length(), 3);
  EXPECT_TRUE(res.slice().get("coordinates").at(0).at(0).isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(0).at(1).isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(0).at(2).isArray());
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(0).at(0).getDouble(), -1.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(0).at(1).getDouble(), -2.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(1).at(0).getDouble(), -3.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(1).at(1).getDouble(), -4.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(2).at(0).getDouble(), -5.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(2).at(1).getDouble(), -6.0);
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "Polygon");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPolygonTest, checking_polygon_with_1_positive_position) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[[1.0, 2.0]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPolygonTest, checking_polygon_with_1_negative_position) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[[-1.0, -2.0]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPolygonTest, checking_polygon_with_2_positive_tuples) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[[1.0, 2.0], [3.0, 4.0]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPolygonTest, checking_polygon_with_2_negative_tuples) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[[-1.0, -2.0], [-3.0, -4.0]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPolygonTest, checking_polygon_with_empty_input) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_ARRAY_EXPECTED);
  });

  char const* p = "\"\"";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPolygonTest, checking_polygon_with_boolean) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[true]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPolygonTest, checking_polygon_with_booleans) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[true, false]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPolygonTest, checking_polygon_with_nested_booleans) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[[true], [false], [true], [false]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPolygonTest, checking_object_with_single_boolean) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_ARRAY_EXPECTED);
  });

  char const* p = "true";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPolygonTest, checking_object_with_single_number) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_ARRAY_EXPECTED);
  });

  char const* p = "123";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPolygonTest, checking_object_with_string) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_ARRAY_EXPECTED);
  });

  char const* p = "\"hallowelt\"";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPolygonTest, checking_object_with_null) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_ARRAY_EXPECTED);
  });

  char const* p = "null";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoPolygonTest, checking_object_with_some_data) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_ARRAY_EXPECTED);
  });

  char const* p = "{\"Hello\": true, \"Hellox\": 123}";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoPolygon(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}
}  // namespace geo_polygon

namespace geo_linestring {
struct GeoLinestringTest : public GeoConstructorTest {
  GeoLinestringTest()
      : GeoConstructorTest(),
        fun("GEO_LINESTRING", &Functions::GeoLinestring),
        funNode(NODE_TYPE_FCALL) {
    funNode.setData(static_cast<void const*>(&fun));
  }
  
  arangodb::aql::Function fun;
  arangodb::aql::AstNode funNode;
};

TEST_F(GeoLinestringTest, checking_polygon_with_2_positions) {
  char const* p = "[[1.0, 2.0], [3.0, 4.0]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_EQ(res.slice().get("coordinates").length(), 2);
  EXPECT_TRUE(res.slice().get("coordinates").at(0).isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(1).isArray());
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(0).getDouble(), 1.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(1).getDouble(), 2.0);
  EXPECT_EQ(res.slice().get("coordinates").at(1).at(0).getDouble(), 3.0);
  EXPECT_EQ(res.slice().get("coordinates").at(1).at(1).getDouble(), 4.0);
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "LineString");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoLinestringTest, checking_linestring_representing_cologne) {
  char const* p =
      "[[6.888427734375,50.91602169392645],[6.9632720947265625,50."
      "87921050161489],[7.013397216796875,50.89480467658874],[7."
      "0731353759765625,50.92424609910128],[7.093048095703125,50."
      "94804539355076],[7.03948974609375,50.9709677364145],[6.985244750976562,"
      "51.000360974529464],[6.8891143798828125,50.996471761616284],[6."
      "867828369140624,50.95669666276118],[6.888427734375,50.91602169392645]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_EQ(res.slice().get("coordinates").length(), 10);
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "LineString");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoLinestringTest, checking_array_with_1_position) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[[1.0, 2.0]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoLinestringTest, checking_array_with_positions_and_invalid_bool) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[[1.0, 2.0], [1.0, 2.0], [1.0, 2.0], [1.0, 2.0], false]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoLinestringTest, checking_array_with_positions_and_invalid_bool_2) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[true, [1.0, 2.0], [1.0, 2.0], [1.0, 2.0], [1.0, 2.0]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoLinestringTest, checking_empty_nested_array) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[[]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoLinestringTest, checking_empty_array) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoLinestringTest, checking_bool) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_ARRAY_EXPECTED);
  });

  char const* p = "true";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoLinestringTest, checking_number) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_ARRAY_EXPECTED);
  });

  char const* p = "123";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoLinestringTest, checking_object) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_ARRAY_EXPECTED);
  });

  char const* p = "{\"Hello\": true, \"Hellox\": 123}";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}
}  // namespace geo_linestring

namespace geo_multilinestring {
struct GeoMultilinestringTest : public GeoConstructorTest {
  GeoMultilinestringTest() 
      : GeoConstructorTest(),
        fun("GEO_MULTILINESTRING", &Functions::GeoMultiLinestring),
        funNode(NODE_TYPE_FCALL) {
    funNode.setData(static_cast<void const*>(&fun));
  }

  arangodb::aql::Function fun;
  arangodb::aql::AstNode funNode;
};

TEST_F(GeoMultilinestringTest, checking_multilinestrings_with_2x2_positions) {
  char const* p = "[ [[1.0, 2.0], [3.0, 4.0]], [[1.0, 2.0], [3.0, 4.0]] ]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_EQ(res.slice().get("coordinates").length(), 2);
  EXPECT_TRUE(res.slice().get("coordinates").at(0).isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(1).isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(0).at(0).isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(0).at(1).isArray());
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(0).at(0).getDouble(), 1.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(0).at(1).getDouble(), 2.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(1).at(0).getDouble(), 3.0);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(1).at(1).getDouble(), 4.0);
  EXPECT_EQ(res.slice().get("coordinates").at(1).at(0).at(0).getDouble(), 1.0);
  EXPECT_EQ(res.slice().get("coordinates").at(1).at(0).at(1).getDouble(), 2.0);
  EXPECT_EQ(res.slice().get("coordinates").at(1).at(1).at(0).getDouble(), 3.0);
  EXPECT_EQ(res.slice().get("coordinates").at(1).at(1).at(1).getDouble(), 4.0);
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "MultiLineString");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoMultilinestringTest, checking_multilinestrings_with_2x2_positions_2) {
  char const* p =
      "[ [[-1.1, -2.2], [-3.3, -4.4]], [[-1.1, -2.2], [-3.3, -4.4]] ]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.isObject());
  EXPECT_TRUE(res.slice().get("coordinates").isArray());
  EXPECT_EQ(res.slice().get("coordinates").length(), 2);
  EXPECT_TRUE(res.slice().get("coordinates").at(0).isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(1).isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(0).at(0).isArray());
  EXPECT_TRUE(res.slice().get("coordinates").at(0).at(1).isArray());
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(0).at(0).getDouble(), -1.1);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(0).at(1).getDouble(), -2.2);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(1).at(0).getDouble(), -3.3);
  EXPECT_EQ(res.slice().get("coordinates").at(0).at(1).at(1).getDouble(), -4.4);
  EXPECT_EQ(res.slice().get("coordinates").at(1).at(0).at(0).getDouble(), -1.1);
  EXPECT_EQ(res.slice().get("coordinates").at(1).at(0).at(1).getDouble(), -2.2);
  EXPECT_EQ(res.slice().get("coordinates").at(1).at(1).at(0).getDouble(), -3.3);
  EXPECT_EQ(res.slice().get("coordinates").at(1).at(1).at(1).getDouble(), -4.4);
  EXPECT_TRUE(res.slice().get("type").isString());
  EXPECT_EQ(res.slice().get("type").copyString(), "MultiLineString");
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoMultilinestringTest, checking_object) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_ARRAY_EXPECTED);
  });

  char const* p = "{\"Hello\": true, \"Hellox\": 123}";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoMultilinestringTest, checking_polygon_with_0_positions_nested) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[[]]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoMultilinestringTest, checking_polygon_with_0_positions) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* p = "[]";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoMultilinestringTest, checking_bool) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_ARRAY_EXPECTED);
  });

  char const* p = "true";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}

TEST_F(GeoMultilinestringTest, checking_number) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](ErrorCode code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_ARRAY_EXPECTED);
  });

  char const* p = "123";
  size_t l = strlen(p);

  std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
  VPackSlice json = builder->slice();
  params.emplace_back(json);

  AqlValue res = Functions::GeoMultiLinestring(&expressionContext, funNode, params);
  EXPECT_TRUE(res.slice().isNull());
  res.destroy();
  // Free input parameters
  for (auto& it : params) {
    it.destroy();
  }
}
}  // namespace geo_multilinestring

}  // namespace geo_constructors_aql
}  // namespace tests
}  // namespace arangodb
