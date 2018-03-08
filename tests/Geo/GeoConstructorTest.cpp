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
//SCENARIO("Testing GEO_POINT", "[AQL][GEO]") {
SCENARIO("Testing GEO_POINT", "[geotest]") {
  fakeit::Mock<Query> queryMock;
  Query& query = queryMock.get();

  fakeit::Mock<transaction::Methods> trxMock;
  transaction::Methods& trx = trxMock.get();

  WHEN("Checking successful combinations of parameters") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{arena};

    // LOG_TOPIC(ERR, Logger::FIXME) << "HASS 3";
    // LOG_TOPIC(ERR, Logger::FIXME) << "result: " << res.slice().toString();
    WHEN("checking two positive integer values") {
      VPackBuilder foo;
      foo.openArray();
      foo.add(VPackValue(1));
      foo.add(VPackValue(2));
      foo.close();
      params.emplace_back(foo.slice().at(0));
      params.emplace_back(foo.slice().at(1));
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).getDouble() == 1);
      CHECK(res.slice().get("coordinates").at(1).getDouble() == 2);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Point");
    }

    WHEN("checking two negative integer values") {
      VPackBuilder foo;
      foo.openArray();
      foo.add(VPackValue(-1));
      foo.add(VPackValue(-2));
      foo.close();
      params.emplace_back(foo.slice().at(0));
      params.emplace_back(foo.slice().at(1));
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).getDouble() == -1);
      CHECK(res.slice().get("coordinates").at(1).getDouble() == -2);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Point");
    }

    WHEN("checking two positive double values") {
      VPackBuilder foo;
      foo.openArray();
      foo.add(VPackValue(1.1));
      foo.add(VPackValue(2.2));
      foo.close();
      params.emplace_back(foo.slice().at(0));
      params.emplace_back(foo.slice().at(1));
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).getDouble() == 1.1);
      CHECK(res.slice().get("coordinates").at(1).getDouble() == 2.2);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Point");
    }

    WHEN("checking two negative double values") {
      VPackBuilder foo;
      foo.openArray();
      foo.add(VPackValue(-1.1));
      foo.add(VPackValue(-2.2));
      foo.close();
      params.emplace_back(foo.slice().at(0));
      params.emplace_back(foo.slice().at(1));
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).getDouble() == -1.1);
      CHECK(res.slice().get("coordinates").at(1).getDouble() == -2.2);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Point");
    }

    WHEN("checking two positive integer and positive double values") {
      VPackBuilder foo;
      foo.openArray();
      foo.add(VPackValue(1));
      foo.add(VPackValue(2.2));
      foo.close();
      params.emplace_back(foo.slice().at(0));
      params.emplace_back(foo.slice().at(1));
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).getDouble() == 1);
      CHECK(res.slice().get("coordinates").at(1).getDouble() == 2.2);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Point");
    }

    WHEN("checking two negative integer and positive double values") {
      VPackBuilder foo;
      foo.openArray();
      foo.add(VPackValue(-1));
      foo.add(VPackValue(2.2));
      foo.close();
      params.emplace_back(foo.slice().at(0));
      params.emplace_back(foo.slice().at(1));
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).getDouble() == -1);
      CHECK(res.slice().get("coordinates").at(1).getDouble() == 2.2);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Point");
    }

    WHEN("checking two positive integer and negative double values") {
      VPackBuilder foo;
      foo.openArray();
      foo.add(VPackValue(1));
      foo.add(VPackValue(-2.2));
      foo.close();
      params.emplace_back(foo.slice().at(0));
      params.emplace_back(foo.slice().at(1));
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.isObject());
      CHECK(res.slice().get("coordinates").isArray());
      CHECK(res.slice().get("coordinates").length() == 2);
      CHECK(res.slice().get("coordinates").at(0).getDouble() == 1);
      CHECK(res.slice().get("coordinates").at(1).getDouble() == -2.2);
      CHECK(res.slice().get("type").isString());
      CHECK(res.slice().get("type").copyString() == "Point");
    }

  }

  // LOG_TOPIC(ERR, Logger::FIXME) << "result: " << res.slice().toString();
  WHEN("Checking wrong combinations of parameters") {
    SmallVector<AqlValue>::allocator_type::arena_type arena;
    SmallVector<AqlValue> params{arena};
    fakeit::When(Method(queryMock, registerWarning)).Do([&](int code, char const* msg) -> void {
      REQUIRE(code==1542);
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
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.slice().isNull());
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
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.slice().isNull());
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
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.slice().isNull());
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
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.slice().isNull());
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
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.slice().isNull());
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
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.slice().isNull());
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
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.slice().isNull());
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
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.slice().isNull());
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
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.slice().isNull());
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
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.slice().isNull());
    }

    WHEN("checking bool and bool") {
      VPackBuilder foo;
      foo(VPackValue(VPackValueType::Object))
        ("boolone", VPackValue(true))
        ("booltwo", VPackValue(false))
      ();

      params.emplace_back(foo.slice().get("boolone"));
      params.emplace_back(foo.slice().get("booltwo"));
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.slice().isNull());
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
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.slice().isNull());
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
      AqlValue res = Functions::GeoPoint(&query, &trx, params);
      CHECK(res.slice().isNull());
    }

  }

}
} // geo_point


}  // geo_constructors_aql
}  // tests
}  // arangodb
