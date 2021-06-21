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

class IResearchQueryGeoInRangeTest : public IResearchQueryTest {};

TEST_F(IResearchQueryGeoInRangeTest, testGeoJson) {
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
      "links" : { "testCollection0" : { "fields" : {
        "geometry" : { "analyzers": ["mygeojson", "mygeocentroid", "mygeopoint"] } }
      } }
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
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(d.missing, origin, 0, 300), 'mygeojson')
           SORT d.id ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  // test missing field
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.missing, 0, 300), 'mygeojson')
           SORT d.id ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  // test missing analyzer
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH GEO_IN_RANGE(d.geometry, origin, 0, 300)
           SORT d.id ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  // test missing analyzer
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH GEO_IN_RANGE(origin, d.geometry, 0, 300)
           SORT d.id ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[16].slice(), insertedDocs[17].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(d.geometry, origin, 0, 300), 'mygeojson')
           SORT d.id ASC
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
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 300), 'mygeojson')
           SORT d.id ASC
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
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 300), 'mygeocentroid')
           SORT d.id ASC
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
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 300), 'mygeopoint')
           SORT d.id ASC
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
      insertedDocs[12].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.613663, 55.704002)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 0), 'mygeojson')
           SORT d.id ASC
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
        R"(LET origin = GEO_POINT(37.613663, 55.704002)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 0, false, false), 'mygeojson')
           SORT d.id ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[16].slice(), insertedDocs[17].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(d.geometry, origin, 100, 300), 'mygeojson')
           SORT d.id ASC
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
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 100, 300), 'mygeojson')
           SORT d.id ASC
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
      insertedDocs[17].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 206, 207), 'mygeojson')
           SORT d.id ASC
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
      insertedDocs[17].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(d.geometry, origin, 206, 207), 'mygeojson')
           SORT d.id ASC
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
    std::vector<arangodb::velocypack::Slice> expected {
      insertedDocs[23].slice(), insertedDocs[24].slice(), insertedDocs[25].slice(),
    };

    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 15000, 20000), 'mygeojson')
           SORT d.id ASC
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
}

TEST_F(IResearchQueryGeoInRangeTest, testGeoPointArray) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  // geo analyzer
  {

    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto json = VPackParser::fromJson(R"({})");
    ASSERT_TRUE(analyzers.emplace(result, vocbase.name() + "::mygeopoint", "geopoint", json->slice(), { }).ok());
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
      "links" : { "testCollection0" : { "fields" : {
        "geometry" : { "fields" : {
          "coordinates" : { "analyzers": ["mygeopoint"] } }
        } }
      } }
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

    auto const columnName = mangleString("geometry.coordinates", "mygeopoint");
    auto* columnReader = segment.column_reader(columnName);
    ASSERT_NE(nullptr, columnReader);
    auto it = columnReader->iterator();
    ASSERT_NE(nullptr, it);
    auto* payload = irs::get<irs::payload>(*it);
    ASSERT_NE(nullptr, payload);

    auto doc = insertedDocs.begin();
    for (; it->next(); ++doc) {
      auto const storedValue = arangodb::iresearch::slice(payload->value);
      ASSERT_TRUE(storedValue.isArray());
      ASSERT_EQ(2, storedValue.length());
      EXPECT_DOUBLE_EQ(storedValue.at(0).getDouble(),
                       doc->slice().get({"geometry", "coordinates"}).at(1).getDouble());
      EXPECT_DOUBLE_EQ(storedValue.at(1).getDouble(),
                       doc->slice().get({"geometry", "coordinates"}).at(0).getDouble());
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
           SEARCH EXISTS(d.geometry.coordinates, 'string')
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
           SEARCH EXISTS(d.geometry.coordinates, 'analyzer', "mygeopoint")
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
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(d.missing, origin, 0, 300), 'mygeopoint')
           SORT d.id ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  // test missing field
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.missing, 0, 300), 'mygeopoint')
           SORT d.id ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  // test missing analyzer
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH GEO_IN_RANGE(d.geometry.coordinates, origin, 0, 300)
           SORT d.id ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  // test missing analyzer
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH GEO_IN_RANGE(origin, d.geometry.coordinates, 0, 300)
           SORT d.id ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[16].slice(), insertedDocs[17].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(d.geometry.coordinates, origin, 0, 300), 'mygeopoint')
           SORT d.id ASC
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
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry.coordinates, 0, 300), 'mygeopoint')
           SORT d.id ASC
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
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry.coordinates, 0, 300), 'mygeopoint')
           SORT d.id ASC
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
      insertedDocs[12].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.613663, 55.704002)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry.coordinates, 0, 0), 'mygeopoint')
           SORT d.id ASC
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
        R"(LET origin = GEO_POINT(37.613663, 55.704002)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry.coordinates, 0, 0, false, false), 'mygeopoint')
           SORT d.id ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[16].slice(), insertedDocs[17].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(d.geometry.coordinates, origin, 100, 300), 'mygeopoint')
           SORT d.id ASC
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
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry.coordinates, 100, 300), 'mygeopoint')
           SORT d.id ASC
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
      insertedDocs[17].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry.coordinates, 206, 207), 'mygeopoint')
           SORT d.id ASC
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
      insertedDocs[17].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(d.geometry.coordinates, origin, 206, 207), 'mygeopoint')
           SORT d.id ASC
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
    std::vector<arangodb::velocypack::Slice> expected {
      insertedDocs[23].slice(), insertedDocs[24].slice(), insertedDocs[25].slice(),
    };

    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry.coordinates, 15000, 20000), 'mygeopoint')
           SORT d.id ASC
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
}

TEST_F(IResearchQueryGeoInRangeTest, testGeoPointObject) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  // geo analyzer
  {

    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto json = VPackParser::fromJson(R"({
      "latitude" : ["coordinates", "lat" ],
      "longitude": ["coordinates", "lon" ]
    })");
    ASSERT_TRUE(analyzers.emplace(result, vocbase.name() + "::mygeopoint", "geopoint", json->slice(), { }).ok());
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
      "links" : { "testCollection0" : { "fields" : {
        "geometry" : { "analyzers": ["mygeopoint"] }
      } } }
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

    auto const columnName = mangleString("geometry", "mygeopoint");
    auto* columnReader = segment.column_reader(columnName);
    ASSERT_NE(nullptr, columnReader);
    auto it = columnReader->iterator();
    ASSERT_NE(nullptr, it);
    auto* payload = irs::get<irs::payload>(*it);
    ASSERT_NE(nullptr, payload);

    auto doc = insertedDocs.begin();
    for (; it->next(); ++doc) {
      auto const storedValue = arangodb::iresearch::slice(payload->value);
      ASSERT_TRUE(storedValue.isArray());
      ASSERT_EQ(2, storedValue.length());
      EXPECT_DOUBLE_EQ(storedValue.at(0).getDouble(),
                       doc->slice().get({"geometry", "coordinates", "lon"}).getDouble());
      EXPECT_DOUBLE_EQ(storedValue.at(1).getDouble(),
                       doc->slice().get({"geometry", "coordinates", "lat"}).getDouble());
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
           SEARCH EXISTS(d.geometry, 'analyzer', "mygeopoint")
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
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(d.missing, origin, 0, 300), 'mygeopoint')
           SORT d.id ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  // test missing field
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.missing, 0, 300), 'mygeopoint')
           SORT d.id ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  // test missing analyzer
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH GEO_IN_RANGE(d.geometry, origin, 0, 300)
           SORT d.id ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  // test missing analyzer
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH GEO_IN_RANGE(origin, d.geometry, 0, 300)
           SORT d.id ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[16].slice(), insertedDocs[17].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(d.geometry, origin, 0, 300), 'mygeopoint')
           SORT d.id ASC
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
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 300), 'mygeopoint')
           SORT d.id ASC
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
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 300), 'mygeopoint')
           SORT d.id ASC
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
      insertedDocs[12].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.613663, 55.704002)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 0), 'mygeopoint')
           SORT d.id ASC
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
        R"(LET origin = GEO_POINT(37.613663, 55.704002)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 0, 0, false, false), 'mygeopoint')
           SORT d.id ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(0, slice.length());
  }

  {
    std::vector<arangodb::velocypack::Slice> expected = {
      insertedDocs[16].slice(), insertedDocs[17].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(d.geometry, origin, 100, 300), 'mygeopoint')
           SORT d.id ASC
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
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 100, 300), 'mygeopoint')
           SORT d.id ASC
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
      insertedDocs[17].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 206, 207), 'mygeopoint')
           SORT d.id ASC
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
      insertedDocs[17].slice()
    };
    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(d.geometry, origin, 206, 207), 'mygeopoint')
           SORT d.id ASC
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
    std::vector<arangodb::velocypack::Slice> expected {
      insertedDocs[23].slice(), insertedDocs[24].slice(), insertedDocs[25].slice(),
    };

    auto result = arangodb::tests::executeQuery(
        vocbase,
        R"(LET origin = GEO_POINT(37.607768, 55.70892)
           FOR d IN testView
           SEARCH ANALYZER(GEO_IN_RANGE(origin, d.geometry, 15000, 20000), 'mygeopoint')
           SORT d.id ASC
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
}

}
