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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "StorageEngineMock.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AstNode.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/asio_ns.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "ClusterEngine/ClusterEngine.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "IResearch/IResearchRocksDBLink.h"
#include "IResearch/VelocyPackHelper.h"
#include "Indexes/IndexIterator.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Indexes/SortedIndexAttributeMatcher.h"
#include "RestServer/FlushFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Helpers.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/ticks.h"

#include "Mocks/IResearchLinkMock.h"

#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <boost/asio/io_context.hpp>

namespace {

arangodb::LocalDocumentId generateDocumentId(arangodb::LogicalCollection const& collection,
                                             arangodb::RevisionId revisionId,
                                             uint64_t& documentId) {
  bool useRev = collection.usesRevisionsAsDocumentIds();
  return useRev ? arangodb::LocalDocumentId::create(revisionId)
                : arangodb::LocalDocumentId::create(++documentId);
}

/// @brief hard-coded vector of the index attributes
/// note that the attribute names must be hard-coded here to avoid an init-order
/// fiasco with StaticStrings::FromString etc.
std::vector<std::vector<arangodb::basics::AttributeName>> const IndexAttributes{
    {arangodb::basics::AttributeName("_from", false)},
    {arangodb::basics::AttributeName("_to", false)}};

/// @brief add a single value node to the iterator's keys
void handleValNode(VPackBuilder* keys, arangodb::aql::AstNode const* valNode) {
  if (!valNode->isStringValue() || valNode->getStringLength() == 0) {
    return;
  }

  keys->openObject();
  keys->add(arangodb::StaticStrings::IndexEq,
            VPackValuePair(valNode->getStringValue(),
                           valNode->getStringLength(), VPackValueType::String));
  keys->close();

  TRI_IF_FAILURE("EdgeIndex::collectKeys") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
}

class EdgeIndexIteratorMock final : public arangodb::IndexIterator {
 public:
  typedef std::unordered_multimap<std::string, arangodb::LocalDocumentId> Map;

  EdgeIndexIteratorMock(arangodb::LogicalCollection* collection,
                        arangodb::transaction::Methods* trx, arangodb::Index const* index,
                        Map const& map, std::unique_ptr<VPackBuilder>&& keys)
      : IndexIterator(collection, trx),
        _map(map),
        _begin(_map.begin()),
        _end(_map.end()),
        _keys(std::move(keys)),
        _keysIt(_keys->slice()) {}

  char const* typeName() const override { return "edge-index-iterator-mock"; }

  bool nextImpl(LocalDocumentIdCallback const& cb, size_t limit) override {
    while (limit && _begin != _end && _keysIt.valid()) {
      auto key = _keysIt.value();

      if (key.isObject()) {
        key = key.get(arangodb::StaticStrings::IndexEq);
      }

      std::tie(_begin, _end) = _map.equal_range(key.toString());

      while (limit && _begin != _end) {
        cb(_begin->second);
        ++_begin;
        --limit;
      }

      ++_keysIt;
    }

    return _begin != _end || _keysIt.valid();
  }

  void resetImpl() override {
    _keysIt.reset();
    _begin = _map.begin();
    _end = _map.end();
  }

 private:
  Map const& _map;
  Map::const_iterator _begin;
  Map::const_iterator _end;
  std::unique_ptr<VPackBuilder> _keys;
  arangodb::velocypack::ArrayIterator _keysIt;
};  // EdgeIndexIteratorMock

class EdgeIndexMock final : public arangodb::Index {
 public:
  static std::shared_ptr<arangodb::Index> make(arangodb::IndexId iid,
                                               arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition) {
    auto const typeSlice = definition.get("type");

    if (typeSlice.isNone()) {
      return nullptr;
    }

    auto const type =
        arangodb::basics::VelocyPackHelper::getStringRef(typeSlice,
                                                         arangodb::velocypack::StringRef());

    if (type.compare("edge") != 0) {
      return nullptr;
    }

    return std::make_shared<EdgeIndexMock>(iid, collection);
  }

  IndexType type() const override { return Index::TRI_IDX_TYPE_EDGE_INDEX; }

  char const* typeName() const override { return "edge"; }

  bool isPersistent() const override { return false; }

  bool canBeDropped() const override { return false; }

  bool isHidden() const override { return false; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return false; }

  size_t memory() const override { return sizeof(EdgeIndexMock); }

  void load() override {}
  void unload() override {}
  void afterTruncate(TRI_voc_tick_t, arangodb::transaction::Methods*) override {
    _edgesFrom.clear();
    _edgesTo.clear();
  }

  void toVelocyPack(VPackBuilder& builder,
                    std::underlying_type<arangodb::Index::Serialize>::type flags) const override {
    builder.openObject();
    Index::toVelocyPack(builder, flags);
    // hard-coded
    builder.add("unique", VPackValue(false));
    builder.add("sparse", VPackValue(false));
    builder.close();
  }

  void toVelocyPackFigures(VPackBuilder& builder) const override {
    Index::toVelocyPackFigures(builder);

    builder.add("from", VPackValue(VPackValueType::Object));
    //_edgesFrom->appendToVelocyPack(builder);
    builder.close();

    builder.add("to", VPackValue(VPackValueType::Object));
    //_edgesTo->appendToVelocyPack(builder);
    builder.close();
  }

  arangodb::Result insert(arangodb::transaction::Methods& trx,
                          arangodb::LocalDocumentId const& documentId,
                          arangodb::velocypack::Slice const doc) {
    if (!doc.isObject()) {
      return {TRI_ERROR_INTERNAL};
    }

    VPackSlice const fromValue(arangodb::transaction::helpers::extractFromFromDocument(doc));

    if (!fromValue.isString()) {
      return {TRI_ERROR_INTERNAL};
    }

    VPackSlice const toValue(arangodb::transaction::helpers::extractToFromDocument(doc));

    if (!toValue.isString()) {
      return {TRI_ERROR_INTERNAL};
    }

    _edgesFrom.emplace(fromValue.toString(), documentId);
    _edgesTo.emplace(toValue.toString(), documentId);

    return {};  // ok
  }

  arangodb::Result remove(arangodb::transaction::Methods& trx,
                          arangodb::LocalDocumentId const&,
                          arangodb::velocypack::Slice const& doc,
                          arangodb::IndexOperationMode) {
    if (!doc.isObject()) {
      return {TRI_ERROR_INTERNAL};
    }

    VPackSlice const fromValue(arangodb::transaction::helpers::extractFromFromDocument(doc));

    if (!fromValue.isString()) {
      return {TRI_ERROR_INTERNAL};
    }

    VPackSlice const toValue(arangodb::transaction::helpers::extractToFromDocument(doc));

    if (!toValue.isString()) {
      return {TRI_ERROR_INTERNAL};
    }

    _edgesFrom.erase(fromValue.toString());
    _edgesTo.erase(toValue.toString());

    return {};  // ok
  }

  Index::FilterCosts supportsFilterCondition(
      std::vector<std::shared_ptr<arangodb::Index>> const& /*allIndexes*/,
      arangodb::aql::AstNode const* node, arangodb::aql::Variable const* reference,
      size_t itemsInIndex) const override {
    arangodb::SimpleAttributeEqualityMatcher matcher(IndexAttributes);
    return matcher.matchOne(this, node, reference, itemsInIndex);
  }

  std::unique_ptr<arangodb::IndexIterator> iteratorForCondition(
      arangodb::transaction::Methods* trx, arangodb::aql::AstNode const* node,
      arangodb::aql::Variable const*, arangodb::IndexIteratorOptions const&) override {
    TRI_ASSERT(node->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND);

    TRI_ASSERT(node->numMembers() == 1);

    auto comp = node->getMember(0);

    // assume a.b == value
    auto attrNode = comp->getMember(0);
    auto valNode = comp->getMember(1);

    if (attrNode->type != arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
      // got value == a.b  -> flip sides
      std::swap(attrNode, valNode);
    }
    TRI_ASSERT(attrNode->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);

    if (comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ) {
      // a.b == value
      return createEqIterator(trx, attrNode, valNode);
    }

    if (comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
      // a.b IN values
      if (!valNode->isArray()) {
        // a.b IN non-array
        return std::make_unique<arangodb::EmptyIndexIterator>(&_collection, trx);
      }

      return createInIterator(trx, attrNode, valNode);
    }

    // operator type unsupported
    return std::make_unique<arangodb::EmptyIndexIterator>(&_collection, trx);
  }

  arangodb::aql::AstNode* specializeCondition(arangodb::aql::AstNode* node,
                                              arangodb::aql::Variable const* reference) const override {
    arangodb::SimpleAttributeEqualityMatcher matcher(IndexAttributes);

    return matcher.specializeOne(this, node, reference);
  }

  EdgeIndexMock(arangodb::IndexId iid, arangodb::LogicalCollection& collection)
      : arangodb::Index(
            iid, collection, arangodb::StaticStrings::IndexNameEdge,
            {{arangodb::basics::AttributeName(arangodb::StaticStrings::FromString, false)},
             {arangodb::basics::AttributeName(arangodb::StaticStrings::ToString, false)}},
            true, false) {}

  std::unique_ptr<arangodb::IndexIterator> createEqIterator(
      arangodb::transaction::Methods* trx, arangodb::aql::AstNode const* attrNode,
      arangodb::aql::AstNode const* valNode) const {
    // lease builder, but immediately pass it to the unique_ptr so we don't leak
    arangodb::transaction::BuilderLeaser builder(trx);
    std::unique_ptr<VPackBuilder> keys(builder.steal());
    keys->openArray();

    handleValNode(keys.get(), valNode);
    TRI_IF_FAILURE("EdgeIndex::noIterator") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    keys->close();

    // _from or _to?
    bool const isFrom = (attrNode->stringEquals(arangodb::StaticStrings::FromString));

    return std::make_unique<EdgeIndexIteratorMock>(&_collection, trx, this,
                                                   isFrom ? _edgesFrom : _edgesTo,
                                                   std::move(keys));
  }

  /// @brief create the iterator
  std::unique_ptr<arangodb::IndexIterator> createInIterator(
      arangodb::transaction::Methods* trx, arangodb::aql::AstNode const* attrNode,
      arangodb::aql::AstNode const* valNode) const {
    // lease builder, but immediately pass it to the unique_ptr so we don't leak
    arangodb::transaction::BuilderLeaser builder(trx);
    std::unique_ptr<VPackBuilder> keys(builder.steal());
    keys->openArray();

    size_t const n = valNode->numMembers();
    for (size_t i = 0; i < n; ++i) {
      handleValNode(keys.get(), valNode->getMemberUnchecked(i));
      TRI_IF_FAILURE("EdgeIndex::iteratorValNodes") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    }

    TRI_IF_FAILURE("EdgeIndex::noIterator") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    keys->close();

    // _from or _to?
    bool const isFrom = (attrNode->stringEquals(arangodb::StaticStrings::FromString));

    return std::make_unique<EdgeIndexIteratorMock>(&_collection, trx, this,
                                                   isFrom ? _edgesFrom : _edgesTo,
                                                   std::move(keys));
  }

  /// @brief the hash table for _from
  EdgeIndexIteratorMock::Map _edgesFrom;
  /// @brief the hash table for _to
  EdgeIndexIteratorMock::Map _edgesTo;
};  // EdgeIndexMock

class ReverseAllIteratorMock final : public arangodb::IndexIterator {
 public:
  ReverseAllIteratorMock(uint64_t size, arangodb::LogicalCollection* coll,
                         arangodb::transaction::Methods* trx)
      : arangodb::IndexIterator(coll, trx), _end(size), _size(size) {}

  virtual char const* typeName() const override {
    return "ReverseAllIteratorMock";
  }

  virtual void resetImpl() override { _end = _size; }

  virtual bool nextImpl(LocalDocumentIdCallback const& callback, size_t limit) override {
    while (_end && limit) {  // `_end` always > 0
      callback(arangodb::LocalDocumentId(_end--));
      --limit;
    }

    return 0 == limit;
  }

 private:
  uint64_t _end;
  uint64_t _size;  // the original size
};                 // ReverseAllIteratorMock

class AllIteratorMock final : public arangodb::IndexIterator {
 public:
  AllIteratorMock(std::unordered_map<arangodb::velocypack::StringRef, PhysicalCollectionMock::DocElement> const& data,
                  arangodb::LogicalCollection& coll, arangodb::transaction::Methods* trx)
      : arangodb::IndexIterator(&coll, trx), _data(data), _it{_data.begin()} {}

  virtual char const* typeName() const override { return "AllIteratorMock"; }

  virtual void resetImpl() override { _it = _data.begin(); }

  virtual bool nextImpl(LocalDocumentIdCallback const& callback, size_t limit) override {
    while (_it != _data.end() && limit != 0) {
      callback(_it->second.docId());
      ++_it;
      --limit;
    }
    return 0 == limit;
  }

 private:
  std::unordered_map<arangodb::velocypack::StringRef, PhysicalCollectionMock::DocElement> const& _data;
  std::unordered_map<arangodb::velocypack::StringRef, PhysicalCollectionMock::DocElement>::const_iterator _it;
};  // AllIteratorMock

struct IndexFactoryMock : arangodb::IndexFactory {
  IndexFactoryMock(arangodb::application_features::ApplicationServer& server)
      : IndexFactory(server) {}
  virtual void fillSystemIndexes(arangodb::LogicalCollection& col,
                                 std::vector<std::shared_ptr<arangodb::Index>>& systemIndexes) const override {
    // NOOP
  }

  /// @brief create indexes from a list of index definitions
  virtual void prepareIndexes(arangodb::LogicalCollection& col,
                              arangodb::velocypack::Slice const& indexesSlice,
                              std::vector<std::shared_ptr<arangodb::Index>>& indexes) const override {
    // NOOP
  }
};

class HashIndexMap {
  struct VPackBuilderHasher {
    std::size_t operator()(VPackBuilder const& builder) const {
      return std::hash<VPackSlice>()(builder.slice());
    }
  };

  struct VPackBuilderComparator {
    bool operator()(VPackBuilder const& builder1, VPackBuilder const& builder2) const {
      return ::arangodb::basics::VelocyPackHelper::compare(builder1.slice(), builder2.slice(), true) == 0;
    }
  };

  using ValueMap = std::unordered_multimap<VPackBuilder, arangodb::LocalDocumentId, VPackBuilderHasher, VPackBuilderComparator>;
  using DocumentsIndexMap = std::unordered_map<arangodb::LocalDocumentId, VPackBuilder>;

  arangodb::velocypack::Slice getSliceByField(arangodb::velocypack::Slice const& doc, size_t i) {
    TRI_ASSERT(i < _fields.size());
    TRI_ASSERT(!doc.isNone());
    auto slice = doc;
    for (auto const& f : _fields[i]) {
      slice = slice.get(f.name);
      if (slice.isNone() || slice.isNull()) {
        break;
      }
    }
    return slice;
  }

  void insertSlice(arangodb::LocalDocumentId const& documentId, arangodb::velocypack::Slice const& slice, size_t i) {
    VPackBuilder builder;
    if (slice.isNone() || slice.isNull()) {
      builder.add(VPackSlice::nullSlice());
    } else {
      builder.add(slice);
    }
    _valueMaps[i].emplace(std::move(builder), documentId);
  }

 public:
  HashIndexMap(std::vector<std::vector<arangodb::basics::AttributeName>> const& fields) : _fields(fields), _valueMaps(fields.size()) {
    TRI_ASSERT(!_fields.empty());
  }

  void insert(arangodb::LocalDocumentId const& documentId, arangodb::velocypack::Slice const& doc) {
    VPackBuilder builder;
    builder.openArray();
    auto toClose = true;
    // find fields for the index
    for (size_t i = 0; i < _fields.size(); ++i) {
      auto slice = doc;
      auto isExpansion = false;
      for (auto fieldIt = _fields[i].begin(); fieldIt != _fields[i].end(); ++fieldIt) {
        TRI_ASSERT(slice.isObject() || slice.isArray());
        if (slice.isObject()) {
          slice = slice.get(fieldIt->name);
          if ((fieldIt->shouldExpand && slice.isObject()) ||
              (!fieldIt->shouldExpand && slice.isArray())) {
            slice = VPackSlice::nullSlice();
            break;
          }
          if (slice.isNone() || slice.isNull()) {
            break;
          }
        } else { // expansion
          isExpansion = slice.isArray();
          TRI_ASSERT(isExpansion);
          auto found = false;
          for (auto sliceIt = arangodb::velocypack::ArrayIterator(slice); sliceIt != sliceIt.end(); ++sliceIt) {
            auto subSlice = sliceIt.value();
            if (!(subSlice.isNone() || subSlice.isNull())) {
              for (auto fieldItForArray = fieldIt; fieldItForArray != _fields[i].end(); ++fieldItForArray) {
                TRI_ASSERT(subSlice.isObject());
                subSlice = subSlice.get(fieldItForArray->name);
                if (subSlice.isNone() || subSlice.isNull()) {
                  break;
                }
              }
              if (!(subSlice.isNone() || subSlice.isNull())) {
                insertSlice(documentId, subSlice, i);
                builder.add(subSlice);
                found = true;
                break;
              }
            }
          }
          if (!found) {
            insertSlice(documentId, VPackSlice::nullSlice(), i);
            builder.add(VPackSlice::nullSlice());
          }
          break;
        }
      }
      if (!isExpansion) {
        // if the last expansion (at the end) leave the array open
        if (slice.isArray() && i == _fields.size() - 1) {
          auto found = false;
          auto wasNull = false;
          for (auto sliceIt = arangodb::velocypack::ArrayIterator(slice); sliceIt != sliceIt.end(); ++sliceIt) {
            auto subSlice = sliceIt.value();
            if (!(subSlice.isNone() || subSlice.isNull())) {
              insertSlice(documentId, subSlice, i);
              found = true;
            } else {
              wasNull = true;
            }
          }
          if (!found || wasNull) {
            insertSlice(documentId, VPackSlice::nullSlice(), i);
          }
          toClose = false;
        } else { // object
          insertSlice(documentId, slice, i);
          builder.add(slice);
        }
      }
    }
    if (toClose) {
      builder.close();
    }
    _docIndexMap.try_emplace(documentId, std::move(builder));
  }

  bool remove(arangodb::LocalDocumentId const& documentId, arangodb::velocypack::Slice const& doc) {
    size_t i = 0;
    auto documentRemoved = false;
    for (auto& map : _valueMaps) {
      auto slice = getSliceByField(doc, i++);
      auto [begin, end] = map.equal_range(VPackBuilder(slice));
      for (; begin != end; ++begin) {
        if (begin->second == documentId) {
          map.erase(begin);
          documentRemoved = true;
          // not break because of expansions
        }
      }
    }
    _docIndexMap.erase(documentId);
    return documentRemoved;
  }

  void clear() {
    _valueMaps.clear();
    _docIndexMap.clear();
  }

  std::unordered_map<arangodb::LocalDocumentId, VPackBuilder> find(std::unique_ptr<VPackBuilder>&& keys) const {
    std::unordered_map<arangodb::LocalDocumentId, VPackBuilder const*> found;
    TRI_ASSERT(keys->slice().isArray());
    auto sliceIt = arangodb::velocypack::ArrayIterator(keys->slice());
    if (!sliceIt.valid()) {
      return std::unordered_map<arangodb::LocalDocumentId, VPackBuilder>();
    }
    for (auto const& map : _valueMaps) {
      auto [begin, end] = map.equal_range(VPackBuilder(sliceIt.value()));
      if (begin == end) {
        return std::unordered_map<arangodb::LocalDocumentId, VPackBuilder>();
      }
      if (found.empty()) {
        std::transform(begin, end, std::inserter(found, found.end()), [] (auto const& item) {
          return std::make_pair(item.second, &item.first);
        });
      } else {
        std::unordered_map<arangodb::LocalDocumentId, VPackBuilder const*> tmpFound;
        for (; begin != end; ++begin) {
          if (found.find(begin->second) != found.cend()) {
            tmpFound.try_emplace(begin->second, &begin->first);
          }
        }
        if (tmpFound.empty()) {
          return std::unordered_map<arangodb::LocalDocumentId, VPackBuilder>();
        }
        found.swap(tmpFound);
      }
      if (!(++sliceIt).valid()) {
        break;
      }
    }
    std::unordered_map<arangodb::LocalDocumentId, VPackBuilder> foundWithCovering;
    for (auto const& d : found) {
      auto doc = _docIndexMap.find(d.first);
      TRI_ASSERT(doc != _docIndexMap.cend());
      auto builder = doc->second;
      // the array was left open for the last expansion (at the end)
      if (doc->second.isOpenArray()) {
        builder.add(d.second->slice());
        builder.close();
      }
      foundWithCovering.try_emplace(doc->first, std::move(builder));
    }
    return foundWithCovering;
  }

 private:
  std::vector<std::vector<arangodb::basics::AttributeName>> const& _fields;
  std::vector<ValueMap> _valueMaps;
  DocumentsIndexMap _docIndexMap;
};

class HashIndexIteratorMock final : public arangodb::IndexIterator {
 public:
  HashIndexIteratorMock(arangodb::LogicalCollection* collection,
                        arangodb::transaction::Methods* trx, arangodb::Index const* index,
                        HashIndexMap const& map, std::unique_ptr<VPackBuilder>&& keys)
    : IndexIterator(collection, trx), _map(map) {
    _documents = _map.find(std::move(keys));
    _begin = _documents.begin();
    _end = _documents.end();
  }

  char const* typeName() const override { return "hash-index-iterator-mock"; }

  bool nextCoveringImpl(DocumentCallback const& cb, size_t limit) override {
    while (limit && _begin != _end) {
      cb(_begin->first, _begin->second.slice());
      ++_begin;
      --limit;
    }

    return _begin != _end;
  }

  bool nextImpl(LocalDocumentIdCallback const& cb, size_t limit) override {
    while (limit && _begin != _end) {
      cb(_begin->first);
      ++_begin;
      --limit;
    }

    return _begin != _end;
  }

  void resetImpl() override {
    _documents.clear();
    _begin = _documents.begin();
    _end = _documents.end();
  }

  bool hasCovering() const override {
    return true;
  }

 private:
  HashIndexMap const& _map;
  std::unordered_map<arangodb::LocalDocumentId, VPackBuilder> _documents;
  std::unordered_map<arangodb::LocalDocumentId, VPackBuilder>::const_iterator _begin;
  std::unordered_map<arangodb::LocalDocumentId, VPackBuilder>::const_iterator _end;
};  // HashIndexIteratorMock

class HashIndexMock final : public arangodb::Index {
 public:
  static std::shared_ptr<arangodb::Index> make(arangodb::IndexId iid,
                                               arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition) {
    auto const typeSlice = definition.get("type");

    if (typeSlice.isNone()) {
      return nullptr;
    }

    auto const type = arangodb::basics::VelocyPackHelper::getStringRef(typeSlice,
                                                                       arangodb::velocypack::StringRef());

    if (type.compare("hash") != 0) {
      return nullptr;
    }

    return std::make_shared<HashIndexMock>(iid, collection, definition);
  }

  IndexType type() const override { return Index::TRI_IDX_TYPE_HASH_INDEX; }

  char const* typeName() const override { return "hash"; }

  bool isPersistent() const override { return false; }

  bool canBeDropped() const override { return false; }

  bool hasCoveringIterator() const override { return true; }

  bool isHidden() const override { return false; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return false; }

  size_t memory() const override { return sizeof(HashIndexMock); }

  void load() override {}

  void unload() override {}

  void afterTruncate(TRI_voc_tick_t, arangodb::transaction::Methods*) override {
    _hashData.clear();
  }

  void toVelocyPack(VPackBuilder& builder,
                    std::underlying_type<arangodb::Index::Serialize>::type flags) const override {
    builder.openObject();
    Index::toVelocyPack(builder, flags);
    builder.add("sparse", VPackValue(sparse()));
    builder.add("unique", VPackValue(unique()));
    builder.close();
  }

  void toVelocyPackFigures(VPackBuilder& builder) const override {
    Index::toVelocyPackFigures(builder);
  }

  arangodb::Result insert(arangodb::transaction::Methods&,
                          arangodb::LocalDocumentId const& documentId,
                          arangodb::velocypack::Slice const doc) {
    if (!doc.isObject()) {
      return {TRI_ERROR_INTERNAL};
    }

    _hashData.insert(documentId, doc);

    return {};  // ok
  }

  arangodb::Result remove(arangodb::transaction::Methods&,
                          arangodb::LocalDocumentId const& documentId,
                          arangodb::velocypack::Slice const doc) {
    if (!doc.isObject()) {
      return {TRI_ERROR_INTERNAL};
    }

    _hashData.remove(documentId, doc);

    return {};  // ok
  }

  Index::FilterCosts supportsFilterCondition(
      std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
      arangodb::aql::AstNode const* node, arangodb::aql::Variable const* reference,
      size_t itemsInIndex) const override {
    return arangodb::SortedIndexAttributeMatcher::supportsFilterCondition(allIndexes, this, node, reference, itemsInIndex);
  }

  Index::SortCosts supportsSortCondition(arangodb::aql::SortCondition const* sortCondition,
                                                            arangodb::aql::Variable const* reference,
                                                            size_t itemsInIndex) const override {
    return arangodb::SortedIndexAttributeMatcher::supportsSortCondition(this, sortCondition, reference, itemsInIndex);
  }

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode* node, arangodb::aql::Variable const* reference) const override {
    return arangodb::SortedIndexAttributeMatcher::specializeCondition(this, node, reference);
  }

  std::unique_ptr<arangodb::IndexIterator> iteratorForCondition(
      arangodb::transaction::Methods* trx, arangodb::aql::AstNode const* node,
      arangodb::aql::Variable const*, arangodb::IndexIteratorOptions const&) override {
    arangodb::transaction::BuilderLeaser builder(trx);
    std::unique_ptr<VPackBuilder> keys(builder.steal());
    keys->openArray();
    if (nullptr == node) {
      keys->close();
      return std::make_unique<HashIndexIteratorMock>(&_collection, trx, this,
                                                     _hashData,
                                                     std::move(keys));
    }
    TRI_ASSERT(node->type == arangodb::aql::NODE_TYPE_OPERATOR_NARY_AND);

    std::vector<std::pair<std::vector<arangodb::basics::AttributeName>, arangodb::aql::AstNode*>> allAttributes;
    for (size_t i = 0; i < node->numMembers(); ++i) {
      auto comp = node->getMember(i);
      // a.b == value
      if (!(comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_EQ ||
            comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN)) {
        // operator type unsupported
        return std::make_unique<arangodb::EmptyIndexIterator>(&_collection, trx);
      }

      // assume a.b == value
      auto attrNode = comp->getMember(0);
      auto valNode = comp->getMember(1);

      if (!(attrNode->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS ||
          attrNode->type == arangodb::aql::NODE_TYPE_EXPANSION)) {
        // got value == a.b -> flip sides
        std::swap(attrNode, valNode);
      }
      TRI_ASSERT(attrNode->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS ||
                 attrNode->type == arangodb::aql::NODE_TYPE_EXPANSION);

      std::vector<arangodb::basics::AttributeName> attributes;
      if (attrNode->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
        do {
          attributes.emplace_back(std::string(attrNode->getStringValue(), attrNode->getStringLength()), false);
          attrNode = attrNode->getMember(0);
        } while (attrNode->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);
        std::reverse(attributes.begin(), attributes.end());
      } else { // expansion
        TRI_ASSERT(attrNode->type == arangodb::aql::NODE_TYPE_EXPANSION);
        auto expNode = attrNode;
        TRI_ASSERT(expNode->numMembers() >= 2);
        auto left = expNode->getMember(0);
        TRI_ASSERT(left->type == arangodb::aql::NODE_TYPE_ITERATOR);
        attrNode = left->getMember(1);
        TRI_ASSERT(attrNode->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);
        do {
          attributes.emplace_back(std::string(attrNode->getStringValue(), attrNode->getStringLength()), false);
          attrNode = attrNode->getMember(0);
        } while (attrNode->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS);
        attributes.front().shouldExpand = true;
        std::reverse(attributes.begin(), attributes.end());

        std::vector<arangodb::basics::AttributeName> attributesRight;
        attrNode = expNode->getMember(1);
        TRI_ASSERT(attrNode->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS ||
                   attrNode->type == arangodb::aql::NODE_TYPE_REFERENCE);
        while (attrNode->type == arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
          attributesRight.emplace_back(std::string(attrNode->getStringValue(), attrNode->getStringLength()), false);
          attrNode = attrNode->getMember(0);
        }
        attributes.insert(attributes.end(), attributesRight.crbegin(), attributesRight.crend());
      }
      allAttributes.emplace_back(std::move(attributes), valNode);
    }
    size_t nullsCount = 0;
    for (auto const& f : _fields) {
      auto it = std::find_if(allAttributes.cbegin(), allAttributes.cend(), [&f] (auto const& attrs) {
        return arangodb::basics::AttributeName::isIdentical(attrs.first, f, true);
      });
      if (it != allAttributes.cend()) {
        while (nullsCount > 0) {
          keys->add(VPackSlice::nullSlice());
          --nullsCount;
        }
        it->second->toVelocyPackValue(*keys);
      } else {
        ++nullsCount;
      }
    }
    keys->close();

    return std::make_unique<HashIndexIteratorMock>(&_collection, trx, this,
                                                   _hashData,
                                                   std::move(keys));
  }

  HashIndexMock(arangodb::IndexId iid, arangodb::LogicalCollection& collection,
                VPackSlice const& slice)
      : arangodb::Index(iid, collection, slice), _hashData(_fields) {}

  /// @brief the hash table for data
  HashIndexMap _hashData;
};  // HashIndexMock
}  // namespace

PhysicalCollectionMock::DocElement::DocElement(
    std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>> data, uint64_t docId)
    : _data(data), _docId(docId) {}

arangodb::velocypack::Slice PhysicalCollectionMock::DocElement::data() const {
  return arangodb::velocypack::Slice(_data->data());
}

std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>> PhysicalCollectionMock::DocElement::rawData() const {
  return _data;
}

void PhysicalCollectionMock::DocElement::swapBuffer(
    std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>& newData) {
  _data.swap(newData);
}

arangodb::LocalDocumentId PhysicalCollectionMock::DocElement::docId() const {
  return arangodb::LocalDocumentId::create(_docId);
}

uint8_t const* PhysicalCollectionMock::DocElement::vptr() const {
  return _data->data();
}

std::function<void()> PhysicalCollectionMock::before = []() -> void {};

PhysicalCollectionMock::PhysicalCollectionMock(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& info)
    : PhysicalCollection(collection, info), _lastDocumentId{0} {}

arangodb::PhysicalCollection* PhysicalCollectionMock::clone(arangodb::LogicalCollection& collection) const {
  before();
  TRI_ASSERT(false);
  return nullptr;
}

int PhysicalCollectionMock::close() {
  for (auto& index : _indexes) {
    index->unload();
  }

  return TRI_ERROR_NO_ERROR;  // assume close successful
}

std::shared_ptr<arangodb::Index> PhysicalCollectionMock::createIndex(
    arangodb::velocypack::Slice const& info, bool restore, bool& created) {
  before();

  std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> docs;
  docs.reserve(_documents.size());
  for (auto const& entry : _documents) {
    auto& doc = entry.second;
    docs.emplace_back(doc.docId(), doc.data());
  }

  struct IndexFactory : public arangodb::IndexFactory {
    using arangodb::IndexFactory::validateSlice;
  };
  auto id = IndexFactory::validateSlice(info, true, false);  // true+false to ensure id generation if missing

  auto const type =
      arangodb::basics::VelocyPackHelper::getStringRef(info.get("type"),
                                                       arangodb::velocypack::StringRef());

  std::shared_ptr<arangodb::Index> index;

  if (0 == type.compare("edge")) {
    index = EdgeIndexMock::make(id, _logicalCollection, info);
  } else if (0 == type.compare("hash")) {
    index = HashIndexMock::make(id, _logicalCollection, info);
  } else if (0 == type.compare(arangodb::iresearch::DATA_SOURCE_TYPE.name())) {
    try {
      auto& server = _logicalCollection.vocbase().server();
      if (arangodb::ServerState::instance()->isCoordinator()) {
        auto& factory =
            server.getFeature<arangodb::iresearch::IResearchFeature>().factory<arangodb::ClusterEngine>();
        index = factory.instantiate(_logicalCollection, info, id, false);
      } else {
        index = StorageEngineMock::buildLinkMock(id, _logicalCollection, info);
      }
    } catch (std::exception const& ex) {
      // ignore the details of all errors here
      LOG_DEVEL << "caught: " << ex.what();
    }
  }

  if (!index) {
    return nullptr;
  }

  asio_ns::io_context ioContext;
  auto poster = [&ioContext](std::function<void()> fn) -> bool {
    ioContext.post(fn);
    return true;
  };
  auto& server = _logicalCollection.vocbase().server();
  arangodb::basics::LocalTaskQueue taskQueue(server, poster);
  std::shared_ptr<arangodb::basics::LocalTaskQueue> taskQueuePtr(
      &taskQueue, [](arangodb::basics::LocalTaskQueue*) -> void {});

  TRI_vocbase_t& vocbase = _logicalCollection.vocbase();
  arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                            _logicalCollection,
                                            arangodb::AccessMode::Type::WRITE);
  auto res = trx.begin();
  TRI_ASSERT(res.ok());

  if (index->type() == arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX) {
    auto* l = dynamic_cast<EdgeIndexMock*>(index.get());
    TRI_ASSERT(l != nullptr);
    for (auto const& pair : docs) {
      l->insert(trx, pair.first, pair.second);
    }
  } else if (index->type() == arangodb::Index::TRI_IDX_TYPE_HASH_INDEX) {
    auto* l = dynamic_cast<HashIndexMock*>(index.get());
    TRI_ASSERT(l != nullptr);
    for (auto const& pair : docs) {
      l->insert(trx, pair.first, pair.second);
    }
  } else if (index->type() == arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK) {
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(index.get());
    TRI_ASSERT(l != nullptr);
    l->batchInsert(trx, docs, taskQueuePtr);
  } else {
    TRI_ASSERT(false);
  }

  if (TRI_ERROR_NO_ERROR != taskQueue.status()) {
    return nullptr;
  }

  _indexes.emplace(index);
  created = true;

  res = trx.commit();
  TRI_ASSERT(res.ok());

  return index;
}

void PhysicalCollectionMock::deferDropCollection(
    std::function<bool(arangodb::LogicalCollection&)> const& callback) {
  before();

  callback(_logicalCollection);  // assume noone is using this collection (drop immediately)
}

bool PhysicalCollectionMock::dropIndex(arangodb::IndexId iid) {
  before();

  for (auto itr = _indexes.begin(), end = _indexes.end(); itr != end; ++itr) {
    if ((*itr)->id() == iid) {
      if ((*itr)->drop().ok()) {
        _indexes.erase(itr);
        return true;
      }
    }
  }

  return false;
}

void PhysicalCollectionMock::figuresSpecific(bool /*details*/, arangodb::velocypack::Builder&) {
  before();
  TRI_ASSERT(false);
}

std::unique_ptr<arangodb::IndexIterator> PhysicalCollectionMock::getAllIterator(
    arangodb::transaction::Methods* trx) const {
  before();

  return std::make_unique<AllIteratorMock>(_documents, this->_logicalCollection, trx);
}

std::unique_ptr<arangodb::IndexIterator> PhysicalCollectionMock::getAnyIterator(
    arangodb::transaction::Methods* trx) const {
  before();
  return std::make_unique<AllIteratorMock>(_documents, this->_logicalCollection, trx);
}

std::unique_ptr<arangodb::ReplicationIterator> PhysicalCollectionMock::getReplicationIterator(
    arangodb::ReplicationIterator::Ordering, uint64_t) {
  return nullptr;
}

void PhysicalCollectionMock::getPropertiesVPack(arangodb::velocypack::Builder&) const {
  before();
}

arangodb::Result PhysicalCollectionMock::insert(
    arangodb::transaction::Methods* trx, arangodb::velocypack::Slice newSlice,
    arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options) {
  before();

  TRI_ASSERT(newSlice.isObject());
  VPackSlice newKey = newSlice.get(arangodb::StaticStrings::KeyString);
  if (newKey.isString()) {
    if (_documents.find(arangodb::velocypack::StringRef{newKey}) != _documents.end()) {
      return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
    }
  }

  arangodb::velocypack::Builder builder;
  auto isEdgeCollection = (TRI_COL_TYPE_EDGE == _logicalCollection.type());

  arangodb::RevisionId revisionId;
  auto res = newObjectForInsert(trx, newSlice, isEdgeCollection, builder,
                                options.isRestore, revisionId);

  if (res.fail()) {
    return res;
  }
  TRI_ASSERT(builder.slice().get(arangodb::StaticStrings::KeyString).isString());

  arangodb::velocypack::StringRef key{builder.slice().get(arangodb::StaticStrings::KeyString)};
  arangodb::LocalDocumentId id = ::generateDocumentId(_logicalCollection, revisionId, _lastDocumentId);
  auto const& [ref, didInsert] =
      _documents.emplace(key, DocElement{builder.steal(), id.id()});
  TRI_ASSERT(didInsert);

  result.setManaged(ref->second.vptr());
  TRI_ASSERT(result.revisionId() == revisionId);

  for (auto& index : _indexes) {
    if (index->type() == arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX) {
      auto* l = static_cast<EdgeIndexMock*>(index.get());
      if (!l->insert(*trx, ref->second.docId(),
                     arangodb::velocypack::Slice(result.vpack()))
               .ok()) {
        return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
      }
      continue;
    } else if (index->type() == arangodb::Index::TRI_IDX_TYPE_HASH_INDEX) {
      auto* l = static_cast<HashIndexMock*>(index.get());
      if (!l->insert(*trx, ref->second.docId(),
                     arangodb::velocypack::Slice(result.vpack()))
               .ok()) {
        return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
      }
      continue;
    } else if (index->type() == arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK) {
      if (arangodb::ServerState::instance()->isCoordinator()) {
        auto* l =
            static_cast<arangodb::iresearch::IResearchLinkCoordinator*>(index.get());
        if (!l->insert(*trx, ref->second.docId(),
                       arangodb::velocypack::Slice(result.vpack()))
                 .ok()) {
          return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
        }
      } else {
        auto* l = static_cast<arangodb::iresearch::IResearchLinkMock*>(index.get());
        if (!l->insert(*trx, ref->second.docId(),
                       arangodb::velocypack::Slice(result.vpack()))
                 .ok()) {
          return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
        }
      }
      continue;
    }
    TRI_ASSERT(false);
  }

  return arangodb::Result();
}

arangodb::Result PhysicalCollectionMock::lookupKey(
    arangodb::transaction::Methods*, arangodb::velocypack::StringRef key,
    std::pair<arangodb::LocalDocumentId, arangodb::RevisionId>& result) const {
  before();
    
  auto it = _documents.find(arangodb::velocypack::StringRef{key});
  if (it != _documents.end()) {
    if (_documents.find(arangodb::velocypack::StringRef{key}) != _documents.end()) {
      result.first = it->second.docId();
      result.second = arangodb::RevisionId::fromSlice(it->second.data());
      return arangodb::Result();
    }
  }

  result.first = arangodb::LocalDocumentId::none();
  result.second = arangodb::RevisionId::none();
  return arangodb::Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

size_t PhysicalCollectionMock::memory() const {
  before();
  TRI_ASSERT(false);
  return 0;
}

uint64_t PhysicalCollectionMock::numberDocuments(arangodb::transaction::Methods*) const {
  before();
  return _documents.size();
}

std::string const& PhysicalCollectionMock::path() const {
  before();

  return physicalPath;
}

bool PhysicalCollectionMock::addIndex(std::shared_ptr<arangodb::Index> idx) {
  auto const id = idx->id();
  for (auto const& it : _indexes) {
    if (it->id() == id) {
      // already have this particular index. do not add it again
      return false;
    }
  }

  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id.id()));

  _indexes.emplace(idx);
  return true;
}

void PhysicalCollectionMock::prepareIndexes(arangodb::velocypack::Slice indexesSlice) {
  before();

  auto& engine = _logicalCollection.vocbase()
                     .server()
                     .getFeature<arangodb::EngineSelectorFeature>()
                     .engine();
  auto& idxFactory = engine.indexFactory();

  for (VPackSlice v : VPackArrayIterator(indexesSlice)) {
    if (arangodb::basics::VelocyPackHelper::getBooleanValue(v, "error", false)) {
      // We have an error here.
      // Do not add index.
      continue;
    }

    try {
      auto idx = idxFactory.prepareIndexFromSlice(v, false, _logicalCollection, true);

      if (!idx) {
        continue;
      }

      if (!addIndex(idx)) {
        return;
      }
    } catch (std::exception const&) {
      // error is just ignored here
    }
  }
}

arangodb::Result PhysicalCollectionMock::read(arangodb::transaction::Methods*,
                                              arangodb::velocypack::StringRef const& key,
                                              arangodb::ManagedDocumentResult& result) {
  before();
  auto it = _documents.find(key);
  if (it != _documents.end()) {
    result.setManaged(it->second.vptr());
    return arangodb::Result(TRI_ERROR_NO_ERROR);
  }
  return arangodb::Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

bool PhysicalCollectionMock::readDocument(arangodb::transaction::Methods* trx,
                                          arangodb::LocalDocumentId const& token,
                                          arangodb::ManagedDocumentResult& result) const {
  before();
  for (auto const& entry : _documents) {
    auto& doc = entry.second;
    if (doc.docId() == token) {
      result.setManaged(doc.vptr());
      return true;
    }
  }
  return false;
}

bool PhysicalCollectionMock::readDocumentWithCallback(
    arangodb::transaction::Methods* trx, arangodb::LocalDocumentId const& token,
    arangodb::IndexIterator::DocumentCallback const& cb) const {
  before();
  for (auto const& entry : _documents) {
    auto& doc = entry.second;
    if (doc.docId() == token) {
      cb(token, doc.data());
      return true;
    }
  }
  return false;
}

arangodb::Result PhysicalCollectionMock::remove(
    arangodb::transaction::Methods& trx, arangodb::velocypack::Slice slice,
    arangodb::ManagedDocumentResult& previous, arangodb::OperationOptions& options) {
  before();

  auto key = slice.get(arangodb::StaticStrings::KeyString);
  TRI_ASSERT(key.isString());
  arangodb::velocypack::StringRef keyRef{key};
  auto old = _documents.find(keyRef);
  if (old != _documents.end()) {
    previous.setManaged(old->second.vptr());
    _graveyard.emplace_back(old->second.rawData());
    TRI_ASSERT(previous.revisionId() ==
               arangodb::RevisionId::fromSlice(old->second.data()));
    _documents.erase(keyRef);
    return arangodb::Result(TRI_ERROR_NO_ERROR);  // assume document was removed
  }
  return arangodb::Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

arangodb::Result PhysicalCollectionMock::update(
    arangodb::transaction::Methods* trx, arangodb::velocypack::Slice newSlice,
    arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options,
    arangodb::ManagedDocumentResult& previous) {
  return updateInternal(trx, newSlice, result, options, previous, true);
}

arangodb::Result PhysicalCollectionMock::replace(
    arangodb::transaction::Methods* trx, arangodb::velocypack::Slice newSlice,
    arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options,
    arangodb::ManagedDocumentResult& previous) {
  return updateInternal(trx, newSlice, result, options, previous, false);
}

arangodb::RevisionId PhysicalCollectionMock::revision(arangodb::transaction::Methods*) const {
  before();
  TRI_ASSERT(false);
  return arangodb::RevisionId::none();
}

arangodb::Result PhysicalCollectionMock::truncate(arangodb::transaction::Methods& trx,
                                                  arangodb::OperationOptions& options) {
  before();
  _documents.clear();
  return arangodb::Result();
}

arangodb::Result PhysicalCollectionMock::compact() {
  return arangodb::Result();
}

arangodb::Result PhysicalCollectionMock::updateInternal(
    arangodb::transaction::Methods* trx, arangodb::velocypack::Slice newSlice,
    arangodb::ManagedDocumentResult& result, arangodb::OperationOptions& options,
    arangodb::ManagedDocumentResult& previous, bool isUpdate) {
  auto key = newSlice.get(arangodb::StaticStrings::KeyString);
  if (!key.isString()) {
    return arangodb::Result(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  before();
  arangodb::velocypack::StringRef keyRef{key};
  auto it = _documents.find(keyRef);
  if (it != _documents.end()) {
    auto doc = it->second.data();
    if (!options.ignoreRevs) {
      arangodb::RevisionId expectedRev = arangodb::RevisionId::none();
      if (newSlice.isObject()) {
        expectedRev = arangodb::RevisionId::fromSlice(newSlice);
      }
      TRI_ASSERT(doc.isObject());
      arangodb::RevisionId oldRev = arangodb::RevisionId::fromSlice(doc);
      if (!checkRevision(trx, expectedRev, oldRev)) {
        return arangodb::Result(TRI_ERROR_ARANGO_CONFLICT, "_rev values mismatch");
      }
    }
    arangodb::velocypack::Builder builder;
    arangodb::RevisionId revisionId = arangodb::RevisionId::none();  // unused
    auto isEdgeCollection = (TRI_COL_TYPE_EDGE == _logicalCollection.type());
    if (isUpdate) {
      arangodb::Result res =
          mergeObjectsForUpdate(trx, doc, newSlice, isEdgeCollection,
                                options.mergeObjects, options.keepNull, builder,
                                options.isRestore, revisionId);
      if (res.fail()) {
        return res;
      }
    } else {
      arangodb::Result res = newObjectForReplace(trx, doc, newSlice, isEdgeCollection,
                                                 builder, options.isRestore, revisionId);
      if (res.fail()) {
        return res;
      }
    }
    auto nextBuffer = builder.steal();
    // Set previous
    previous.setManaged(it->second.vptr());
    TRI_ASSERT(previous.revisionId() == arangodb::RevisionId::fromSlice(doc));

    // swap with new data
    // Replace the existing Buffer and nextBuffer
    it->second.swapBuffer(nextBuffer);
    // Put the now old buffer into the graveyour for previous to stay valid
    _graveyard.emplace_back(nextBuffer);

    result.setManaged(it->second.vptr());
    TRI_ASSERT(result.revisionId() != previous.revisionId());
    return TRI_ERROR_NO_ERROR;
  }
  return arangodb::Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

arangodb::Result PhysicalCollectionMock::updateProperties(arangodb::velocypack::Slice const& slice,
                                                          bool doSync) {
  before();

  return arangodb::Result(TRI_ERROR_NO_ERROR);  // assume mock collection updated OK
}

std::shared_ptr<arangodb::iresearch::IResearchLinkMock> StorageEngineMock::buildLinkMock(
  arangodb::IndexId id, arangodb::LogicalCollection& collection, VPackSlice const& info) {
  auto index = std::shared_ptr<arangodb::iresearch::IResearchLinkMock>(
    new arangodb::iresearch::IResearchLinkMock(id, collection));
  auto res = static_cast<arangodb::iresearch::IResearchLinkMock*>(index.get())->init(info, [](irs::directory& dir) {
    if (arangodb::iresearch::IResearchLinkMock::InitCallback != nullptr) {
      arangodb::iresearch::IResearchLinkMock::InitCallback(dir);
    }
    });

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  return index;
}

std::function<void()> StorageEngineMock::before = []() -> void {};
arangodb::RecoveryState StorageEngineMock::recoveryStateResult =
    arangodb::RecoveryState::DONE;
TRI_voc_tick_t StorageEngineMock::recoveryTickResult = 0;
std::function<void()> StorageEngineMock::recoveryTickCallback = []() -> void {};

/*static*/ std::string StorageEngineMock::versionFilenameResult;

StorageEngineMock::StorageEngineMock(arangodb::application_features::ApplicationServer& server)
    : StorageEngine(server, "Mock", "",
                    std::unique_ptr<arangodb::IndexFactory>(new IndexFactoryMock(server))),
      vocbaseCount(1),
      _releasedTick(0) {}

arangodb::WalAccess const* StorageEngineMock::walAccess() const {
  TRI_ASSERT(false);
  return nullptr;
}

void StorageEngineMock::addOptimizerRules(arangodb::aql::OptimizerRulesFeature& /*feature*/) {
  before();
  // NOOP
}

void StorageEngineMock::addRestHandlers(arangodb::rest::RestHandlerFactory& handlerFactory) {
  TRI_ASSERT(false);
}

void StorageEngineMock::addV8Functions() {
  TRI_ASSERT(false); 
}

void StorageEngineMock::changeCollection(TRI_vocbase_t& vocbase,
                                         arangodb::LogicalCollection const& collection,
                                         bool doSync) {
  // NOOP, assume physical collection changed OK
}

arangodb::Result StorageEngineMock::changeView(TRI_vocbase_t& vocbase,
                                               arangodb::LogicalView const& view,
                                               bool doSync) {
  before();
  TRI_ASSERT(views.find(std::make_pair(vocbase.id(), view.id())) != views.end());
  arangodb::velocypack::Builder builder;

  builder.openObject();
  auto res = view.properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
  if (!res.ok()) {
    return res;
  }
  builder.close();
  views[std::make_pair(vocbase.id(), view.id())] = std::move(builder);
  return {};
}

void StorageEngineMock::createCollection(TRI_vocbase_t& vocbase,
                                         arangodb::LogicalCollection const& collection) {}

std::unique_ptr<TRI_vocbase_t> StorageEngineMock::createDatabase(arangodb::CreateDatabaseInfo&& info,
                                                                 int& status) {
  if (arangodb::ServerState::instance()->isCoordinator()) {
    return std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR,
                                           std::move(info));
  }
  return std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                         std::move(info));
}

arangodb::Result StorageEngineMock::createLoggerState(TRI_vocbase_t*, VPackBuilder&) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<arangodb::PhysicalCollection> StorageEngineMock::createPhysicalCollection(
    arangodb::LogicalCollection& collection, arangodb::velocypack::Slice const& info) {
  before();
  return std::unique_ptr<arangodb::PhysicalCollection>(
      new PhysicalCollectionMock(collection, info));
}

arangodb::Result StorageEngineMock::createTickRanges(VPackBuilder&) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<arangodb::TransactionCollection> StorageEngineMock::createTransactionCollection(
    arangodb::TransactionState& state, arangodb::DataSourceId cid,
    arangodb::AccessMode::Type accessType) {
  return std::unique_ptr<arangodb::TransactionCollection>(
      new TransactionCollectionMock(&state, cid, accessType));
}

std::unique_ptr<arangodb::transaction::Manager> StorageEngineMock::createTransactionManager(
    arangodb::transaction::ManagerFeature& feature) {
  return std::make_unique<arangodb::transaction::Manager>(feature);
}

std::shared_ptr<arangodb::TransactionState> StorageEngineMock::createTransactionState(
    TRI_vocbase_t& vocbase, arangodb::TransactionId tid,
    arangodb::transaction::Options const& options) {
  return std::make_shared<TransactionStateMock>(vocbase, tid, options);
}

arangodb::Result StorageEngineMock::createView(TRI_vocbase_t& vocbase,
                                               arangodb::DataSourceId id,
                                               arangodb::LogicalView const& view) {
  before();
  TRI_ASSERT(views.find(std::make_pair(vocbase.id(), view.id())) == views.end());  // called after createView()
  arangodb::velocypack::Builder builder;

  builder.openObject();
  auto res = view.properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
  if (!res.ok()) {
    return res;
  }
  builder.close();
  views[std::make_pair(vocbase.id(), view.id())] = std::move(builder);

  return arangodb::Result(TRI_ERROR_NO_ERROR);  // assume mock view persisted OK
}
  
arangodb::Result StorageEngineMock::compactAll(bool changeLevels, bool compactBottomMostLevel) {
  TRI_ASSERT(false);
  return arangodb::Result();
}

TRI_voc_tick_t StorageEngineMock::currentTick() const {
  return TRI_CurrentTickServer();
}

std::string StorageEngineMock::dataPath() const {
  before();
  return "";  // no valid path filesystem persisted, return empty string
}

std::string StorageEngineMock::databasePath(TRI_vocbase_t const* vocbase) const {
  before();
  return "";  // no valid path filesystem persisted, return empty string
}

arangodb::Result StorageEngineMock::dropCollection(TRI_vocbase_t& vocbase,
                                                   arangodb::LogicalCollection& collection) {
  return arangodb::Result(TRI_ERROR_NO_ERROR);  // assume physical collection dropped OK
}

arangodb::Result StorageEngineMock::dropDatabase(TRI_vocbase_t& vocbase) {
  TRI_ASSERT(false);
  return arangodb::Result();
}

arangodb::Result StorageEngineMock::dropView(TRI_vocbase_t const& vocbase,
                                             arangodb::LogicalView const& view) {
  before();
  TRI_ASSERT(views.find(std::make_pair(vocbase.id(), view.id())) != views.end());
  views.erase(std::make_pair(vocbase.id(), view.id()));

  return arangodb::Result(TRI_ERROR_NO_ERROR);  // assume mock view dropped OK
}

arangodb::Result StorageEngineMock::firstTick(uint64_t&) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
}

void StorageEngineMock::getCollectionInfo(TRI_vocbase_t& vocbase, arangodb::DataSourceId cid,
                                          arangodb::velocypack::Builder& result,
                                          bool includeIndexes, TRI_voc_tick_t maxTick) {
  arangodb::velocypack::Builder parameters;

  parameters.openObject();
  parameters.close();

  result.openObject();
  result.add("parameters", parameters.slice());  // required entry of type object
  result.close();

  // nothing more required, assume info used for PhysicalCollectionMock
}

int StorageEngineMock::getCollectionsAndIndexes(TRI_vocbase_t& vocbase,
                                                arangodb::velocypack::Builder& result,
                                                bool wasCleanShutdown, bool isUpgrade) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

void StorageEngineMock::getDatabases(arangodb::velocypack::Builder& result) {
  before();
  arangodb::velocypack::Builder system;

  system.openObject();
  system.add("name", arangodb::velocypack::Value(arangodb::StaticStrings::SystemDatabase));
  system.close();

  // array expected
  result.openArray();
  result.add(system.slice());
  result.close();
}

void StorageEngineMock::cleanupReplicationContexts() {
  // nothing to do here
}

arangodb::velocypack::Builder StorageEngineMock::getReplicationApplierConfiguration(
    TRI_vocbase_t& vocbase, int& result) {
  before();
  result = TRI_ERROR_FILE_NOT_FOUND;  // assume no ReplicationApplierConfiguration for vocbase

  return arangodb::velocypack::Builder();
}

arangodb::velocypack::Builder StorageEngineMock::getReplicationApplierConfiguration(int& result) {
  before();
  result = TRI_ERROR_FILE_NOT_FOUND;

  return arangodb::velocypack::Builder();
}

int StorageEngineMock::getViews(TRI_vocbase_t& vocbase, arangodb::velocypack::Builder& result) {
  result.openArray();

  for (auto& entry : views) {
    result.add(entry.second.slice());
  }

  result.close();

  return TRI_ERROR_NO_ERROR;
}

arangodb::Result StorageEngineMock::handleSyncKeys(arangodb::DatabaseInitialSyncer& syncer,
                                                   arangodb::LogicalCollection& col,
                                                   std::string const& keysId) {
  TRI_ASSERT(false);
  return arangodb::Result();
}

arangodb::RecoveryState StorageEngineMock::recoveryState() {
  return recoveryStateResult;
}
TRI_voc_tick_t StorageEngineMock::recoveryTick() {
  if (recoveryTickCallback) {
    recoveryTickCallback();
  }
  return recoveryTickResult;
}

arangodb::Result StorageEngineMock::lastLogger(
    TRI_vocbase_t& vocbase, 
    uint64_t tickStart, uint64_t tickEnd, arangodb::velocypack::Builder& builderSPtr) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<TRI_vocbase_t> StorageEngineMock::openDatabase(arangodb::CreateDatabaseInfo&& info,
                                                               bool isUpgrade) {
  before();

  auto new_info = info;
  new_info.setId(++vocbaseCount);

  return std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                         std::move(new_info));
}

TRI_voc_tick_t StorageEngineMock::releasedTick() const {
  before();
  return _releasedTick;
}

void StorageEngineMock::releaseTick(TRI_voc_tick_t tick) {
  before();
  _releasedTick = tick;
}

int StorageEngineMock::removeReplicationApplierConfiguration(TRI_vocbase_t& vocbase) {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

int StorageEngineMock::removeReplicationApplierConfiguration() {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

arangodb::Result StorageEngineMock::renameCollection(TRI_vocbase_t& vocbase,
                                                     arangodb::LogicalCollection const& collection,
                                                     std::string const& oldName) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_INTERNAL);
}

int StorageEngineMock::saveReplicationApplierConfiguration(TRI_vocbase_t& vocbase,
                                                           arangodb::velocypack::Slice slice,
                                                           bool doSync) {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

int StorageEngineMock::saveReplicationApplierConfiguration(arangodb::velocypack::Slice, bool) {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

std::string StorageEngineMock::versionFilename(TRI_voc_tick_t) const {
  return versionFilenameResult;
}

void StorageEngineMock::waitForEstimatorSync(std::chrono::milliseconds) {
  TRI_ASSERT(false);
}

std::vector<std::string> StorageEngineMock::currentWalFiles() const {
  return std::vector<std::string>();
}

arangodb::Result StorageEngineMock::flushWal(bool waitForSync, bool waitForCollector) {
  TRI_ASSERT(false);
  return arangodb::Result();
}

TransactionCollectionMock::TransactionCollectionMock(arangodb::TransactionState* state,
                                                     arangodb::DataSourceId cid,
                                                     arangodb::AccessMode::Type accessType)
    : TransactionCollection(state, cid, accessType) {}

bool TransactionCollectionMock::canAccess(arangodb::AccessMode::Type accessType) const {
  return nullptr != _collection;  // collection must have be opened previously
}

bool TransactionCollectionMock::hasOperations() const {
  TRI_ASSERT(false);
  return false;
}

void TransactionCollectionMock::releaseUsage() {
  if (_collection) {
    if (!arangodb::ServerState::instance()->isCoordinator()) {
      _transaction->vocbase().releaseCollection(_collection.get());
    }
    _collection = nullptr;
  }
}

arangodb::Result TransactionCollectionMock::lockUsage() {
  bool shouldLock = !arangodb::AccessMode::isNone(_accessType);

  if (shouldLock && !isLocked()) {
    // r/w lock the collection
    arangodb::Result res = doLock(_accessType);

    if (res.is(TRI_ERROR_LOCKED)) {
      // TRI_ERROR_LOCKED is not an error, but it indicates that the lock
      // operation has actually acquired the lock (and that the lock has not
      // been held before)
      res.reset();
    } else if (res.fail()) {
      return res;
    }
  }

  if (!_collection) {
    if (arangodb::ServerState::instance()->isCoordinator()) {
      auto& ci = _transaction->vocbase()
                     .server()
                     .getFeature<arangodb::ClusterFeature>()
                     .clusterInfo();
      _collection = ci.getCollectionNT(_transaction->vocbase().name(),
                                       std::to_string(_cid.id()));
    } else {
      _collection = _transaction->vocbase().useCollection(_cid, true);
    }
  }

  return arangodb::Result(_collection ? TRI_ERROR_NO_ERROR : TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

arangodb::Result TransactionCollectionMock::doLock(arangodb::AccessMode::Type type) {
  if (_lockType > _accessType) {
    return {TRI_ERROR_INTERNAL};
  }

  _lockType = type;

  return {};
}

arangodb::Result TransactionCollectionMock::doUnlock(arangodb::AccessMode::Type type) {
  if (_lockType != type) {
    return {TRI_ERROR_INTERNAL};
  }

  _lockType = arangodb::AccessMode::Type::NONE;

  return {};
}

size_t TransactionStateMock::abortTransactionCount;
size_t TransactionStateMock::beginTransactionCount;
size_t TransactionStateMock::commitTransactionCount;

// ensure each transaction state has a unique ID
TransactionStateMock::TransactionStateMock(TRI_vocbase_t& vocbase, arangodb::TransactionId tid,
                                           arangodb::transaction::Options const& options)
    : TransactionState(vocbase, tid, options) {}

arangodb::Result TransactionStateMock::abortTransaction(arangodb::transaction::Methods* trx) {
  ++abortTransactionCount;
  updateStatus(arangodb::transaction::Status::ABORTED);
//  releaseUsage();
  // avoid use of TransactionManagerFeature::manager()->unregisterTransaction(...)
  const_cast<arangodb::TransactionId&>(_id) = arangodb::TransactionId::none();

  return arangodb::Result();
}

arangodb::Result TransactionStateMock::beginTransaction(arangodb::transaction::Hints hints) {
  ++beginTransactionCount;
  _hints = hints;
  
  arangodb::Result res = useCollections();
  if (res.fail()) { // something is wrong
    return res;
  }


  if (!res.ok()) {
    updateStatus(arangodb::transaction::Status::ABORTED);
    // avoid use of TransactionManagerFeature::manager()->unregisterTransaction(...)
    const_cast<arangodb::TransactionId&>(_id) = arangodb::TransactionId::none();
    return res;
  }
  updateStatus(arangodb::transaction::Status::RUNNING);
  return arangodb::Result();
}

arangodb::Result TransactionStateMock::commitTransaction(arangodb::transaction::Methods* trx) {
  ++commitTransactionCount;
  updateStatus(arangodb::transaction::Status::COMMITTED);
  // avoid use of TransactionManagerFeature::manager()->unregisterTransaction(...)
  const_cast<arangodb::TransactionId&>(_id) = arangodb::TransactionId::none();
  //  releaseUsage();

  return arangodb::Result();
}
  
uint64_t TransactionStateMock::numCommits() const {
  return commitTransactionCount;
}

bool TransactionStateMock::hasFailedOperations() const {
  return false;  // assume no failed operations
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
