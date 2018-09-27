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
namespace geo_constructors_aql {

namespace geo_point {
SCENARIO("Testing GEO_POINT", "[AQL][GEOC][GEOPOINT]") {
  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  WHEN("Checking correct combinations of parameters") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{arena};

    // LOG_TOPIC(ERR, Logger::FIXME) << "result: " << res.slice().toString();
    WHEN("checking two positive integer values") {
      VPackBuilder foo;
      foo.openArray();
      foo.add(VPackValue(1));
      foo.add(VPackValue(2));
      foo.close();
      params.emplace_back(foo.slice().at(0));
      params.emplace_back(foo.slice().at(1));
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).getDouble() == 1);
      CHECK(res.slice().get("coordinates").at(1).getDouble() == 2);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Point");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking two negative integer values") {
      VPackBuilder foo;
      foo.openArray();
      foo.add(VPackValue(-1));
      foo.add(VPackValue(-2));
      foo.close();
      params.emplace_back(foo.slice().at(0));
      params.emplace_back(foo.slice().at(1));
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).getDouble() == -1);
      CHECK(res.slice().get("coordinates").at(1).getDouble() == -2);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Point");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking two positive double values") {
      VPackBuilder foo;
      foo.openArray();
      foo.add(VPackValue(1.1));
      foo.add(VPackValue(2.2));
      foo.close();
      params.emplace_back(foo.slice().at(0));
      params.emplace_back(foo.slice().at(1));
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).getDouble() == 1.1);
      CHECK(res.slice().get("coordinates").at(1).getDouble() == 2.2);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Point");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking two negative double values") {
      VPackBuilder foo;
      foo.openArray();
      foo.add(VPackValue(-1.1));
      foo.add(VPackValue(-2.2));
      foo.close();
      params.emplace_back(foo.slice().at(0));
      params.emplace_back(foo.slice().at(1));
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).getDouble() == -1.1);
      CHECK(res.slice().get("coordinates").at(1).getDouble() == -2.2);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Point");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking two positive integer and positive double values") {
      VPackBuilder foo;
      foo.openArray();
      foo.add(VPackValue(1));
      foo.add(VPackValue(2.2));
      foo.close();
      params.emplace_back(foo.slice().at(0));
      params.emplace_back(foo.slice().at(1));
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).getDouble() == 1);
      CHECK(res.slice().get("coordinates").at(1).getDouble() == 2.2);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Point");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking two negative integer and positive double values") {
      VPackBuilder foo;
      foo.openArray();
      foo.add(VPackValue(-1));
      foo.add(VPackValue(2.2));
      foo.close();
      params.emplace_back(foo.slice().at(0));
      params.emplace_back(foo.slice().at(1));
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).getDouble() == -1);
      CHECK(res.slice().get("coordinates").at(1).getDouble() == 2.2);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Point");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking two positive integer and negative double values") {
      VPackBuilder foo;
      foo.openArray();
      foo.add(VPackValue(1));
      foo.add(VPackValue(-2.2));
      foo.close();
      params.emplace_back(foo.slice().at(0));
      params.emplace_back(foo.slice().at(1));
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).getDouble() == 1);
      CHECK(res.slice().get("coordinates").at(1).getDouble() == -2.2);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Point");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

  }

  WHEN("Checking wrong combinations of parameters") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{arena};
    fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
      REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    });

    WHEN("checking bool and positive double") {
      VPackBuilder foo;
      foo(VPackValue(VPackValueType::Object))
        ("boolean", VPackValue(true))
        ("coords", VPackValue(VPackValueType::Array))
          (VPackValue(2.2)) ()
      ();

      params.emplace_back(foo.slice().get("boolean"));
      params.emplace_back(foo.slice().get("coords").at(0));
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking null") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "null";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);
      params.emplace_back(json);

      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking string") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "\"hallowelt\"";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);
      params.emplace_back(json);

      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking positive int and bool") {
      VPackBuilder foo;
      foo(VPackValue(VPackValueType::Object))
        ("boolean", VPackValue(true))
        ("coords", VPackValue(VPackValueType::Array))
          (VPackValue(2)) ()
      ();

      params.emplace_back(foo.slice().get("coords").at(0));
      params.emplace_back(foo.slice().get("boolean"));
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking bool and negative double") {
      VPackBuilder foo;
      foo(VPackValue(VPackValueType::Object))
        ("boolean", VPackValue(false))
        ("coords", VPackValue(VPackValueType::Array))
          (VPackValue(-2.2)) ()
      ();

      params.emplace_back(foo.slice().get("boolean"));
      params.emplace_back(foo.slice().get("coords").at(0));
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking positive int and bool") {
      VPackBuilder foo;
      foo(VPackValue(VPackValueType::Object))
        ("boolean", VPackValue(false))
        ("coords", VPackValue(VPackValueType::Array))
          (VPackValue(2)) ()
      ();

      params.emplace_back(foo.slice().get("coords").at(0));
      params.emplace_back(foo.slice().get("boolean"));
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking array and positive double") {
      VPackBuilder foo;
      foo(VPackValue(VPackValueType::Object))
        ("array", VPackValue(VPackValueType::Array))
          (VPackValue(1.0)) (VPackValue(2)) (VPackValue(-3.3)) ()
        ("coords", VPackValue(VPackValueType::Array))
          (VPackValue(2.2)) ()
      ();

      params.emplace_back(foo.slice().get("array"));
      params.emplace_back(foo.slice().get("coords").at(0));
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking negative double and array") {
      VPackBuilder foo;
      foo(VPackValue(VPackValueType::Object))
        ("array", VPackValue(VPackValueType::Array))
          (VPackValue(1.0)) (VPackValue(2)) (VPackValue(-3.3)) ()
        ("coords", VPackValue(VPackValueType::Array))
          (VPackValue(-2.2)) ()
      ();

      params.emplace_back(foo.slice().get("coords").at(0));
      params.emplace_back(foo.slice().get("array"));
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking object and positive double") {
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
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking object and positive double") {
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
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking object and array") {
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
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking array and object") {
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
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking bool and bool") {
      VPackBuilder foo;
      foo(VPackValue(VPackValueType::Object))
        ("boolone", VPackValue(true))
        ("booltwo", VPackValue(false))
      ();

      params.emplace_back(foo.slice().get("boolone"));
      params.emplace_back(foo.slice().get("booltwo"));
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking array and array") {
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
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking object and object") {
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
      AqlValue res = Functions::GeoPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

  }

}
} // geo_point

namespace geo_multipoint {
SCENARIO("Testing GEO_MULTPOINT", "[AQL][GEOC][GEOMULTIPOINT]") {

  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();
  
  WHEN("Checking correct combinations of parameters") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{arena};

    WHEN("checking multipoint with 2 positions") {
      char const* p = "[[1.0, 2.0], [3.0, 4.0]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiPoint(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).isArray());
      CHECK(res.slice().get("coordinates").at(1).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(0).getDouble() == 1.0);
      CHECK(res.slice().get("coordinates").at(0).at(1).getDouble() == 2.0);
      CHECK(res.slice().get("coordinates").at(1).at(0).getDouble() == 3.0);
      CHECK(res.slice().get("coordinates").at(1).at(1).getDouble() == 4.0);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "MultiPoint");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking points representing points in cologne") {
      char const* p = "[[6.888427734375,50.91602169392645],[6.9632720947265625,50.87921050161489],[7.013397216796875,50.89480467658874],[7.0731353759765625,50.92424609910128],[7.093048095703125,50.94804539355076],[7.03948974609375,50.9709677364145],[6.985244750976562,51.000360974529464],[6.8891143798828125,50.996471761616284],[6.867828369140624,50.95669666276118],[6.888427734375,50.91602169392645]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiPoint(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 10);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "MultiPoint");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }
  }

  WHEN("Checking negative combinations of parameters") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{arena};

    WHEN("checking array with 1 position") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[[1.0, 2.0]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking array with positions and invalid bool") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[[1.0, 2.0], [1.0, 2.0], [1.0, 2.0], [1.0, 2.0], false]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking array with positions and invalid bool") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[true, [1.0, 2.0], [1.0, 2.0], [1.0, 2.0], [1.0, 2.0]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking array with 0 position - nested") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[[]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking array with 0 position") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking bool") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_ARRAY_EXPECTED);
      });

      char const* p = "true";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking number") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_ARRAY_EXPECTED);
      });

      char const* p = "123";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
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

    WHEN("checking object") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_ARRAY_EXPECTED);
      });

      char const* p = "{\"Hello\": true, \"Hellox\": 123}";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiPoint(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
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
  }
}
} // geo_multipoint

// LOG_TOPIC(ERR, Logger::FIXME) << "json: " << json.toString();
// LOG_TOPIC(ERR, Logger::FIXME) << "result: " << res.slice().toString();
namespace geo_polygon {
SCENARIO("Testing GEO_POLYGON", "[AQL][GEOC][GEOPOLYGON]") {

  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  WHEN("Checking correct combinations of parameters") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{arena};

    WHEN("checking polygon with 3 positive tupels") {
      char const* p = "[[1.0, 2.0], [3.0, 4.0], [5.0, 6.0]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").at(0).length() == 3);
      CHECK(res.slice().get("coordinates").at(0).at(0).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(1).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(2).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(0).at(0).getDouble() == 1.0);
      CHECK(res.slice().get("coordinates").at(0).at(0).at(1).getDouble() == 2.0);
      CHECK(res.slice().get("coordinates").at(0).at(1).at(0).getDouble() == 3.0);
      CHECK(res.slice().get("coordinates").at(0).at(1).at(1).getDouble() == 4.0);
      CHECK(res.slice().get("coordinates").at(0).at(2).at(0).getDouble() == 5.0);
      CHECK(res.slice().get("coordinates").at(0).at(2).at(1).getDouble() == 6.0);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Polygon");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking polygon representing cologne") {
      char const* p = "[[6.888427734375,50.91602169392645],[6.9632720947265625,50.87921050161489],[7.013397216796875,50.89480467658874],[7.0731353759765625,50.92424609910128],[7.093048095703125,50.94804539355076],[7.03948974609375,50.9709677364145],[6.985244750976562,51.000360974529464],[6.8891143798828125,50.996471761616284],[6.867828369140624,50.95669666276118],[6.888427734375,50.91602169392645]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").at(0).isArray());
      CHECK(res.slice().get("coordinates").at(0).length() == 10);
      CHECK(res.slice().get("coordinates").at(0).at(0).isArray());
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Polygon");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking polygon with 3 negative positions") {
      char const* p = "[[-1.0, -2.0], [-3.0, -4.0], [-5.0, -6.0]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").at(0).isArray());
      CHECK(res.slice().get("coordinates").at(0).length() == 3);
      CHECK(res.slice().get("coordinates").at(0).at(0).length() == 2);
      CHECK(res.slice().get("coordinates").at(0).at(1).length() == 2);
      CHECK(res.slice().get("coordinates").at(0).at(2).length() == 2);
      CHECK(res.slice().get("coordinates").at(0).at(0).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(1).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(2).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(0).at(0).getDouble() == -1.0);
      CHECK(res.slice().get("coordinates").at(0).at(0).at(1).getDouble() == -2.0);
      CHECK(res.slice().get("coordinates").at(0).at(1).at(0).getDouble() == -3.0);
      CHECK(res.slice().get("coordinates").at(0).at(1).at(1).getDouble() == -4.0);
      CHECK(res.slice().get("coordinates").at(0).at(2).at(0).getDouble() == -5.0);
      CHECK(res.slice().get("coordinates").at(0).at(2).at(1).getDouble() == -6.0);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Polygon");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking polygon with 2x3 negative positions") {
      char const* p = "[ [[-1.0, -2.0], [-3.0, -4.0], [-5.0, -6.0]], [[-1.0, -2.0], [-3.0, -4.0], [-5.0, -6.0]] ]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      // TODO check also at 1 position
      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").at(0).isArray());
      CHECK(res.slice().get("coordinates").at(0).length() == 3);
      CHECK(res.slice().get("coordinates").at(1).isArray());
      CHECK(res.slice().get("coordinates").at(1).length() == 3);
      CHECK(res.slice().get("coordinates").at(0).at(0).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(1).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(2).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(0).at(0).getDouble() == -1.0);
      CHECK(res.slice().get("coordinates").at(0).at(0).at(1).getDouble() == -2.0);
      CHECK(res.slice().get("coordinates").at(0).at(1).at(0).getDouble() == -3.0);
      CHECK(res.slice().get("coordinates").at(0).at(1).at(1).getDouble() == -4.0);
      CHECK(res.slice().get("coordinates").at(0).at(2).at(0).getDouble() == -5.0);
      CHECK(res.slice().get("coordinates").at(0).at(2).at(1).getDouble() == -6.0);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Polygon");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }
  }

  WHEN("Checking negative combinations of parameters") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{arena};

    WHEN("checking polygon with 1 positive positions") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[[1.0, 2.0]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking polygon with 1 negative positions") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[[-1.0, -2.0]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking polygon with 2 positive tupel") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[[1.0, 2.0], [3.0, 4.0]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking polygon with 2 negative tuples") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[[-1.0, -2.0], [-3.0, -4.0]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking polygon with empty input") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_ARRAY_EXPECTED);
      });

      
      char const* p = "\"\"";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking polygon with boolean") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      
      char const* p = "[true]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking polygon with booleans") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[true, false]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking polygon with nested booleans") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[[true], [false], [true], [false]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking object with single boolean") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_ARRAY_EXPECTED);
      });

      char const* p = "true";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking object with single number") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_ARRAY_EXPECTED);
      });

      char const* p = "123";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking object with string") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_ARRAY_EXPECTED);
      });

      char const* p = "\"hallowelt\"";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking object with null") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_ARRAY_EXPECTED);
      });

      char const* p = "null";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking object with some data") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_ARRAY_EXPECTED);
      });

      char const* p = "{\"Hello\": true, \"Hellox\": 123}";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoPolygon(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }
  }

}
} // geo_polygon

namespace geo_linestring {
SCENARIO("Testing GEO_LINESTRING", "[AQL][GEOC][GEOLINESTRING]") {

  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();
  
  WHEN("Checking correct combinations of parameters") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{arena};

    WHEN("checking polygon with 2 positions") {
      char const* p = "[[1.0, 2.0], [3.0, 4.0]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoLinestring(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).isArray());
      CHECK(res.slice().get("coordinates").at(1).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(0).getDouble() == 1.0);
      CHECK(res.slice().get("coordinates").at(0).at(1).getDouble() == 2.0);
      CHECK(res.slice().get("coordinates").at(1).at(0).getDouble() == 3.0);
      CHECK(res.slice().get("coordinates").at(1).at(1).getDouble() == 4.0);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "LineString");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking linestring representing cologne") {
      char const* p = "[[6.888427734375,50.91602169392645],[6.9632720947265625,50.87921050161489],[7.013397216796875,50.89480467658874],[7.0731353759765625,50.92424609910128],[7.093048095703125,50.94804539355076],[7.03948974609375,50.9709677364145],[6.985244750976562,51.000360974529464],[6.8891143798828125,50.996471761616284],[6.867828369140624,50.95669666276118],[6.888427734375,50.91602169392645]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoLinestring(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 10);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "LineString");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }
  }

  WHEN("Checking negative combinations of parameters") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{arena};

    WHEN("checking array with 1 position") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[[1.0, 2.0]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoLinestring(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking array with positions and invalid bool") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[[1.0, 2.0], [1.0, 2.0], [1.0, 2.0], [1.0, 2.0], false]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoLinestring(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking array with positions and invalid bool") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[true, [1.0, 2.0], [1.0, 2.0], [1.0, 2.0], [1.0, 2.0]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoLinestring(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking empty nested array") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[[]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoLinestring(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking empty array") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoLinestring(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking bool") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_ARRAY_EXPECTED);
      });

      char const* p = "true";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoLinestring(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking number") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_ARRAY_EXPECTED);
      });

      char const* p = "123";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoLinestring(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking object") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_ARRAY_EXPECTED);
      });

      char const* p = "{\"Hello\": true, \"Hellox\": 123}";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoLinestring(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }
  }
}
} // geo_linestring

namespace geo_multilinestring {
SCENARIO("Testing GEO_MULTILINESTRING", "[AQL][GEOC][GEOMULTILINESTRING]") {

  fakeit::Mock<ExpressionContext> expressionContextMock;
  ExpressionContext& expressionContext = expressionContextMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  WHEN("Checking correct combinations of parameters") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{arena};

    WHEN("checking multilinestrings with 2x2 positions") {
      char const* p = "[ [[1.0, 2.0], [3.0, 4.0]], [[1.0, 2.0], [3.0, 4.0]] ]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiLinestring(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).isArray());
      CHECK(res.slice().get("coordinates").at(1).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(0).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(1).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(0).at(0).getDouble() == 1.0);
      CHECK(res.slice().get("coordinates").at(0).at(0).at(1).getDouble() == 2.0);
      CHECK(res.slice().get("coordinates").at(0).at(1).at(0).getDouble() == 3.0);
      CHECK(res.slice().get("coordinates").at(0).at(1).at(1).getDouble() == 4.0);
      CHECK(res.slice().get("coordinates").at(1).at(0).at(0).getDouble() == 1.0);
      CHECK(res.slice().get("coordinates").at(1).at(0).at(1).getDouble() == 2.0);
      CHECK(res.slice().get("coordinates").at(1).at(1).at(0).getDouble() == 3.0);
      CHECK(res.slice().get("coordinates").at(1).at(1).at(1).getDouble() == 4.0);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "MultiLineString");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking multilinestrings with 2x2 positions") {
      char const* p = "[ [[-1.1, -2.2], [-3.3, -4.4]], [[-1.1, -2.2], [-3.3, -4.4]] ]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiLinestring(&expressionContext, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).isArray());
      CHECK(res.slice().get("coordinates").at(1).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(0).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(1).isArray());
      CHECK(res.slice().get("coordinates").at(0).at(0).at(0).getDouble() == -1.1);
      CHECK(res.slice().get("coordinates").at(0).at(0).at(1).getDouble() == -2.2);
      CHECK(res.slice().get("coordinates").at(0).at(1).at(0).getDouble() == -3.3);
      CHECK(res.slice().get("coordinates").at(0).at(1).at(1).getDouble() == -4.4);
      CHECK(res.slice().get("coordinates").at(1).at(0).at(0).getDouble() == -1.1);
      CHECK(res.slice().get("coordinates").at(1).at(0).at(1).getDouble() == -2.2);
      CHECK(res.slice().get("coordinates").at(1).at(1).at(0).getDouble() == -3.3);
      CHECK(res.slice().get("coordinates").at(1).at(1).at(1).getDouble() == -4.4);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "MultiLineString");
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }
  }

  WHEN("Checking wrong combinations of parameters") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{arena};

    WHEN("checking object") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_ARRAY_EXPECTED);
      });

      char const* p = "{\"Hello\": true, \"Hellox\": 123}";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiLinestring(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking polygon with 0 position - nested") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[[]]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiLinestring(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking polygon with 0 positions") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
      });

      char const* p = "[]";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiLinestring(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking bool") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_ARRAY_EXPECTED);
      });

      char const* p = "true";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiLinestring(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }

    WHEN("checking number") {
      fakeit::When(Method(expressionContextMock, registerWarning)).Do([&](int code, char const* msg) -> void {
        REQUIRE(code == TRI_ERROR_QUERY_ARRAY_EXPECTED);
      });

      char const* p = "123";
      size_t l = strlen(p);

      std::shared_ptr<VPackBuilder> builder = VPackParser::fromJson(p, l);
      VPackSlice json = builder->slice();
      params.emplace_back(json);

      AqlValue res = Functions::GeoMultiLinestring(&expressionContext, &trx, params);
      CHECK(res.slice().isNull());
      res.destroy();
      // Free input parameters
      for (auto& it : params) {
        it.destroy();
      }
    }
  }
}
} // geo_multilinestring

}  // geo_constructors_aql
}  // tests
}  // arangodb
