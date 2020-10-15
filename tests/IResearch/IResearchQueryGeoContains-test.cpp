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

class IResearchQueryGeoContainsTest : public IResearchQueryTest {};

TEST_F(IResearchQueryGeoContainsTest, test) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  // geo analyzer
  {

    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto json = VPackParser::fromJson(R"({})");
    ASSERT_TRUE(analyzers.emplace(result, vocbase.name() + "::mygeojson", "geojson", json->slice(), { }).ok());
  }

  // create collection
  {
    auto createJson = VPackParser::fromJson("{ \"name\": \"testCollection0\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);

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
        { "geometry": { "type": "Point", "coordinates": [ 37.525177, 55.802825 ] } }
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
  }

  // create view
  {
    auto createJson = VPackParser::fromJson(R"({ "name": "testView", "type": "arangosearch" })");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_FALSE(!logicalView);

    view = logicalView.get();
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    ASSERT_FALSE(!impl);

    auto updateJson = VPackParser::fromJson(R"({
      "links" : { "testCollection0" : { "fields" : { "geometry" : { "analyzers": ["mygeojson"] } } } }
    })");
    EXPECT_TRUE(impl->properties(updateJson->slice(), true).ok());
    std::set<arangodb::DataSourceId> cids;
    impl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_EQ(1, cids.size());
    ASSERT_TRUE(arangodb::tests::executeQuery(vocbase, "FOR d IN testView OPTIONS { waitForSync: true } RETURN d").result.ok());
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
           SEARCH ANALYZER(GEO_CONTAINS(box, d.geometry), 'mygeojson')
           SORT d._key ASC
           RETURN d)");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    ASSERT_EQ(2, slice.length());
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
    ASSERT_EQ(0, slice.length());
  }
}

}
