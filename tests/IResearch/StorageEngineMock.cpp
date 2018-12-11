//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "StorageEngineMock.h"

#include "Basics/LocalTaskQueue.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/IndexIterator.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchMMFilesLink.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "IResearch/VelocyPackHelper.h"
#include "StorageEngine/TransactionManager.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/OperationOptions.h"
#include "velocypack/Iterator.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/ticks.h"
#include "Transaction/Helpers.h"
#include "Aql/AstNode.h"

#include <asio/io_context.hpp>

namespace {

/// @brief hard-coded vector of the index attributes
/// note that the attribute names must be hard-coded here to avoid an init-order
/// fiasco with StaticStrings::FromString etc.
std::vector<std::vector<arangodb::basics::AttributeName>> const IndexAttributes{
  {arangodb::basics::AttributeName("_from", false)},
  {arangodb::basics::AttributeName("_to", false)}
};

/// @brief add a single value node to the iterator's keys
void handleValNode(
    VPackBuilder* keys,
    arangodb::aql::AstNode const* valNode
) {
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

  EdgeIndexIteratorMock(
      arangodb::LogicalCollection* collection,
      arangodb::transaction::Methods* trx,
      arangodb::Index const* index,
      Map const& map,
      std::unique_ptr<VPackBuilder>&& keys
  ) : IndexIterator(collection, trx),
      _map(map),
      _begin(_map.begin()),
      _end(_map.end()),
      _keys(std::move(keys)),
      _keysIt(_keys->slice()) {
  }

  char const* typeName() const override {
    return "edge-index-iterator-mock";
  }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override {
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

  void reset() override {
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
}; // EdgeIndexIteratorMock

class EdgeIndexMock final : public arangodb::Index {
 public:
  static std::shared_ptr<arangodb::Index> make(
      TRI_idx_iid_t iid,
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice const& definition
  ) {
    auto const typeSlice = definition.get("type");

    if (typeSlice.isNone()) {
      return nullptr;
    }

    auto const type = arangodb::basics::VelocyPackHelper::getStringRef(
      typeSlice,
      arangodb::velocypack::StringRef()
    );

    if (type.compare("edge") != 0) {
      return nullptr;
    }

    return std::make_shared<EdgeIndexMock>(iid, collection);
  }

  IndexType type() const override { return Index::TRI_IDX_TYPE_EDGE_INDEX; }

  char const* typeName() const override { return "edge"; }

  bool canBeDropped() const override { return false; }

  bool isSorted() const override { return false; }

  bool hasSelectivityEstimate() const override { return false; }

  size_t memory() const override { return sizeof(EdgeIndexMock); }

  bool hasBatchInsert() const override { return false; }

  void load() override {}
  void unload() override {}
  void afterTruncate(TRI_voc_tick_t) override {
    _edgesFrom.clear();
    _edgesTo.clear();
  }

  void toVelocyPack(
      VPackBuilder& builder,
      std::underlying_type<arangodb::Index::Serialize>::type flags
  ) const override {
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

  arangodb::Result insert(
      arangodb::transaction::Methods& trx,
      arangodb::LocalDocumentId const& documentId,
      arangodb::velocypack::Slice const& doc,
      OperationMode
  ) override {
    if (!doc.isObject()) {
      return { TRI_ERROR_INTERNAL };
    }

    VPackSlice const fromValue(arangodb::transaction::helpers::extractFromFromDocument(doc));

    if (!fromValue.isString()) {
      return { TRI_ERROR_INTERNAL };
    }

    VPackSlice const toValue(arangodb::transaction::helpers::extractToFromDocument(doc));

    if (!toValue.isString()) {
      return { TRI_ERROR_INTERNAL };
    }

    _edgesFrom.emplace(fromValue.toString(), documentId);
    _edgesTo.emplace(toValue.toString(), documentId);

    return {}; // ok
  }

  arangodb::Result remove(
      arangodb::transaction::Methods& trx,
      arangodb::LocalDocumentId const&,
      arangodb::velocypack::Slice const& doc,
      OperationMode
  ) override {
    if (!doc.isObject()) {
      return { TRI_ERROR_INTERNAL };
    }

    VPackSlice const fromValue(arangodb::transaction::helpers::extractFromFromDocument(doc));

    if (!fromValue.isString()) {
      return { TRI_ERROR_INTERNAL };
    }

    VPackSlice const toValue(arangodb::transaction::helpers::extractToFromDocument(doc));

    if (!toValue.isString()) {
      return { TRI_ERROR_INTERNAL };
    }

    _edgesFrom.erase(fromValue.toString());
    _edgesTo.erase(toValue.toString());

    return {}; // ok
  }

  bool supportsFilterCondition(
      std::vector<std::shared_ptr<arangodb::Index>> const& /*allIndexes*/,
      arangodb::aql::AstNode const* node,
      arangodb::aql::Variable const* reference,
      size_t itemsInIndex,
      size_t& estimatedItems,
      double& estimatedCost
  ) const override {
    arangodb::SimpleAttributeEqualityMatcher matcher(IndexAttributes);

    return matcher.matchOne(
      this, node, reference, itemsInIndex, estimatedItems, estimatedCost
    );
  }

  arangodb::IndexIterator* iteratorForCondition(
      arangodb::transaction::Methods* trx,
      arangodb::ManagedDocumentResult* mmdr,
      arangodb::aql::AstNode const* node,
      arangodb::aql::Variable const*,
      arangodb::IndexIteratorOptions const&
  ) override {
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
      return createEqIterator(trx, mmdr, attrNode, valNode);
    }

    if (comp->type == arangodb::aql::NODE_TYPE_OPERATOR_BINARY_IN) {
      // a.b IN values
      if (!valNode->isArray()) {
        // a.b IN non-array
        return new arangodb::EmptyIndexIterator(&_collection, trx);
      }

      return createInIterator(trx, mmdr, attrNode, valNode);
    }

    // operator type unsupported
    return new arangodb::EmptyIndexIterator(&_collection, trx);
  }

  arangodb::aql::AstNode* specializeCondition(
      arangodb::aql::AstNode* node,
      arangodb::aql::Variable const* reference
  ) const override {
    arangodb::SimpleAttributeEqualityMatcher matcher(IndexAttributes);

    return matcher.specializeOne(this, node, reference);
  }

  EdgeIndexMock(
      TRI_idx_iid_t iid,
      arangodb::LogicalCollection& collection
  ): arangodb::Index(
      iid,
      collection,
      {
        {arangodb::basics::AttributeName(arangodb::StaticStrings::FromString, false)},
        {arangodb::basics::AttributeName(arangodb::StaticStrings::ToString, false)}
      }, true, false) {
  }

  arangodb::IndexIterator* createEqIterator(
      arangodb::transaction::Methods* trx,
      arangodb::ManagedDocumentResult* mmdr,
      arangodb::aql::AstNode const* attrNode,
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

    return new EdgeIndexIteratorMock(
      &_collection,
      trx,
      this,
      isFrom ? _edgesFrom : _edgesTo,
      std::move(keys)
    );
  }

  /// @brief create the iterator
  arangodb::IndexIterator* createInIterator(
      arangodb::transaction::Methods* trx,
      arangodb::ManagedDocumentResult* mmdr,
      arangodb::aql::AstNode const* attrNode,
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

    return new EdgeIndexIteratorMock(
      &_collection,
      trx,
      this,
      isFrom ? _edgesFrom : _edgesTo,
      std::move(keys)
    );
  }

  /// @brief the hash table for _from
  EdgeIndexIteratorMock::Map _edgesFrom;
  /// @brief the hash table for _to
  EdgeIndexIteratorMock::Map _edgesTo;
}; // EdgeIndexMock

class ReverseAllIteratorMock final : public arangodb::IndexIterator {
 public:
  ReverseAllIteratorMock(
      uint64_t size,
      arangodb::LogicalCollection* coll,
      arangodb::transaction::Methods* trx)
    : arangodb::IndexIterator(coll, trx),
      _end(size), _size(size) {
  }

  virtual char const* typeName() const override {
    return "ReverseAllIteratorMock";
  }

  virtual void reset() override {
    _end = _size;
  }

  virtual bool next(LocalDocumentIdCallback const& callback, size_t limit) override {
    while (_end && limit) { // `_end` always > 0
      callback(arangodb::LocalDocumentId(_end--));
      --limit;
    }

    return 0 == limit;
  }

 private:
  uint64_t _end;
  uint64_t _size; // the original size
}; // ReverseAllIteratorMock

class AllIteratorMock final : public arangodb::IndexIterator {
 public:
  AllIteratorMock(
      uint64_t size,
      arangodb::LogicalCollection& coll,
      arangodb::transaction::Methods* trx)
    : arangodb::IndexIterator(&coll, trx),
      _end(size) {
  }

  virtual char const* typeName() const override {
    return "AllIteratorMock";
  }

  virtual void reset() override {
    _begin = 0;
  }

  virtual bool next(LocalDocumentIdCallback const& callback, size_t limit) override {
    while (_begin < _end && limit) {
      callback(arangodb::LocalDocumentId(++_begin)); // always > 0
      --limit;
    }

    return 0 == limit;
  }

 private:
  uint64_t _begin{};
  uint64_t _end;
}; // AllIteratorMock

struct IndexFactoryMock : arangodb::IndexFactory {
  virtual void fillSystemIndexes(
    arangodb::LogicalCollection& col,
    std::vector<std::shared_ptr<arangodb::Index>>& systemIndexes
  ) const override {
    // NOOP
  }

  /// @brief create indexes from a list of index definitions
  virtual void prepareIndexes(
      arangodb::LogicalCollection& col,
      arangodb::velocypack::Slice const& indexesSlice,
      std::vector<std::shared_ptr<arangodb::Index>>& indexes
  ) const override {
    // NOOP
  }
};

}

void ContextDataMock::pinData(arangodb::LogicalCollection* collection) {
  if (collection) {
    pinned.emplace(collection->id());
  }
}

bool ContextDataMock::isPinned(TRI_voc_cid_t cid) const {
  return pinned.find(cid) != pinned.end();
}

std::function<void()> PhysicalCollectionMock::before = []()->void {};

PhysicalCollectionMock::PhysicalCollectionMock(
    arangodb::LogicalCollection& collection,
    arangodb::velocypack::Slice const& info
): PhysicalCollection(collection, info) {
}

arangodb::PhysicalCollection* PhysicalCollectionMock::clone(
    arangodb::LogicalCollection& collection
) const {
  before();
  TRI_ASSERT(false);
  return nullptr;
}

int PhysicalCollectionMock::close() {
  for (auto& index: _indexes) {
    index->unload();
  }

  return TRI_ERROR_NO_ERROR; // assume close successful
}

std::shared_ptr<arangodb::Index> PhysicalCollectionMock::createIndex(arangodb::velocypack::Slice const& info, bool restore, bool& created) {
  before();

  std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> docs;

  for (size_t i = 0, count = documents.size(); i < count; ++i) {
    auto& entry = documents[i];

    if (entry.second) {
      auto revId = arangodb::LocalDocumentId::create(i + 1); // always > 0

      docs.emplace_back(revId, entry.first.slice());
    }
  }

  struct IndexFactory: public arangodb::IndexFactory {
    using arangodb::IndexFactory::validateSlice;
  };
  auto id = IndexFactory::validateSlice(info, true, false); // trie+false to ensure id generation if missing

  auto const type = arangodb::basics::VelocyPackHelper::getStringRef(
    info.get("type"),
    arangodb::velocypack::StringRef()
  );

  std::shared_ptr<arangodb::Index> index;

  if (0 == type.compare("edge")) {
    index = EdgeIndexMock::make(id, _logicalCollection, info);
#ifdef USE_IRESEARCH
  } else if (0 == type.compare(arangodb::iresearch::DATA_SOURCE_TYPE.name())) {

    if (arangodb::ServerState::instance()->isCoordinator()) {
      arangodb::iresearch::IResearchLinkCoordinator::factory().instantiate(
        index, _logicalCollection, info, id, false
      );
    } else {
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(
        index, _logicalCollection, info, id, false
      );
    }
#endif
  }

  if (!index) {
    return nullptr;
  }


  asio::io_context ioContext;
  auto poster = [&ioContext](std::function<void()> fn) -> void {
    ioContext.post(fn);
  };
  arangodb::basics::LocalTaskQueue taskQueue(poster);
  std::shared_ptr<arangodb::basics::LocalTaskQueue> taskQueuePtr(&taskQueue, [](arangodb::basics::LocalTaskQueue*)->void{});
  
  TRI_vocbase_t& vocbase = _logicalCollection.vocbase();
  arangodb::SingleCollectionTransaction trx(
    arangodb::transaction::StandaloneContext::Create(vocbase),
    _logicalCollection,
    arangodb::AccessMode::Type::WRITE
  );
  auto res = trx.begin();
  TRI_ASSERT(res.ok());

  index->batchInsert(trx, docs, taskQueuePtr);

  if (TRI_ERROR_NO_ERROR != taskQueue.status()) {
    return nullptr;
  }

  _indexes.emplace_back(std::move(index));
  created = true;

  res = trx.commit();
  TRI_ASSERT(res.ok());

  return _indexes.back();
}

void PhysicalCollectionMock::deferDropCollection(
    std::function<bool(arangodb::LogicalCollection&)> const& callback
) {
  before();

  callback(_logicalCollection); // assume noone is using this collection (drop immediately)
}

bool PhysicalCollectionMock::dropIndex(TRI_idx_iid_t iid) {
  before();

  for (auto itr = _indexes.begin(), end = _indexes.end(); itr != end; ++itr) {
    if ((*itr)->id() == iid) {
      if ((*itr)->drop().ok()) {
        _indexes.erase(itr); return true;
      }
    }
  }

  return false;
}

void PhysicalCollectionMock::figuresSpecific(std::shared_ptr<arangodb::velocypack::Builder>&) {
  before();
  TRI_ASSERT(false);
}

std::unique_ptr<arangodb::IndexIterator> PhysicalCollectionMock::getAllIterator(arangodb::transaction::Methods* trx) const {
  before();

  return std::make_unique<AllIteratorMock>(documents.size(), this->_logicalCollection, trx);
}

std::unique_ptr<arangodb::IndexIterator> PhysicalCollectionMock::getAnyIterator(arangodb::transaction::Methods* trx) const {
  before();
  return std::make_unique<AllIteratorMock>(documents.size(), this->_logicalCollection, trx);
}

void PhysicalCollectionMock::getPropertiesVPack(arangodb::velocypack::Builder&) const {
  before();
}

arangodb::Result PhysicalCollectionMock::insert(
    arangodb::transaction::Methods* trx,
    arangodb::velocypack::Slice const newSlice,
    arangodb::ManagedDocumentResult& result,
    arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick,
    bool lock, TRI_voc_tick_t& revisionId,
    arangodb::KeyLockInfo* /*keyLockInfo*/,
    std::function<arangodb::Result(void)> callbackDuringLock) {
  TRI_ASSERT(callbackDuringLock == nullptr); // not implemented
  before();

  arangodb::velocypack::Builder builder;
  auto isEdgeCollection = (TRI_COL_TYPE_EDGE == _logicalCollection.type());

  TRI_voc_rid_t unused;
  auto res = newObjectForInsert(
    trx,
    newSlice,
    isEdgeCollection,
    builder,
    options.isRestore,
    unused
  );

  if (res.fail()) {
    return res;
  }

  documents.emplace_back(std::move(builder), true);
  arangodb::LocalDocumentId docId(documents.size()); // always > 0

  result.setUnmanaged(documents.back().first.data(), docId);

  for (auto& index : _indexes) {
    if (!index->insert(*trx, docId, newSlice, arangodb::Index::OperationMode::normal).ok()) {
      return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
    }
  }

  return arangodb::Result();
}

void PhysicalCollectionMock::invokeOnAllElements(arangodb::transaction::Methods*, std::function<bool(arangodb::LocalDocumentId const&)> callback) {
  before();


  for (size_t i = 0, count = documents.size(); i < count; ++i) {
    arangodb::LocalDocumentId token(i + 1); // '_data' always > 0

    if (documents[i].second && !callback(token)) {
      return;
    }
  }
}

std::shared_ptr<arangodb::Index> PhysicalCollectionMock::lookupIndex(arangodb::velocypack::Slice const&) const {
  before();
  TRI_ASSERT(false);
  return nullptr;
}

arangodb::LocalDocumentId PhysicalCollectionMock::lookupKey(arangodb::transaction::Methods*, arangodb::velocypack::Slice const&) const {
  before();
  TRI_ASSERT(false);
  return arangodb::LocalDocumentId();
}

size_t PhysicalCollectionMock::memory() const {
  before();
  TRI_ASSERT(false);
  return 0;
}

uint64_t PhysicalCollectionMock::numberDocuments(arangodb::transaction::Methods*) const {
  before();
  return documents.size();
}

void PhysicalCollectionMock::open(bool ignoreErrors) {
  before();
  TRI_ASSERT(false);
}

std::string const& PhysicalCollectionMock::path() const {
  before();

  return physicalPath;
}

arangodb::Result PhysicalCollectionMock::persistProperties() {
  before();
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_INTERNAL);
}

bool PhysicalCollectionMock::addIndex(std::shared_ptr<arangodb::Index> idx) {
  auto const id = idx->id();
  for (auto const& it : _indexes) {
    if (it->id() == id) {
      // already have this particular index. do not add it again
      return false;
    }
  }

  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id));

  _indexes.emplace_back(idx);
  return true;
}

void PhysicalCollectionMock::prepareIndexes(arangodb::velocypack::Slice indexesSlice) {
  before();

  auto* engine = arangodb::EngineSelectorFeature::ENGINE;
  auto& idxFactory = engine->indexFactory();

  for (auto const& v : VPackArrayIterator(indexesSlice)) {
    if (arangodb::basics::VelocyPackHelper::getBooleanValue(v, "error",
                                                            false)) {
      // We have an error here.
      // Do not add index.
      continue;
    }

    auto idx =
      idxFactory.prepareIndexFromSlice(v, false, _logicalCollection, true);

    if (!idx) {
      continue;
    }

    if (!addIndex(idx)) {
      return;
    }
  }
}

arangodb::Result PhysicalCollectionMock::read(arangodb::transaction::Methods*, arangodb::StringRef const& key, arangodb::ManagedDocumentResult& result, bool) {
  before();

  for (size_t i = documents.size(); i; --i) {
    auto& entry = documents[i - 1];

    if (!entry.second) {
      continue; // removed document
    }

    auto& doc = entry.first;
    auto const keySlice = doc.slice().get(arangodb::StaticStrings::KeyString);

    if (!keySlice.isString()) {
      continue;
    }

    arangodb::StringRef const docKey(keySlice);

    if (key == docKey) {
      result.setUnmanaged(doc.data(), arangodb::LocalDocumentId(i));
      return arangodb::Result(TRI_ERROR_NO_ERROR);
    }
  }

  return arangodb::Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

arangodb::Result PhysicalCollectionMock::read(arangodb::transaction::Methods*, arangodb::velocypack::Slice const& key, arangodb::ManagedDocumentResult& result, bool) {
  before();
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

bool PhysicalCollectionMock::readDocument(arangodb::transaction::Methods* trx, arangodb::LocalDocumentId const& token, arangodb::ManagedDocumentResult& result) const {
  before();

  if (token.id() > documents.size()) {
    return false;
  }

  auto& entry = documents[token.id() - 1]; // '_data' always > 0

  if (!entry.second) {
    return false; // removed document
  }

  result.setUnmanaged(entry.first.data(), token);

  return true;
}

bool PhysicalCollectionMock::readDocumentWithCallback(arangodb::transaction::Methods* trx, arangodb::LocalDocumentId const& token, arangodb::IndexIterator::DocumentCallback const& cb) const {
  before();

  if (token.id() > documents.size()) {
    return false;
  }

  auto& entry = documents[token.id() - 1]; // '_data' always > 0

  if (!entry.second) {
    return false; // removed document
  }

  cb(token, VPackSlice(entry.first.data()));

  return true;
}

arangodb::Result PhysicalCollectionMock::remove(
    arangodb::transaction::Methods& trx,
    arangodb::velocypack::Slice slice,
    arangodb::ManagedDocumentResult& previous,
    arangodb::OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick,
    bool lock,
    TRI_voc_rid_t& prevRev,
    TRI_voc_rid_t& revisionId,
    arangodb::KeyLockInfo* /*keyLockInfo*/,
    std::function<arangodb::Result(void)> callbackDuringLock
) {
  TRI_ASSERT(callbackDuringLock == nullptr); // not implemented
  before();

  auto key = slice.get(arangodb::StaticStrings::KeyString);

  for (size_t i = documents.size(); i; --i) {
    auto& entry = documents[i - 1];

    if (!entry.second) {
      continue; // removed document
    }

    auto& doc = entry.first;

    if (key == doc.slice().get(arangodb::StaticStrings::KeyString)) {
      TRI_voc_rid_t revId = i; // always > 0

      entry.second = false;
      previous.setUnmanaged(doc.data(), arangodb::LocalDocumentId(revId));
      prevRev = revId;

      return arangodb::Result(TRI_ERROR_NO_ERROR); // assume document was removed
    }
  }

  return arangodb::Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

arangodb::Result PhysicalCollectionMock::replace(
    arangodb::transaction::Methods* trx,
    arangodb::velocypack::Slice const newSlice,
    arangodb::ManagedDocumentResult& result,
    arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick,
    bool lock, TRI_voc_rid_t& prevRev,
    arangodb::ManagedDocumentResult& previous,
    std::function<arangodb::Result(void)> callbackDuringLock) {
  before();

  auto key = newSlice.get(arangodb::StaticStrings::KeyString);

  return update(trx, newSlice, result, options, resultMarkerTick, lock, prevRev,
                previous, key, callbackDuringLock);
}

TRI_voc_rid_t PhysicalCollectionMock::revision(arangodb::transaction::Methods*) const {
  before();
  TRI_ASSERT(false);
  return 0;
}

void PhysicalCollectionMock::setPath(std::string const& value) {
  before();
  physicalPath = value;
}

arangodb::Result PhysicalCollectionMock::truncate(
    arangodb::transaction::Methods& trx,
    arangodb::OperationOptions& options
) {
  before();
  documents.clear();
  return arangodb::Result();
}

arangodb::Result PhysicalCollectionMock::update(
    arangodb::transaction::Methods* trx,
    arangodb::velocypack::Slice const newSlice,
    arangodb::ManagedDocumentResult& result,
    arangodb::OperationOptions& options, TRI_voc_tick_t& resultMarkerTick,
    bool lock, TRI_voc_rid_t& prevRev,
    arangodb::ManagedDocumentResult& previous,
    arangodb::velocypack::Slice const key,
    std::function<arangodb::Result(void)> callbackDuringLock) {
  TRI_ASSERT(callbackDuringLock == nullptr); // not implemented
  before();

  for (size_t i = documents.size(); i; --i) {
    auto& entry = documents[i - 1];

    if (!entry.second) {
      continue; // removed document
    }

    auto& doc = entry.first;

    if (key == doc.slice().get(arangodb::StaticStrings::KeyString)) {
      TRI_voc_rid_t revId = i; // always > 0

      if (!options.mergeObjects) {
        entry.second = false;
        previous.setUnmanaged(doc.data(), arangodb::LocalDocumentId(revId));
        prevRev = revId;

        TRI_voc_rid_t unused;
        return insert(trx, newSlice, result, options, resultMarkerTick, lock,
                      unused, nullptr, nullptr);
      }

      arangodb::velocypack::Builder builder;

      builder.openObject();

      if (!arangodb::iresearch::mergeSlice(builder, newSlice)) {
        return arangodb::Result(TRI_ERROR_BAD_PARAMETER);
      }

      for (arangodb::velocypack::ObjectIterator itr(doc.slice()); itr.valid(); ++itr) {
        auto key = itr.key().copyString();

        if (!newSlice.hasKey(key)) {
          builder.add(key, itr.value());
        }
      }

      builder.close();
      entry.second = false;
      previous.setUnmanaged(doc.data(), arangodb::LocalDocumentId(revId));
      prevRev = revId;

      TRI_voc_rid_t unused;
      return insert(trx, builder.slice(), result, options, resultMarkerTick,
                    lock, unused, nullptr, nullptr);
    }
  }

  return arangodb::Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
}

arangodb::Result PhysicalCollectionMock::updateProperties(arangodb::velocypack::Slice const& slice, bool doSync) {
  before();

  return arangodb::Result(TRI_ERROR_NO_ERROR); // assume mock collection updated OK
}

std::function<void()> StorageEngineMock::before = []()->void {};
bool StorageEngineMock::inRecoveryResult = false;

StorageEngineMock::StorageEngineMock(
    arangodb::application_features::ApplicationServer& server
)
  : StorageEngine(
      server,
      "Mock",
      "",
      std::unique_ptr<arangodb::IndexFactory>(new IndexFactoryMock())
    ),
    _releasedTick(0) {
}

arangodb::WalAccess const* StorageEngineMock::walAccess() const {
  TRI_ASSERT(false);
  return nullptr;
}

void StorageEngineMock::addOptimizerRules() {
  before();
  // NOOP
}

void StorageEngineMock::addRestHandlers(
    arangodb::rest::RestHandlerFactory& handlerFactory
) {
  TRI_ASSERT(false);
}

void StorageEngineMock::addV8Functions() {
  TRI_ASSERT(false);
}

void StorageEngineMock::changeCollection(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t id,
    arangodb::LogicalCollection const& collection,
    bool doSync
) {
  // NOOP, assume physical collection changed OK
}

arangodb::Result StorageEngineMock::changeView(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalView const& view,
    bool doSync
) {
  before();
  TRI_ASSERT(views.find(std::make_pair(vocbase.id(), view.id())) != views.end());
  arangodb::velocypack::Builder builder;

  builder.openObject();
  view.properties(builder, true, true);
  builder.close();
  views[std::make_pair(vocbase.id(), view.id())] = std::move(builder);
  return {};
}

std::string StorageEngineMock::collectionPath(
    TRI_vocbase_t const& vocbase,
    TRI_voc_cid_t id
) const {
  TRI_ASSERT(false);
  return "<invalid>";
}

std::string StorageEngineMock::createCollection(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t id,
    arangodb::LogicalCollection const& collection
) {
  return "<invalid>"; // physical path of the new collection
}

std::unique_ptr<TRI_vocbase_t> StorageEngineMock::createDatabase(
    TRI_voc_tick_t id, 
    arangodb::velocypack::Slice const& args, 
    int& status
) {
  if (!args.get("name").isString()) {
    status = TRI_ERROR_BAD_PARAMETER;
  }

  status = TRI_ERROR_NO_ERROR;
  
  std::string cname = args.get("name").copyString();
  if (arangodb::ServerState::instance()->isCoordinator()) {
    return std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR, id, cname);
  }
  return std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, id, cname);
}

arangodb::Result StorageEngineMock::createLoggerState(TRI_vocbase_t*, VPackBuilder&) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<arangodb::PhysicalCollection> StorageEngineMock::createPhysicalCollection(
    arangodb::LogicalCollection& collection,
    arangodb::velocypack::Slice const& info
) {
  before();
  return std::unique_ptr<arangodb::PhysicalCollection>(
    new PhysicalCollectionMock(collection, info)
  );
}

arangodb::Result StorageEngineMock::createTickRanges(VPackBuilder&) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<arangodb::TransactionCollection> StorageEngineMock::createTransactionCollection(
    arangodb::TransactionState& state,
    TRI_voc_cid_t cid,
    arangodb::AccessMode::Type accessType,
    int nestingLevel
) {
  return std::unique_ptr<arangodb::TransactionCollection>(
      new TransactionCollectionMock(&state, cid, accessType)
  );
}

std::unique_ptr<arangodb::transaction::ContextData> StorageEngineMock::createTransactionContextData() {
  before();
  return std::unique_ptr<arangodb::transaction::ContextData>(
    new ContextDataMock()
  );
}

std::unique_ptr<arangodb::TransactionManager> StorageEngineMock::createTransactionManager() {
  TRI_ASSERT(false);
  return nullptr;
}

std::unique_ptr<arangodb::TransactionState> StorageEngineMock::createTransactionState(
    TRI_vocbase_t& vocbase,
    arangodb::transaction::Options const& options
) {
  return std::unique_ptr<arangodb::TransactionState>(
    new TransactionStateMock(vocbase, options)
  );
}

arangodb::Result StorageEngineMock::createView(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t id,
    arangodb::LogicalView const& view
) {
  before();
  TRI_ASSERT(views.find(std::make_pair(vocbase.id(), view.id())) == views.end()); // called after createView()
  arangodb::velocypack::Builder builder;
  
  builder.openObject();
  view.properties(builder, true, true);
  builder.close();
  views[std::make_pair(vocbase.id(), view.id())] = std::move(builder);
  
  return arangodb::Result(TRI_ERROR_NO_ERROR); // assume mock view persisted OK
}

void StorageEngineMock::getViewProperties(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalView const& view,
    VPackBuilder& builder
) {
  before();
 // NOOP
}

TRI_voc_tick_t StorageEngineMock::currentTick() const {
  before();
  return TRI_CurrentTickServer();
}

std::string StorageEngineMock::databasePath(TRI_vocbase_t const* vocbase) const {
  before();
  return ""; // no valid path filesystem persisted, return empty string
}

void StorageEngineMock::destroyCollection(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalCollection& collection
) {
  // NOOP, assume physical collection destroyed OK
}

void StorageEngineMock::destroyView(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalView& view
) noexcept {
  before();
  // NOOP, assume physical view destroyed OK
}

arangodb::Result StorageEngineMock::dropCollection(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalCollection& collection
) {
  return arangodb::Result(TRI_ERROR_NO_ERROR); // assume physical collection dropped OK
}

arangodb::Result StorageEngineMock::dropDatabase(TRI_vocbase_t& vocbase) {
  TRI_ASSERT(false);
  return arangodb::Result();
}

arangodb::Result StorageEngineMock::dropView(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalView& view
) {
  before();
  TRI_ASSERT(views.find(std::make_pair(vocbase.id(), view.id())) != views.end());
  views.erase(std::make_pair(vocbase.id(), view.id()));

  return arangodb::Result(TRI_ERROR_NO_ERROR); // assume mock view dropped OK
}

arangodb::Result StorageEngineMock::firstTick(uint64_t&) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
}

void StorageEngineMock::getCollectionInfo(
    TRI_vocbase_t& vocbase,
    TRI_voc_cid_t cid,
    arangodb::velocypack::Builder& result,
    bool includeIndexes,
    TRI_voc_tick_t maxTick
) {
  arangodb::velocypack::Builder parameters;

  parameters.openObject();
  parameters.close();

  result.openObject();
  result.add("parameters", parameters.slice()); // required entry of type object
  result.close();

  // nothing more required, assume info used for PhysicalCollectionMock
}

int StorageEngineMock::getCollectionsAndIndexes(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Builder& result,
    bool wasCleanShutdown,
    bool isUpgrade
) {
  TRI_ASSERT(false);
  return TRI_ERROR_INTERNAL;
}

void StorageEngineMock::getDatabases(arangodb::velocypack::Builder& result) {
  before();
  arangodb::velocypack::Builder system;

  system.openObject();
  system.add("name", arangodb::velocypack::Value(TRI_VOC_SYSTEM_DATABASE));
  system.close();

  // array expected
  result.openArray();
  result.add(system.slice());
  result.close();
}

arangodb::velocypack::Builder StorageEngineMock::getReplicationApplierConfiguration(
    TRI_vocbase_t& vocbase,
    int& result
) {
  before();
  result = TRI_ERROR_FILE_NOT_FOUND; // assume no ReplicationApplierConfiguration for vocbase

  return arangodb::velocypack::Builder();
}

arangodb::velocypack::Builder StorageEngineMock::getReplicationApplierConfiguration(int& result) {
  before();
  result = TRI_ERROR_FILE_NOT_FOUND;

  return arangodb::velocypack::Builder();
}

int StorageEngineMock::getViews(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Builder& result
) {
  result.openArray();

  for (auto& entry: views) {
    result.add(entry.second.slice());
  }

  result.close();

  return TRI_ERROR_NO_ERROR;
}

arangodb::Result StorageEngineMock::handleSyncKeys(
    arangodb::DatabaseInitialSyncer& syncer,
    arangodb::LogicalCollection& col,
    std::string const& keysId
) {
  TRI_ASSERT(false);
  return arangodb::Result();
}

bool StorageEngineMock::inRecovery() {
  return inRecoveryResult;
}

arangodb::Result StorageEngineMock::lastLogger(
    TRI_vocbase_t& vocbase,
    std::shared_ptr<arangodb::transaction::Context> transactionContext,
    uint64_t tickStart,
    uint64_t tickEnd,
    std::shared_ptr<VPackBuilder>& builderSPtr
) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_NOT_IMPLEMENTED);
}

std::unique_ptr<TRI_vocbase_t> StorageEngineMock::openDatabase(
    arangodb::velocypack::Slice const& args,
    bool isUpgrade,
    int& status
) {
  before();

  if (!args.isObject() || !args.hasKey("name") || !args.get("name").isString()) {
    status = TRI_ERROR_ARANGO_DATABASE_NAME_INVALID;

    return nullptr;
  }

  return std::make_unique<TRI_vocbase_t>(
    TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
    vocbaseCount++,
    args.get("name").copyString()
  );
}

arangodb::Result StorageEngineMock::persistCollection(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalCollection const& collection
) {
  before();
  return arangodb::Result(TRI_ERROR_NO_ERROR); // assume mock collection persisted OK
}

void StorageEngineMock::prepareDropDatabase(
    TRI_vocbase_t& vocbase,
    bool useWriteMarker,
    int& status
) {
  // NOOP
}

TRI_voc_tick_t StorageEngineMock::releasedTick() const {
  before();
  return _releasedTick;
}

void StorageEngineMock::releaseTick(TRI_voc_tick_t tick) {
  before();
  _releasedTick = tick;
}

int StorageEngineMock::removeReplicationApplierConfiguration(
    TRI_vocbase_t& vocbase
) {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

int StorageEngineMock::removeReplicationApplierConfiguration() {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

arangodb::Result StorageEngineMock::renameCollection(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalCollection const& collection,
    std::string const& oldName
) {
  TRI_ASSERT(false);
  return arangodb::Result(TRI_ERROR_INTERNAL);
}

int StorageEngineMock::saveReplicationApplierConfiguration(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice slice,
    bool doSync
) {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

int StorageEngineMock::saveReplicationApplierConfiguration(arangodb::velocypack::Slice, bool) {
  TRI_ASSERT(false);
  return TRI_ERROR_NO_ERROR;
}

int StorageEngineMock::shutdownDatabase(TRI_vocbase_t& vocbase) {
  before();
  return TRI_ERROR_NO_ERROR; // assume shutdown successful
}

void StorageEngineMock::signalCleanup(TRI_vocbase_t& vocbase) {
  before();
  // NOOP, assume cleanup thread signaled OK
}

bool StorageEngineMock::supportsDfdb() const {
  TRI_ASSERT(false);
  return false;
}

void StorageEngineMock::unloadCollection(
    TRI_vocbase_t& vocbase,
    arangodb::LogicalCollection& collection
) {
  before();
  // NOOP assume collection unloaded OK
}

std::string StorageEngineMock::versionFilename(TRI_voc_tick_t) const {
  TRI_ASSERT(false);
  return std::string();
}

void StorageEngineMock::waitForEstimatorSync(std::chrono::milliseconds) {
  TRI_ASSERT(false);
}

void StorageEngineMock::waitForSyncTick(TRI_voc_tick_t tick) {
  TRI_ASSERT(false);
}
  
std::vector<std::string> StorageEngineMock::currentWalFiles() const {
  return std::vector<std::string>(); 
}

arangodb::Result StorageEngineMock::flushWal(bool waitForSync, bool waitForCollector, bool writeShutdownFile) {
  TRI_ASSERT(false);
  return arangodb::Result();
}

void StorageEngineMock::waitUntilDeletion(TRI_voc_tick_t id, bool force, int& status) {
  // NOOP
}

int StorageEngineMock::writeCreateDatabaseMarker(TRI_voc_tick_t id, VPackSlice const& slice) {
  return TRI_ERROR_NO_ERROR;
}

TransactionCollectionMock::TransactionCollectionMock(
    arangodb::TransactionState* state, TRI_voc_cid_t cid,
    arangodb::AccessMode::Type accessType)
    : TransactionCollection(state, cid, accessType) {}

bool TransactionCollectionMock::canAccess(arangodb::AccessMode::Type accessType) const {
  return nullptr != _collection; // collection must have be opened previously
}

void TransactionCollectionMock::freeOperations(arangodb::transaction::Methods* activeTrx, bool mustRollback) {
  TRI_ASSERT(false);
}

bool TransactionCollectionMock::hasOperations() const {
  TRI_ASSERT(false);
  return false;
}

void TransactionCollectionMock::release() {
  if (_collection) {
    _transaction->vocbase().releaseCollection(_collection.get());
    _collection = nullptr;
  }
}

int TransactionCollectionMock::updateUsage(arangodb::AccessMode::Type accessType, int nestingLevel) {
  if (arangodb::AccessMode::isWriteOrExclusive(accessType) &&
      !arangodb::AccessMode::isWriteOrExclusive(_accessType)) {
    if (nestingLevel > 0) {
      // trying to write access a collection that is only marked with
      // read-access
      return TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION;
    }

    TRI_ASSERT(nestingLevel == 0);

    // upgrade collection type to write-access
    _accessType = accessType;
  }

  // if (nestingLevel < _nestingLevel) {
  //   _nestingLevel = nestingLevel;
  // }

  return TRI_ERROR_NO_ERROR;
}

void TransactionCollectionMock::unuse(int nestingLevel) {
  // NOOP, assume success
}

int TransactionCollectionMock::use(int nestingLevel) {
  TRI_vocbase_col_status_e status;

  bool shouldLock = !arangodb::AccessMode::isNone(_accessType);

  if (shouldLock && !isLocked()) {
    // r/w lock the collection
    int res = doLock(_accessType, nestingLevel);

    if (res == TRI_ERROR_LOCKED) {
      // TRI_ERROR_LOCKED is not an error, but it indicates that the lock
      // operation has actually acquired the lock (and that the lock has not
      // been held before)
      res = TRI_ERROR_NO_ERROR;
    } else if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  _collection = _transaction->vocbase().useCollection(_cid, status);

  return _collection ? TRI_ERROR_NO_ERROR : TRI_ERROR_INTERNAL;
}

int TransactionCollectionMock::doLock(arangodb::AccessMode::Type type, int nestingLevel) {
  if (_lockType > _accessType) {
    return TRI_ERROR_INTERNAL;
  }

  _lockType = type;

  return TRI_ERROR_NO_ERROR;
}

int TransactionCollectionMock::doUnlock(arangodb::AccessMode::Type type, int nestingLevel) {
  if (_lockType != type) {
    return TRI_ERROR_INTERNAL;
  }

  _lockType = arangodb::AccessMode::Type::NONE;

  return TRI_ERROR_NO_ERROR;
}

size_t TransactionStateMock::abortTransactionCount;
size_t TransactionStateMock::beginTransactionCount;
size_t TransactionStateMock::commitTransactionCount;

// ensure each transaction state has a unique ID
TransactionStateMock::TransactionStateMock(
    TRI_vocbase_t& vocbase,
    arangodb::transaction::Options const& options
): TransactionState(vocbase, 0, options) {
}

arangodb::Result TransactionStateMock::abortTransaction(arangodb::transaction::Methods* trx) {
  ++abortTransactionCount;
  updateStatus(arangodb::transaction::Status::ABORTED);
  unuseCollections(_nestingLevel);
  const_cast<TRI_voc_tid_t&>(_id) = 0; // avoid use of TransactionManagerFeature::manager()->unregisterTransaction(...)

  return arangodb::Result();
}

arangodb::Result TransactionStateMock::beginTransaction(arangodb::transaction::Hints hints) {
  static std::atomic<TRI_voc_tid_t> lastId(0);
  ++beginTransactionCount;
  _hints = hints;

  auto res = useCollections(_nestingLevel);

  if (!res.ok()) {
    updateStatus(arangodb::transaction::Status::ABORTED);
    const_cast<TRI_voc_tid_t&>(_id) = 0; // avoid use of TransactionManagerFeature::manager()->unregisterTransaction(...)

    return res;
  }

  const_cast<TRI_voc_tid_t&>(_id) = ++lastId; // ensure each transaction state has a unique ID
  updateStatus(arangodb::transaction::Status::RUNNING);

  return arangodb::Result();
}

arangodb::Result TransactionStateMock::commitTransaction(arangodb::transaction::Methods* trx) {
  ++commitTransactionCount;
  updateStatus(arangodb::transaction::Status::COMMITTED);
  unuseCollections(_nestingLevel);
  const_cast<TRI_voc_tid_t&>(_id) = 0; // avoid use of TransactionManagerFeature::manager()->unregisterTransaction(...)

  return arangodb::Result();
}

bool TransactionStateMock::hasFailedOperations() const {
  return false; // assume no failed operations
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------