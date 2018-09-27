////////////////////////////////////////////////////////////////////////////////
/// @brief test sr AQL Geo Constructors
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "catch.hpp"
#include "fakeit.hpp"

#include "Aql/AqlValue.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Functions.h"
#include "Aql/Query.h"
#include "Basics/SmallVector.h"
#include "Transaction/Methods.h"

#include "lib/Random/RandomGenerator.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace geo_functions_aql {
    
auto clearVector = [](SmallVector<AqlValue>& v) {
  for (auto& it : v) {
    it.destroy();
  }
  v.clear();
};

namespace geo_equals_point {
SCENARIO("Testing GEO_EQUALS Point", "[AQL][GEOF][GEOEQUALSPOINT]") {

  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  WHEN("Checking valid point comparison") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> paramsA{arena};
    SmallVector<AqlValue> paramsB{arena};
    SmallVector<AqlValue> paramsC{arena};
  
    auto guard = scopeGuard([&paramsA, &paramsB, &paramsC]() {
      clearVector(paramsA);
      clearVector(paramsB);
      clearVector(paramsC);
    });

    WHEN("checking two equal points") {
      VPackBuilder foo;
      foo.openArray();
      foo.add(VPackValue(1));
      foo.add(VPackValue(-2.2));
      foo.close();
      paramsA.emplace_back(foo.slice().at(0));
      paramsA.emplace_back(foo.slice().at(1));
      AqlValue pointA = Functions::GeoPoint(&expressionContext, &trx, paramsA);

      paramsC.emplace_back(pointA.clone());
      paramsC.emplace_back(pointA.clone());
      pointA.destroy();

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == true);
    }

    WHEN("checking two unequal points") {
      VPackBuilder foo;
      foo.openArray();
      foo.add(VPackValue(1));
      foo.add(VPackValue(-2.2));
      foo.close();
      paramsA.emplace_back(foo.slice().at(0));
      paramsA.emplace_back(foo.slice().at(1));
      AqlValue pointA = Functions::GeoPoint(&expressionContext, &trx, paramsA);
      paramsC.emplace_back(pointA.clone());
      pointA.destroy();
      
      VPackBuilder bar;
      bar.openArray();
      bar.add(VPackValue(-2.2));
      bar.add(VPackValue(-1));
      bar.close();
      paramsB.emplace_back(bar.slice().at(0));
      paramsB.emplace_back(bar.slice().at(1));
      AqlValue pointB = Functions::GeoPoint(&expressionContext, &trx, paramsB);
      paramsC.emplace_back(pointB.clone());
      pointB.destroy();

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == false);
    }
  }
}
} // geo_equals_point

namespace geo_equals_multipoint {
SCENARIO("Testing GEO_EQUALS MultiPoint", "[AQL][GEOF][GEOEQUALSMULTIPOINT]") {

  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  WHEN("Checking valid point comparison") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> paramsA{arena};
    SmallVector<AqlValue> paramsB{arena};
    SmallVector<AqlValue> paramsC{arena};
    
    auto guard = scopeGuard([&paramsA, &paramsB, &paramsC]() {
      clearVector(paramsA);
      clearVector(paramsB);
      clearVector(paramsC);
    });

    WHEN("checking two equal multipoints") {
      char const* polyA = "[[1.0, 2.0], [3.0, 4.0], [5.0, 6.0], [1.0, 2.0]]";
      size_t lA = strlen(polyA);

      std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
      VPackSlice jsonA = builderA->slice();

      paramsA.emplace_back(jsonA);

      AqlValue resA = Functions::GeoMultiPoint(&expressionContext, &trx, paramsA);
      paramsC.emplace_back(resA.clone());
      paramsC.emplace_back(resA.clone());
      resA.destroy();

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == true);
    }

    WHEN("checking two unequal multipoints") {
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

      AqlValue resA = Functions::GeoMultiPoint(&expressionContext, &trx, paramsA);
      paramsC.emplace_back(resA);
      AqlValue resB = Functions::GeoMultiPoint(&expressionContext, &trx, paramsB);
      paramsC.emplace_back(resB);

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == false);
    }
  }
}
} // geo_equals_multipoint

namespace geo_equals_polygon {
SCENARIO("Testing GEO_EQUALS Polygon", "[AQL][GEOF][GEOEQUALSPOLYGON]") {

  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  WHEN("Checking valid polygon comparison") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> paramsA{arena};
    SmallVector<AqlValue> paramsB{arena};
    SmallVector<AqlValue> paramsC{arena};
    
    auto guard = scopeGuard([&paramsA, &paramsB, &paramsC]() {
      clearVector(paramsA);
      clearVector(paramsB);
      clearVector(paramsC);
    });

    WHEN("checking two equal polygons") {
      char const* polyA = "[[1.0, 2.0], [3.0, 4.0], [5.0, 6.0], [1.0, 2.0]]";
      size_t lA = strlen(polyA);

      std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
      VPackSlice jsonA = builderA->slice();

      paramsA.emplace_back(jsonA);

      AqlValue resA = Functions::GeoPolygon(&expressionContext, &trx, paramsA);
      paramsC.emplace_back(resA.clone());
      paramsC.emplace_back(resA.clone());
      resA.destroy();

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      resC.destroy();
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == true);
    }

    WHEN("checking two equal more detailed polygons") {
      char const* polyA = "[[6.888427734375,50.91602169392645],[6.9632720947265625,50.87921050161489],[7.013397216796875,50.89480467658874],[7.0731353759765625,50.92424609910128],[7.093048095703125,50.94804539355076],[7.03948974609375,50.9709677364145],[6.985244750976562,51.000360974529464],[6.8891143798828125,50.996471761616284],[6.867828369140624,50.95669666276118],[6.888427734375,50.91602169392645]]";
      size_t lA = strlen(polyA);

      std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
      VPackSlice jsonA = builderA->slice();

      paramsA.emplace_back(jsonA);

      AqlValue resA = Functions::GeoPolygon(&expressionContext, &trx, paramsA);
      paramsC.emplace_back(resA.clone());
      paramsC.emplace_back(resA.clone());
      resA.destroy();

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == true);
    }

    WHEN("checking two unequal polygons") {
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

      AqlValue resA = Functions::GeoPolygon(&expressionContext, &trx, paramsA);
      paramsC.emplace_back(resA);
      AqlValue resB = Functions::GeoPolygon(&expressionContext, &trx, paramsB);
      paramsC.emplace_back(resB);

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == false);
    }

    WHEN("checking two nested equal polygons") {
      char const* polyA = "[[[35, 10], [45, 45], [15, 40], [10, 20], [35, 10]],[[20, 30], [35, 35], [30, 20], [20, 30]]]";
      size_t lA = strlen(polyA);

      std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
      VPackSlice jsonA = builderA->slice();

      paramsA.emplace_back(jsonA);

      AqlValue resA = Functions::GeoPolygon(&expressionContext, &trx, paramsA);
      paramsC.emplace_back(resA.clone());
      paramsC.emplace_back(resA.clone());
      resA.destroy();

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == true);
    }

    WHEN("checking two unequal nested polygons - outer loop difference") {
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

      AqlValue resA = Functions::GeoPolygon(&expressionContext, &trx, paramsA);
      paramsC.emplace_back(resA);
      AqlValue resB = Functions::GeoPolygon(&expressionContext, &trx, paramsB);
      paramsC.emplace_back(resB);

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == false);
    }

    WHEN("checking two unequal nested polygons - inner loop difference") {
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

      AqlValue resA = Functions::GeoPolygon(&expressionContext, &trx, paramsA);
      paramsC.emplace_back(resA);
      AqlValue resB = Functions::GeoPolygon(&expressionContext, &trx, paramsB);
      paramsC.emplace_back(resB);

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == false);
    }

    WHEN("checking two unequal nested polygons - inner and outer loop difference") {
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

      AqlValue resA = Functions::GeoPolygon(&expressionContext, &trx, paramsA);
      paramsC.emplace_back(resA);
      AqlValue resB = Functions::GeoPolygon(&expressionContext, &trx, paramsB);
      paramsC.emplace_back(resB);

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == false);
    }
  }

  WHEN("Checking invalid polygon comparison") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> paramsA{arena};
    SmallVector<AqlValue> paramsB{arena};
    SmallVector<AqlValue> paramsC{arena};
    
    auto guard = scopeGuard([&paramsA, &paramsB, &paramsC]() {
      clearVector(paramsA);
      clearVector(paramsB);
      clearVector(paramsC);
    });

    WHEN("checking only one polygon - first parameter") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
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

      AqlValue resA = Functions::GeoPolygon(&expressionContext, &trx, paramsA);
      paramsC.emplace_back(resA);
      paramsC.emplace_back(jsonB);

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isNull());
    }

    WHEN("checking only one polygon - second parameter") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
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

      AqlValue resA = Functions::GeoPolygon(&expressionContext, &trx, paramsA);
      paramsC.emplace_back(jsonB);
      paramsC.emplace_back(resA);

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isNull());
    }
  }

}
} // geo_equals_polygon

namespace geo_equals_linestring {
SCENARIO("Testing GEO_EQUALS Linestring", "[AQL][GEOF][GEOEQUALSLINESTRING]") {

  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  WHEN("Checking valid linestring comparison") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> paramsA{arena};
    SmallVector<AqlValue> paramsB{arena};
    SmallVector<AqlValue> paramsC{arena};
    
    auto guard = scopeGuard([&paramsA, &paramsB, &paramsC]() {
      clearVector(paramsA);
      clearVector(paramsB);
      clearVector(paramsC);
    });

    WHEN("checking two equal linestrings") {
      char const* polyA = "[[1.0, 2.0], [3.0, 4.0], [5.0, 6.0], [1.0, 2.0]]";
      size_t lA = strlen(polyA);

      std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
      VPackSlice jsonA = builderA->slice();

      paramsA.emplace_back(jsonA);

      AqlValue resA = Functions::GeoLinestring(&expressionContext, &trx, paramsA);

      paramsC.emplace_back(resA.clone());
      paramsC.emplace_back(resA.clone());
      resA.destroy();

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == true);
    }

    WHEN("checking two unequal linestrings") {
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

      AqlValue resA = Functions::GeoLinestring(&expressionContext, &trx, paramsA);
      AqlValue resB = Functions::GeoLinestring(&expressionContext, &trx, paramsB);

      paramsC.emplace_back(resA);
      paramsC.emplace_back(resB);

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == false);
    }
  }
}
} // geo_equals_linestring

namespace geo_equals_multilinestring {
SCENARIO("Testing GEO_EQUALS MultiLinestring", "[AQL][GEOF][GEOEQUALSMULTILINESTRING]") {

  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  WHEN("Checking valid linestring comparison") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> paramsA{arena};
    SmallVector<AqlValue> paramsB{arena};
    SmallVector<AqlValue> paramsC{arena};
    
    auto guard = scopeGuard([&paramsA, &paramsB, &paramsC]() {
      clearVector(paramsA);
      clearVector(paramsB);
      clearVector(paramsC);
    });

    WHEN("checking two equal multilinestrings") {
      char const* polyA = "[ [[1.0, 2.0], [3.0, 4.0]], [[1.0, 2.0], [3.0, 4.0]] ]";
      size_t lA = strlen(polyA);

      std::shared_ptr<VPackBuilder> builderA = VPackParser::fromJson(polyA, lA);
      VPackSlice jsonA = builderA->slice();

      paramsA.emplace_back(jsonA);

      AqlValue resA = Functions::GeoMultiLinestring(&expressionContext, &trx, paramsA);
      paramsC.emplace_back(resA.clone());
      paramsC.emplace_back(resA.clone());
      resA.destroy();

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == true);
    }

    WHEN("checking two unequal multilinestrings") {
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

      AqlValue resA = Functions::GeoMultiLinestring(&expressionContext, &trx, paramsA);
      AqlValue resB = Functions::GeoMultiLinestring(&expressionContext, &trx, paramsB);

      paramsC.emplace_back(resA);
      paramsC.emplace_back(resB);

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == false);
    }
  }
}
}  // geo_equals_multilinestring

namespace geo_equals_mixings {
SCENARIO("Testing GEO_EQUALS Mixed Types", "[AQL][GEOF][GEOEQUALSMIXINGS]") {

  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  WHEN("Checking polygon with different types") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> paramsA{arena};
    SmallVector<AqlValue> paramsB{arena};
    SmallVector<AqlValue> paramsC{arena};
    
    auto guard = scopeGuard([&paramsA, &paramsB, &paramsC]() {
      clearVector(paramsA);
      clearVector(paramsB);
      clearVector(paramsC);
    });

    WHEN("checking polygon with multilinestring") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
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

      AqlValue resA = Functions::GeoPolygon(&expressionContext, &trx, paramsA);
      AqlValue resB = Functions::GeoMultiLinestring(&expressionContext, &trx, paramsB);

      paramsC.emplace_back(resA);
      paramsC.emplace_back(resB);

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == false);
    }

    WHEN("checking multipoint with multilinestring") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
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

      AqlValue resA = Functions::GeoMultiPoint(&expressionContext, &trx, paramsA);
      paramsC.emplace_back(resA);
      AqlValue resB = Functions::GeoMultiLinestring(&expressionContext, &trx, paramsB);
      paramsC.emplace_back(resB);

      AqlValue resC = Functions::GeoEquals(&expressionContext, &trx, paramsC);
      CHECK(resC.slice().isBoolean());
      CHECK(resC.slice().getBool() == false);
    }
  }
}
}  // geo_equals_mixings

}  // geo_functions_aql
}  // tests
}  // arangodb
