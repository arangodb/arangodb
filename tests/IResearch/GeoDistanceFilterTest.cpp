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
#include "search/score.hpp"
#include "search/cost.hpp"
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

struct custom_sort : public irs::sort {
  static constexpr irs::string_ref type_name() noexcept {
    return "custom_sort";
  }

  class prepared : public irs::PreparedSortBase<void> {
   public:
    class field_collector : public irs::sort::field_collector {
     public:
      field_collector(const custom_sort& sort) : sort_(sort) {}

      virtual void collect(const irs::sub_reader& segment,
                           const irs::term_reader& field) override {
        if (sort_.field_collector_collect) {
          sort_.field_collector_collect(segment, field);
        }
      }

      virtual void collect(irs::bytes_ref) override {}

      virtual void reset() override {}

      virtual void write(irs::data_output& out) const override {}

     private:
      const custom_sort& sort_;
    };

    class term_collector : public irs::sort::term_collector {
     public:
      term_collector(const custom_sort& sort) : sort_(sort) {}

      virtual void collect(const irs::sub_reader& segment,
                           const irs::term_reader& field,
                           const irs::attribute_provider& term_attrs) override {
        if (sort_.term_collector_collect) {
          sort_.term_collector_collect(segment, field, term_attrs);
        }
      }

      virtual void collect(irs::bytes_ref in) override {}

      virtual void reset() override {}

      virtual void write(irs::data_output& out) const override {}

     private:
      const custom_sort& sort_;
    };

    struct scorer : public irs::score_ctx {
      scorer(const custom_sort& sort, const irs::sub_reader& segment_reader,
             const irs::term_reader& term_reader, const irs::byte_type* stats,
             const irs::attribute_provider& document_attrs)
          : document_attrs_(document_attrs),
            stats_(stats),
            segment_reader_(segment_reader),
            sort_(sort),
            term_reader_(term_reader) {}

      const irs::attribute_provider& document_attrs_;
      const irs::byte_type* stats_;
      const irs::sub_reader& segment_reader_;
      const custom_sort& sort_;
      const irs::term_reader& term_reader_;
    };

    prepared(const custom_sort& sort) : sort_(sort) {}

    virtual void collect(irs::byte_type* filter_attrs,
                         const irs::index_reader& index,
                         const irs::sort::field_collector* field,
                         const irs::sort::term_collector* term) const override {
      if (sort_.collector_finish) {
        sort_.collector_finish(filter_attrs, index, field, term);
      }
    }

    virtual irs::IndexFeatures features() const override {
      return irs::IndexFeatures::NONE;
    }

    virtual irs::sort::field_collector::ptr prepare_field_collector()
        const override {
      if (sort_.prepare_field_collector) {
        return sort_.prepare_field_collector();
      }

      return irs::memory::make_unique<custom_sort::prepared::field_collector>(
          sort_);
    }

    virtual irs::ScoreFunction prepare_scorer(
        irs::sub_reader const& segment_reader,
        irs::term_reader const& term_reader,
        irs::byte_type const* filter_node_attrs,
        irs::attribute_provider const& document_attrs,
        irs::score_t boost) const override {
      if (sort_.prepare_scorer) {
        sort_.prepare_scorer(segment_reader, term_reader, filter_node_attrs,
                             document_attrs, boost);
      }

      return {
          std::make_unique<custom_sort::prepared::scorer>(
              sort_, segment_reader, term_reader, filter_node_attrs,
              document_attrs),
          [](irs::score_ctx* ctx, irs::score_t* res) {
            auto& ctxImpl =
                *reinterpret_cast<const custom_sort::prepared::scorer*>(ctx);

            auto doc_id =
                irs::get<irs::document>(ctxImpl.document_attrs_)->value;

            if (ctxImpl.sort_.scorer_score) {
              ctxImpl.sort_.scorer_score(doc_id, res);
            }
          }};
    }

    virtual irs::sort::term_collector::ptr prepare_term_collector()
        const override {
      if (sort_.prepare_term_collector) {
        return sort_.prepare_term_collector();
      }

      return irs::memory::make_unique<custom_sort::prepared::term_collector>(
          sort_);
    }

   private:
    const custom_sort& sort_;
  };

  std::function<void(const irs::sub_reader&, const irs::term_reader&)>
      field_collector_collect;
  std::function<void(const irs::sub_reader&, const irs::term_reader&,
                     const irs::attribute_provider&)>
      term_collector_collect;
  std::function<void(irs::byte_type*, const irs::index_reader&,
                     const irs::sort::field_collector*,
                     const irs::sort::term_collector*)>
      collector_finish;
  std::function<irs::sort::field_collector::ptr()> prepare_field_collector;
  std::function<void(const irs::sub_reader&, const irs::term_reader&,
                     const irs::byte_type*, const irs::attribute_provider&,
                     irs::score_t)>
      prepare_scorer;
  std::function<irs::sort::term_collector::ptr()> prepare_term_collector;
  std::function<void(irs::doc_id_t&, irs::score_t*)> scorer_score;

  static ptr make();
  custom_sort() : sort(irs::type<custom_sort>::get()) {}
  virtual prepared::ptr prepare() const override {
    return std::make_unique<custom_sort::prepared>(*this);
  }
};

}  // namespace

TEST(GeoDistanceFilterTest, options) {
  S2RegionTermIndexer::Options const s2opts;
  GeoDistanceFilterOptions const opts;
  ASSERT_TRUE(opts.prefix.empty());
  ASSERT_EQ(0., opts.range.min);
  ASSERT_EQ(irs::BoundType::UNBOUNDED, opts.range.min_type);
  ASSERT_EQ(0., opts.range.max);
  ASSERT_EQ(irs::BoundType::UNBOUNDED, opts.range.max_type);
  ASSERT_EQ(S2Point{}, opts.origin);
  ASSERT_EQ(s2opts.level_mod(), opts.options.level_mod());
  ASSERT_EQ(s2opts.min_level(), opts.options.min_level());
  ASSERT_EQ(s2opts.max_level(), opts.options.max_level());
  ASSERT_EQ(s2opts.max_cells(), opts.options.max_cells());
  ASSERT_EQ(s2opts.marker(), opts.options.marker());
  ASSERT_EQ(s2opts.index_contains_points_only(),
            opts.options.index_contains_points_only());
  ASSERT_EQ(s2opts.optimize_for_space(), opts.options.optimize_for_space());
}

TEST(GeoDistanceFilterTest, ctor) {
  GeoDistanceFilter q;
  ASSERT_EQ(irs::type<GeoDistanceFilter>::id(), q.type());
  ASSERT_EQ("", q.field());
  ASSERT_EQ(irs::kNoBoost, q.boost());
#ifndef ARANGODB_ENABLE_MAINTAINER_MODE
  ASSERT_EQ(GeoDistanceFilterOptions{}, q.options());
#endif
}

TEST(GeoDistanceFilterTest, equal) {
  GeoDistanceFilter q;
  q.mutable_options()->origin = S2Point{1., 2., 3.};
  q.mutable_options()->range.min = 5000.;
  q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
  q.mutable_options()->range.max = 7000.;
  q.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
  *q.mutable_field() = "field";

  {
    GeoDistanceFilter q1;
    q1.mutable_options()->origin = S2Point{1., 2., 3.};
    q1.mutable_options()->range.min = 5000.;
    q1.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q1.mutable_options()->range.max = 7000.;
    q1.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q1.mutable_field() = "field";

    ASSERT_EQ(q, q1);
    ASSERT_EQ(q.hash(), q1.hash());
  }

  {
    GeoDistanceFilter q1;
    q1.boost(1.5);
    q1.mutable_options()->origin = S2Point{1., 2., 3.};
    q1.mutable_options()->range.min = 5000.;
    q1.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q1.mutable_options()->range.max = 7000.;
    q1.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q1.mutable_field() = "field";

    ASSERT_EQ(q, q1);
    ASSERT_EQ(q.hash(), q1.hash());
  }

  {
    GeoDistanceFilter q1;
    q1.boost(1.5);
    q1.mutable_options()->origin = S2Point{1., 2., 3.};
    q1.mutable_options()->range.min = 5000.;
    q1.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q1.mutable_options()->range.max = 7000.;
    q1.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q1.mutable_field() = "field1";

    ASSERT_NE(q, q1);
  }

  {
    GeoDistanceFilter q1;
    q1.mutable_options()->origin = S2Point{1., 2., 3.};
    q1.mutable_options()->range.min = 5000.;
    q1.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    q1.mutable_options()->range.max = 7000.;
    q1.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q1.mutable_field() = "field";

    ASSERT_NE(q, q1);
  }

  {
    GeoDistanceFilter q1;
    q1.mutable_options()->origin = S2Point{1., 2., 3.};
    q1.mutable_options()->range.min = 6000.;
    q1.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q1.mutable_options()->range.max = 7000.;
    q1.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q1.mutable_field() = "field";

    ASSERT_NE(q, q1);
  }

  {
    GeoDistanceFilter q1;
    q1.mutable_options()->origin = S2Point{1., 2., 3.};
    q1.mutable_options()->range.min = 5000.;
    q1.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q1.mutable_options()->range.max = 7000.;
    q1.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
    *q1.mutable_field() = "field";

    ASSERT_NE(q, q1);
  }

  {
    GeoDistanceFilter q1;
    q1.mutable_options()->origin = S2Point{1., 2., 3.};
    q1.mutable_options()->range.min = 5000.;
    q1.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q1.mutable_options()->range.max = 6000.;
    q1.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q1.mutable_field() = "field";

    ASSERT_NE(q, q1);
  }

  {
    GeoDistanceFilter q1;
    q1.mutable_options()->origin = S2Point{2., 2., 3.};
    q1.mutable_options()->range.min = 5000.;
    q1.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    q1.mutable_options()->range.max = 7000.;
    q1.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q1.mutable_field() = "field";

    ASSERT_NE(q, q1);
  }
}

TEST(GeoDistanceFilterTest, boost) {
  {
    GeoDistanceFilter q;
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(-41.69642, 77.91159).ToPoint();
    q.mutable_options()->range.min = 5000.;
    q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    *q.mutable_field() = "field";

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(irs::kNoBoost, prepared->boost());
  }

  {
    GeoDistanceFilter q;
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(-41.69642, 77.91159).ToPoint();
    q.mutable_options()->range.min = 5000.;
    q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q.mutable_options()->range.max = 6000.;
    q.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q.mutable_field() = "field";

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(irs::kNoBoost, prepared->boost());
  }

  {
    irs::score_t boost = 1.5f;
    GeoDistanceFilter q;
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(-41.69642, 77.91159).ToPoint();
    q.mutable_options()->range.min = 5000.;
    q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    *q.mutable_field() = "field";
    q.boost(boost);

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(boost, prepared->boost());
  }

  {
    irs::score_t boost = 1.5f;
    GeoDistanceFilter q;
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(-41.69642, 77.91159).ToPoint();
    q.mutable_options()->range.min = 5000.;
    q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q.mutable_options()->range.max = 6000.;
    q.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    *q.mutable_field() = "field";
    q.boost(boost);

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(boost, prepared->boost());
  }
}

TEST(GeoDistanceFilterTest, query) {
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

  // index data
  {
    constexpr auto formatId =
        arangodb::iresearch::getFormat(arangodb::iresearch::LinkVersion::MAX);
    auto codec = irs::formats::get(formatId);
    ASSERT_NE(nullptr, codec);
    auto writer = irs::index_writer::make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);
    GeoField geoField;
    geoField.fieldName = "geometry";
    StringField nameField;
    nameField.fieldName = "name";
    {
      auto segment0 = writer->documents();
      auto segment1 = writer->documents();
      {
        size_t i = 0;
        for (auto docSlice : VPackArrayIterator(docs->slice())) {
          geoField.shapeSlice = docSlice.get("geometry");
          nameField.value = getStringRef(docSlice.get("name"));

          auto doc = (i++ % 2 ? segment0 : segment1).insert();
          ASSERT_TRUE(
              doc.insert<irs::Action::INDEX | irs::Action::STORE>(nameField));
          ASSERT_TRUE(
              doc.insert<irs::Action::INDEX | irs::Action::STORE>(geoField));
        }
      }
    }
    writer->commit();
  }

  auto reader = irs::directory_reader::open(dir);
  ASSERT_NE(nullptr, reader);
  ASSERT_EQ(2, reader->size());
  ASSERT_EQ(docs->slice().length(), reader->docs_count());
  ASSERT_EQ(docs->slice().length(), reader->live_docs_count());

  auto executeQuery = [&reader](irs::filter const& q,
                                std::vector<irs::cost::cost_t> const& costs) {
    std::set<std::string> actualResults;

    auto prepared = q.prepare(*reader);
    EXPECT_NE(nullptr, prepared);
    auto expectedCost = costs.begin();
    for (auto& segment : *reader) {
      auto column = segment.column("name");
      EXPECT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      EXPECT_NE(nullptr, values);
      auto* value = irs::get<irs::payload>(*values);
      EXPECT_NE(nullptr, value);
      auto it = prepared->execute(segment);
      EXPECT_NE(nullptr, it);
      auto seek_it = prepared->execute(segment);
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
      EXPECT_EQ(*score, irs::ScoreFunction::kDefault);

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
        EXPECT_FALSE(value->value.null());

        actualResults.emplace(
            irs::to_string<std::string>(value->value.c_str()));
      }
      EXPECT_TRUE(irs::doc_limits::eof(it->value()));
      EXPECT_TRUE(irs::doc_limits::eof(seek_it->seek(it->value())));

      {
        auto it = prepared->execute(segment);
        EXPECT_NE(nullptr, it);

        while (it->next()) {
          auto const docId = it->value();
          auto seek_it = prepared->execute(segment);
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
                            payload->value.c_str())));
            }
          } while (seek_it->next());
          EXPECT_TRUE(irs::doc_limits::eof(seek_it->value()));
        }
        EXPECT_TRUE(irs::doc_limits::eof(it->value()));
      }
    }
    EXPECT_EQ(expectedCost, costs.end());

    return actualResults;
  };

  {
    std::set<std::string> const expected{"Q", "R"};

    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.70892, 37.607768).ToPoint();
    auto& range = q.mutable_options()->range;
    range.max_type = irs::BoundType::INCLUSIVE;
    range.max = 300;

    ASSERT_EQ(expected, executeQuery(q, {2, 2}));
  }

  {
    std::set<std::string> const expected{"Q"};

    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.709754, 37.610235).ToPoint();
    auto& range = q.mutable_options()->range;
    range.min_type = irs::BoundType::INCLUSIVE;
    range.max = 0;
    range.max_type = irs::BoundType::INCLUSIVE;
    range.min = 0;

    ASSERT_EQ(expected, executeQuery(q, {2, 0}));
  }

  {
    std::set<std::string> const expected{};

    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.709754, 37.610235).ToPoint();
    auto& range = q.mutable_options()->range;
    range.min_type = irs::BoundType::EXCLUSIVE;
    range.max = 0;
    range.max_type = irs::BoundType::INCLUSIVE;
    range.min = 0;

    ASSERT_EQ(expected, executeQuery(q, {0, 0}));
  }

  {
    std::set<std::string> const expected{};

    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.709754, 37.610235).ToPoint();
    auto& range = q.mutable_options()->range;
    range.min_type = irs::BoundType::INCLUSIVE;
    range.max = 0;
    range.max_type = irs::BoundType::EXCLUSIVE;
    range.min = 0;

    ASSERT_EQ(expected, executeQuery(q, {0, 0}));
  }

  {
    std::set<std::string> const expected{};

    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.709754, 37.610235).ToPoint();
    auto& range = q.mutable_options()->range;
    range.min_type = irs::BoundType::EXCLUSIVE;
    range.max = 0;
    range.max_type = irs::BoundType::EXCLUSIVE;
    range.min = 0;

    ASSERT_EQ(expected, executeQuery(q, {0, 0}));
  }

  {
    std::set<std::string> expected;
    for (auto doc : VPackArrayIterator(docs->slice())) {
      auto name = doc.get("name");
      ASSERT_TRUE(name.isString());
      expected.emplace(arangodb::iresearch::getStringRef(name));
    }

    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.709754, 37.610235).ToPoint();
    auto& range = q.mutable_options()->range;
    range.min_type = irs::BoundType::UNBOUNDED;
    range.max = 0;
    range.max_type = irs::BoundType::UNBOUNDED;
    range.min = 0;

    ASSERT_EQ(expected,
              executeQuery(q, {expected.size() / 2, expected.size() / 2}));
  }

  {
    std::set<std::string> expected;
    for (auto doc : VPackArrayIterator(docs->slice())) {
      auto name = arangodb::iresearch::getStringRef(doc.get("name"));

      if (name == "Q") {
        continue;
      }

      expected.emplace(name);
    }

    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.709754, 37.610235).ToPoint();
    auto& range = q.mutable_options()->range;
    range.max_type = irs::BoundType::UNBOUNDED;
    range.min_type = irs::BoundType::EXCLUSIVE;
    range.min = 0;

    ASSERT_EQ(expected, executeQuery(q, {14, 14}));
  }

  {
    auto origin = docs->slice().at(7).get("geometry");
    ASSERT_TRUE(origin.isObject());
    arangodb::geo::ShapeContainer lhs, rhs;
    ASSERT_TRUE(arangodb::iresearch::parseShape(origin, lhs, true));
    std::set<std::string> expected;
    for (auto doc : VPackArrayIterator(docs->slice())) {
      auto geo = doc.get("geometry");
      ASSERT_TRUE(geo.isObject());
      ASSERT_TRUE(arangodb::iresearch::parseShape(geo, rhs, true));
      auto const dist = lhs.distanceFromCentroid(rhs.centroid());
      if (dist < 100 || dist > 2000) {
        continue;
      }

      auto name = doc.get("name");
      ASSERT_TRUE(name.isString());
      expected.emplace(arangodb::iresearch::getStringRef(name));
    }

    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.70461, 37.617053).ToPoint();
    auto& range = q.mutable_options()->range;
    range.min_type = irs::BoundType::INCLUSIVE;
    range.min = 100;
    range.max_type = irs::BoundType::INCLUSIVE;
    range.max = 2000;

    ASSERT_EQ(expected, executeQuery(q, {18, 18}));
  }

  {
    auto origin = docs->slice().at(7).get("geometry");
    ASSERT_TRUE(origin.isObject());
    arangodb::geo::ShapeContainer lhs, rhs;
    ASSERT_TRUE(arangodb::iresearch::parseShape(origin, lhs, true));
    std::set<std::string> expected;
    for (auto doc : VPackArrayIterator(docs->slice())) {
      auto geo = doc.get("geometry");
      ASSERT_TRUE(geo.isObject());
      ASSERT_TRUE(arangodb::iresearch::parseShape(geo, rhs, true));
      auto const dist = lhs.distanceFromCentroid(rhs.centroid());
      if (dist >= 2000) {
        continue;
      }

      auto name = doc.get("name");
      ASSERT_TRUE(name.isString());
      expected.emplace(arangodb::iresearch::getStringRef(name));
    }

    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.70461, 37.617053).ToPoint();
    auto& range = q.mutable_options()->range;
    range.min_type = irs::BoundType::INCLUSIVE;
    range.min = -100;
    range.max_type = irs::BoundType::INCLUSIVE;
    range.max = 2000;

    ASSERT_EQ(expected, executeQuery(q, {18, 18}));
  }

  {
    std::set<std::string> expected;

    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.70461, 37.617053).ToPoint();
    auto& range = q.mutable_options()->range;
    range.min_type = irs::BoundType::INCLUSIVE;
    range.min = 100;
    range.max_type = irs::BoundType::INCLUSIVE;
    range.max = -2000;

    ASSERT_EQ(expected, executeQuery(q, {0, 0}));
  }

  {
    std::set<std::string> expected;

    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.70461, 37.617053).ToPoint();
    auto& range = q.mutable_options()->range;
    range.min_type = irs::BoundType::UNBOUNDED;
    range.max_type = irs::BoundType::INCLUSIVE;
    range.max = -2000;

    ASSERT_EQ(expected, executeQuery(q, {0, 0}));
  }

  {
    std::set<std::string> expected;
    for (auto doc : VPackArrayIterator(docs->slice())) {
      auto geo = doc.get("geometry");
      ASSERT_TRUE(geo.isObject());

      auto name = doc.get("name");
      ASSERT_TRUE(name.isString());
      expected.emplace(arangodb::iresearch::getStringRef(name));
    }

    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.70461, 37.617053).ToPoint();

    ASSERT_EQ(expected,
              executeQuery(q, {expected.size() / 2, expected.size() / 2}));
  }

  {
    std::set<std::string> expected;
    for (auto doc : VPackArrayIterator(docs->slice())) {
      auto geo = doc.get("geometry");
      ASSERT_TRUE(geo.isObject());

      auto name = doc.get("name");
      ASSERT_TRUE(name.isString());
      expected.emplace(arangodb::iresearch::getStringRef(name));
    }

    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.70461, 37.617053).ToPoint();
    auto& range = q.mutable_options()->range;
    range.min_type = irs::BoundType::INCLUSIVE;
    range.min = -100;
    range.max_type = irs::BoundType::UNBOUNDED;

    ASSERT_EQ(expected,
              executeQuery(q, {expected.size() / 2, expected.size() / 2}));
  }

  {
    std::set<std::string> expected;

    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.70461, 37.617053).ToPoint();
    auto& range = q.mutable_options()->range;
    range.min_type = irs::BoundType::INCLUSIVE;
    range.min = -100;
    range.max_type = irs::BoundType::INCLUSIVE;
    range.max = -2000;

    ASSERT_EQ(expected, executeQuery(q, {0, 0}));
  }

  {
    auto origin = docs->slice().at(7).get("geometry");
    ASSERT_TRUE(origin.isObject());
    arangodb::geo::ShapeContainer lhs, rhs;
    ASSERT_TRUE(arangodb::iresearch::parseShape(origin, lhs, true));
    std::set<std::string> expected;
    for (auto doc : VPackArrayIterator(docs->slice())) {
      auto geo = doc.get("geometry");
      ASSERT_TRUE(geo.isObject());
      ASSERT_TRUE(arangodb::iresearch::parseShape(geo, rhs, true));
      auto const dist = lhs.distanceFromCentroid(rhs.centroid());
      if (dist <= 2000) {
        continue;
      }

      auto name = doc.get("name");
      ASSERT_TRUE(name.isString());
      expected.emplace(arangodb::iresearch::getStringRef(name));
    }

    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.70461, 37.617053).ToPoint();
    auto& range = q.mutable_options()->range;
    range.min_type = irs::BoundType::EXCLUSIVE;
    range.min = 2000;
    range.max_type = irs::BoundType::UNBOUNDED;
    range.max = 2000;

    ASSERT_EQ(expected, executeQuery(q, {28, 28}));
  }

  {
    std::set<std::string> expected;
    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.70461, 37.617053).ToPoint();
    auto& range = q.mutable_options()->range;
    range.min_type = irs::BoundType::INCLUSIVE;
    range.min = 2000;
    range.max_type = irs::BoundType::INCLUSIVE;
    range.max = 100;

    ASSERT_EQ(expected, executeQuery(q, {0, 0}));
  }

  {
    std::set<std::string> expected;
    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.70461, 37.617053).ToPoint();
    auto& range = q.mutable_options()->range;
    range.min_type = irs::BoundType::INCLUSIVE;
    range.min = 2000;
    range.max_type = irs::BoundType::EXCLUSIVE;
    range.max = 2000;

    ASSERT_EQ(expected, executeQuery(q, {0, 0}));
  }
}

TEST(GeoDistanceFilterTest, checkScorer) {
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

  // index data
  {
    constexpr auto formatId =
        arangodb::iresearch::getFormat(arangodb::iresearch::LinkVersion::MAX);
    auto codec = irs::formats::get(formatId);
    ASSERT_NE(nullptr, codec);
    auto writer = irs::index_writer::make(dir, codec, irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);
    GeoField geoField;
    geoField.fieldName = "geometry";
    StringField nameField;
    nameField.fieldName = "name";
    {
      auto segment0 = writer->documents();
      auto segment1 = writer->documents();
      {
        size_t i = 0;
        for (auto docSlice : VPackArrayIterator(docs->slice())) {
          geoField.shapeSlice = docSlice.get("geometry");
          nameField.value = getStringRef(docSlice.get("name"));

          auto doc = (i++ % 2 ? segment0 : segment1).insert();
          ASSERT_TRUE(
              doc.insert<irs::Action::INDEX | irs::Action::STORE>(nameField));
          ASSERT_TRUE(
              doc.insert<irs::Action::INDEX | irs::Action::STORE>(geoField));
        }
      }
    }
    writer->commit();
  }

  auto reader = irs::directory_reader::open(dir);
  ASSERT_NE(nullptr, reader);
  ASSERT_EQ(2, reader->size());
  ASSERT_EQ(docs->slice().length(), reader->docs_count());
  ASSERT_EQ(docs->slice().length(), reader->live_docs_count());

  auto executeQuery = [&reader](irs::filter const& q, irs::Order const& ord) {
    std::map<std::string, irs::bstring> actualResults;

    auto prepared = q.prepare(*reader, ord);
    EXPECT_NE(nullptr, prepared);
    for (auto& segment : *reader) {
      auto column = segment.column("name");
      EXPECT_NE(nullptr, column);
      auto column_it = column->iterator(irs::ColumnHint::kNormal);
      EXPECT_NE(nullptr, column_it);
      auto* payload = irs::get<irs::payload>(*column_it);
      EXPECT_NE(nullptr, payload);
      auto it = prepared->execute(segment, ord);
      EXPECT_NE(nullptr, it);
      auto seek_it = prepared->execute(segment, ord);
      EXPECT_NE(nullptr, seek_it);
      auto* cost = irs::get<irs::cost>(*it);
      EXPECT_NE(nullptr, cost);

      if (irs::doc_limits::eof(it->value())) {
        continue;
      }

      auto* score = irs::get<irs::score>(*it);
      EXPECT_NE(nullptr, score);
      EXPECT_NE(*score, irs::ScoreFunction::kDefault);
      auto* seek_score = irs::get<irs::score>(*seek_it);
      EXPECT_NE(nullptr, seek_score);
      EXPECT_NE(*seek_score, irs::ScoreFunction::kDefault);

      auto* doc = irs::get<irs::document>(*it);
      EXPECT_NE(nullptr, doc);
      EXPECT_FALSE(irs::doc_limits::valid(doc->value));
      EXPECT_FALSE(irs::doc_limits::valid(it->value()));
      while (it->next()) {
        auto const docId = it->value();
        EXPECT_EQ(docId, seek_it->seek(docId));
        EXPECT_EQ(docId, column_it->seek(docId));
        EXPECT_FALSE(payload->value.null());

        irs::bstring score_value(ord.score_size(), 0);
        irs::bstring seek_score_value(ord.score_size(), 0);

        (*score)(reinterpret_cast<irs::score_t*>(score_value.data()));
        (*seek_score)(reinterpret_cast<irs::score_t*>(seek_score_value.data()));

        EXPECT_EQ(score_value, seek_score_value);

        actualResults.emplace(
            irs::to_string<std::string>(payload->value.c_str()),
            std::move(score_value));
      }
      EXPECT_TRUE(irs::doc_limits::eof(it->value()));
      EXPECT_TRUE(irs::doc_limits::eof(seek_it->seek(it->value())));

      {
        auto it = prepared->execute(segment, ord);
        EXPECT_NE(nullptr, it);

        while (it->next()) {
          auto const docId = it->value();
          auto seek_it = prepared->execute(segment, ord);
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
                            payload->value.c_str())));
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
    GeoDistanceFilter q;
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.70892, 37.607768).ToPoint();
    auto& range = q.mutable_options()->range;
    range.max_type = irs::BoundType::INCLUSIVE;
    range.max = 300;

    size_t collector_collect_field_count = 0;
    size_t collector_collect_term_count = 0;
    size_t collector_finish_count = 0;
    size_t scorer_score_count = 0;
    size_t prepare_scorer_count = 0;

    ::custom_sort sort;

    sort.field_collector_collect = [&collector_collect_field_count, &q](
                                       const irs::sub_reader&,
                                       const irs::term_reader& field) -> void {
      collector_collect_field_count += (q.field() == field.meta().name);
    };
    sort.term_collector_collect = [&collector_collect_term_count, &q](
                                      const irs::sub_reader&,
                                      const irs::term_reader& field,
                                      const irs::attribute_provider&) -> void {
      collector_collect_term_count += (q.field() == field.meta().name);
    };
    sort.collector_finish = [&collector_finish_count](
                                irs::byte_type*, const irs::index_reader&,
                                const irs::sort::field_collector*,
                                const irs::sort::term_collector*) -> void {
      ++collector_finish_count;
    };
    sort.prepare_scorer = [&prepare_scorer_count, &q](
                              const irs::sub_reader&, const irs::term_reader&,
                              const irs::byte_type*,
                              const irs::attribute_provider&,
                              irs::score_t boost) {
      EXPECT_EQ(q.boost(), boost);
      ++prepare_scorer_count;
    };
    sort.scorer_score = [&scorer_score_count](irs::doc_id_t& score,
                                              irs::score_t* res) {
      ASSERT_TRUE(res);
      ++scorer_score_count;
      *res = score;
    };

    std::map<std::string, irs::bstring> const expected{{"Q", encodeDocId(9)},
                                                       {"R", encodeDocId(9)}};

    ASSERT_EQ(expected, executeQuery(q, irs::Order::Prepare(sort)));
    ASSERT_EQ(2, collector_collect_field_count);  // 2 segments
    ASSERT_EQ(0, collector_collect_term_count);
    ASSERT_EQ(1, collector_finish_count);
    ASSERT_EQ(4, scorer_score_count);
  }

  {
    GeoDistanceFilter q;
    q.boost(1.5f);
    *q.mutable_field() = "geometry";
    q.mutable_options()->origin =
        S2LatLng::FromDegrees(55.70892, 37.607768).ToPoint();
    auto& range = q.mutable_options()->range;
    range.max_type = irs::BoundType::INCLUSIVE;
    range.max = 300;

    size_t collector_collect_field_count = 0;
    size_t collector_collect_term_count = 0;
    size_t collector_finish_count = 0;
    size_t scorer_score_count = 0;
    size_t prepare_scorer_count = 0;

    ::custom_sort sort;

    sort.field_collector_collect = [&collector_collect_field_count, &q](
                                       const irs::sub_reader&,
                                       const irs::term_reader& field) -> void {
      collector_collect_field_count += (q.field() == field.meta().name);
    };
    sort.term_collector_collect = [&collector_collect_term_count, &q](
                                      const irs::sub_reader&,
                                      const irs::term_reader& field,
                                      const irs::attribute_provider&) -> void {
      collector_collect_term_count += (q.field() == field.meta().name);
    };
    sort.collector_finish = [&collector_finish_count](
                                irs::byte_type*, const irs::index_reader&,
                                const irs::sort::field_collector*,
                                const irs::sort::term_collector*) -> void {
      ++collector_finish_count;
    };
    sort.prepare_scorer = [&prepare_scorer_count, &q](
                              const irs::sub_reader&, const irs::term_reader&,
                              const irs::byte_type*,
                              const irs::attribute_provider&,
                              irs::score_t boost) {
      EXPECT_EQ(q.boost(), boost);
      ++prepare_scorer_count;
    };
    sort.scorer_score = [&scorer_score_count](irs::doc_id_t& score,
                                              irs::score_t* res) {
      ASSERT_TRUE(res);
      ++scorer_score_count;
      *res = score;
    };

    std::map<std::string, irs::bstring> const expected{{"Q", encodeDocId(9)},
                                                       {"R", encodeDocId(9)}};

    ASSERT_EQ(expected, executeQuery(q, irs::Order::Prepare(sort)));
    ASSERT_EQ(2, collector_collect_field_count);  // 2 segments
    ASSERT_EQ(0, collector_collect_term_count);
    ASSERT_EQ(1, collector_finish_count);
    ASSERT_EQ(4, scorer_score_count);
  }
}
