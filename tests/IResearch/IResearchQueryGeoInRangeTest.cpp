////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <absl/strings/str_replace.h>

#include <s2/s2latlng.h>

#include "Geo/GeoJson.h"
#include "Geo/ShapeContainer.h"
#include "IResearch/IResearchView.h"
#include "IResearch/MakeViewSnapshot.h"
#include "IResearchQueryCommon.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb::tests {
namespace {

std::vector<VPackSlice> const empty;

class QueryGeoInRange : public QueryTest {
 protected:
  void createAnalyzers(std::string_view analyzer,
                       std::string_view params = {}) {
    auto& analyzers = server.getFeature<iresearch::IResearchAnalyzerFeature>();
    iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    {
      auto json = VPackParser::fromJson(
          absl::Substitute(R"({$0 "type": "shape"})", params));
      auto r = analyzers.emplace(
          result, _vocbase.name() + "::mygeojson", analyzer, json->slice(),
          arangodb::transaction::OperationOriginTestCase{});
      ASSERT_TRUE(r.ok()) << r.errorMessage();
    }
    {
      auto json = VPackParser::fromJson(
          absl::Substitute(R"({$0 "type": "centroid"})", params));
      auto r = analyzers.emplace(
          result, _vocbase.name() + "::mygeocentroid", analyzer, json->slice(),
          arangodb::transaction::OperationOriginTestCase{});
      ASSERT_TRUE(r.ok()) << r.errorMessage();
    }
    {
      auto json = VPackParser::fromJson(
          absl::Substitute(R"({$0 "type": "point"})", params));
      auto r = analyzers.emplace(
          result, _vocbase.name() + "::mygeopoint", analyzer, json->slice(),
          arangodb::transaction::OperationOriginTestCase{});
      ASSERT_TRUE(r.ok()) << r.errorMessage();
    }
  }

  void createAnalyzers2() {
    auto& analyzers = server.getFeature<iresearch::IResearchAnalyzerFeature>();
    iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto json = VPackParser::fromJson(R"({})");
    ASSERT_TRUE(analyzers
                    .emplace(result, _vocbase.name() + "::mygeopoint",
                             "geopoint", json->slice(),
                             arangodb::transaction::OperationOriginTestCase{})
                    .ok());
  }

  void createAnalyzers3() {
    auto& analyzers = server.getFeature<iresearch::IResearchAnalyzerFeature>();
    iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto json = VPackParser::fromJson(R"({
      "latitude" : ["coordinates", "lat" ],
      "longitude": ["coordinates", "lon" ]
    })");
    ASSERT_TRUE(analyzers
                    .emplace(result, _vocbase.name() + "::mygeopoint",
                             "geopoint", json->slice(),
                             arangodb::transaction::OperationOriginTestCase{})
                    .ok());
  }

  void createCollections() {
    auto createJson = VPackParser::fromJson(R"({ "name": "testCollection0" })");
    auto collection = _vocbase.createCollection(createJson->slice());
    ASSERT_TRUE(collection);
  }

  void queryTests() {
    // populate collection
    {
      auto collection = _vocbase.lookupCollection("testCollection0");
      ASSERT_TRUE(collection);
      auto docs = VPackParser::fromJson(R"([
        { "id": 1,  "geometry": { "type": "Point", "coordinates": [ 37.615895, 55.7039   ] } },
        { "id": 2,  "geometry": { "type": "Point", "coordinates": [ 37.615315, 55.703915 ] } },
        { "id": 3,  "geometry": { "type": "Point", "coordinates": [ 37.61509, 55.703537  ] } },
        { "id": 4,  "geometry": { "type": "Point", "coordinates": [ 37.614183, 55.703806 ] } },
        { "id": 5,  "geometry": { "type": "Point", "coordinates": [ 37.613792, 55.704405 ] } },
        { "id": 6,  "geometry": { "type": "Point", "coordinates": [ 37.614956, 55.704695 ] } },
        { "id": 7,  "geometry": { "type": "Point", "coordinates": [ 37.616297, 55.704831 ] } },
        { "id": 8,  "geometry": { "type": "Point", "coordinates": [ 37.617053, 55.70461  ] } },
        { "id": 9,  "geometry": { "type": "Point", "coordinates": [ 37.61582, 55.704459  ] } },
        { "id": 10, "geometry": { "type": "Point", "coordinates": [ 37.614634, 55.704338 ] } },
        { "id": 11, "geometry": { "type": "Point", "coordinates": [ 37.613121, 55.704193 ] } },
        { "id": 12, "geometry": { "type": "Point", "coordinates": [ 37.614135, 55.703298 ] } },
        { "id": 13, "geometry": { "type": "Point", "coordinates": [ 37.613663, 55.704002 ] } },
        { "id": 14, "geometry": { "type": "Point", "coordinates": [ 37.616522, 55.704235 ] } },
        { "id": 15, "geometry": { "type": "Point", "coordinates": [ 37.615508, 55.704172 ] } },
        { "id": 16, "geometry": { "type": "Point", "coordinates": [ 37.614629, 55.704081 ] } },
        { "id": 17, "geometry": { "type": "Point", "coordinates": [ 37.610235, 55.709754 ] } },
        { "id": 18, "geometry": { "type": "Point", "coordinates": [ 37.605,    55.707917 ] } },
        { "id": 19, "geometry": { "type": "Point", "coordinates": [ 37.545776, 55.722083 ] } },
        { "id": 20, "geometry": { "type": "Point", "coordinates": [ 37.559509, 55.715895 ] } },
        { "id": 21, "geometry": { "type": "Point", "coordinates": [ 37.701645, 55.832144 ] } },
        { "id": 22, "geometry": { "type": "Point", "coordinates": [ 37.73735,  55.816715 ] } },
        { "id": 23, "geometry": { "type": "Point", "coordinates": [ 37.75589,  55.798193 ] } },
        { "id": 24, "geometry": { "type": "Point", "coordinates": [ 37.659073, 55.843711 ] } },
        { "id": 25, "geometry": { "type": "Point", "coordinates": [ 37.778549, 55.823659 ] } },
        { "id": 26, "geometry": { "type": "Point", "coordinates": [ 37.729797, 55.853733 ] } },
        { "id": 27, "geometry": { "type": "Point", "coordinates": [ 37.608261, 55.784682 ] } },
        { "id": 28, "geometry": { "type": "Point", "coordinates": [ 37.525177, 55.802825 ] } }
      ])");

      OperationOptions options;
      options.returnNew = true;
      SingleCollectionTransaction trx(
          transaction::StandaloneContext::create(
              _vocbase, arangodb::transaction::OperationOriginTestCase{}),
          *collection, AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (auto doc : VPackArrayIterator(docs->slice())) {
        auto res = trx.insert(collection->name(), doc, options);
        EXPECT_TRUE(res.ok());
        _insertedDocs.emplace_back(res.slice().get("new"));
      }

      EXPECT_TRUE(trx.commit().ok());

      // sync view
      ASSERT_TRUE(
          executeQuery(
              _vocbase,
              "FOR d IN testView OPTIONS { waitForSync: true } RETURN d")
              .result.ok());
    }
    // EXISTS will also work
    {
      EXPECT_TRUE(runQuery(R"(FOR d IN testView
        SEARCH EXISTS(d.geometry)
        RETURN d)"));
    }
    // EXISTS will also work
    if (type() == ViewType::kArangoSearch) {
      EXPECT_TRUE(runQuery(
          R"(FOR d IN testView SEARCH EXISTS(d.geometry, 'string') RETURN d)"));
    } else if (type() == ViewType::kSearchAlias) {
      // Because for search/inverted-index
      // we consider strings can be found as normal fields,
      // so them all have suffix \0_s,
      // but geo analyzer fields can be handled by geo functions or analyzer
      EXPECT_TRUE(runQuery(
          R"(FOR d IN testView SEARCH EXISTS(d.geometry, 'string') RETURN d)",
          empty));
    } else {
      ASSERT_TRUE(false);
    }
    // test missing analyzer
    {
      static constexpr std::string_view kQueryStr =
          R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH GEO_IN_RANGE(d.geometry, origin, 0, 300)
        SORT d.id ASC
        RETURN d)";
      if (type() == ViewType::kSearchAlias) {
        auto expected =
            std::vector{_insertedDocs[16].slice(), _insertedDocs[17].slice()};
        EXPECT_TRUE(runQuery(kQueryStr, expected)) << kQueryStr;
      } else {
        auto r = executeQuery(_vocbase, std::string{kQueryStr});
        EXPECT_EQ(r.result.errorNumber(), TRI_ERROR_BAD_PARAMETER) << kQueryStr;
      }
    }
    // test missing analyzer
    {
      static constexpr std::string_view kQueryStr =
          R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH GEO_IN_RANGE(origin, d.geometry, 0, 300)
        SORT d.id ASC
        RETURN d)";
      if (type() == ViewType::kSearchAlias) {
        auto expected =
            std::vector{_insertedDocs[16].slice(), _insertedDocs[17].slice()};
        EXPECT_TRUE(runQuery(kQueryStr, expected)) << kQueryStr;
      } else {
        auto r = executeQuery(_vocbase, std::string{kQueryStr});
        EXPECT_EQ(r.result.errorNumber(), TRI_ERROR_BAD_PARAMETER) << kQueryStr;
      }
    }
  }

  void queryTestsGeoJson(bool isVPack) {
    // ensure presence of special a column for geo indices
    {
      auto collection = _vocbase.lookupCollection("testCollection0");
      ASSERT_TRUE(collection);
      auto view = _vocbase.lookupView("testView");
      ASSERT_TRUE(view);
      auto links = [&] {
        if (view->type() == ViewType::kSearchAlias) {
          auto& impl = basics::downCast<iresearch::Search>(*view);
          return impl.getLinks(nullptr);
        }
        auto& impl = basics::downCast<iresearch::IResearchView>(*view);
        return impl.getLinks(nullptr);
      };
      SingleCollectionTransaction trx(
          transaction::StandaloneContext::create(
              _vocbase, arangodb::transaction::OperationOriginTestCase{}),
          *collection, AccessMode::Type::READ);
      ASSERT_TRUE(trx.begin().ok());
      ASSERT_TRUE(trx.state());
      auto* snapshot =
          makeViewSnapshot(trx, iresearch::ViewSnapshotMode::FindOrCreate,
                           links(), view.get(), view->name());
      ASSERT_NE(nullptr, snapshot);
      ASSERT_EQ(1U, snapshot->size());
      ASSERT_EQ(_insertedDocs.size(), snapshot->docs_count());
      ASSERT_EQ(_insertedDocs.size(), snapshot->live_docs_count());

      if (isVPack) {
        auto& segment = (*snapshot)[0];

        auto const columnName = mangleString("geometry", "mygeojson");
        auto* columnReader = segment.column(columnName);
        ASSERT_NE(nullptr, columnReader);
        auto it = columnReader->iterator(irs::ColumnHint::kNormal);
        ASSERT_NE(nullptr, it);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);

        auto doc = _insertedDocs.begin();
        for (; it->next(); ++doc) {
          EXPECT_EQUAL_SLICES(doc->slice().get("geometry"),
                              iresearch::slice(payload->value));
        }
      }

      ASSERT_TRUE(trx.commit().ok());
    }
    // EXISTS will also work
    {
      EXPECT_TRUE(runQuery(R"(FOR d IN testView
        SEARCH EXISTS(d.geometry, 'analyzer', "mygeojson")
        RETURN d)"));
    }
    // test missing field
    if (type() == ViewType::kArangoSearch) {  // TODO kSearch check error
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(d.missing, origin, 0, 300), 'mygeojson')
        SORT d.id ASC
        RETURN d)",
                           empty));
    }
    // test missing field
    if (type() == ViewType::kArangoSearch) {  // TODO kSearch check error
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.missing, 0, 300), 'mygeojson')
        SORT d.id ASC
        RETURN d)",
                           empty));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[16].slice(),
                                          _insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(d.geometry, origin, 0, 300), 'mygeojson')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[16].slice(),
                                          _insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 300), 'mygeojson')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[12].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.613663, 55.704002)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 0), 'mygeojson')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.613663, 55.704002)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 0, false, false), 'mygeojson')
        SORT d.id ASC
        RETURN d)",
                           empty));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[16].slice(),
                                          _insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(d.geometry, origin, 100, 300), 'mygeojson')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[16].slice(),
                                          _insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 100, 300), 'mygeojson')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 206, 207), 'mygeojson')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(d.geometry, origin, 206, 207), 'mygeojson')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected{
          _insertedDocs[23].slice(),
          _insertedDocs[24].slice(),
          _insertedDocs[25].slice(),
      };
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 15000, 20000), 'mygeojson')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
  }

  void queryTestsGeoCentroid() {
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[16].slice(),
                                          _insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 300), 'mygeocentroid')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
  }

  void queryTestsGeoPoint() {
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[16].slice(),
                                          _insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 300), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
  }

  void queryTestsMulti() {}

  void queryTests2() {
    auto collection = _vocbase.lookupCollection("testCollection0");
    ASSERT_TRUE(collection);
    // populate collection
    {
      auto docs = VPackParser::fromJson(R"([
        { "id": 1,  "geometry": { "type": "Point", "coordinates": [55.7039   , 37.615895  ] } },
        { "id": 2,  "geometry": { "type": "Point", "coordinates": [55.703915 , 37.615315  ] } },
        { "id": 3,  "geometry": { "type": "Point", "coordinates": [55.703537 , 37.61509    ] } },
        { "id": 4,  "geometry": { "type": "Point", "coordinates": [55.703806 , 37.614183  ] } },
        { "id": 5,  "geometry": { "type": "Point", "coordinates": [55.704405 , 37.613792  ] } },
        { "id": 6,  "geometry": { "type": "Point", "coordinates": [55.704695 , 37.614956  ] } },
        { "id": 7,  "geometry": { "type": "Point", "coordinates": [55.704831 , 37.616297  ] } },
        { "id": 8,  "geometry": { "type": "Point", "coordinates": [55.70461  , 37.617053  ] } },
        { "id": 9,  "geometry": { "type": "Point", "coordinates": [55.704459 , 37.61582    ] } },
        { "id": 10, "geometry": { "type": "Point", "coordinates": [55.704338 , 37.614634  ] } },
        { "id": 11, "geometry": { "type": "Point", "coordinates": [55.704193 , 37.613121  ] } },
        { "id": 12, "geometry": { "type": "Point", "coordinates": [55.703298 , 37.614135  ] } },
        { "id": 13, "geometry": { "type": "Point", "coordinates": [55.704002 , 37.613663  ] } },
        { "id": 14, "geometry": { "type": "Point", "coordinates": [55.704235 , 37.616522  ] } },
        { "id": 15, "geometry": { "type": "Point", "coordinates": [55.704172 , 37.615508  ] } },
        { "id": 16, "geometry": { "type": "Point", "coordinates": [55.704081 , 37.614629  ] } },
        { "id": 17, "geometry": { "type": "Point", "coordinates": [55.709754 , 37.610235  ] } },
        { "id": 18, "geometry": { "type": "Point", "coordinates": [55.707917 , 37.605     ] } },
        { "id": 19, "geometry": { "type": "Point", "coordinates": [55.722083 , 37.545776  ] } },
        { "id": 20, "geometry": { "type": "Point", "coordinates": [55.715895 , 37.559509  ] } },
        { "id": 21, "geometry": { "type": "Point", "coordinates": [55.832144 , 37.701645  ] } },
        { "id": 22, "geometry": { "type": "Point", "coordinates": [55.816715 , 37.73735   ] } },
        { "id": 23, "geometry": { "type": "Point", "coordinates": [55.798193 , 37.75589   ] } },
        { "id": 24, "geometry": { "type": "Point", "coordinates": [55.843711 , 37.659073  ] } },
        { "id": 25, "geometry": { "type": "Point", "coordinates": [55.823659 , 37.778549  ] } },
        { "id": 26, "geometry": { "type": "Point", "coordinates": [55.853733 , 37.729797  ] } },
        { "id": 27, "geometry": { "type": "Point", "coordinates": [55.784682 , 37.608261  ] } },
        { "id": 28, "geometry": { "type": "Point", "coordinates": [55.802825 , 37.525177  ] } }
      ])");

      OperationOptions options;
      options.returnNew = true;
      SingleCollectionTransaction trx(
          transaction::StandaloneContext::create(
              _vocbase, arangodb::transaction::OperationOriginTestCase{}),
          *collection, AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (auto doc : VPackArrayIterator(docs->slice())) {
        auto res = trx.insert(collection->name(), doc, options);
        EXPECT_TRUE(res.ok());
        _insertedDocs.emplace_back(res.slice().get("new"));
      }

      EXPECT_TRUE(trx.commit().ok());

      // sync view
      ASSERT_TRUE(
          executeQuery(
              _vocbase,
              "FOR d IN testView OPTIONS { waitForSync: true } RETURN d")
              .result.ok());
    }
    auto view = _vocbase.lookupView("testView");
    ASSERT_TRUE(view);
    auto links = [&] {
      if (view->type() == ViewType::kSearchAlias) {
        auto& impl = basics::downCast<iresearch::Search>(*view);
        return impl.getLinks(nullptr);
      }
      auto& impl = basics::downCast<iresearch::IResearchView>(*view);
      return impl.getLinks(nullptr);
    };
    // ensure presence of special a column for geo indices
    {
      SingleCollectionTransaction trx(
          transaction::StandaloneContext::create(
              _vocbase, arangodb::transaction::OperationOriginTestCase{}),
          *collection, AccessMode::Type::READ);
      ASSERT_TRUE(trx.begin().ok());
      ASSERT_TRUE(trx.state());
      auto* snapshot =
          makeViewSnapshot(trx, iresearch::ViewSnapshotMode::FindOrCreate,
                           links(), view.get(), view->name());
      ASSERT_NE(nullptr, snapshot);
      ASSERT_EQ(1U, snapshot->size());
      ASSERT_EQ(_insertedDocs.size(), snapshot->docs_count());
      ASSERT_EQ(_insertedDocs.size(), snapshot->live_docs_count());

      auto& segment = (*snapshot)[0];

      auto const columnName =
          mangleString("geometry.coordinates", "mygeopoint");
      auto* columnReader = segment.column(columnName);
      ASSERT_NE(nullptr, columnReader);
      auto it = columnReader->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, it);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);

      auto doc = _insertedDocs.begin();
      for (; it->next(); ++doc) {
        auto const storedValue = iresearch::slice(payload->value);
        ASSERT_TRUE(storedValue.isArray());
        ASSERT_EQ(2U, storedValue.length());
        EXPECT_DOUBLE_EQ(
            storedValue.at(0).getDouble(),
            doc->slice().get({"geometry", "coordinates"}).at(1).getDouble());
        EXPECT_DOUBLE_EQ(
            storedValue.at(1).getDouble(),
            doc->slice().get({"geometry", "coordinates"}).at(0).getDouble());
      }

      ASSERT_TRUE(trx.commit().ok());
    }
    if (type() == ViewType::kArangoSearch) {  // TODO kSearch check error
      EXPECT_TRUE(runQuery(R"(FOR d IN testView
        SEARCH EXISTS(d.geometry)
        RETURN d)"));
    }
    // EXISTS will also work
    {
      EXPECT_TRUE(runQuery(R"(FOR d IN testView
        SEARCH EXISTS(d.geometry.coordinates)
        RETURN d)"));
    }
    // EXISTS will also work
    if (type() == ViewType::kArangoSearch) {
      EXPECT_TRUE(runQuery(R"(FOR d IN testView
        SEARCH EXISTS(d.geometry.coordinates, 'string') RETURN d)"));
    } else if (type() == ViewType::kSearchAlias) {
      // Because for search/inverted-index
      // we consider strings can be found as normal fields,
      // so them all have suffix \0_s,
      // but geo analyzer fields can be handled by geo functions or analyzer
      EXPECT_TRUE(runQuery(R"(FOR d IN testView
        SEARCH EXISTS(d.geometry.coordinates, 'string') RETURN d)",
                           empty));
    } else {
      ASSERT_TRUE(false);
    }
    // EXISTS will also work
    {
      EXPECT_TRUE(runQuery(R"(FOR d IN testView
        SEARCH EXISTS(d.geometry.coordinates, 'analyzer', "mygeopoint")
        RETURN d)"));
    }
    // test missing field
    if (type() == ViewType::kArangoSearch) {  // TODO kSearch check error
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(d.missing, origin, 0, 300), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           empty));
    }
    // test missing field
    if (type() == ViewType::kArangoSearch) {  // TODO kSearch check error
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.missing, 0, 300), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           empty));
    }
    // test missing analyzer
    {
      static constexpr std::string_view kQueryStr =
          R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH GEO_IN_RANGE(d.geometry.coordinates, origin, 0, 300)
        SORT d.id ASC
        RETURN d)";
      if (type() == ViewType::kSearchAlias) {
        auto expected =
            std::vector{_insertedDocs[16].slice(), _insertedDocs[17].slice()};
        EXPECT_TRUE(runQuery(kQueryStr, expected)) << kQueryStr;
      } else {
        auto r = executeQuery(_vocbase, std::string{kQueryStr});
        EXPECT_EQ(r.result.errorNumber(), TRI_ERROR_BAD_PARAMETER) << kQueryStr;
      }
    }
    // test missing analyzer
    {
      static constexpr std::string_view kQueryStr =
          R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH GEO_IN_RANGE(origin, d.geometry.coordinates, 0, 300)
        SORT d.id ASC
        RETURN d)";
      if (type() == ViewType::kSearchAlias) {
        auto expected =
            std::vector{_insertedDocs[16].slice(), _insertedDocs[17].slice()};
        EXPECT_TRUE(runQuery(kQueryStr, expected)) << kQueryStr;
      } else {
        auto r = executeQuery(_vocbase, std::string{kQueryStr});
        EXPECT_EQ(r.result.errorNumber(), TRI_ERROR_BAD_PARAMETER) << kQueryStr;
      }
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[16].slice(),
                                          _insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(d.geometry.coordinates, origin, 0, 300), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[16].slice(),
                                          _insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry.coordinates, 0, 300), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[16].slice(),
                                          _insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry.coordinates, 0, 300), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[12].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.613663, 55.704002)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry.coordinates, 0, 0), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.613663, 55.704002)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry.coordinates, 0, 0, false, false), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           empty));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[16].slice(),
                                          _insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(d.geometry.coordinates, origin, 100, 300), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[16].slice(),
                                          _insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry.coordinates, 100, 300), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry.coordinates, 206, 207), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(d.geometry.coordinates, origin, 206, 207), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected{
          _insertedDocs[23].slice(),
          _insertedDocs[24].slice(),
          _insertedDocs[25].slice(),
      };
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry.coordinates, 15000, 20000), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
  }

  void queryTests3() {
    auto collection = _vocbase.lookupCollection("testCollection0");
    ASSERT_TRUE(collection);
    // populate collection
    {
      auto docs = VPackParser::fromJson(R"([
        { "id": 1,  "geometry": { "type": "Point", "coordinates": { "lat": 55.7039  , "lon" : 37.615895  } } },
        { "id": 2,  "geometry": { "type": "Point", "coordinates": { "lat": 55.703915, "lon" : 37.615315  } } },
        { "id": 3,  "geometry": { "type": "Point", "coordinates": { "lat": 55.703537, "lon" : 37.61509   } } },
        { "id": 4,  "geometry": { "type": "Point", "coordinates": { "lat": 55.703806, "lon" : 37.614183  } } },
        { "id": 5,  "geometry": { "type": "Point", "coordinates": { "lat": 55.704405, "lon" : 37.613792  } } },
        { "id": 6,  "geometry": { "type": "Point", "coordinates": { "lat": 55.704695, "lon" : 37.614956  } } },
        { "id": 7,  "geometry": { "type": "Point", "coordinates": { "lat": 55.704831, "lon" : 37.616297  } } },
        { "id": 8,  "geometry": { "type": "Point", "coordinates": { "lat": 55.70461 , "lon" : 37.617053  } } },
        { "id": 9,  "geometry": { "type": "Point", "coordinates": { "lat": 55.704459, "lon" : 37.61582   } } },
        { "id": 10, "geometry": { "type": "Point", "coordinates": { "lat": 55.704338, "lon" : 37.614634  } } },
        { "id": 11, "geometry": { "type": "Point", "coordinates": { "lat": 55.704193, "lon" : 37.613121  } } },
        { "id": 12, "geometry": { "type": "Point", "coordinates": { "lat": 55.703298, "lon" : 37.614135  } } },
        { "id": 13, "geometry": { "type": "Point", "coordinates": { "lat": 55.704002, "lon" : 37.613663  } } },
        { "id": 14, "geometry": { "type": "Point", "coordinates": { "lat": 55.704235, "lon" : 37.616522  } } },
        { "id": 15, "geometry": { "type": "Point", "coordinates": { "lat": 55.704172, "lon" : 37.615508  } } },
        { "id": 16, "geometry": { "type": "Point", "coordinates": { "lat": 55.704081, "lon" : 37.614629  } } },
        { "id": 17, "geometry": { "type": "Point", "coordinates": { "lat": 55.709754, "lon" : 37.610235  } } },
        { "id": 18, "geometry": { "type": "Point", "coordinates": { "lat": 55.707917, "lon" : 37.605     } } },
        { "id": 19, "geometry": { "type": "Point", "coordinates": { "lat": 55.722083, "lon" : 37.545776  } } },
        { "id": 20, "geometry": { "type": "Point", "coordinates": { "lat": 55.715895, "lon" : 37.559509  } } },
        { "id": 21, "geometry": { "type": "Point", "coordinates": { "lat": 55.832144, "lon" : 37.701645  } } },
        { "id": 22, "geometry": { "type": "Point", "coordinates": { "lat": 55.816715, "lon" : 37.73735   } } },
        { "id": 23, "geometry": { "type": "Point", "coordinates": { "lat": 55.798193, "lon" : 37.75589   } } },
        { "id": 24, "geometry": { "type": "Point", "coordinates": { "lat": 55.843711, "lon" : 37.659073  } } },
        { "id": 25, "geometry": { "type": "Point", "coordinates": { "lat": 55.823659, "lon" : 37.778549  } } },
        { "id": 26, "geometry": { "type": "Point", "coordinates": { "lat": 55.853733, "lon" : 37.729797  } } },
        { "id": 27, "geometry": { "type": "Point", "coordinates": { "lat": 55.784682, "lon" : 37.608261  } } },
        { "id": 28, "geometry": { "type": "Point", "coordinates": { "lat": 55.802825, "lon" : 37.525177  } } }
      ])");

      OperationOptions options;
      options.returnNew = true;
      SingleCollectionTransaction trx(
          transaction::StandaloneContext::create(
              _vocbase, arangodb::transaction::OperationOriginTestCase{}),
          *collection, AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (auto doc : VPackArrayIterator(docs->slice())) {
        auto res = trx.insert(collection->name(), doc, options);
        EXPECT_TRUE(res.ok());
        _insertedDocs.emplace_back(res.slice().get("new"));
      }

      EXPECT_TRUE(trx.commit().ok());

      // sync view
      ASSERT_TRUE(
          executeQuery(
              _vocbase,
              "FOR d IN testView OPTIONS { waitForSync: true } RETURN d")
              .result.ok());
    }
    auto view = _vocbase.lookupView("testView");
    ASSERT_TRUE(view);
    auto links = [&] {
      if (view->type() == ViewType::kSearchAlias) {
        auto& impl = basics::downCast<iresearch::Search>(*view);
        return impl.getLinks(nullptr);
      }
      auto& impl = basics::downCast<iresearch::IResearchView>(*view);
      return impl.getLinks(nullptr);
    };
    // ensure presence of special a column for geo indices
    {
      SingleCollectionTransaction trx(
          transaction::StandaloneContext::create(
              _vocbase, arangodb::transaction::OperationOriginTestCase{}),
          *collection, AccessMode::Type::READ);
      ASSERT_TRUE(trx.begin().ok());
      ASSERT_TRUE(trx.state());
      auto* snapshot =
          makeViewSnapshot(trx, iresearch::ViewSnapshotMode::FindOrCreate,
                           links(), view.get(), view->name());
      ASSERT_NE(nullptr, snapshot);
      ASSERT_EQ(1U, snapshot->size());
      ASSERT_EQ(_insertedDocs.size(), snapshot->docs_count());
      ASSERT_EQ(_insertedDocs.size(), snapshot->live_docs_count());

      auto& segment = (*snapshot)[0];

      auto const columnName = mangleString("geometry", "mygeopoint");
      auto* columnReader = segment.column(columnName);
      ASSERT_NE(nullptr, columnReader);
      auto it = columnReader->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, it);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);

      auto doc = _insertedDocs.begin();
      for (; it->next(); ++doc) {
        auto const storedValue = iresearch::slice(payload->value);
        ASSERT_TRUE(storedValue.isArray());
        ASSERT_EQ(2U, storedValue.length());
        EXPECT_DOUBLE_EQ(
            storedValue.at(0).getDouble(),
            doc->slice().get({"geometry", "coordinates", "lon"}).getDouble());
        EXPECT_DOUBLE_EQ(
            storedValue.at(1).getDouble(),
            doc->slice().get({"geometry", "coordinates", "lat"}).getDouble());
      }

      ASSERT_TRUE(trx.commit().ok());
    }
    // EXISTS will also work
    {
      EXPECT_TRUE(runQuery(R"(FOR d IN testView
        SEARCH EXISTS(d.geometry)
        RETURN d)"));
    }
    // EXISTS will also work
    if (type() == ViewType::kArangoSearch) {
      EXPECT_TRUE(runQuery(
          R"(FOR d IN testView SEARCH EXISTS(d.geometry, 'string') RETURN d)"));
    } else if (type() == ViewType::kSearchAlias) {
      // Because for search/inverted-index
      // we consider strings can be found as normal fields,
      // so them all have suffix \0_s,
      // but geo analyzer fields can be handled by geo functions or analyzer
      EXPECT_TRUE(runQuery(
          R"(FOR d IN testView SEARCH EXISTS(d.geometry, 'string') RETURN d)",
          empty));
    } else {
      ASSERT_TRUE(false);
    }
    // EXISTS will also work
    {
      EXPECT_TRUE(runQuery(R"(FOR d IN testView
        SEARCH EXISTS(d.geometry, 'analyzer', "mygeopoint")
        RETURN d)"));
    }
    // test missing field
    if (type() == ViewType::kArangoSearch) {  // TODO kSearch check error
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(d.missing, origin, 0, 300), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           empty));
    }
    // test missing field
    if (type() == ViewType::kArangoSearch) {  // TODO kSearch check error
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.missing, 0, 300), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           empty));
    }
    // test missing analyzer
    {
      static constexpr std::string_view kQueryStr =
          R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH GEO_IN_RANGE(d.geometry, origin, 0, 300)
        SORT d.id ASC
        RETURN d)";
      if (type() == ViewType::kSearchAlias) {
        auto expected =
            std::vector{_insertedDocs[16].slice(), _insertedDocs[17].slice()};
        EXPECT_TRUE(runQuery(kQueryStr, expected)) << kQueryStr;
      } else {
        auto r = executeQuery(_vocbase, std::string{kQueryStr});
        EXPECT_EQ(r.result.errorNumber(), TRI_ERROR_BAD_PARAMETER) << kQueryStr;
      }
    }
    // test missing analyzer
    {
      static constexpr std::string_view kQueryStr =
          R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH GEO_IN_RANGE(origin, d.geometry, 0, 300)
        SORT d.id ASC
        RETURN d)";
      if (type() == ViewType::kSearchAlias) {
        auto expected =
            std::vector{_insertedDocs[16].slice(), _insertedDocs[17].slice()};
        EXPECT_TRUE(runQuery(kQueryStr, expected)) << kQueryStr;
      } else {
        auto r = executeQuery(_vocbase, std::string{kQueryStr});
        EXPECT_EQ(r.result.errorNumber(), TRI_ERROR_BAD_PARAMETER) << kQueryStr;
      }
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[16].slice(),
                                          _insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(d.geometry, origin, 0, 300), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[16].slice(),
                                          _insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 300), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[16].slice(),
                                          _insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 300), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[12].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.613663, 55.704002)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 0), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.613663, 55.704002)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 0, false, false), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           empty));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[16].slice(),
                                          _insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(d.geometry, origin, 100, 300), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[16].slice(),
                                          _insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 100, 300), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 206, 207), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected = {_insertedDocs[17].slice()};
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(d.geometry, origin, 206, 207), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
    //
    {
      std::vector<VPackSlice> expected{
          _insertedDocs[23].slice(),
          _insertedDocs[24].slice(),
          _insertedDocs[25].slice(),
      };
      EXPECT_TRUE(runQuery(R"(LET origin = GEO_POINT(37.607768, 55.70892)
        FOR d IN testView
        SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 15000, 20000), 'mygeopoint')
        SORT d.id ASC
        RETURN d)",
                           expected));
    }
  }
};

class QueryGeoInRangeView : public QueryGeoInRange {
 protected:
  ViewType type() const final { return ViewType::kArangoSearch; }

  void createView(std::string_view fields) {
    auto createJson = VPackParser::fromJson(
        R"({ "name": "testView", "type": "arangosearch" })");
    auto logicalView = _vocbase.createView(createJson->slice(), false);
    ASSERT_FALSE(!logicalView);
    auto& implView = basics::downCast<iresearch::IResearchView>(*logicalView);
    auto updateJson =
        VPackParser::fromJson(absl::Substitute(R"({ "links": {
          "testCollection0": {
            "fields" : $1,
            "version": $0 } } })",
                                               version(), fields));
    auto r = implView.properties(updateJson->slice(), true, true);
    EXPECT_TRUE(r.ok()) << r.errorMessage();
    checkView(implView, 1);
  }
};

class QueryGeoInRangeSearch : public QueryGeoInRange {
 protected:
  ViewType type() const final { return ViewType::kSearchAlias; }

  void createIndexes(std::string_view fields) {
    bool created = false;
    // TODO kSearch remove fields, also see SEARCH-334
    auto createJson = VPackParser::fromJson(absl::Substitute(
        R"({ "name": "testIndex0", "type": "inverted",
             "fields": $1,
             "version": $0 })",
        version(), fields));
    auto collection = _vocbase.lookupCollection("testCollection0");
    EXPECT_TRUE(collection);
    collection->createIndex(createJson->slice(), created).waitAndGet();
    ASSERT_TRUE(created);
  }

  void createSearch() {
    auto createJson = VPackParser::fromJson(
        R"({ "name": "testView", "type": "search-alias" })");
    auto logicalView = _vocbase.createView(createJson->slice(), false);
    ASSERT_FALSE(!logicalView);
    auto& implView = basics::downCast<iresearch::Search>(*logicalView);
    auto updateJson = VPackParser::fromJson(R"({ "indexes": [
      { "collection": "testCollection0", "index": "testIndex0" } ] })");
    auto r = implView.properties(updateJson->slice(), true, true);
    EXPECT_TRUE(r.ok()) << r.errorMessage();
    checkView(implView, 1);
  }
};

TEST_P(QueryGeoInRangeView, Test) {
  createAnalyzers("geojson");
  createCollections();
  createView(
      R"({ "geometry": { "analyzers": [ "mygeojson", "mygeocentroid", "mygeopoint" ] } })");
  queryTests();
  queryTestsGeoJson(true);
  queryTestsGeoCentroid();
  queryTestsGeoPoint();
  queryTestsMulti();
}

TEST_P(QueryGeoInRangeSearch, TestGeoJson) {
  createAnalyzers("geojson");
  createCollections();
  createIndexes(R"([ { "name": "geometry", "analyzer": "mygeojson" } ])");
  createSearch();
  queryTests();
  queryTestsGeoJson(true);
}

TEST_P(QueryGeoInRangeSearch, TestGeoCentroid) {
  createAnalyzers("geojson");
  createCollections();
  createIndexes(R"([ { "name": "geometry", "analyzer": "mygeocentroid" } ])");
  createSearch();
  queryTests();
  queryTestsGeoCentroid();
}

TEST_P(QueryGeoInRangeSearch, TestGeoPoint) {
  createAnalyzers("geojson");
  createCollections();
  createIndexes(R"([ { "name": "geometry", "analyzer": "mygeopoint" } ])");
  createSearch();
  queryTests();
  queryTestsGeoPoint();
}

#ifdef USE_ENTERPRISE

TEST_P(QueryGeoInRangeView, TestS2LatLng) {
  createAnalyzers("geo_s2", R"("format":"latLngDouble",)");
  createCollections();
  createView(
      R"({ "geometry": { "analyzers": [ "mygeojson", "mygeocentroid", "mygeopoint" ] } })");
  queryTests();
  queryTestsGeoJson(false);
  queryTestsGeoCentroid();
  queryTestsGeoPoint();
  queryTestsMulti();
}

TEST_P(QueryGeoInRangeSearch, TestGeoJsonS2LatLng) {
  createAnalyzers("geo_s2", R"("format":"latLngDouble",)");
  createCollections();
  createIndexes(R"([ { "name": "geometry", "analyzer": "mygeojson" } ])");
  createSearch();
  queryTests();
  queryTestsGeoJson(false);
}

TEST_P(QueryGeoInRangeSearch, TestGeoCentroidS2LatLng) {
  createAnalyzers("geo_s2", R"("format":"latLngDouble",)");
  createCollections();
  createIndexes(R"([ { "name": "geometry", "analyzer": "mygeocentroid" } ])");
  createSearch();
  queryTests();
  queryTestsGeoCentroid();
}

TEST_P(QueryGeoInRangeSearch, TestGeoPointS2LatLng) {
  createAnalyzers("geo_s2", R"("format":"latLngDouble",)");
  createCollections();
  createIndexes(R"([ { "name": "geometry", "analyzer": "mygeopoint" } ])");
  createSearch();
  queryTests();
  queryTestsGeoPoint();
}

TEST_P(QueryGeoInRangeView, TestS2LatLngInt) {
  createAnalyzers("geo_s2", R"("format":"latLngInt",)");
  createCollections();
  createView(
      R"({ "geometry": { "analyzers": [ "mygeojson", "mygeocentroid", "mygeopoint" ] } })");
  queryTests();
  queryTestsGeoJson(false);
  queryTestsGeoCentroid();
  queryTestsGeoPoint();
  queryTestsMulti();
}

TEST_P(QueryGeoInRangeSearch, TestGeoJsonS2LatLngInt) {
  createAnalyzers("geo_s2", R"("format":"latLngInt",)");
  createCollections();
  createIndexes(R"([ { "name": "geometry", "analyzer": "mygeojson" } ])");
  createSearch();
  queryTests();
  queryTestsGeoJson(false);
}

TEST_P(QueryGeoInRangeSearch, TestGeoCentroidS2LatLngInt) {
  createAnalyzers("geo_s2", R"("format":"latLngInt",)");
  createCollections();
  createIndexes(R"([ { "name": "geometry", "analyzer": "mygeocentroid" } ])");
  createSearch();
  queryTests();
  queryTestsGeoCentroid();
}

TEST_P(QueryGeoInRangeSearch, TestGeoPointS2LatLngInt) {
  createAnalyzers("geo_s2", R"("format":"latLngInt",)");
  createCollections();
  createIndexes(R"([ { "name": "geometry", "analyzer": "mygeopoint" } ])");
  createSearch();
  queryTests();
  queryTestsGeoPoint();
}

TEST_P(QueryGeoInRangeView, TestS2Point) {
  createAnalyzers("geo_s2", R"("format":"s2Point",)");
  createCollections();
  createView(
      R"({ "geometry": { "analyzers": [ "mygeojson", "mygeocentroid", "mygeopoint" ] } })");
  queryTests();
  queryTestsGeoJson(false);
  queryTestsGeoCentroid();
  queryTestsGeoPoint();
  queryTestsMulti();
}

TEST_P(QueryGeoInRangeSearch, TestGeoJsonS2Point) {
  createAnalyzers("geo_s2", R"("format":"s2Point",)");
  createCollections();
  createIndexes(R"([ { "name": "geometry", "analyzer": "mygeojson" } ])");
  createSearch();
  queryTests();
  queryTestsGeoJson(false);
}

TEST_P(QueryGeoInRangeSearch, TestGeoCentroidS2Point) {
  createAnalyzers("geo_s2", R"("format":"s2Point",)");
  createCollections();
  createIndexes(R"([ { "name": "geometry", "analyzer": "mygeocentroid" } ])");
  createSearch();
  queryTests();
  queryTestsGeoCentroid();
}

TEST_P(QueryGeoInRangeSearch, TestGeoPointS2Point) {
  createAnalyzers("geo_s2", R"("format":"s2Point",)");
  createCollections();
  createIndexes(R"([ { "name": "geometry", "analyzer": "mygeopoint" } ])");
  createSearch();
  queryTests();
  queryTestsGeoPoint();
}

#endif

TEST_P(QueryGeoInRangeView, testGeoPointArray) {
  createAnalyzers2();
  createCollections();
  createView(
      R"({ "geometry": { "fields": { "coordinates": { "analyzers": [ "mygeopoint" ] } } } })");
  queryTests2();
}

TEST_P(QueryGeoInRangeSearch, testGeoPointArray) {
  createAnalyzers2();
  createCollections();
  createIndexes(
      R"([ { "name": "geometry.coordinates", "analyzer": "mygeopoint" } ])");
  createSearch();
  queryTests2();
}

TEST_P(QueryGeoInRangeView, testGeoPointObject) {
  createAnalyzers3();
  createCollections();
  createView(R"({ "geometry": { "analyzers": [ "mygeopoint" ] } })");
  queryTests3();
}

TEST_P(QueryGeoInRangeSearch, testGeoPointObject) {
  createAnalyzers3();
  createCollections();
  createIndexes(R"([ { "name": "geometry", "analyzer": "mygeopoint" } ])");
  createSearch();
  queryTests3();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryGeoInRangeView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryGeoInRangeSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
