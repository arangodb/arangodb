////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>

#include "Inspection/Types.h"
#include "Inspection/VPack.h"

namespace {
namespace inspection = arangodb::inspection;
namespace velocypack = arangodb::velocypack;

enum class IndexType {
  Persistent,
  Hash,
  Skiplist,
  Ttl,
  Geo,
  Geo1,
  Geo2,
  Mdi,
  Zkd,
  Vector,
};

struct IndexCommonProperties {
  std::string name;
  std::vector<std::string> fields;
  bool unique{false};
  bool sparse{false};

  bool operator==(IndexCommonProperties const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, IndexCommonProperties& p) {
  return f.object(p).fields(f.field("name", p.name).fallback(f.keep()),
                            f.field("fields", p.fields),
                            f.field("unique", p.unique).fallback(false),
                            f.field("sparse", p.sparse).fallback(false));
}

struct PersistentIndexDescriptor {
  static constexpr std::string_view name = "persistent";
  static constexpr IndexType type = IndexType::Persistent;
  IndexCommonProperties common;
  bool deduplicate{true};
  bool estimates{true};

  bool operator==(PersistentIndexDescriptor const&) const = default;
};

// Aliases add no fields, so the base's inspect covers them.
struct HashIndexDescriptor : PersistentIndexDescriptor {
  static constexpr std::string_view name = "hash";
  static constexpr IndexType type = IndexType::Hash;
};

struct SkiplistIndexDescriptor : PersistentIndexDescriptor {
  static constexpr std::string_view name = "skiplist";
  static constexpr IndexType type = IndexType::Skiplist;
};

struct TtlIndexDescriptor : SkiplistIndexDescriptor {
  static constexpr std::string_view name = "ttl";
  static constexpr IndexType type = IndexType::Ttl;
  double expireAfter{0};

  bool operator==(TtlIndexDescriptor const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, PersistentIndexDescriptor& d) {
  return f.object(d).fields(
      f.embedFields(d.common),
      f.field("deduplicate", d.deduplicate).fallback(true),
      f.field("estimates", d.estimates).fallback(true));
}

template<class Inspector>
auto inspect(Inspector& f, TtlIndexDescriptor& d) {
  return f.object(d).fields(
      f.embedFields(static_cast<SkiplistIndexDescriptor&>(d)),
      f.field("expireAfter", d.expireAfter));
}

struct GeoIndexDescriptor {
  static constexpr std::string_view name = "geo";
  static constexpr IndexType type = IndexType::Geo;
  IndexCommonProperties common;
  bool geoJson{false};

  bool operator==(GeoIndexDescriptor const&) const = default;
};

struct Geo1IndexDescriptor : GeoIndexDescriptor {
  static constexpr std::string_view name = "geo1";
  static constexpr IndexType type = IndexType::Geo1;
};

struct Geo2IndexDescriptor : GeoIndexDescriptor {
  static constexpr std::string_view name = "geo2";
  static constexpr IndexType type = IndexType::Geo2;
};

template<class Inspector>
auto inspect(Inspector& f, GeoIndexDescriptor& d) {
  return f.object(d).fields(f.embedFields(d.common),
                            f.field("geoJson", d.geoJson).fallback(false));
}

struct MdiIndexDescriptor {
  static constexpr std::string_view name = "mdi";
  static constexpr IndexType type = IndexType::Mdi;
  IndexCommonProperties common;
  std::string fieldValueTypes;

  bool operator==(MdiIndexDescriptor const&) const = default;
};

struct ZkdIndexDescriptor : MdiIndexDescriptor {
  static constexpr std::string_view name = "zkd";
  static constexpr IndexType type = IndexType::Zkd;
};

template<class Inspector>
auto inspect(Inspector& f, MdiIndexDescriptor& d) {
  return f.object(d).fields(f.embedFields(d.common),
                            f.field("fieldValueTypes", d.fieldValueTypes));
}

struct VectorParams {
  std::uint64_t dimension{0};
  std::string metric;
  std::uint64_t nLists{0};

  bool operator==(VectorParams const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, VectorParams& p) {
  return f.object(p).fields(f.field("dimension", p.dimension),
                            f.field("metric", p.metric),
                            f.field("nLists", p.nLists));
}

struct VectorIndexDescriptor {
  static constexpr std::string_view name = "vector";
  static constexpr IndexType type = IndexType::Vector;
  IndexCommonProperties common;
  VectorParams params;

  bool operator==(VectorIndexDescriptor const&) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, VectorIndexDescriptor& d) {
  return f.object(d).fields(f.embedFields(d.common),
                            f.field("params", d.params));
}

struct IndexDescriptor
    : std::variant<PersistentIndexDescriptor, HashIndexDescriptor,
                   SkiplistIndexDescriptor, TtlIndexDescriptor,
                   GeoIndexDescriptor, Geo1IndexDescriptor, Geo2IndexDescriptor,
                   MdiIndexDescriptor, ZkdIndexDescriptor,
                   VectorIndexDescriptor> {
  IndexCommonProperties const& common() const {
    return std::visit([](auto const& d) -> auto const& { return d.common; },
                      *this);
  }
  IndexType type() const {
    return std::visit(
        [](auto const& d) { return std::decay_t<decltype(d)>::type; }, *this);
  }
};

template<class... Ts>
consteval bool hasDistinctNames(std::variant<Ts...> const*) {
  std::array<std::string_view, sizeof...(Ts)> names{Ts::name...};
  for (std::size_t i = 0; i < names.size(); ++i) {
    for (std::size_t j = i + 1; j < names.size(); ++j) {
      if (names[i] == names[j]) {
        return false;
      }
    }
  }
  return true;
}

// An alias that forgets its own `name` would silently reuse its base's.
static_assert(hasDistinctNames(static_cast<IndexDescriptor const*>(nullptr)),
              "every IndexDescriptor alternative needs its own name");

template<class Inspector, class... Ts>
auto inspectAlternatives(Inspector& f, std::variant<Ts...>& v) {
  return f.variant(v).embedded("type").alternatives(
      inspection::type<Ts>(Ts::name)...);
}

template<class Inspector>
auto inspect(Inspector& f, IndexDescriptor& d) {
  return inspectAlternatives(f, d);
}

// std::get_if only matches the exact alternative, not its aliases.
template<class Base>
Base const* getIfFamily(IndexDescriptor const& d) {
  return std::visit(
      [](auto const& alt) -> Base const* {
        if constexpr (std::is_base_of_v<Base, std::decay_t<decltype(alt)>>) {
          return &alt;
        } else {
          return nullptr;
        }
      },
      d);
}

inspection::Status tryLoad(std::string_view json, IndexDescriptor& d) {
  auto builder = velocypack::Parser::fromJson(json);
  return velocypack::deserializeWithStatus(builder->slice(), d);
}

IndexDescriptor load(std::string_view json) {
  IndexDescriptor d;
  auto res = tryLoad(json, d);
  EXPECT_TRUE(res.ok()) << res.error() << " (path: " << res.path() << ")";
  return d;
}

constexpr std::string_view persistentDefinition =
    R"({"type": "persistent", "name": "p", "fields": ["a", "b"],
        "unique": true, "deduplicate": false})";
constexpr std::string_view hashDefinition =
    R"({"type": "hash", "name": "h", "fields": ["a"], "sparse": true})";
constexpr std::string_view skiplistDefinition =
    R"({"type": "skiplist", "name": "s", "fields": ["a"]})";
constexpr std::string_view ttlDefinition =
    R"({"type": "ttl", "name": "t", "fields": ["createdAt"], "sparse": true,
        "expireAfter": 3600})";
constexpr std::string_view geoDefinition =
    R"({"type": "geo", "name": "g", "fields": ["loc"], "geoJson": true})";
constexpr std::string_view geo1Definition =
    R"({"type": "geo1", "name": "g1", "fields": ["loc"]})";
constexpr std::string_view geo2Definition =
    R"({"type": "geo2", "name": "g2", "fields": ["lat", "lon"]})";
constexpr std::string_view mdiDefinition =
    R"({"type": "mdi", "name": "m", "fields": ["x", "y"],
        "fieldValueTypes": "double"})";
constexpr std::string_view zkdDefinition =
    R"({"type": "zkd", "name": "z", "fields": ["x", "y"],
        "fieldValueTypes": "double"})";
constexpr std::string_view vectorDefinition =
    R"({"type": "vector", "name": "v", "fields": ["emb"],
        "params": {"dimension": 128, "metric": "cosine", "nLists": 10}})";

}  // namespace

TEST(IndexDescriptorPrototypeTest, flat_json_loads_into_matching_alternative) {
  {
    auto d = load(persistentDefinition);
    auto const* p = std::get_if<PersistentIndexDescriptor>(&d);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->common.name, "p");
    EXPECT_EQ(p->common.fields, (std::vector<std::string>{"a", "b"}));
    EXPECT_TRUE(p->common.unique);
    EXPECT_FALSE(p->common.sparse);
    EXPECT_FALSE(p->deduplicate);
    EXPECT_TRUE(p->estimates);
  }
  {
    auto d = load(ttlDefinition);
    auto const* t = std::get_if<TtlIndexDescriptor>(&d);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->common.name, "t");
    EXPECT_EQ(t->common.fields, (std::vector<std::string>{"createdAt"}));
    EXPECT_FALSE(t->common.unique);
    EXPECT_TRUE(t->common.sparse);
    EXPECT_EQ(t->expireAfter, 3600.0);
    EXPECT_TRUE(t->deduplicate);
    EXPECT_TRUE(t->estimates);
  }
  {
    auto d = load(vectorDefinition);
    auto const* v = std::get_if<VectorIndexDescriptor>(&d);
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->common.name, "v");
    EXPECT_EQ(v->common.fields, (std::vector<std::string>{"emb"}));
    EXPECT_EQ(v->params.dimension, 128u);
    EXPECT_EQ(v->params.metric, "cosine");
    EXPECT_EQ(v->params.nLists, 10u);
  }
}

TEST(IndexDescriptorPrototypeTest, round_trip_keeps_alternative_and_name) {
  for (auto json :
       {persistentDefinition, hashDefinition, skiplistDefinition, ttlDefinition,
        geoDefinition, geo1Definition, geo2Definition, mdiDefinition,
        zkdDefinition, vectorDefinition}) {
    SCOPED_TRACE(json);
    auto input = velocypack::Parser::fromJson(json);
    auto expectedType = input->slice().get("type").stringView();

    auto first = load(json);
    velocypack::Builder saved;
    velocypack::serialize(saved, first);
    EXPECT_EQ(saved.slice().get("type").stringView(), expectedType);

    IndexDescriptor second;
    auto res = velocypack::deserializeWithStatus(saved.slice(), second);
    ASSERT_TRUE(res.ok()) << res.error();
    EXPECT_EQ(first.index(), second.index());
    EXPECT_TRUE(first == second);
  }
}

TEST(IndexDescriptorPrototypeTest, rejects_unknown_key_and_bad_type) {
  IndexDescriptor d;
  {
    auto res =
        tryLoad(R"({"type": "persistent", "fields": ["a"], "foo": 1})", d);
    ASSERT_FALSE(res.ok());
    EXPECT_EQ(res.error(), "Found unexpected attribute 'foo'");
  }
  {
    auto res =
        tryLoad(R"({"type": "hash", "fields": ["a"], "expireAfter": 10})", d);
    ASSERT_FALSE(res.ok());
    EXPECT_EQ(res.error(), "Found unexpected attribute 'expireAfter'");
  }
  {
    auto res = tryLoad(R"({"fields": ["a"]})", d);
    ASSERT_FALSE(res.ok());
    EXPECT_EQ(res.error(), R"(Variant type field "type" is missing)");
  }
  {
    auto res = tryLoad(R"({"type": "blubb", "fields": ["a"]})", d);
    ASSERT_FALSE(res.ok());
    EXPECT_EQ(res.error(), "Found invalid type: blubb");
  }
}

TEST(IndexDescriptorPrototypeTest, common_and_type_work_for_every_alternative) {
  struct Case {
    std::string_view json;
    IndexType type;
    std::string_view name;
  };
  for (auto const& c : {Case{persistentDefinition, IndexType::Persistent, "p"},
                        Case{hashDefinition, IndexType::Hash, "h"},
                        Case{skiplistDefinition, IndexType::Skiplist, "s"},
                        Case{ttlDefinition, IndexType::Ttl, "t"},
                        Case{geoDefinition, IndexType::Geo, "g"},
                        Case{geo1Definition, IndexType::Geo1, "g1"},
                        Case{geo2Definition, IndexType::Geo2, "g2"},
                        Case{mdiDefinition, IndexType::Mdi, "m"},
                        Case{zkdDefinition, IndexType::Zkd, "z"},
                        Case{vectorDefinition, IndexType::Vector, "v"}}) {
    SCOPED_TRACE(c.json);
    auto d = load(c.json);
    EXPECT_TRUE(d.type() == c.type);
    EXPECT_EQ(d.common().name, c.name);
    EXPECT_FALSE(d.common().fields.empty());
  }
}

TEST(IndexDescriptorPrototypeTest,
     get_if_is_exact_and_get_if_family_follows_bases) {
  auto persistent = load(persistentDefinition);
  auto hash = load(hashDefinition);
  auto skiplist = load(skiplistDefinition);
  auto ttl = load(ttlDefinition);
  auto geo = load(geoDefinition);
  auto vector = load(vectorDefinition);

  EXPECT_NE(std::get_if<HashIndexDescriptor>(&hash), nullptr);
  EXPECT_EQ(std::get_if<PersistentIndexDescriptor>(&hash), nullptr);

  EXPECT_NE(getIfFamily<PersistentIndexDescriptor>(persistent), nullptr);
  EXPECT_NE(getIfFamily<PersistentIndexDescriptor>(hash), nullptr);
  EXPECT_NE(getIfFamily<PersistentIndexDescriptor>(skiplist), nullptr);
  EXPECT_NE(getIfFamily<PersistentIndexDescriptor>(ttl), nullptr);
  EXPECT_EQ(getIfFamily<PersistentIndexDescriptor>(geo), nullptr);
  EXPECT_EQ(getIfFamily<PersistentIndexDescriptor>(vector), nullptr);

  EXPECT_NE(getIfFamily<SkiplistIndexDescriptor>(ttl), nullptr);
  EXPECT_EQ(getIfFamily<SkiplistIndexDescriptor>(hash), nullptr);

  EXPECT_EQ(getIfFamily<PersistentIndexDescriptor>(hash)->common.name, "h");
  EXPECT_TRUE(getIfFamily<PersistentIndexDescriptor>(hash)->common.sparse);
}
