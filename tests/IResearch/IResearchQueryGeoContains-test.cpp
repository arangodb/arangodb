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
////////////////////////////////////////////////////////////////////////////////

#include "IResearchQueryCommon.h"
#include "common.h"

#include "IResearch/IResearchView.h"
#include "Transaction/StandaloneContext.h"
#include "Geo/GeoJson.h"
#include "Geo/ShapeContainer.h"
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

class IResearchQueryGeoContainsTest : public IResearchQueryTest {};

TEST_F(IResearchQueryGeoContainsTest, test) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  // geo analyzer
  {

    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    // shape
    {
      auto json = VPackParser::fromJson(R"({})");
      ASSERT_TRUE(analyzers.emplace(result, vocbase.name() + "::mygeojson", "geojson", json->slice(), { }).ok());
    }

    // centroid
    {
      auto json = VPackParser::fromJson(R"({"type": "centroid"})");
      ASSERT_TRUE(analyzers.emplace(result, vocbase.name() + "::mygeocentroid", "geojson", json->slice(), { }).ok());
    }

    // point
    {
      auto json = VPackParser::fromJson(R"({"type": "point"})");
      ASSERT_TRUE(analyzers.emplace(result, vocbase.name() + "::mygeopoint", "geojson", json->slice(), { }).ok());
    }
  }

  // create collection
  std::shared_ptr<arangodb::LogicalCollection> collection;
  {
    auto createJson = VPackParser::fromJson("{ \"name\": \"testCollection0\" }");
    collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);
  }

  // create view
  arangodb::iresearch::IResearchView* impl{};
  {
    auto createJson = VPackParser::fromJson(R"({ "name": "testView", "type": "arangosearch" })");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_FALSE(!logicalView);

    view = logicalView.get();
    impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    ASSERT_NE(nullptr, impl);

    auto updateJson = VPackParser::fromJson(R"({
      "links" : { "testCollection0" : { "fields" : { "geometry" : { "analyzers": ["mygeojson", "mygeocentroid", "mygeopoint"] } } } }
    })");
    EXPECT_TRUE(impl->properties(updateJson->slice(), true).ok());
    std::set<arangodb::DataSourceId> cids;
    impl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_EQ(1, cids.size());
  }

  // populate collection
  {
    auto docs = VPackParser::fromJson(R"([
        { "geometry": { "type": "Point", "coordinates": [ 37.615895, 55.7039   ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.615315, 55.703915 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.61509, 55.703537  ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.614183, 55.703806 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.613792, 55.704405 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.614956, 55.704695 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.616297, 55.704831 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.617053, 55.70461  ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.61582, 55.704459  ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.614634, 55.704338 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.613121, 55.704193 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.614135, 55.703298 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.613663, 55.704002 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.616522, 55.704235 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.615508, 55.704172 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.614629, 55.704081 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.610235, 55.709754 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.605,    55.707917 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.545776, 55.722083 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.559509, 55.715895 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.701645, 55.832144 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.73735,  55.816715 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.75589,  55.798193 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.659073, 55.843711 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.778549, 55.823659 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.729797, 55.853733 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.608261, 55.784682 ] } },
        { "geometry": { "type": "Point", "coordinates": [ 37.525177, 55.802825 ] } },
        { "geometry": { "type": "Polygon", "coordinates": [
           [[37.602682, 55.706853],
            [37.613025, 55.706853],
            [37.613025, 55.711906],
            [37.602682, 55.711906],
            [37.602682, 55.706853]]
        ]}}
      ])");

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
    EXPECT_TRUE(trx.begin().ok());

    for (auto doc: VPackArrayIterator(docs->slice())) {
      auto res = trx.insert(collection->name(), doc, options);
      EXPECT_TRUE(res.ok());
      insertedDocs.emplace_back(res.slice().get("new"));
    }

    EXPECT_TRUE(trx.commit().ok());

    // sync view
    ASSERT_TRUE(arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView OPTIONS { waitForSync: true } RETURN d").result.ok());
  }

  // ensure presence of special a column for geo indices
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      *collection,
      arangodb::AccessMode::Type::READ);
    ASSERT_TRUE(trx.begin().ok());

    auto snapshot = impl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    ASSERT_NE(nullptr, snapshot);
    ASSERT_EQ(1, snapshot->size());
    ASSERT_EQ(insertedDocs.size(), snapshot->docs_count());
    ASSERT_EQ(insertedDocs.size(), snapshot->live_docs_count());

    auto& segment = (*snapshot)[0];

    {
      auto const columnName = mangleString("geometry", "mygeojson");
      auto* columnReader = segment.column_reader(columnName);
      ASSERT_NE(nullptr, columnReader);
      auto it = columnReader->iterator();
      ASSERT_NE(nullptr, it);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);

      auto doc = insertedDocs.begin();
      for (; it->next(); ++doc) {
        EXPECT_EQUAL_SLICES(doc->slice().get("geometry"), arangodb::iresearch::slice(payload->value));
      }
      ASSERT_EQ(doc, insertedDocs.end());
    }

    {
      auto const columnName = mangleString("geometry", "mygeocentroid");
      auto* columnReader = segment.column_reader(columnName);
      ASSERT_NE(nullptr, columnReader);
      auto it = columnReader->iterator();
      ASSERT_NE(nullptr, it);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);

      auto doc = insertedDocs.begin();
      arangodb::geo::ShapeContainer shape;
      for (; it->next(); ++doc) {
        ASSERT_TRUE(arangodb::geo::geojson::parseRegion(doc->slice().get("geometry"), shape).ok());
        S2LatLng const centroid(shape.centroid());

        auto const storedValue = arangodb::iresearch::slice(payload->value);
        ASSERT_TRUE(storedValue.isArray());
        ASSERT_EQ(2, storedValue.length());
        EXPECT_DOUBLE_EQ(centroid.lng().degrees(), storedValue.at(0).getDouble());
        EXPECT_DOUBLE_EQ(centroid.lat().degrees(), storedValue.at(1).getDouble());
      }
      ASSERT_EQ(doc, insertedDocs.end());
    }

    {
      auto const columnName = mangleString("geometry", "mygeopoint");
      auto* columnReader = segment.column_reader(columnName);
      ASSERT_NE(nullptr, columnReader);
      auto it = columnReader->iterator();
      ASSERT_NE(nullptr, it);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);

      auto doc = insertedDocs.begin();
      for (; it->next(); ++doc) {
        EXPECT_EQUAL_SLICES(doc->slice().get("geometry"), arangodb::iresearch::slice(payload->value));
      }
      ASSERT_EQ(doc, insertedDocs.end()-1);
    }

    ASSERT_TRUE(trx.commit().ok());
  }

  // EXISTS will also work
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(FOR d IN testView
           SEARCH EXISTS(d.geometry)
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(insertedDocs.size(), slice.length());
    size_t i = 0;
    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_LT(i, insertedDocs.size());
      EXPECT_EQUAL_SLICES(insertedDocs[i++].slice(), resolved);
    }
    EXPECT_EQ(i, insertedDocs.size());
  }

  // EXISTS will also work
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(FOR d IN testView
           SEARCH EXISTS(d.geometry, 'string')
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(insertedDocs.size(), slice.length());
    size_t i = 0;
    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_LT(i, insertedDocs.size());
      EXPECT_EQUAL_SLICES(insertedDocs[i++].slice(), resolved);
    }
    EXPECT_EQ(i, insertedDocs.size());
  }

  // EXISTS will also work
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(FOR d IN testView
           SEARCH EXISTS(d.geometry, 'analyzer', "mygeojson")
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(insertedDocs.size(), slice.length());
    size_t i = 0;
    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_LT(i, insertedDocs.size());
      EXPECT_EQUAL_SLICES(insertedDocs[i++].slice(), resolved);
    }
    EXPECT_EQ(i, insertedDocs.size());
  }

  // test missing field
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET box = GEO_POLYGON([
             [37.602682, 55.706853],
             [37.613025, 55.706853],
             [37.613025, 55.711906],
             [37.602682, 55.711906],
             [37.602682, 55.706853]
           ])
           FOR d IN testView
           SEARCH ANALYZER(GEO_CONTAINS(d.missing, box), 'mygeojson')
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  // test missing field
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET box = GEO_POLYGON([
             [37.602682, 55.706853],
             [37.613025, 55.706853],
             [37.613025, 55.711906],
             [37.602682, 55.711906],
             [37.602682, 55.706853]
           ])
           FOR d IN testView
           SEARCH ANALYZER(GEO_CONTAINS(box, d.missing), 'mygeojson')
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  // test missing analyzer
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET box = GEO_POLYGON([
             [37.602682, 55.706853],
             [37.613025, 55.706853],
             [37.613025, 55.711906],
             [37.602682, 55.711906],
             [37.602682, 55.706853]
           ])
           FOR d IN testView
           SEARCH GEO_CONTAINS(d.geometry, box)
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  // test missing analyzer
  {
    std::vector<arangodb::velocypack::Slice> expected = {};
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET box = GEO_POLYGON([
             [37.602682, 55.706853],
             [37.613025, 55.706853],
             [37.613025, 55.711906],
             [37.602682, 55.711906],
             [37.602682, 55.706853]
           ])
           FOR d IN testView
           SEARCH GEO_CONTAINS(box, d.geometry)
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[16].slice(), insertedDocs[17].slice(), insertedDocs[28].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET box = GEO_POLYGON([
             [37.602682, 55.706853],
             [37.613025, 55.706853],
             [37.613025, 55.711906],
             [37.602682, 55.711906],
             [37.602682, 55.706853]
           ])
           FOR d IN testView
           SEARCH ANALYZER(GEO_CONTAINS(box, d.geometry), 'mygeojson')
           SORT d._key ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(expected.size(), slice.length());
    size_t i = 0;
    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_LT(i, expected.size());
      EXPECT_EQUAL_SLICES(expected[i++], resolved);
    }
    EXPECT_EQ(i, expected.size());
  }

  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[16].slice(), insertedDocs[17].slice(), insertedDocs[28].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET box = GEO_POLYGON([
             [37.602682, 55.706853],
             [37.613025, 55.706853],
             [37.613025, 55.711906],
             [37.602682, 55.711906],
             [37.602682, 55.706853]
           ])
           FOR d IN testView
           SEARCH ANALYZER(GEO_CONTAINS(box, d.geometry), 'mygeocentroid')
           SORT d._key ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(expected.size(), slice.length());
    size_t i = 0;
    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_LT(i, expected.size());
      EXPECT_EQUAL_SLICES(expected[i++], resolved);
    }
    EXPECT_EQ(i, expected.size());
  }

  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[16].slice(), insertedDocs[17].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET box = GEO_POLYGON([
             [37.602682, 55.706853],
             [37.613025, 55.706853],
             [37.613025, 55.711906],
             [37.602682, 55.711906],
             [37.602682, 55.706853]
           ])
           FOR d IN testView
           SEARCH ANALYZER(GEO_CONTAINS(box, d.geometry), 'mygeopoint')
           SORT d._key ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(expected.size(), slice.length());
    size_t i = 0;
    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_LT(i, expected.size());
      EXPECT_EQUAL_SLICES(expected[i++], resolved);
    }
    EXPECT_EQ(i, expected.size());
  }

  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[28].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET box = GEO_POLYGON([
             [37.602682, 55.706853],
             [37.613025, 55.706853],
             [37.613025, 55.711906],
             [37.602682, 55.711906],
             [37.602682, 55.706853]
           ])
           FOR d IN testView
           SEARCH ANALYZER(GEO_CONTAINS(d.geometry, box), 'mygeojson')
           SORT d._key ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(expected.size(), slice.length());
    size_t i = 0;
    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_LT(i, expected.size());
      EXPECT_EQUAL_SLICES(expected[i++], resolved);
    }
    EXPECT_EQ(i, expected.size());
  }

  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[21].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET point = GEO_POINT(37.73735,  55.816715)
           FOR d IN testView
           SEARCH ANALYZER(GEO_CONTAINS(point, d.geometry), 'mygeojson')
           SORT d._key ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(expected.size(), slice.length());
    size_t i = 0;
    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_LT(i, expected.size());
      EXPECT_EQUAL_SLICES(expected[i++], resolved);
    }
    EXPECT_EQ(i, expected.size());
  }

  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[21].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET point = GEO_POINT(37.73735,  55.816715)
           FOR d IN testView
           SEARCH ANALYZER(GEO_CONTAINS(d.geometry, point), 'mygeojson')
           SORT d._key ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(expected.size(), slice.length());
    size_t i = 0;
    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_LT(i, expected.size());
      EXPECT_EQUAL_SLICES(expected[i++], resolved);
    }
    EXPECT_EQ(i, expected.size());
  }

  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET box = GEO_POLYGON([
             [37.613025, 55.709029],
             [37.618818, 55.709029],
             [37.618818, 55.711906],
             [37.613025, 55.711906],
             [37.613025, 55.709029]
           ])
           FOR d IN testView
           SEARCH ANALYZER(GEO_CONTAINS(box, d.geometry), 'mygeojson')
           SORT d._key ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET box = GEO_POLYGON([
             [37.613025, 55.709029],
             [37.618818, 55.709029],
             [37.618818, 55.711906],
             [37.613025, 55.711906],
             [37.613025, 55.709029]
           ])
           FOR d IN testView
           SEARCH ANALYZER(GEO_CONTAINS(d.geometry, box), 'mygeojson')
           SORT d._key ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }


  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[28].slice()
    };

    // box lies within an indexed polygon
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET box = GEO_POLYGON([
             [37.602682, 55.711906],
             [37.603412, 55.71164],
             [37.604227, 55.711906],
             [37.602682, 55.711906]
           ])
           FOR d IN testView
           SEARCH ANALYZER(GEO_CONTAINS(d.geometry, box), 'mygeojson')
           SORT d._key ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(expected.size(), slice.length());
    size_t i = 0;
    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_LT(i, expected.size());
      EXPECT_EQUAL_SLICES(expected[i++], resolved);
    }
    EXPECT_EQ(i, expected.size());
  }

  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET box = GEO_POLYGON([
             [37.602682, 55.711906],
             [37.603412, 55.71164],
             [37.604227, 55.711906],
             [37.602682, 55.711906]
           ])
           FOR d IN testView
           SEARCH ANALYZER(GEO_CONTAINS(box, d.geometry), 'mygeocentroid')
           SORT d._key ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  {
    // box lies within an indexed polygon
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET box = GEO_POLYGON([
             [37.602682, 55.711906],
             [37.603412, 55.71164],
             [37.604227, 55.711906],
             [37.602682, 55.711906]
           ])
           FOR d IN testView
           SEARCH ANALYZER(GEO_CONTAINS(d.geometry, box), 'mygeocentroid')
           SORT d._key ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  {
    // box lies within an indexed polygon
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET box = GEO_POLYGON([
             [37.602682, 55.711906],
             [37.603412, 55.71164],
             [37.604227, 55.711906],
             [37.602682, 55.711906]
           ])
           FOR d IN testView
           SEARCH ANALYZER(GEO_CONTAINS(d.geometry, box), 'mygeopoint')
           SORT d._key ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  {
    // box lies within an indexed polygon
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET box = GEO_POLYGON([
             [37.602682, 55.711906],
             [37.603412, 55.71164],
             [37.604227, 55.711906],
             [37.602682, 55.711906]
           ])
           FOR d IN testView
           SEARCH ANALYZER(GEO_CONTAINS(box, d.geometry), 'mygeojson')
           SORT d._key ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }
}

}
