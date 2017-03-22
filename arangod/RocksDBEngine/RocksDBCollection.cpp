#include "RocksDBCollection.h"
#include <Basics/Result.h>
#include <stdexcept>
#include "Indexes/IndexIterator.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {
RocksDBCollection::RocksDBCollection(LogicalCollection* collection,
                                     VPackSlice const& info)
    : PhysicalCollection(collection, info) {
  throw std::logic_error("not implemented");
}

RocksDBCollection::RocksDBCollection(LogicalCollection* collection,
                                     PhysicalCollection*)
    : PhysicalCollection(collection, VPackSlice::emptyObjectSlice()) {
  throw std::logic_error("not implemented");
}

RocksDBCollection::~RocksDBCollection(){};

std::string const& RocksDBCollection::path() const {
  throw std::logic_error("not implemented");
  static std::string const rv("not implmented");
  return rv;
}
void RocksDBCollection::setPath(std::string const& path) {
  throw std::logic_error("not implemented");
}

arangodb::Result RocksDBCollection::updateProperties(VPackSlice const& slice,
                                                     bool doSync) {
  throw std::logic_error("not implemented");
  return arangodb::Result{};
}
arangodb::Result RocksDBCollection::persistProperties() {
  throw std::logic_error("not implemented");
  return arangodb::Result{};
}

PhysicalCollection* RocksDBCollection::clone(LogicalCollection*,
                                             PhysicalCollection*) {
  throw std::logic_error("not implemented");
  return nullptr;
}

TRI_voc_rid_t RocksDBCollection::revision() const {
  throw std::logic_error("not implemented");
  return 0;
}

void RocksDBCollection::updateCount(int64_t) {
  throw std::logic_error("not implemented");
}

void RocksDBCollection::getPropertiesVPack(velocypack::Builder&) const {
  throw std::logic_error("not implemented");
}

// datafile management

/// @brief closes an open collection
int RocksDBCollection::close() {
  throw std::logic_error("not implemented");
  return 0;
}


uint64_t RocksDBCollection::numberDocuments() const {
  throw std::logic_error("not implemented");
  return 0;
}

void RocksDBCollection::sizeHint(transaction::Methods* trx, int64_t hint) {
  throw std::logic_error("not implemented");
}

/// @brief report extra memory used by indexes etc.
size_t RocksDBCollection::memory() const {
  throw std::logic_error("not implemented");
  return 0;
}
void RocksDBCollection::open(bool ignoreErrors) {
  throw std::logic_error("not implemented");
}

/// @brief iterate all markers of a collection on load
int RocksDBCollection::iterateMarkersOnLoad(
    arangodb::transaction::Methods* trx) {
  throw std::logic_error("not implemented");
  return 0;
}

bool RocksDBCollection::isFullyCollected() const {
  throw std::logic_error("not implemented");
  return false;
}

void RocksDBCollection::prepareIndexes(
    arangodb::velocypack::Slice indexesSlice) {
  throw std::logic_error("not implemented");
}

/// @brief Find index by definition
std::shared_ptr<Index> RocksDBCollection::lookupIndex(
    velocypack::Slice const&) const {
  throw std::logic_error("not implemented");
  return nullptr;
}

std::shared_ptr<Index> RocksDBCollection::createIndex(
    transaction::Methods* trx, arangodb::velocypack::Slice const& info,
    bool& created) {
  throw std::logic_error("not implemented");
  return nullptr;
}
/// @brief Restores an index from VelocyPack.
int RocksDBCollection::restoreIndex(transaction::Methods*,
                                    velocypack::Slice const&,
                                    std::shared_ptr<Index>&) {
  throw std::logic_error("not implemented");
  return 0;
}
/// @brief Drop an index with the given iid.
bool RocksDBCollection::dropIndex(TRI_idx_iid_t iid) {
  throw std::logic_error("not implemented");
  return false;
}
std::unique_ptr<IndexIterator> RocksDBCollection::getAllIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr, bool reverse) {
  throw std::logic_error("not implemented");
  return nullptr;
}
std::unique_ptr<IndexIterator> RocksDBCollection::getAnyIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr) {
  throw std::logic_error("not implemented");
  return nullptr;
}
void RocksDBCollection::invokeOnAllElements(
    std::function<bool(DocumentIdentifierToken const&)> callback) {
  throw std::logic_error("not implemented");
}

////////////////////////////////////
// -- SECTION DML Operations --
///////////////////////////////////

void RocksDBCollection::truncate(transaction::Methods* trx,
                                 OperationOptions& options) {
  throw std::logic_error("not implemented");
}

int RocksDBCollection::read(transaction::Methods*,
                            arangodb::velocypack::Slice const key,
                            ManagedDocumentResult& result, bool) {
  throw std::logic_error("not implemented");
  return 0;
}

bool RocksDBCollection::readDocument(transaction::Methods* trx,
                                     DocumentIdentifierToken const& token,
                                     ManagedDocumentResult& result) {
  throw std::logic_error("not implemented");
  return false;
}

bool RocksDBCollection::readDocumentConditional(
    transaction::Methods* trx, DocumentIdentifierToken const& token,
    TRI_voc_tick_t maxTick, ManagedDocumentResult& result) {
  throw std::logic_error("not implemented");
  return false;
}

int RocksDBCollection::insert(arangodb::transaction::Methods* trx,
                              arangodb::velocypack::Slice const newSlice,
                              arangodb::ManagedDocumentResult& result,
                              OperationOptions& options,
                              TRI_voc_tick_t& resultMarkerTick, bool lock) {
  throw std::logic_error("not implemented");
  return 0;
}

int RocksDBCollection::update(arangodb::transaction::Methods* trx,
                              arangodb::velocypack::Slice const newSlice,
                              arangodb::ManagedDocumentResult& result,
                              OperationOptions& options,
                              TRI_voc_tick_t& resultMarkerTick, bool lock,
                              TRI_voc_rid_t& prevRev,
                              ManagedDocumentResult& previous,
                              TRI_voc_rid_t const& revisionId,
                              arangodb::velocypack::Slice const key) {
  throw std::logic_error("not implemented");
  return 0;
}

int RocksDBCollection::replace(
    transaction::Methods* trx, arangodb::velocypack::Slice const newSlice,
    ManagedDocumentResult& result, OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t& prevRev,
    ManagedDocumentResult& previous, TRI_voc_rid_t const revisionId,
    arangodb::velocypack::Slice const fromSlice,
    arangodb::velocypack::Slice const toSlice) {
  throw std::logic_error("not implemented");
  return 0;
}

int RocksDBCollection::remove(arangodb::transaction::Methods* trx,
                              arangodb::velocypack::Slice const slice,
                              arangodb::ManagedDocumentResult& previous,
                              OperationOptions& options,
                              TRI_voc_tick_t& resultMarkerTick, bool lock,
                              TRI_voc_rid_t const& revisionId,
                              TRI_voc_rid_t& prevRev) {
  throw std::logic_error("not implemented");
  return 0;
}

void RocksDBCollection::deferDropCollection(
    std::function<bool(LogicalCollection*)> callback) {
  throw std::logic_error("not implemented");
}
}
