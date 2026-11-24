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
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEngine/InvertedIndex/RocksDBInvertedIndex.h"

#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "RocksDBEngine/InvertedIndex/Matcher.h"
#include "RocksDBEngine/InvertedIndex/Format.h"
#include "RocksDBEngine/InvertedIndex/Iterator.h"
#include "RocksDBEngine/InvertedIndex/PendingWrites.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "VocBase/LogicalCollection.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

#include <algorithm>

using namespace arangodb;

namespace {

constexpr std::string_view kWindowBitsAttribute = "windowBits";

velocypack::Slice extractField(velocypack::Slice doc,
                               std::vector<basics::AttributeName> const& path) {
  velocypack::Slice current = doc;
  for (auto const& part : path) {
    if (!current.isObject()) {
      return velocypack::Slice::noneSlice();
    }
    current = current.get(part.name);
  }
  return current;
}

}  // namespace

ResultT<std::vector<std::string>> inverted_index::indexedTerms(
    velocypack::Slice doc, std::vector<basics::AttributeName> const& field) {
  std::vector<std::string> terms;
  velocypack::Slice array = extractField(doc, field);
  if (!array.isArray()) {
    return terms;
  }
  for (velocypack::Slice element : velocypack::ArrayIterator(array)) {
    auto term = normalizedTerm(element);
    if (!term.has_value()) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    absl::StrCat("rocksdb-inverted indexes cannot index ",
                                 element.typeName(), " values"));
    }
    terms.push_back(std::move(*term));
  }
  std::ranges::sort(terms);
  auto duplicates = std::ranges::unique(terms);
  terms.erase(duplicates.begin(), duplicates.end());
  return terms;
}

RocksDBInvertedIndex::RocksDBInvertedIndex(IndexId iid,
                                           LogicalCollection& collection,
                                           velocypack::Slice info)
    : RocksDBIndex(
          iid, collection, info,
          RocksDBColumnFamilyManager::get(
              RocksDBColumnFamilyManager::Family::RocksDBInvertedIndex),
          /*useCache*/ false,
          /*cacheManager*/ nullptr,
          /*engine*/
          collection.vocbase().engine<RocksDBEngine>()) {
  TRI_ASSERT(iid.isSet());
  _unique = false;
  _sparse = false;
  if (_fields.size() != 1) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "rocksdb-inverted index definition needs exactly one attribute");
  }
  if (velocypack::Slice bits = info.get(kWindowBitsAttribute);
      bits.isNumber()) {
    _layout.windowBits = bits.getNumber<unsigned>();
  }
  if (!inverted_index::isValid(_layout)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("rocksdb-inverted index windowBits must be 0 or between "
                     "16 and 32, got ",
                     _layout.windowBits));
  }
}

void RocksDBInvertedIndex::toVelocyPack(
    velocypack::Builder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  builder.openObject();
  RocksDBIndex::toVelocyPack(builder, flags);
  builder.add(kWindowBitsAttribute, velocypack::Value(_layout.windowBits));
  builder.close();
}

Index::FilterCosts RocksDBInvertedIndex::supportsFilterCondition(
    transaction::Methods& /*trx*/,
    std::vector<std::shared_ptr<Index>> const& /*allIndexes*/,
    aql::AstNode const* node, aql::Variable const* reference,
    size_t itemsInIndex) const {
  return inverted_index::supportsFilterCondition(*this, node, reference,
                                                 itemsInIndex);
}

aql::AstNode* RocksDBInvertedIndex::specializeCondition(
    transaction::Methods& /*trx*/, aql::AstNode* node,
    aql::Variable const* reference) const {
  return inverted_index::specializeCondition(*this, node, reference);
}

std::unique_ptr<IndexIterator> RocksDBInvertedIndex::iteratorForCondition(
    ResourceMonitor& /*monitor*/, transaction::Methods* trx,
    aql::AstNode const* node, aql::Variable const* reference,
    IndexIteratorOptions const& /*opts*/, ReadOwnWrites readOwnWrites,
    int /*mutableConditionIdx*/) {
  return makeRocksDBInvertedIndexIterator(&_collection, trx, *this, node,
                                          reference, readOwnWrites);
}

Result RocksDBInvertedIndex::insert(transaction::Methods& /*trx*/,
                                    RocksDBMethods* methods,
                                    LocalDocumentId documentId,
                                    velocypack::Slice doc,
                                    OperationOptions const& /*options*/,
                                    bool /*performChecks*/) {
  return write(*methods, documentId, doc, /*isInsert*/ true);
}

Result RocksDBInvertedIndex::remove(transaction::Methods& /*trx*/,
                                    RocksDBMethods* methods,
                                    LocalDocumentId documentId,
                                    velocypack::Slice doc,
                                    OperationOptions const& /*options*/) {
  return write(*methods, documentId, doc, /*isInsert*/ false);
}

Result RocksDBInvertedIndex::write(RocksDBMethods& methods,
                                   LocalDocumentId documentId,
                                   velocypack::Slice doc, bool isInsert) {
  auto terms = inverted_index::indexedTerms(doc, _fields[0]);
  if (terms.fail()) {
    Result res = terms.result();
    return addErrorMsg(res);
  }
  inverted_index::PendingWrites* pending = methods.invertedIndexWrites();
  // every RocksDBMethods that can write buffers RocksDB inverted index writes
  TRI_ASSERT(pending != nullptr);
  for (auto const& term : terms.get()) {
    pending->add(_cf, inverted_index::termKey(objectId(), term),
                 inverted_index::shardOf(_layout, documentId.id()),
                 inverted_index::offsetOf(_layout, documentId.id()), isInsert);
  }
  return {};
}
