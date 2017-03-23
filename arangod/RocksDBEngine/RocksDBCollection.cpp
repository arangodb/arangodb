#include "RocksDBCollection.h"
#include <Basics/Result.h>
#include <stdexcept>
#include "Indexes/IndexIterator.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
  
static std::string const Empty;

RocksDBCollection::RocksDBCollection(LogicalCollection* collection,
                                     VPackSlice const& info)
    : PhysicalCollection(collection, info) {}

RocksDBCollection::RocksDBCollection(LogicalCollection* collection,
                                     PhysicalCollection*)
    : PhysicalCollection(collection, VPackSlice::emptyObjectSlice()) {
}

RocksDBCollection::~RocksDBCollection() {}

std::string const& RocksDBCollection::path() const {
  return Empty; // we do not have any path
}

void RocksDBCollection::setPath(std::string const&) {
  // we do not have any path
}

arangodb::Result RocksDBCollection::updateProperties(VPackSlice const& slice,
                                                     bool doSync) {
  // nothing to do
  return arangodb::Result{};
}

arangodb::Result RocksDBCollection::persistProperties() {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return arangodb::Result{};
}

PhysicalCollection* RocksDBCollection::clone(LogicalCollection* logical,
                                             PhysicalCollection* physical) {
  return new RocksDBCollection(logical, physical);
}

TRI_voc_rid_t RocksDBCollection::revision() const {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return 0;
}

int64_t RocksDBCollection::initialCount() const {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return 0;
}

void RocksDBCollection::updateCount(int64_t) {
  THROW_ARANGO_NOT_IMPLEMENTED();
}

void RocksDBCollection::getPropertiesVPack(velocypack::Builder&) const {
  THROW_ARANGO_NOT_IMPLEMENTED();
}

void RocksDBCollection::getPropertiesVPackCoordinator(velocypack::Builder&) const {
  THROW_ARANGO_NOT_IMPLEMENTED();
}

/// @brief closes an open collection
int RocksDBCollection::close() {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return 0;
}

uint64_t RocksDBCollection::numberDocuments() const {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return 0;
}

void RocksDBCollection::sizeHint(transaction::Methods* trx, int64_t hint) {
  THROW_ARANGO_NOT_IMPLEMENTED();
}

/// @brief report extra memory used by indexes etc.
size_t RocksDBCollection::memory() const {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return 0;
}

void RocksDBCollection::open(bool ignoreErrors) {
  THROW_ARANGO_NOT_IMPLEMENTED();
}

/// @brief iterate all markers of a collection on load
int RocksDBCollection::iterateMarkersOnLoad(
    arangodb::transaction::Methods* trx) {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return 0;
}

bool RocksDBCollection::isFullyCollected() const {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return false;
}

void RocksDBCollection::prepareIndexes(
    arangodb::velocypack::Slice indexesSlice) {
  THROW_ARANGO_NOT_IMPLEMENTED();
}

/// @brief Find index by definition
std::shared_ptr<Index> RocksDBCollection::lookupIndex(
    velocypack::Slice const&) const {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return nullptr;
}

std::shared_ptr<Index> RocksDBCollection::createIndex(
    transaction::Methods* trx, arangodb::velocypack::Slice const& info,
    bool& created) {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return nullptr;
}
/// @brief Restores an index from VelocyPack.
int RocksDBCollection::restoreIndex(transaction::Methods*,
                                    velocypack::Slice const&,
                                    std::shared_ptr<Index>&) {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return 0;
}
/// @brief Drop an index with the given iid.
bool RocksDBCollection::dropIndex(TRI_idx_iid_t iid) {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return false;
}
std::unique_ptr<IndexIterator> RocksDBCollection::getAllIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr, bool reverse) {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return nullptr;
}
std::unique_ptr<IndexIterator> RocksDBCollection::getAnyIterator(
    transaction::Methods* trx, ManagedDocumentResult* mdr) {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return nullptr;
}
void RocksDBCollection::invokeOnAllElements(
    std::function<bool(DocumentIdentifierToken const&)> callback) {
  THROW_ARANGO_NOT_IMPLEMENTED();
}

////////////////////////////////////
// -- SECTION DML Operations --
///////////////////////////////////

void RocksDBCollection::truncate(transaction::Methods* trx,
                                 OperationOptions& options) {
  THROW_ARANGO_NOT_IMPLEMENTED();
}

int RocksDBCollection::read(transaction::Methods*,
                            arangodb::velocypack::Slice const key,
                            ManagedDocumentResult& result, bool) {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return 0;
}

bool RocksDBCollection::readDocument(transaction::Methods* trx,
                                     DocumentIdentifierToken const& token,
                                     ManagedDocumentResult& result) {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return false;
}

bool RocksDBCollection::readDocumentConditional(
    transaction::Methods* trx, DocumentIdentifierToken const& token,
    TRI_voc_tick_t maxTick, ManagedDocumentResult& result) {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return false;
}

int RocksDBCollection::insert(arangodb::transaction::Methods* trx,
                              arangodb::velocypack::Slice const newSlice,
                              arangodb::ManagedDocumentResult& result,
                              OperationOptions& options,
                              TRI_voc_tick_t& resultMarkerTick, bool lock) {
  THROW_ARANGO_NOT_IMPLEMENTED();
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
  THROW_ARANGO_NOT_IMPLEMENTED();
  return 0;
}

int RocksDBCollection::replace(
    transaction::Methods* trx, arangodb::velocypack::Slice const newSlice,
    ManagedDocumentResult& result, OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick, bool lock, TRI_voc_rid_t& prevRev,
    ManagedDocumentResult& previous, TRI_voc_rid_t const revisionId,
    arangodb::velocypack::Slice const fromSlice,
    arangodb::velocypack::Slice const toSlice) {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return 0;
}

int RocksDBCollection::remove(arangodb::transaction::Methods* trx,
                              arangodb::velocypack::Slice const slice,
                              arangodb::ManagedDocumentResult& previous,
                              OperationOptions& options,
                              TRI_voc_tick_t& resultMarkerTick, bool lock,
                              TRI_voc_rid_t const& revisionId,
                              TRI_voc_rid_t& prevRev) {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return 0;
}

void RocksDBCollection::deferDropCollection(
    std::function<bool(LogicalCollection*)> callback) {
  THROW_ARANGO_NOT_IMPLEMENTED();
}
  
/// @brief return engine-specific figures
void RocksDBCollection::figuresSpecific(std::shared_ptr<arangodb::velocypack::Builder>&) {
  THROW_ARANGO_NOT_IMPLEMENTED();
}
