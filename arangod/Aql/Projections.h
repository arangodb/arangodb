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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AttributeNamePath.h"
#include "Containers/FlatHashSet.h"
#include "VocBase/Identifiers/DataSourceId.h"

#include <function2.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <string_view>
#include <vector>

namespace arangodb {

class Index;
class IndexIteratorCoveringData;

namespace transaction {
class Methods;
}
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {
class Ast;
struct Variable;

/// @brief helper class to manage projections and extract attribute data from
/// documents and index entries
class Projections {
 public:
  /// @brief projection for a single top-level attribute or a nested attribute
  struct Projection {
    /// @brief the attribute path
    AttributeNamePath path;
    /// @brief attribute position in a covering index entry. only valid if
    /// usesCoveringIndex() is true
    uint16_t coveringIndexPosition;
    /// @brief attribute length in a covering index entry. this can be shorter
    /// than the projection
    uint16_t coveringIndexCutoff;

    uint16_t startsAtLevel;
    uint16_t levelsToClose;
    /// @brief attribute type
    AttributeNamePath::Type type;

    Variable const* variable;
  };

  static constexpr uint16_t kNoCoveringIndexPosition =
      std::numeric_limits<uint16_t>::max();

  Projections();

  /// @brief create projections from the vector of attributes passed.
  /// attributes will be sorted and made unique inside
  explicit Projections(std::vector<AttributeNamePath> paths);
  explicit Projections(containers::FlatHashSet<AttributeNamePath> paths);

  Projections(Projections&&) = default;
  Projections& operator=(Projections&&) = default;
  Projections(Projections const&) = default;
  Projections& operator=(Projections const&) = default;

  static bool isCoveringIndexPosition(uint16_t position) noexcept;

  /// @brief reset all projections
  void clear() noexcept;

  /// @brief erase projection members if cb returns true
  void erase(std::function<bool(Projection&)> const& cb);

  /// @brief set covering index context for these projections
  void setCoveringContext(DataSourceId const& id,
                          std::shared_ptr<Index> const& index);

  /// @brief whether or not the projections are backed by the specific index
  bool usesCoveringIndex(std::shared_ptr<Index> const& index) const noexcept {
    return _index == index;
  }

  /// @brief whether or not the projections are backed by any index
  bool usesCoveringIndex() const noexcept { return _index != nullptr; }

  /// @brief whether or not there are any projections
  bool empty() const noexcept { return _projections.empty(); }

  /// @brief number of projections
  size_t size() const noexcept { return _projections.size(); }

  bool contains(Projection const& other) const noexcept;

  /// @brief checks if we have a single attribute projection on the attribute
  bool isSingle(std::string_view attribute) const noexcept;

  /// @brief returns true if any of the projections will write into an
  /// output variable/register
  bool hasOutputRegisters() const noexcept;

  // return the covering index position for a specific attribute type.
  // will throw if the index does not cover!
  uint16_t coveringIndexPosition(aql::AttributeNamePath::Type type) const;

  /// @brief get projection at position
  Projection const& operator[](size_t index) const;

  /// @brief get projection at position
  Projection& operator[](size_t index);

  /// @brief extract projections from a full document, calling a callback for
  /// each projection value
  void produceFromDocument(
      velocypack::Builder& b, velocypack::Slice slice,
      transaction::Methods const* trxPtr,
      fu2::unique_function<void(Variable const*, velocypack::Slice)
                               const> const& cb) const;

  /// @brief extract projections from a covering index, calling a callback for
  /// each projection value
  void produceFromIndex(
      velocypack::Builder& b, IndexIteratorCoveringData& covering,
      transaction::Methods const* trxPtr,
      fu2::unique_function<void(Variable const*, velocypack::Slice)
                               const> const& cb) const;

  void produceFromIndexCompactArray(
      velocypack::Builder& b, IndexIteratorCoveringData& covering,
      transaction::Methods const* trxPtr,
      fu2::unique_function<void(Variable const*, velocypack::Slice)
                               const> const& cb) const;

  /// @brief extract projections from a full document
  void toVelocyPackFromDocument(velocypack::Builder& b, velocypack::Slice slice,
                                transaction::Methods const* trxPtr) const;

  /// @brief extract projections from a covering index
  void toVelocyPackFromIndex(velocypack::Builder& b,
                             IndexIteratorCoveringData& covering,
                             transaction::Methods const* trxPtr) const;

  /// @brief extract projections from a covering index
  /// this variant accesses the covering data using the projection index
  /// instead of coveringIndexPosition attribute.
  void toVelocyPackFromIndexCompactArray(
      velocypack::Builder& b, IndexIteratorCoveringData& covering,
      transaction::Methods const* trxPtr) const;

  /// @brief serialize the projections to velocypack, under the attribute
  /// name "projections"
  void toVelocyPack(velocypack::Builder& b) const;
  /// @brief serialize the projections to velocypack, under a custom
  /// attribute name
  void toVelocyPack(velocypack::Builder& b,
                    std::string_view attributeName) const;

  std::vector<Projection> const& projections() const noexcept;

  /// @brief build projections from velocypack, looking for the attribute
  /// name "projections"
  static Projections fromVelocyPack(Ast* ast, velocypack::Slice slice,
                                    ResourceMonitor& resourceMonitor);

  /// @brief build projections from velocypack, looking for a custom
  /// attribute name
  static Projections fromVelocyPack(Ast* ast, velocypack::Slice slice,
                                    std::string_view attributeName,
                                    ResourceMonitor& resourceMonitor);

  std::size_t hash() const noexcept;

 private:
  /// @brief shared init function
  template<typename T>
  void init(T paths);

  /// @brief clean up projections, so that there are no 2 projections where one
  /// is a true prefix of another. also sets level attributes
  void handleSharedPrefixes();

  /// @brief all our projections (sorted, unique)
  std::vector<Projection> _projections;

  /// @brief collection data source id (in case _id is queries and we need to
  /// resolve the collection name)
  DataSourceId _datasourceId;

  /// @brief whether or not the projections are backed by a covering index
  std::shared_ptr<Index> _index;
};

std::ostream& operator<<(std::ostream& stream,
                         Projections::Projection const& projection);

std::ostream& operator<<(std::ostream& stream, Projections const& projections);

}  // namespace aql
}  // namespace arangodb
