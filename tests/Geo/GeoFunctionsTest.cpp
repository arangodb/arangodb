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
namespace geo_functions_aql {
    
auto clearVector = [](SmallVector<AqlValue>& v) {
  for (auto& it : v) {
    it.destroy();
  }
  v.clear();
};

class GeoEqualsTest : public ::testing::Test {
 protected:
  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext;

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx;
  fakeit::Mock<transaction::Context> contextMock;
  transaction::Context& context;

  SmallVector<AqlValue>::allocator_type::arena_type arena;
  SmallVector<AqlValue> paramsA;
  SmallVector<AqlValue> paramsB;
  SmallVector<AqlValue> paramsC;
  
  arangodb::aql::Function equalsFun;
  arangodb::aql::AstNode equalsFunNode;

  GeoEqualsTest()
      : expressionContext(expressionContextMock.get()),
        trx(trxMock.get()),
        context(contextMock.get()),
        paramsA{arena},
        paramsB{arena},
        paramsC{arena},
        equalsFun("GEO_EQUALS", &Functions::GeoEquals),
        equalsFunNode(NODE_TYPE_FCALL) {
    equalsFunNode.setData(static_cast<void const*>(&equalsFun));

    static auto options = velocypack::Options::Defaults;
    fakeit::When(Method(trxMock, transactionContextPtr)).AlwaysReturn(&context);
    fakeit::When(Method(contextMock, getVPackOptions)).AlwaysReturn(&options);
    fakeit::When(Method(trxMock, vpackOptions)).AlwaysReturn(options);
    fakeit::When(Method(contextMock, leaseBuilder)).AlwaysDo([]() { return new arangodb::velocypack::Builder(); });
    fakeit::When(Method(contextMock, returnBuilder)).AlwaysDo([](arangodb::velocypack::Builder* b) { delete b; });
    fakeit::When(Method(expressionContextMock, trx)).AlwaysDo([&]() -> transaction::Methods& { return this->trx; });
  }

  ~GeoEqualsTest() {
    clearVector(paramsA);
    clearVector(paramsB);
    clearVector(paramsC);
  }
};


namespace geo_equals_point {
struct GeoEqualsPointTest : public GeoEqualsTest {
  GeoEqualsPointTest() 
      : GeoEqualsTest(),
        fun("GEO_POiNT", &Functions::GeoPoint),
        funNode(NODE_TYPE_FCALL) {
    funNode.setData(static_cast<void const*>(&fun));
  }

  arangodb::aql::Function fun;
  arangodb::aql::AstNode funNode;
};

TEST_F(GeoEqualsPointTest, checking_two_equal_points) {
  VPackBuilder foo;
  foo.openArray();
  foo.add(VPackValue(1));
  foo.add(VPackValue(-2.2));
  foo.close();
  paramsA.emplace_back(foo.slice().at(0));
  paramsA.emplace_back(foo.slice().at(1));
  AqlValue pointA = Functions::GeoPoint(&expressionContext, funNode, paramsA);

  paramsC.emplace_back(pointA.clone());
  paramsC.emplace_back(pointA.clone());
  pointA.destroy();

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_TRUE(resC.slice().getBool());
}

TEST_F(GeoEqualsPointTest, checking_two_unequal_points) {
  VPackBuilder foo;
  foo.openArray();
  foo.add(VPackValue(1));
  foo.add(VPackValue(-2.2));
  foo.close();
  paramsA.emplace_back(foo.slice().at(0));
  paramsA.emplace_back(foo.slice().at(1));
  AqlValue pointA = Functions::GeoPoint(&expressionContext, funNode, paramsA);
  paramsC.emplace_back(pointA.clone());
  pointA.destroy();
  
  VPackBuilder bar;
  bar.openArray();
  bar.add(VPackValue(-2.2));
  bar.add(VPackValue(-1));
  bar.close();
  paramsB.emplace_back(bar.slice().at(0));
  paramsB.emplace_back(bar.slice().at(1));
  AqlValue pointB = Functions::GeoPoint(&expressionContext, funNode, paramsB);
  paramsC.emplace_back(pointB.clone());
  pointB.destroy();

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_FALSE(resC.slice().getBool());
}

} // geo_equals_point

namespace geo_equals_multipoint {
struct GeoEqualsMultipointTest : public GeoEqualsTest {
  GeoEqualsMultipointTest() 
      : GeoEqualsTest(),
        fun("GEO_MULTIPOiNT", &Functions::GeoMultiPoint),
        funNode(NODE_TYPE_FCALL) {
    funNode.setData(static_cast<void const*>(&fun));
  }

  arangodb::aql::Function fun;
  arangodb::aql::AstNode funNode;
};

TEST_F(GeoEqualsMultipointTest, checking_two_equal_multipoints) {
  char const* polyA = "[[1.0, 2.0], [3.0, 4.0], [5.0, 6.0], [1.0, 2.0]]";
  size_t lA = strlen(polyA);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();

  paramsA.emplace_back(jsonA);

  AqlValue resA = Functions::GeoMultiPoint(&expressionContext, funNode, paramsA);
  paramsC.emplace_back(resA.clone());
  paramsC.emplace_back(resA.clone());
  resA.destroy();

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_TRUE(resC.slice().getBool());
}

TEST_F(GeoEqualsMultipointTest, checking_two_unequal_multipoints) {
  char const* polyA = "[[0.5, 1.5], [3.0, 4.0], [5.0, 6.0], [0.5, 1.5]]";
  size_t lA = strlen(polyA);
  char const* polyB = "[[1.0, 2.0], [3.0, 4.0], [5.0, 6.0], [1.0, 2.0]]";
  size_t lB = strlen(polyB);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();
  std::shared_ptr<VPackBuilder> builderB = VPackParser::fromJson(polyB, lB);
  VPackSlice jsonB = builderB->slice();

  paramsA.emplace_back(jsonA);
  paramsB.emplace_back(jsonB);

  AqlValue resA = Functions::GeoMultiPoint(&expressionContext, funNode, paramsA);
  paramsC.emplace_back(resA);
  AqlValue resB = Functions::GeoMultiPoint(&expressionContext, funNode, paramsB);
  paramsC.emplace_back(resB);

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_FALSE(resC.slice().getBool());
}


} // geo_equals_multipoint

namespace geo_equals_polygon {
struct GeoEqualsPolygonTest : public GeoEqualsTest {
  GeoEqualsPolygonTest() 
      : GeoEqualsTest(),
        fun("GEO_POLYGON", &Functions::GeoPolygon),
        funNode(NODE_TYPE_FCALL) {
    funNode.setData(static_cast<void const*>(&fun));
  }

  arangodb::aql::Function fun;
  arangodb::aql::AstNode funNode;
};

TEST_F(GeoEqualsPolygonTest, checking_two_equal_polygons) {
  char const* polyA = "[[1.0, 2.0], [3.0, 4.0], [5.0, 6.0], [1.0, 2.0]]";
  size_t lA = strlen(polyA);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();

  paramsA.emplace_back(jsonA);

  AqlValue resA = Functions::GeoPolygon(&expressionContext, funNode, paramsA);
  paramsC.emplace_back(resA.clone());
  paramsC.emplace_back(resA.clone());
  resA.destroy();

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  resC.destroy();
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_TRUE(resC.slice().getBool());
}

TEST_F(GeoEqualsPolygonTest, checking_two_equal_more_detailed_polygons) {
  char const* polyA = "[[6.888427734375,50.91602169392645],[6.9632720947265625,50.87921050161489],[7.013397216796875,50.89480467658874],[7.0731353759765625,50.92424609910128],[7.093048095703125,50.94804539355076],[7.03948974609375,50.9709677364145],[6.985244750976562,51.000360974529464],[6.8891143798828125,50.996471761616284],[6.867828369140624,50.95669666276118],[6.888427734375,50.91602169392645]]";
  size_t lA = strlen(polyA);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();

  paramsA.emplace_back(jsonA);

  AqlValue resA = Functions::GeoPolygon(&expressionContext, funNode, paramsA);
  paramsC.emplace_back(resA.clone());
  paramsC.emplace_back(resA.clone());
  resA.destroy();

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_TRUE(resC.slice().getBool());
}

TEST_F(GeoEqualsPolygonTest, checking_two_unequal_polygons) {
  char const* polyA = "[[0.5, 1.5], [3.0, 4.0], [5.0, 6.0], [0.5, 1.5]]";
  size_t lA = strlen(polyA);
  char const* polyB = "[[1.0, 2.0], [3.0, 4.0], [5.0, 6.0], [1.0, 2.0]]";
  size_t lB = strlen(polyB);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();
  std::shared_ptr<VPackBuilder> builderB = VPackParser::fromJson(polyB, lB);
  VPackSlice jsonB = builderB->slice();

  paramsA.emplace_back(jsonA);
  paramsB.emplace_back(jsonB);

  AqlValue resA = Functions::GeoPolygon(&expressionContext, funNode, paramsA);
  paramsC.emplace_back(resA);
  AqlValue resB = Functions::GeoPolygon(&expressionContext, funNode, paramsB);
  paramsC.emplace_back(resB);

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_FALSE(resC.slice().getBool());
}

TEST_F(GeoEqualsPolygonTest, checking_two_nested_equal_polygons) {
  char const* polyA = "[[[35, 10], [45, 45], [15, 40], [10, 20], [35, 10]],[[20, 30], [35, 35], [30, 20], [20, 30]]]";
  size_t lA = strlen(polyA);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();

  paramsA.emplace_back(jsonA);

  AqlValue resA = Functions::GeoPolygon(&expressionContext, funNode, paramsA);
  paramsC.emplace_back(resA.clone());
  paramsC.emplace_back(resA.clone());
  resA.destroy();

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_TRUE(resC.slice().getBool());
}

TEST_F(GeoEqualsPolygonTest, checking_two_unequal_nested_polygons_outer_loop_difference) {
  char const* polyA = "[[[30, 10], [45, 45], [15, 40], [10, 20], [30, 10]],[[20, 30], [35, 35], [30, 20], [20, 30]]]";
  size_t lA = strlen(polyA);
  char const* polyB = "[[[35, 10], [45, 45], [15, 40], [10, 20], [35, 10]],[[20, 30], [35, 35], [30, 20], [20, 30]]]";
  size_t lB = strlen(polyB);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();
  std::shared_ptr<VPackBuilder> builderB = VPackParser::fromJson(polyB, lB);
  VPackSlice jsonB = builderB->slice();

  paramsA.emplace_back(jsonA);
  paramsB.emplace_back(jsonB);

  AqlValue resA = Functions::GeoPolygon(&expressionContext, funNode, paramsA);
  paramsC.emplace_back(resA);
  AqlValue resB = Functions::GeoPolygon(&expressionContext, funNode, paramsB);
  paramsC.emplace_back(resB);

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_FALSE(resC.slice().getBool());
}

TEST_F(GeoEqualsPolygonTest, checking_two_unequal_nested_polygons_inner_loop_difference) {
  char const* polyA = "[[[35, 10], [45, 45], [15, 40], [10, 20], [35, 10]],[[15, 30], [35, 35], [30, 20], [15, 30]]]";
  size_t lA = strlen(polyA);
  char const* polyB = "[[[35, 10], [45, 45], [15, 40], [10, 20], [35, 10]],[[20, 30], [35, 35], [30, 20], [20, 30]]]";
  size_t lB = strlen(polyB);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();
  std::shared_ptr<VPackBuilder> builderB = VPackParser::fromJson(polyB, lB);
  VPackSlice jsonB = builderB->slice();

  paramsA.emplace_back(jsonA);
  paramsB.emplace_back(jsonB);

  AqlValue resA = Functions::GeoPolygon(&expressionContext, funNode, paramsA);
  paramsC.emplace_back(resA);
  AqlValue resB = Functions::GeoPolygon(&expressionContext, funNode, paramsB);
  paramsC.emplace_back(resB);

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_FALSE(resC.slice().getBool());
}

TEST_F(GeoEqualsPolygonTest, checking_two_unequal_nested_polygons_inner_and_outer_polygons) {
  char const* polyA = "[[[30, 10], [45, 45], [15, 40], [10, 20], [30, 10]],[[20, 30], [35, 35], [30, 20], [20, 30]]]";
  size_t lA = strlen(polyA);
  char const* polyB = "[[[35, 10], [45, 45], [15, 40], [10, 20], [35, 10]],[[15, 30], [35, 35], [30, 20], [15, 30]]]";
  size_t lB = strlen(polyB);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();
  std::shared_ptr<VPackBuilder> builderB = VPackParser::fromJson(polyB, lB);
  VPackSlice jsonB = builderB->slice();

  paramsA.emplace_back(jsonA);
  paramsB.emplace_back(jsonB);

  AqlValue resA = Functions::GeoPolygon(&expressionContext, funNode, paramsA);
  paramsC.emplace_back(resA);
  AqlValue resB = Functions::GeoPolygon(&expressionContext, funNode, paramsB);
  paramsC.emplace_back(resB);

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_FALSE(resC.slice().getBool());
}

TEST_F(GeoEqualsPolygonTest, checking_only_one_polygon_first_parameter) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* polyA = "[[1.0, 2.0], [3.0, 4.0], [5.0, 6.0], [1.0, 2.0]]";
  size_t lA = strlen(polyA);
  char const* nullValue = "null";
  size_t lB = strlen(nullValue);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();
  
  std::shared_ptr<VPackBuilder> builderB = VPackParser::fromJson(nullValue, lB);
  VPackSlice jsonB = builderB->slice();

  paramsA.emplace_back(jsonA);

  AqlValue resA = Functions::GeoPolygon(&expressionContext, funNode, paramsA);
  paramsC.emplace_back(resA);
  paramsC.emplace_back(jsonB);

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isNull());
}

TEST_F(GeoEqualsPolygonTest, checking_only_one_polygon_second_parameter) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* polyA = "[[1.0, 2.0], [3.0, 4.0], [5.0, 6.0], [1.0, 2.0]]";
  size_t lA = strlen(polyA);
  char const* nullValue = "null";
  size_t lB = strlen(nullValue);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();
  
  std::shared_ptr<VPackBuilder> builderB = VPackParser::fromJson(nullValue, lB);
  VPackSlice jsonB = builderB->slice();

  paramsA.emplace_back(jsonA);

  AqlValue resA = Functions::GeoPolygon(&expressionContext, funNode, paramsA);
  paramsC.emplace_back(jsonB);
  paramsC.emplace_back(resA);

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isNull());
}
} // geo_equals_polygon

namespace geo_equals_linestring {
struct GeoEqualsLinestringTest : public GeoEqualsTest {
  GeoEqualsLinestringTest() 
      : GeoEqualsTest(),
        fun("GEO_LINESTRING", &Functions::GeoLinestring),
        funNode(NODE_TYPE_FCALL) {
    funNode.setData(static_cast<void const*>(&fun));
  }

  arangodb::aql::Function fun;
  arangodb::aql::AstNode funNode;
};

TEST_F(GeoEqualsLinestringTest, checking_two_equal_linestrings) {
  char const* polyA = "[[1.0, 2.0], [3.0, 4.0], [5.0, 6.0], [1.0, 2.0]]";
  size_t lA = strlen(polyA);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();

  paramsA.emplace_back(jsonA);

  AqlValue resA = Functions::GeoLinestring(&expressionContext, funNode, paramsA);

  paramsC.emplace_back(resA.clone());
  paramsC.emplace_back(resA.clone());
  resA.destroy();

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_TRUE(resC.slice().getBool());
}

TEST_F(GeoEqualsLinestringTest, checking_two_unequal_linestrings) {
  char const* polyA = "[[0.5, 1.5], [3.0, 4.0], [5.0, 6.0], [0.5, 1.5]]";
  size_t lA = strlen(polyA);
  char const* polyB = "[[1.0, 2.0], [3.0, 4.0], [5.0, 6.0], [1.0, 2.0]]";
  size_t lB = strlen(polyB);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();
  std::shared_ptr<VPackBuilder> builderB = VPackParser::fromJson(polyB, lB);
  VPackSlice jsonB = builderB->slice();

  paramsA.emplace_back(jsonA);
  paramsB.emplace_back(jsonB);

  AqlValue resA = Functions::GeoLinestring(&expressionContext, funNode, paramsA);
  AqlValue resB = Functions::GeoLinestring(&expressionContext, funNode, paramsB);

  paramsC.emplace_back(resA);
  paramsC.emplace_back(resB);

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_FALSE(resC.slice().getBool());
}

} // geo_equals_linestring

namespace geo_equals_multilinestring {
struct GeoEqualsMultilinestringTest : public GeoEqualsTest {
  GeoEqualsMultilinestringTest() 
      : GeoEqualsTest(),
        fun("GEO_MULTILINESTRING", &Functions::GeoMultiLinestring),
        funNode(NODE_TYPE_FCALL) {
    funNode.setData(static_cast<void const*>(&fun));
  }

  arangodb::aql::Function fun;
  arangodb::aql::AstNode funNode;
};

TEST_F(GeoEqualsMultilinestringTest, checking_two_equal_multilinestrings) {
  char const* polyA = "[ [[1.0, 2.0], [3.0, 4.0]], [[1.0, 2.0], [3.0, 4.0]] ]";
  size_t lA = strlen(polyA);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();

  paramsA.emplace_back(jsonA);

  AqlValue resA = Functions::GeoMultiLinestring(&expressionContext, funNode, paramsA);
  paramsC.emplace_back(resA.clone());
  paramsC.emplace_back(resA.clone());
  resA.destroy();

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_TRUE(resC.slice().getBool());
}

TEST_F(GeoEqualsMultilinestringTest, checking_two_unequal_multilinestrings) {
  char const* polyA = "[ [[1.0, 2.0], [3.0, 4.0]], [[1.0, 2.0], [5.0, 6.0]] ]";
  size_t lA = strlen(polyA);
  char const* polyB = "[ [[1.0, 2.0], [3.0, 4.0]], [[1.0, 2.0], [3.0, 4.0]] ]";
  size_t lB = strlen(polyB);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();
  std::shared_ptr<VPackBuilder> builderB = VPackParser::fromJson(polyB, lB);
  VPackSlice jsonB = builderB->slice();

  paramsA.emplace_back(jsonA);
  paramsB.emplace_back(jsonB);

  AqlValue resA = Functions::GeoMultiLinestring(&expressionContext, funNode, paramsA);
  AqlValue resB = Functions::GeoMultiLinestring(&expressionContext, funNode, paramsB);

  paramsC.emplace_back(resA);
  paramsC.emplace_back(resB);

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_FALSE(resC.slice().getBool());
}

}  // geo_equals_multilinestring

namespace geo_equals_mixings {
struct GeoEqualsMixedTypeTest : public GeoEqualsTest {
  GeoEqualsMixedTypeTest() : GeoEqualsTest() {}
};

TEST_F(GeoEqualsMixedTypeTest, checking_polygon_with_multilinestring) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* polyA = "[ [[1.0, 2.0], [3.0, 4.0], [3.3, 4.4], [1.0, 2.0]] ]";
  size_t lA = strlen(polyA);
  char const* lineB = "[ [[1.0, 2.0], [3.0, 4.0]], [[1.0, 2.0], [3.0, 4.0]] ]";
  size_t lB = strlen(lineB);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();
  std::shared_ptr<VPackBuilder> builderB = VPackParser::fromJson(lineB, lB);
  VPackSlice jsonB = builderB->slice();

  paramsA.emplace_back(jsonA);
  paramsB.emplace_back(jsonB);
    
  arangodb::aql::Function f1("GEO_POLYGON", &Functions::GeoPolygon);
  arangodb::aql::AstNode node1(NODE_TYPE_FCALL);
  node1.setData(static_cast<void const*>(&f1));
  
  arangodb::aql::Function f2("GEO_MULTILINESTRING", &Functions::GeoMultiLinestring);
  arangodb::aql::AstNode node2(NODE_TYPE_FCALL);
  node2.setData(static_cast<void const*>(&f2));

  AqlValue resA = Functions::GeoPolygon(&expressionContext, node1, paramsA);
  AqlValue resB = Functions::GeoMultiLinestring(&expressionContext, node2, paramsB);

  paramsC.emplace_back(resA);
  paramsC.emplace_back(resB);

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_FALSE(resC.slice().getBool());
}

TEST_F(GeoEqualsMixedTypeTest, checking_multipoint_with_multilinestring) {
  fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
    ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  });

  char const* polyA = "[ [1.0, 2.0], [3.0, 4.0], [1.0, 2.0], [5.0, 6.0] ]";
  size_t lA = strlen(polyA);
  char const* lineB = "[ [[1.0, 2.0], [3.0, 4.0]], [[1.0, 2.0], [3.0, 4.0]] ]";
  size_t lB = strlen(lineB);

  std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
  VPackSlice jsonA = builderA->slice();
  std::shared_ptr<VPackBuilder> builderB = VPackParser::fromJson(lineB, lB);
  VPackSlice jsonB = builderB->slice();

  paramsA.emplace_back(jsonA);
  paramsB.emplace_back(jsonB);
  
  arangodb::aql::Function f1("GEO_MULTIPOINT", &Functions::GeoMultiPoint);
  arangodb::aql::AstNode node1(NODE_TYPE_FCALL);
  node1.setData(static_cast<void const*>(&f1));
  
  arangodb::aql::Function f2("GEO_MULTILINESTRING", &Functions::GeoMultiLinestring);
  arangodb::aql::AstNode node2(NODE_TYPE_FCALL);
  node2.setData(static_cast<void const*>(&f2));

  AqlValue resA = Functions::GeoMultiPoint(&expressionContext, node1, paramsA);
  paramsC.emplace_back(resA);
  AqlValue resB = Functions::GeoMultiLinestring(&expressionContext, node2, paramsB);
  paramsC.emplace_back(resB);

  AqlValue resC = Functions::GeoEquals(&expressionContext, equalsFunNode, paramsC);
  EXPECT_TRUE(resC.slice().isBoolean());
  EXPECT_FALSE(resC.slice().getBool());
}

}  // geo_equals_mixings

TEST(GeoInRangeTest, test) {
  fakeit::Mock<ExpressionContext> expressionContextMock;
  fakeit::Mock<transaction::Methods> trxMock;
  fakeit::Mock<transaction::Context> contextMock;
  fakeit::When(Method(trxMock, transactionContextPtr)).AlwaysReturn(&contextMock.get());
  fakeit::When(Method(contextMock, getVPackOptions)).AlwaysReturn(&velocypack::Options::Defaults);
  fakeit::When(Method(trxMock, vpackOptions)).AlwaysReturn(velocypack::Options::Defaults);
  fakeit::When(Method(contextMock, leaseBuilder)).AlwaysDo([]() { return new arangodb::velocypack::Builder(); });
  fakeit::When(Method(contextMock, returnBuilder)).AlwaysDo([](arangodb::velocypack::Builder* b) { delete b; });
  fakeit::When(Method(expressionContextMock, trx)).AlwaysDo([&]() -> transaction::Methods& { return trxMock.get(); });

  arangodb::aql::Function f("GEO_IN_RANGE", &Functions::GeoInRange);
  arangodb::aql::AstNode node(NODE_TYPE_FCALL);
  node.setData(static_cast<void const*>(&f));

  auto lhs = VPackParser::fromJson(R"({
    "type": "Point",
    "coordinates": [37.610235, 55.709754] })");
  auto rhs = VPackParser::fromJson(R"({
    "type": "Point",
    "coordinates": [37.605, 55.707917] })");

  SmallVector<AqlValue>::allocator_type::arena_type arena;

  {
    fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* ) -> void {
      ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
    });

    SmallVector<AqlValue> params {{
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isNull(false));
  }

  {
    fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* ) -> void {
      ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
    });

    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isNull(false));
  }

  {
    fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* ) -> void {
      ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
    });

    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{lhs->slice()},
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isNull(false));
  }

  {
    fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* ) -> void {
      ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
    });

    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{lhs->slice()},
      AqlValue{AqlValueHintDouble{0}},
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isNull(false));
  }

  {
    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{lhs->slice()},
      AqlValue{AqlValueHintDouble{0}},
      AqlValue{AqlValueHintDouble{0}},
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isBoolean());
    ASSERT_TRUE(res.slice().getBool());
  }

  {
    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{lhs->slice()},
      AqlValue{AqlValueHintDouble{0}},
      AqlValue{AqlValueHintDouble{0}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isBoolean());
    ASSERT_FALSE(res.slice().getBool());
  }

  {
    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{lhs->slice()},
      AqlValue{AqlValueHintDouble{0}},
      AqlValue{AqlValueHintDouble{0}},
      AqlValue{AqlValueHintBool{true}},
      AqlValue{AqlValueHintBool{false}}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isBoolean());
    ASSERT_FALSE(res.slice().getBool());
  }

  {
    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{lhs->slice()},
      AqlValue{AqlValueHintDouble{0}},
      AqlValue{AqlValueHintDouble{0}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{true}}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isBoolean());
    ASSERT_FALSE(res.slice().getBool());
  }

  {
    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintDouble{0}},
      AqlValue{AqlValueHintDouble{100}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isBoolean());
    ASSERT_FALSE(res.slice().getBool());
  }

  {
    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintDouble{0}},
      AqlValue{AqlValueHintDouble{100}},
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isBoolean());
    ASSERT_FALSE(res.slice().getBool());
  }

  {
    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintDouble{0}},
      AqlValue{AqlValueHintDouble{100}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{"wg84"}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isBoolean());
    ASSERT_FALSE(res.slice().getBool());
  }


  {
    SmallVector<AqlValue> params {{
      AqlValue{rhs->slice()},
      AqlValue{lhs->slice()},
      AqlValue{AqlValueHintDouble{0}},
      AqlValue{AqlValueHintDouble{100}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{"wg84"}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isBoolean());
    ASSERT_FALSE(res.slice().getBool());
  }

  {
    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintDouble{0}},
      AqlValue{AqlValueHintDouble{100}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{"foo"} // fallback to 'sphere'
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isBoolean());
    ASSERT_FALSE(res.slice().getBool());
  }

  {
    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintDouble{0}},
      AqlValue{AqlValueHintDouble{100}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}} // fallback to 'sphere'
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isBoolean());
    ASSERT_FALSE(res.slice().getBool());
  }

  {
    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintDouble{1}},
      AqlValue{AqlValueHintDouble{400}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isBoolean());
    ASSERT_TRUE(res.slice().getBool());
  }

  {
    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintDouble{1}},
      AqlValue{AqlValueHintDouble{400}},
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isBoolean());
    ASSERT_TRUE(res.slice().getBool());
  }

  {
    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintDouble{1}},
      AqlValue{AqlValueHintDouble{400}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{"wg84"}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isBoolean());
    ASSERT_TRUE(res.slice().getBool());
  }

  {
    auto shapeJson = VPackParser::fromJson(R"({
      "type": "Polygon",
      "coordinates": [
          [
              [37.602682, 55.706853],
              [37.613025, 55.706853],
              [37.613025, 55.711906],
              [37.602682, 55.711906],
              [37.602682, 55.706853]
          ]
      ]
    })");

    SmallVector<AqlValue> params {{
      AqlValue{shapeJson->slice()},
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintDouble{240}},
      AqlValue{AqlValueHintDouble{242}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}},
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isBoolean());
    ASSERT_TRUE(res.slice().getBool());
  }

  {
    fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* ) -> void {
      ASSERT_EQ(code, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
    });

    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintDouble{1}},
      AqlValue{AqlValueHintDouble{400}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{"wg84"},
      AqlValue{"wg84"}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isNull(false));
  }

  {
    fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* ) -> void {
      ASSERT_EQ(code, TRI_ERROR_BAD_PARAMETER);
    });

    auto invalidJson = VPackParser::fromJson(R"({
      "type": "PPint",
      "coordinates": [37.610235, 55.709754] })");

    SmallVector<AqlValue> params {{
      AqlValue{invalidJson->slice()},
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintDouble{1}},
      AqlValue{AqlValueHintDouble{400}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{"wg84"}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isNull(false));
  }

  {
    fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* ) -> void {
      ASSERT_EQ(code, TRI_ERROR_BAD_PARAMETER);
    });

    SmallVector<AqlValue> params {{
      AqlValue{AqlValueHintBool{false}},
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintDouble{1}},
      AqlValue{AqlValueHintDouble{400}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{"wg84"}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isNull(false));
  }

  {
    fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* ) -> void {
      ASSERT_EQ(code, TRI_ERROR_BAD_PARAMETER);
    });

    auto invalidJson = VPackParser::fromJson(R"({
      "type": "PPint",
      "coordinates": [37.610235, 55.709754] })");

    SmallVector<AqlValue> params {{
      AqlValue{rhs->slice()},
      AqlValue{invalidJson->slice()},
      AqlValue{AqlValueHintDouble{1}},
      AqlValue{AqlValueHintDouble{400}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{"wg84"}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isNull(false));
  }

  {
    fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* ) -> void {
      ASSERT_EQ(code, TRI_ERROR_BAD_PARAMETER);
    });

    SmallVector<AqlValue> params {{
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintDouble{1}},
      AqlValue{AqlValueHintDouble{400}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{"wg84"}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isNull(false));
  }

  {
    fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* ) -> void {
      ASSERT_EQ(code, TRI_ERROR_BAD_PARAMETER);
    });

    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintDouble{400}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{"wg84"}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isNull(false));
  }

  {
    fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* ) -> void {
      ASSERT_EQ(code, TRI_ERROR_BAD_PARAMETER);
    });

    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintDouble{400}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{"wg84"}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isNull(false));
  }

  {
    fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* ) -> void {
      ASSERT_EQ(code, TRI_ERROR_BAD_PARAMETER);
    });

    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintDouble{100}},
      AqlValue{AqlValueHintDouble{400}},
      AqlValue{AqlValueHintDouble{400}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{"wg84"}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isNull(false));
  }

  {
    fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* ) -> void {
      ASSERT_EQ(code, TRI_ERROR_BAD_PARAMETER);
    });

    SmallVector<AqlValue> params {{
      AqlValue{lhs->slice()},
      AqlValue{rhs->slice()},
      AqlValue{AqlValueHintDouble{100}},
      AqlValue{AqlValueHintDouble{400}},
      AqlValue{AqlValueHintBool{false}},
      AqlValue{AqlValueHintDouble{400}},
      AqlValue{"wg84"}
    }, arena};

    auto guard = arangodb::scopeGuard([&params]() {
      for (auto& p : params) {
        p.destroy();
      }
    });

    auto const res = f.implementation(&expressionContextMock.get(), node, params);
    ASSERT_TRUE(res.isNull(false));
  }
}

}  // geo_functions_aql
}  // tests
}  // arangodb
