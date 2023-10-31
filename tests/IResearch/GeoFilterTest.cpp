////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include <set>

#include "s2/s2point_region.h"
#include "s2/s2polygon.h"

#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "search/cost.hpp"
#include "search/score.hpp"
#include "store/memory_directory.hpp"

#include "IResearch/GeoFilter.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFields.h"

#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>

using namespace arangodb;
using namespace arangodb::iresearch;
using namespace arangodb::tests;

namespace {

struct custom_sort final : public irs::ScorerBase<void> {
  static constexpr std::string_view type_name() noexcept {
    return "custom_sort";
  }

  class field_collector final : public irs::FieldCollector {
   public:
    field_collector(const custom_sort& sort) : sort_(sort) {}

    void collect(const irs::SubReader& segment,
                 const irs::term_reader& field) final {
      if (sort_.fieldCollectorCollect) {
        sort_.fieldCollectorCollect(segment, field);
      }
    }

    void collect(irs::bytes_view) final {}

    void reset() final {}

    void write(irs::data_output&) const final {}

   private:
    const custom_sort& sort_;
  };

  class term_collector final : public irs::TermCollector {
   public:
    term_collector(const custom_sort& sort) : sort_(sort) {}

    virtual void collect(const irs::SubReader& segment,
                         const irs::term_reader& field,
                         const irs::attribute_provider& term_attrs) final {
      if (sort_.termCollectorCollect) {
        sort_.termCollectorCollect(segment, field, term_attrs);
      }
    }

    void collect(irs::bytes_view) final {}

    void reset() final {}

    void write(irs::data_output&) const final {}

   private:
    const custom_sort& sort_;
  };

  struct scorer final : public irs::score_ctx {
    scorer(const custom_sort& sort, irs::ColumnProvider const& segment,
           std::map<irs::type_info::type_id, irs::field_id> const& features,
           irs::byte_type const* stats,
           irs::attribute_provider const& doc_attrs)
        : document_attrs_(doc_attrs),
          stats_(stats),
          segment_reader_(segment),
          sort_(sort) {}

    const irs::attribute_provider& document_attrs_;
    const irs::byte_type* stats_;
    const irs::ColumnProvider& segment_reader_;
    const custom_sort& sort_;
  };

  void collect(irs::byte_type* stats, irs::FieldCollector const* field,
               irs::TermCollector const* term) const final {
    if (collectorFinish) {
      collectorFinish(stats, field, term);
    }
  }

  irs::IndexFeatures index_features() const final {
    return irs::IndexFeatures::NONE;
  }

  irs::FieldCollector::ptr prepare_field_collector() const final {
    if (prepareFieldCollector) {
      return prepareFieldCollector();
    }

    return std::make_unique<custom_sort::field_collector>(*this);
  }

  irs::ScoreFunction prepare_scorer(
      irs::ColumnProvider const& segment,
      std::map<irs::type_info::type_id, irs::field_id> const& features,
      irs::byte_type const* stats, irs::attribute_provider const& doc_attrs,
      irs::score_t boost) const final {
    if (prepareScorer) {
      prepareScorer(segment, features, stats, doc_attrs, boost);
    }

    return irs::ScoreFunction::Make<custom_sort::scorer>(
        [](irs::score_ctx* ctx, irs::score_t* res) noexcept {
          auto& ctxImpl = *reinterpret_cast<const custom_sort::scorer*>(ctx);

          irs::doc_id_t doc_id =
              irs::get<irs::document>(ctxImpl.document_attrs_)->value;

          if (ctxImpl.sort_.scorerScore) {
            ctxImpl.sort_.scorerScore(doc_id, res);
          }
        },
        irs::ScoreFunction::DefaultMin, *this, segment, features, stats,
        doc_attrs);
  }

  irs::TermCollector::ptr prepare_term_collector() const final {
    if (prepareTermCollector) {
      return prepareTermCollector();
    }

    return std::make_unique<custom_sort::term_collector>(*this);
  }

  std::function<void(const irs::SubReader&, const irs::term_reader&)>
      fieldCollectorCollect;
  std::function<void(const irs::SubReader&, const irs::term_reader&,
                     const irs::attribute_provider&)>
      termCollectorCollect;
  std::function<void(irs::byte_type*, irs::FieldCollector const*,
                     irs::TermCollector const*)>
      collectorFinish;
  std::function<irs::FieldCollector::ptr()> prepareFieldCollector;
  std::function<void(
      const irs::ColumnProvider& segment,
      std::map<irs::type_info::type_id, irs::field_id> const& features,
      irs::byte_type const* stats, irs::attribute_provider const& doc_attrs,
      irs::score_t boost)>
      prepareScorer;
  std::function<irs::TermCollector::ptr()> prepareTermCollector;
  std::function<void(irs::doc_id_t&, irs::score_t*)> scorerScore;

  custom_sort() = default;
};

}  // namespace

TEST(GeoFilterTest, options) {
  S2RegionTermIndexer::Options const s2opts;
  GeoFilterOptions const opts;
  ASSERT_TRUE(opts.prefix.empty());
  ASSERT_TRUE(opts.shape.empty());
  ASSERT_EQ(s2opts.level_mod(), opts.options.level_mod());
  ASSERT_EQ(s2opts.min_level(), opts.options.min_level());
  ASSERT_EQ(s2opts.max_level(), opts.options.max_level());
  ASSERT_EQ(s2opts.max_cells(), opts.options.max_cells());
  ASSERT_EQ(s2opts.marker(), opts.options.marker());
  ASSERT_EQ(s2opts.index_contains_points_only(),
            opts.options.index_contains_points_only());
  ASSERT_EQ(s2opts.optimize_for_space(), opts.options.optimize_for_space());
  ASSERT_EQ(arangodb::iresearch::GeoFilterType::INTERSECTS, opts.type);
}

TEST(GeoFilterTest, ctor) {
  GeoFilter q;
  ASSERT_EQ(irs::type<GeoFilter>::id(), q.type());
  ASSERT_EQ("", q.field());
  ASSERT_EQ(irs::kNoBoost, q.boost());
#ifndef ARANGODB_ENABLE_MAINTAINER_MODE
  ASSERT_EQ(GeoFilterOptions{}, q.options());
#endif
}

TEST(GeoFilterTest, equal) {
  GeoFilter q;
  q.mutable_options()->type = GeoFilterType::INTERSECTS;
  q.mutable_options()->shape.reset(
      std::make_unique<S2PointRegion>(S2Point{1., 0., 0.}),
      geo::ShapeContainer::Type::S2_POINT);
  *q.mutable_field() = "field";

  {
    GeoFilter q1;
    q1.mutable_options()->type = GeoFilterType::INTERSECTS;
    q1.mutable_options()->shape.reset(
        std::make_unique<S2PointRegion>(S2Point{1., 0., 0.}),
        geo::ShapeContainer::Type::S2_POINT);
    *q1.mutable_field() = "field";
    ASSERT_EQ(q, q1);
    ASSERT_EQ(q.hash(), q1.hash());
  }

  {
    GeoFilter q1;
    q1.boost(1.5);
    q1.mutable_options()->type = GeoFilterType::INTERSECTS;
    q1.mutable_options()->shape.reset(
        std::make_unique<S2PointRegion>(S2Point{1., 0., 0.}),
        geo::ShapeContainer::Type::S2_POINT);
    *q1.mutable_field() = "field";
    ASSERT_EQ(q, q1);
    ASSERT_EQ(q.hash(), q1.hash());
  }

  {
    GeoFilter q1;
    q1.mutable_options()->type = GeoFilterType::INTERSECTS;
    q1.mutable_options()->shape.reset(
        std::make_unique<S2PointRegion>(S2Point{1., 0., 0.}),
        geo::ShapeContainer::Type::S2_POINT);
    *q1.mutable_field() = "field1";
    ASSERT_NE(q, q1);
  }

  {
    GeoFilter q1;
    q1.mutable_options()->type = GeoFilterType::CONTAINS;
    q1.mutable_options()->shape.reset(
        std::make_unique<S2PointRegion>(S2Point{1., 0., 0.}),
        geo::ShapeContainer::Type::S2_POINT);
    *q1.mutable_field() = "field";
    ASSERT_NE(q, q1);
  }

  {
    GeoFilter q1;
    q1.mutable_options()->type = GeoFilterType::CONTAINS;
    q1.mutable_options()->shape.reset(std::make_unique<S2Polygon>(),
                                      geo::ShapeContainer::Type::S2_POLYGON);
    *q1.mutable_field() = "field";
    ASSERT_NE(q, q1);
  }
}

TEST(GeoFilterTest, boost) {
  // no boost
  {
    GeoFilter q;
    q.mutable_options()->type = GeoFilterType::INTERSECTS;
    q.mutable_options()->shape.reset(
        std::make_unique<S2PointRegion>(S2Point{1., 0., 0.}),
        geo::ShapeContainer::Type::S2_POINT);
    *q.mutable_field() = "field";

    auto prepared = q.prepare({.index = irs::SubReader::empty()});
    ASSERT_EQ(irs::kNoBoost, prepared->boost());
  }

  // with boost
  {
    irs::score_t boost = 1.5f;
    GeoFilter q;
    q.mutable_options()->type = GeoFilterType::INTERSECTS;
    q.mutable_options()->shape.reset(
        std::make_unique<S2PointRegion>(S2Point{1., 0., 0.}),
        geo::ShapeContainer::Type::S2_POINT);
    *q.mutable_field() = "field";
    q.boost(boost);

    auto prepared = q.prepare({.index = irs::SubReader::empty()});
    ASSERT_EQ(boost, prepared->boost());
  }
}

TEST(GeoFilterTest, query) {
  auto docs = VPackParser::fromJson(R"([
    { "name": "A", "geometry": { "type": "Point", "coordinates": [ 37.615895, 55.7039   ] } },
    { "name": "B", "geometry": { "type": "Point", "coordinates": [ 37.615315, 55.703915 ] } },
    { "name": "C", "geometry": { "type": "Point", "coordinates": [ 37.61509, 55.703537  ] } },
    { "name": "D", "geometry": { "type": "Point", "coordinates": [ 37.614183, 55.703806 ] } },
    { "name": "E", "geometry": { "type": "Point", "coordinates": [ 37.613792, 55.704405 ] } },
    { "name": "F", "geometry": { "type": "Point", "coordinates": [ 37.614956, 55.704695 ] } },
    { "name": "G", "geometry": { "type": "Point", "coordinates": [ 37.616297, 55.704831 ] } },
    { "name": "H", "geometry": { "type": "Point", "coordinates": [ 37.617053, 55.70461  ] } },
    { "name": "I", "geometry": { "type": "Point", "coordinates": [ 37.61582, 55.704459  ] } },
    { "name": "J", "geometry": { "type": "Point", "coordinates": [ 37.614634, 55.704338 ] } },
    { "name": "K", "geometry": { "type": "Point", "coordinates": [ 37.613121, 55.704193 ] } },
    { "name": "L", "geometry": { "type": "Point", "coordinates": [ 37.614135, 55.703298 ] } },
    { "name": "M", "geometry": { "type": "Point", "coordinates": [ 37.613663, 55.704002 ] } },
    { "name": "N", "geometry": { "type": "Point", "coordinates": [ 37.616522, 55.704235 ] } },
    { "name": "O", "geometry": { "type": "Point", "coordinates": [ 37.615508, 55.704172 ] } },
    { "name": "P", "geometry": { "type": "Point", "coordinates": [ 37.614629, 55.704081 ] } },
    { "name": "Q", "geometry": { "type": "Point", "coordinates": [ 37.610235, 55.709754 ] } },
    { "name": "R", "geometry": { "type": "Point", "coordinates": [ 37.605,    55.707917 ] } },
    { "name": "S", "geometry": { "type": "Point", "coordinates": [ 37.545776, 55.722083 ] } },
    { "name": "T", "geometry": { "type": "Point", "coordinates": [ 37.559509, 55.715895 ] } },
    { "name": "U", "geometry": { "type": "Point", "coordinates": [ 37.701645, 55.832144 ] } },
    { "name": "V", "geometry": { "type": "Point", "coordinates": [ 37.73735,  55.816715 ] } },
    { "name": "W", "geometry": { "type": "Point", "coordinates": [ 37.75589,  55.798193 ] } },
    { "name": "X", "geometry": { "type": "Point", "coordinates": [ 37.659073, 55.843711 ] } },
    { "name": "Y", "geometry": { "type": "Point", "coordinates": [ 37.778549, 55.823659 ] } },
    { "name": "Z", "geometry": { "type": "Point", "coordinates": [ 37.729797, 55.853733 ] } },
    { "name": "1", "geometry": { "type": "Point", "coordinates": [ 37.608261, 55.784682 ] } },
    { "name": "2", "geometry": { "type": "Point", "coordinates": [ 37.525177, 55.802825 ] } }
  ])");

  irs::memory_directory dir;
  irs::DirectoryReader reader;

  // index data
  {
    constexpr auto formatId = arangodb::iresearch::getFormat(LinkVersion::MAX);
    auto codec = irs::formats::get(formatId);
    ASSERT_NE(nullptr, codec);
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);
    GeoField geoField;
    geoField.fieldName = "geometry";
    StringField nameField;
    nameField.fieldName = "name";
    {
      auto segment0 = writer->GetBatch();
      auto segment1 = writer->GetBatch();
      {
        size_t i = 0;
        for (auto docSlice : VPackArrayIterator(docs->slice())) {
          geoField.shapeSlice = docSlice.get("geometry");
          nameField.value = getStringRef(docSlice.get("name"));

          auto doc = (i++ % 2 ? segment0 : segment1).Insert();
          ASSERT_TRUE(
              doc.Insert<irs::Action::INDEX | irs::Action::STORE>(nameField));
          ASSERT_TRUE(
              doc.Insert<irs::Action::INDEX | irs::Action::STORE>(geoField));
        }
      }
    }
    writer->Commit();
    reader = writer->GetSnapshot();
  }

  ASSERT_NE(nullptr, reader);
  ASSERT_EQ(2U, reader->size());
  ASSERT_EQ(docs->slice().length(), reader->docs_count());
  ASSERT_EQ(docs->slice().length(), reader->live_docs_count());

  auto executeQuery = [&reader](irs::filter const& q,
                                std::vector<irs::cost::cost_t> const& costs) {
    std::set<std::string> actualResults;

    struct MaxMemoryCounter final : irs::IResourceManager {
      void Reset() noexcept {
        current = 0;
        max = 0;
      }

      void Increase(size_t value) final {
        current += value;
        max = std::max(max, current);
      }

      void Decrease(size_t value) noexcept final { current -= value; }

      size_t current{0};
      size_t max{0};
    };

    MaxMemoryCounter counter;
    auto prepared = q.prepare({
        .index = *reader,
        .memory = counter,
    });
    EXPECT_NE(nullptr, prepared);
    auto expectedCost = costs.begin();
    for (auto& segment : *reader) {
      auto column = segment.column("name");
      EXPECT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      EXPECT_NE(nullptr, values);
      auto* value = irs::get<irs::payload>(*values);
      EXPECT_NE(nullptr, value);
      auto it = prepared->execute({.segment = segment});
      EXPECT_NE(nullptr, it);
      auto seek_it = prepared->execute({.segment = segment});
      EXPECT_NE(nullptr, seek_it);
      auto* cost = irs::get<irs::cost>(*it);
      EXPECT_NE(nullptr, cost);

      EXPECT_NE(expectedCost, costs.end());
      EXPECT_EQ(*expectedCost, cost->estimate());
      ++expectedCost;

      if (irs::doc_limits::eof(it->value())) {
        continue;
      }

      auto* score = irs::get<irs::score>(*it);
      EXPECT_NE(nullptr, score);
      EXPECT_TRUE(score->IsDefault());

      auto* doc = irs::get<irs::document>(*it);
      EXPECT_NE(nullptr, doc);
      EXPECT_FALSE(irs::doc_limits::valid(doc->value));
      EXPECT_FALSE(irs::doc_limits::valid(it->value()));
      while (it->next()) {
        auto docId = it->value();
        EXPECT_EQ(docId, seek_it->seek(docId));
        EXPECT_EQ(docId, seek_it->seek(docId));
        EXPECT_EQ(docId, doc->value);
        EXPECT_EQ(docId, values->seek(docId));
        EXPECT_FALSE(irs::IsNull(value->value));

        actualResults.emplace(irs::to_string<std::string>(value->value.data()));
      }
      EXPECT_TRUE(irs::doc_limits::eof(it->value()));
      EXPECT_TRUE(irs::doc_limits::eof(seek_it->seek(it->value())));

      {
        auto it = prepared->execute({.segment = segment});
        EXPECT_NE(nullptr, it);

        while (it->next()) {
          auto const docId = it->value();
          auto seek_it = prepared->execute({.segment = segment});
          EXPECT_NE(nullptr, seek_it);
          auto column_it = column->iterator(irs::ColumnHint::kNormal);
          EXPECT_NE(nullptr, column_it);
          auto* payload = irs::get<irs::payload>(*column_it);
          EXPECT_NE(nullptr, payload);
          EXPECT_EQ(docId, seek_it->seek(docId));
          do {
            EXPECT_EQ(seek_it->value(), column_it->seek(seek_it->value()));
            if (!irs::doc_limits::eof(column_it->value())) {
              EXPECT_NE(actualResults.end(),
                        actualResults.find(irs::to_string<std::string>(
                            payload->value.data())));
            }
          } while (seek_it->next());
          EXPECT_TRUE(irs::doc_limits::eof(seek_it->value()));
        }
        EXPECT_TRUE(irs::doc_limits::eof(it->value()));
      }
    }
    EXPECT_EQ(expectedCost, costs.end());

    prepared.reset();
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);

    return actualResults;
  };

  {
    std::set<std::string> const expected{"Q"};

    auto json = VPackParser::fromJson(R"({
      "type": "Point",
      "coordinates": [ 37.610235, 55.709754 ]
    })");

    GeoFilter q;
    q.mutable_options()->type = GeoFilterType::INTERSECTS;
    ASSERT_TRUE(
        geo::json::parseRegion(json->slice(), q.mutable_options()->shape, false)
            .ok());
    ASSERT_EQ(geo::ShapeContainer::Type::S2_POINT,
              q.mutable_options()->shape.type());
    *q.mutable_field() = "geometry";

    ASSERT_EQ(expected, executeQuery(q, {2, 0}));
  }

  {
    std::set<std::string> const expected{"Q", "R"};

    auto json = VPackParser::fromJson(R"({
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

    GeoFilter q;
    q.mutable_options()->type = GeoFilterType::INTERSECTS;
    ASSERT_TRUE(
        geo::json::parseRegion(json->slice(), q.mutable_options()->shape, false)
            .ok());
    ASSERT_EQ(geo::ShapeContainer::Type::S2_POLYGON,
              q.mutable_options()->shape.type());
    *q.mutable_field() = "geometry";

    ASSERT_EQ(expected, executeQuery(q, {2, 2}));
  }

  {
    auto const origin = docs->slice().at(7);
    std::set<std::string> expected{origin.get("name").copyString()};

    GeoFilter q;
    *q.mutable_field() = "geometry";
    std::vector<S2LatLng> cache;
    ASSERT_TRUE(arangodb::iresearch::parseShape<
                arangodb::iresearch::Parsing::OnlyPoint>(
        origin.get("geometry"), q.mutable_options()->shape, cache, false,
        arangodb::geo::coding::Options::kInvalid, nullptr));
    q.mutable_options()->type = GeoFilterType::INTERSECTS;
    q.mutable_options()->options.set_index_contains_points_only(true);

    ASSERT_EQ(expected, executeQuery(q, {2, 4}));
  }

  {
    auto const origin = docs->slice().at(7);
    std::set<std::string> expected{origin.get("name").copyString()};

    GeoFilter q;
    *q.mutable_field() = "geometry";
    std::vector<S2LatLng> cache;
    ASSERT_TRUE(arangodb::iresearch::parseShape<
                arangodb::iresearch::Parsing::OnlyPoint>(
        origin.get("geometry"), q.mutable_options()->shape, cache, false,
        arangodb::geo::coding::Options::kInvalid, nullptr));
    q.mutable_options()->type = GeoFilterType::CONTAINS;
    q.mutable_options()->options.set_index_contains_points_only(true);

    ASSERT_EQ(expected, executeQuery(q, {2, 4}));
  }

  {
    auto const origin = docs->slice().at(7);
    std::set<std::string> expected{origin.get("name").copyString()};

    GeoFilter q;
    *q.mutable_field() = "geometry";
    std::vector<S2LatLng> cache;
    ASSERT_TRUE(arangodb::iresearch::parseShape<
                arangodb::iresearch::Parsing::OnlyPoint>(
        origin.get("geometry"), q.mutable_options()->shape, cache, false,
        arangodb::geo::coding::Options::kInvalid, nullptr));
    q.mutable_options()->type = GeoFilterType::IS_CONTAINED;
    q.mutable_options()->options.set_index_contains_points_only(true);

    ASSERT_EQ(expected, executeQuery(q, {2, 4}));
  }

  {
    auto shapeJson = VPackParser::fromJson(R"({
      "type": "Polygon",
        "coordinates": [
            [
                [37.590322, 55.695583],
                [37.626114, 55.695583],
                [37.626114, 55.71488],
                [37.590322, 55.71488],
                [37.590322, 55.695583]
            ]
      ]
    })");

    arangodb::geo::ShapeContainer shape;
    arangodb::geo::ShapeContainer point;
    std::vector<S2LatLng> cache;
    ASSERT_TRUE(
        arangodb::iresearch::parseShape<arangodb::iresearch::Parsing::GeoJson>(
            shapeJson->slice(), shape, cache, false,
            arangodb::geo::coding::Options::kInvalid, nullptr));
    std::set<std::string> expected;
    for (auto doc : VPackArrayIterator(docs->slice())) {
      auto geo = doc.get("geometry");
      ASSERT_TRUE(geo.isObject());
      ASSERT_TRUE(arangodb::iresearch::parseShape<
                  arangodb::iresearch::Parsing::OnlyPoint>(
          geo, point, cache, false, arangodb::geo::coding::Options::kInvalid,
          nullptr));
      if (!shape.contains(point)) {
        continue;
      }

      auto name = doc.get("name");
      ASSERT_TRUE(name.isString());
      expected.emplace(arangodb::iresearch::getStringRef(name));
    }

    GeoFilter q;
    *q.mutable_field() = "geometry";
    ASSERT_TRUE(
        arangodb::iresearch::parseShape<arangodb::iresearch::Parsing::GeoJson>(
            shapeJson->slice(), q.mutable_options()->shape, cache, false,
            arangodb::geo::coding::Options::kInvalid, nullptr));
    q.mutable_options()->type = GeoFilterType::CONTAINS;
    q.mutable_options()->options.set_index_contains_points_only(true);

    EXPECT_EQ(expected, executeQuery(q, {18, 18}));
  }

  {
    auto shapeJson = VPackParser::fromJson(R"({
      "type": "Polygon",
        "coordinates": [
            [
                [37.590322, 55.695583],
                [37.626114, 55.695583],
                [37.626114, 55.71488],
                [37.590322, 55.71488],
                [37.590322, 55.695583]
            ]
      ]
    })");

    arangodb::geo::ShapeContainer shape;
    arangodb::geo::ShapeContainer point;
    std::vector<S2LatLng> cache;
    ASSERT_TRUE(
        arangodb::iresearch::parseShape<arangodb::iresearch::Parsing::GeoJson>(
            shapeJson->slice(), shape, cache, false,
            arangodb::geo::coding::Options::kInvalid, nullptr));
    std::set<std::string> expected;
    for (auto doc : VPackArrayIterator(docs->slice())) {
      auto geo = doc.get("geometry");
      ASSERT_TRUE(geo.isObject());
      ASSERT_TRUE(arangodb::iresearch::parseShape<
                  arangodb::iresearch::Parsing::OnlyPoint>(
          geo, point, cache, false, arangodb::geo::coding::Options::kInvalid,
          nullptr));
      if (!shape.contains(point)) {
        continue;
      }

      auto name = doc.get("name");
      ASSERT_TRUE(name.isString());
      expected.emplace(arangodb::iresearch::getStringRef(name));
    }

    GeoFilter q;
    *q.mutable_field() = "geometry";
    ASSERT_TRUE(
        arangodb::iresearch::parseShape<arangodb::iresearch::Parsing::GeoJson>(
            shapeJson->slice(), q.mutable_options()->shape, cache, false,
            arangodb::geo::coding::Options::kInvalid, nullptr));
    q.mutable_options()->type = GeoFilterType::INTERSECTS;

    EXPECT_EQ(expected, executeQuery(q, {18, 18}));
  }

  {
    auto shapeJson = VPackParser::fromJson(R"({
      "type": "Polygon",
        "coordinates": [
            [
                [37.590322, 55.695583],
                [37.626114, 55.695583],
                [37.626114, 55.71488],
                [37.590322, 55.71488],
                [37.590322, 55.695583]
            ]
      ]
    })");

    arangodb::geo::ShapeContainer shape;
    arangodb::geo::ShapeContainer point;
    std::set<std::string> expected;
    std::vector<S2LatLng> cache;

    GeoFilter q;
    *q.mutable_field() = "geometry";
    ASSERT_TRUE(
        arangodb::iresearch::parseShape<arangodb::iresearch::Parsing::GeoJson>(
            shapeJson->slice(), q.mutable_options()->shape, cache, false,
            arangodb::geo::coding::Options::kInvalid, nullptr));
    q.mutable_options()->type = GeoFilterType::IS_CONTAINED;

    EXPECT_EQ(expected, executeQuery(q, {18, 18}));
  }
}

TEST(GeoFilterTest, checkScorer) {
  auto docs = VPackParser::fromJson(R"([
    { "name": "A", "geometry": { "type": "Point", "coordinates": [ 37.615895, 55.7039   ] } },
    { "name": "B", "geometry": { "type": "Point", "coordinates": [ 37.615315, 55.703915 ] } },
    { "name": "C", "geometry": { "type": "Point", "coordinates": [ 37.61509, 55.703537  ] } },
    { "name": "D", "geometry": { "type": "Point", "coordinates": [ 37.614183, 55.703806 ] } },
    { "name": "E", "geometry": { "type": "Point", "coordinates": [ 37.613792, 55.704405 ] } },
    { "name": "F", "geometry": { "type": "Point", "coordinates": [ 37.614956, 55.704695 ] } },
    { "name": "G", "geometry": { "type": "Point", "coordinates": [ 37.616297, 55.704831 ] } },
    { "name": "H", "geometry": { "type": "Point", "coordinates": [ 37.617053, 55.70461  ] } },
    { "name": "I", "geometry": { "type": "Point", "coordinates": [ 37.61582, 55.704459  ] } },
    { "name": "J", "geometry": { "type": "Point", "coordinates": [ 37.614634, 55.704338 ] } },
    { "name": "K", "geometry": { "type": "Point", "coordinates": [ 37.613121, 55.704193 ] } },
    { "name": "L", "geometry": { "type": "Point", "coordinates": [ 37.614135, 55.703298 ] } },
    { "name": "M", "geometry": { "type": "Point", "coordinates": [ 37.613663, 55.704002 ] } },
    { "name": "N", "geometry": { "type": "Point", "coordinates": [ 37.616522, 55.704235 ] } },
    { "name": "O", "geometry": { "type": "Point", "coordinates": [ 37.615508, 55.704172 ] } },
    { "name": "P", "geometry": { "type": "Point", "coordinates": [ 37.614629, 55.704081 ] } },
    { "name": "Q", "geometry": { "type": "Point", "coordinates": [ 37.610235, 55.709754 ] } },
    { "name": "R", "geometry": { "type": "Point", "coordinates": [ 37.605,    55.707917 ] } },
    { "name": "S", "geometry": { "type": "Point", "coordinates": [ 37.545776, 55.722083 ] } },
    { "name": "T", "geometry": { "type": "Point", "coordinates": [ 37.559509, 55.715895 ] } },
    { "name": "U", "geometry": { "type": "Point", "coordinates": [ 37.701645, 55.832144 ] } },
    { "name": "V", "geometry": { "type": "Point", "coordinates": [ 37.73735,  55.816715 ] } },
    { "name": "W", "geometry": { "type": "Point", "coordinates": [ 37.75589,  55.798193 ] } },
    { "name": "X", "geometry": { "type": "Point", "coordinates": [ 37.659073, 55.843711 ] } },
    { "name": "Y", "geometry": { "type": "Point", "coordinates": [ 37.778549, 55.823659 ] } },
    { "name": "Z", "geometry": { "type": "Point", "coordinates": [ 37.729797, 55.853733 ] } },
    { "name": "1", "geometry": { "type": "Point", "coordinates": [ 37.608261, 55.784682 ] } },
    { "name": "2", "geometry": { "type": "Point", "coordinates": [ 37.525177, 55.802825 ] } }
  ])");

  irs::memory_directory dir;
  irs::DirectoryReader reader;

  // index data
  {
    constexpr auto formatId = arangodb::iresearch::getFormat(LinkVersion::MAX);
    auto codec = irs::formats::get(formatId);
    ASSERT_NE(nullptr, codec);
    auto writer = irs::IndexWriter::Make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);
    GeoField geoField;
    geoField.fieldName = "geometry";
    StringField nameField;
    nameField.fieldName = "name";
    {
      auto segment0 = writer->GetBatch();
      auto segment1 = writer->GetBatch();
      {
        size_t i = 0;
        for (auto docSlice : VPackArrayIterator(docs->slice())) {
          geoField.shapeSlice = docSlice.get("geometry");
          nameField.value = getStringRef(docSlice.get("name"));

          auto doc = (i++ % 2 ? segment0 : segment1).Insert();
          ASSERT_TRUE(
              doc.Insert<irs::Action::INDEX | irs::Action::STORE>(nameField));
          ASSERT_TRUE(
              doc.Insert<irs::Action::INDEX | irs::Action::STORE>(geoField));
        }
      }
    }
    writer->Commit();
    reader = writer->GetSnapshot();
  }

  ASSERT_NE(nullptr, reader);
  ASSERT_EQ(2, reader->size());
  ASSERT_EQ(docs->slice().length(), reader->docs_count());
  ASSERT_EQ(docs->slice().length(), reader->live_docs_count());

  auto executeQuery = [&reader](irs::filter const& q, irs::Scorers const& ord) {
    std::map<std::string, irs::bstring> actualResults;

    auto prepared = q.prepare({.index = *reader, .scorers = ord});
    EXPECT_NE(nullptr, prepared);
    for (auto& segment : *reader) {
      auto column = segment.column("name");
      EXPECT_NE(nullptr, column);
      auto column_it = column->iterator(irs::ColumnHint::kNormal);
      EXPECT_NE(nullptr, column_it);
      auto* payload = irs::get<irs::payload>(*column_it);
      EXPECT_NE(nullptr, payload);
      auto it = prepared->execute({.segment = segment, .scorers = ord});
      EXPECT_NE(nullptr, it);
      auto seek_it = prepared->execute({.segment = segment});
      EXPECT_NE(nullptr, seek_it);
      auto* cost = irs::get<irs::cost>(*it);
      EXPECT_NE(nullptr, cost);

      if (irs::doc_limits::eof(it->value())) {
        continue;
      }

      auto* score = irs::get<irs::score>(*it);
      EXPECT_NE(nullptr, score);
      EXPECT_FALSE(score->IsDefault());

      auto* doc = irs::get<irs::document>(*it);
      EXPECT_NE(nullptr, doc);
      EXPECT_FALSE(irs::doc_limits::valid(doc->value));
      EXPECT_FALSE(irs::doc_limits::valid(it->value()));
      while (it->next()) {
        auto const docId = it->value();
        EXPECT_EQ(docId, seek_it->seek(docId));
        EXPECT_EQ(docId, seek_it->seek(docId));
        EXPECT_EQ(docId, column_it->seek(docId));
        EXPECT_EQ(docId, doc->value);
        EXPECT_FALSE(irs::IsNull(payload->value));

        irs::bstring score_value(ord.score_size(), 0);
        (*score)(reinterpret_cast<irs::score_t*>(score_value.data()));

        actualResults.emplace(
            irs::to_string<std::string>(payload->value.data()),
            std::move(score_value));
      }
      EXPECT_TRUE(irs::doc_limits::eof(it->value()));
      EXPECT_TRUE(irs::doc_limits::eof(seek_it->seek(it->value())));

      {
        auto it = prepared->execute({.segment = segment, .scorers = ord});
        EXPECT_NE(nullptr, it);

        while (it->next()) {
          auto const docId = it->value();
          auto seek_it = prepared->execute({.segment = segment});
          EXPECT_NE(nullptr, seek_it);
          auto column_it = column->iterator(irs::ColumnHint::kNormal);
          EXPECT_NE(nullptr, column_it);
          auto* payload = irs::get<irs::payload>(*column_it);
          EXPECT_NE(nullptr, payload);
          EXPECT_EQ(docId, seek_it->seek(docId));
          do {
            EXPECT_EQ(seek_it->value(), column_it->seek(seek_it->value()));
            if (!irs::doc_limits::eof(column_it->value())) {
              EXPECT_NE(actualResults.end(),
                        actualResults.find(irs::to_string<std::string>(
                            payload->value.data())));
            }
          } while (seek_it->next());
          EXPECT_TRUE(irs::doc_limits::eof(seek_it->value()));
        }
        EXPECT_TRUE(irs::doc_limits::eof(it->value()));
      }
    }

    return actualResults;
  };

  auto encodeDocId = [](irs::doc_id_t id) {
    irs::bstring str(sizeof(irs::score_t), 0);
    *reinterpret_cast<irs::score_t*>(&str[0]) = static_cast<irs::score_t>(id);
    return str;
  };

  {
    auto json = VPackParser::fromJson(R"({
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

    GeoFilter q;
    q.mutable_options()->type = GeoFilterType::INTERSECTS;
    ASSERT_TRUE(
        geo::json::parseRegion(json->slice(), q.mutable_options()->shape, false)
            .ok());
    ASSERT_EQ(geo::ShapeContainer::Type::S2_POLYGON,
              q.mutable_options()->shape.type());
    *q.mutable_field() = "geometry";

    size_t collector_collect_field_count = 0;
    size_t collector_collect_term_count = 0;
    size_t collector_finish_count = 0;
    size_t scorer_score_count = 0;
    size_t prepare_scorer_count = 0;

    ::custom_sort sort;

    sort.fieldCollectorCollect = [&collector_collect_field_count, &q](
                                     const irs::SubReader&,
                                     const irs::term_reader& field) -> void {
      collector_collect_field_count += (q.field() == field.meta().name);
    };
    sort.termCollectorCollect = [&collector_collect_term_count, &q](
                                    const irs::SubReader&,
                                    const irs::term_reader& field,
                                    const irs::attribute_provider&) -> void {
      collector_collect_term_count += (q.field() == field.meta().name);
    };
    sort.collectorFinish = [&collector_finish_count](
                               irs::byte_type*, irs::FieldCollector const*,
                               irs::TermCollector const*) -> void {
      ++collector_finish_count;
    };
    sort.prepareScorer =
        [&prepare_scorer_count, &q](
            irs::ColumnProvider const&,
            std::map<irs::type_info::type_id, irs::field_id> const&,
            irs::byte_type const*, irs::attribute_provider const&,
            irs::score_t boost) {
          EXPECT_EQ(q.boost(), boost);
          ++prepare_scorer_count;
        };

    sort.scorerScore = [&scorer_score_count](irs::doc_id_t& score,
                                             irs::score_t* res) -> void {
      ASSERT_TRUE(res);
      *res = score;
      ++scorer_score_count;
    };

    std::map<std::string, irs::bstring> const expected{{"Q", encodeDocId(9)},
                                                       {"R", encodeDocId(9)}};

    ASSERT_EQ(expected, executeQuery(q, irs::Scorers::Prepare(sort)));
    ASSERT_EQ(2, collector_collect_field_count);  // 2 segments
    ASSERT_EQ(0, collector_collect_term_count);
    ASSERT_EQ(1, collector_finish_count);
    ASSERT_EQ(2, scorer_score_count);
  }

  {
    auto json = VPackParser::fromJson(R"({
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

    GeoFilter q;
    q.boost(1.5f);
    q.mutable_options()->type = GeoFilterType::INTERSECTS;
    ASSERT_TRUE(
        geo::json::parseRegion(json->slice(), q.mutable_options()->shape, false)
            .ok());
    ASSERT_EQ(geo::ShapeContainer::Type::S2_POLYGON,
              q.mutable_options()->shape.type());
    *q.mutable_field() = "geometry";

    size_t collector_collect_field_count = 0;
    size_t collector_collect_term_count = 0;
    size_t collector_finish_count = 0;
    size_t scorer_score_count = 0;
    size_t prepare_scorer_count = 0;

    ::custom_sort sort;

    sort.fieldCollectorCollect = [&collector_collect_field_count, &q](
                                     const irs::SubReader&,
                                     const irs::term_reader& field) -> void {
      collector_collect_field_count += (q.field() == field.meta().name);
    };
    sort.termCollectorCollect = [&collector_collect_term_count, &q](
                                    const irs::SubReader&,
                                    const irs::term_reader& field,
                                    const irs::attribute_provider&) -> void {
      collector_collect_term_count += (q.field() == field.meta().name);
    };
    sort.collectorFinish = [&collector_finish_count](
                               irs::byte_type*, const irs::FieldCollector*,
                               const irs::TermCollector*) -> void {
      ++collector_finish_count;
    };
    sort.prepareScorer =
        [&prepare_scorer_count, &q](
            const irs::ColumnProvider&,
            std::map<irs::type_info::type_id, irs::field_id> const&,
            irs::byte_type const*, irs::attribute_provider const&,
            irs::score_t boost) {
          EXPECT_EQ(q.boost(), boost);
          ++prepare_scorer_count;
        };

    sort.scorerScore = [&scorer_score_count](irs::doc_id_t& score,
                                             irs::score_t* res) -> void {
      ASSERT_TRUE(res != nullptr);
      *res = score;
      ++scorer_score_count;
    };

    std::map<std::string, irs::bstring> const expected{{"Q", encodeDocId(9)},
                                                       {"R", encodeDocId(9)}};

    ASSERT_EQ(expected, executeQuery(q, irs::Scorers::Prepare(sort)));
    ASSERT_EQ(2, collector_collect_field_count);  // 2 segments
    ASSERT_EQ(0, collector_collect_term_count);
    ASSERT_EQ(1, collector_finish_count);
    ASSERT_EQ(2, scorer_score_count);
  }
}
