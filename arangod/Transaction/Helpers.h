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

#include <string_view>
#include "Transaction/CountCache.h"
#include "Utils/OperationResult.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/Identifiers/RevisionId.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class CollectionNameResolver;
class LogicalCollection;
struct OperationOptions;

namespace velocypack {

class Builder;
class Slice;

}  // namespace velocypack
namespace transaction {

struct BatchOptions;
class Context;
class Methods;

namespace helpers {
/// @brief extract the _key attribute from a slice
std::string_view extractKeyPart(velocypack::Slice slice, bool& keyPresent);

std::string_view extractKeyPart(velocypack::Slice slice);

/** @brief Given a string, returns the substring after the first '/' or
 *          the whole string if it contains no '/'.
 */
std::string_view extractKeyPart(std::string_view);

std::string extractIdString(CollectionNameResolver const* resolver,
                            VPackSlice slice, VPackSlice base);

/// @brief quick access to the _key attribute in a database document
/// the document must have at least two attributes, and _key is supposed to
/// be the first one
VPackSlice extractKeyFromDocument(VPackSlice);

/// @brief quick access to the _id attribute in a database document
/// the document must have at least two attributes, and _id is supposed to
/// be the second one
/// note that this may return a Slice of type Custom!
/// do NOT call this method when
/// - the input slice is not a database document
/// - you are not willing to deal with slices of type Custom
VPackSlice extractIdFromDocument(VPackSlice);

/// @brief quick access to the _from attribute in a database document
/// the document must have at least five attributes: _key, _id, _from, _to
/// and _rev (in this order)
VPackSlice extractFromFromDocument(VPackSlice);

/// @brief quick access to the _to attribute in a database document
/// the document must have at least five attributes: _key, _id, _from, _to
/// and _rev (in this order)
VPackSlice extractToFromDocument(VPackSlice);

/// @brief extract _key and _rev from a document, in one go
/// this is an optimized version used when loading collections, WAL
/// collection and compaction
void extractKeyAndRevFromDocument(VPackSlice slice, VPackSlice& keySlice,
                                  RevisionId& revisionId);

std::string_view extractCollectionFromId(std::string_view id);

/// @brief extract _rev from a database document
RevisionId extractRevFromDocument(VPackSlice slice);
VPackSlice extractRevSliceFromDocument(VPackSlice slice);

OperationResult buildCountResult(
    OperationOptions const& options,
    std::vector<std::pair<std::string, uint64_t>> const& count, CountType type,
    uint64_t& total);

/// @brief creates an id string from a custom _id value and the _key string
std::string makeIdFromCustom(CollectionNameResolver const* resolver,
                             VPackSlice idPart, VPackSlice keyPart);

std::string makeIdFromParts(CollectionNameResolver const* resolver,
                            DataSourceId const& cid, VPackSlice keyPart);

/// @brief new object for insert, value must have _key set correctly.
Result newObjectForInsert(Methods& trx, LogicalCollection& collection,
                          std::string_view key, velocypack::Slice value,
                          RevisionId& revisionId, velocypack::Builder& builder,
                          OperationOptions const& options,
                          BatchOptions& batchOptions);

/// @brief merge two objects for update
Result mergeObjectsForUpdate(Methods& trx, LogicalCollection& collection,
                             velocypack::Slice oldValue,
                             velocypack::Slice newValue, bool isNoOpUpdate,
                             RevisionId previousRevisionId,
                             RevisionId& revisionId,
                             velocypack::Builder& builder,
                             OperationOptions const& options,
                             BatchOptions& batchOptions);

/// @brief new object for replace
Result newObjectForReplace(Methods& trx, LogicalCollection& collection,
                           velocypack::Slice oldValue,
                           velocypack::Slice newValue, bool isNoOpReplace,
                           RevisionId previousRevisionId,
                           RevisionId& revisionId, velocypack::Builder& builder,
                           OperationOptions const& options,
                           BatchOptions& batchOptions);

bool isValidEdgeAttribute(velocypack::Slice slice, bool allowExtendedNames);

}  // namespace helpers

/// @brief std::string leaser
class StringLeaser {
 public:
  explicit StringLeaser(Methods*);
  explicit StringLeaser(Context*);
  ~StringLeaser();

  auto release() {
    return std::unique_ptr<std::string>{std::exchange(_string, nullptr)};
  }
  void acquire(std::unique_ptr<std::string> r) {
    TRI_ASSERT(_string == nullptr);
    _string = r.release();
  }

  std::string* string() const { return _string; }
  std::string* operator->() const { return _string; }
  std::string& operator*() { return *_string; }
  std::string const& operator*() const { return *_string; }
  std::string* get() const { return _string; }

 private:
  Context* _transactionContext;
  std::string* _string;
};

class BuilderLeaser {
 public:
  explicit BuilderLeaser(Context*);
  explicit BuilderLeaser(Methods*);

  BuilderLeaser(BuilderLeaser const&) = delete;
  BuilderLeaser& operator=(BuilderLeaser const&) = delete;
  BuilderLeaser& operator=(BuilderLeaser&&) = delete;

  BuilderLeaser(BuilderLeaser&& source)
      : _transactionContext(source._transactionContext),
        _builder(source.steal()) {}

  ~BuilderLeaser();

  velocypack::Builder* builder() const noexcept { return _builder; }
  velocypack::Builder* operator->() const noexcept { return _builder; }
  velocypack::Builder& operator*() noexcept { return *_builder; }
  velocypack::Builder& operator*() const noexcept { return *_builder; }
  velocypack::Builder* get() const noexcept { return _builder; }
  velocypack::Builder* steal() { return std::exchange(_builder, nullptr); }

  void clear();

 private:
  Context* _transactionContext;
  velocypack::Builder* _builder;
};

}  // namespace transaction
}  // namespace arangodb
