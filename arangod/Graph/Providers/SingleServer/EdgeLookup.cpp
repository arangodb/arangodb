#include "Graph/Providers/SingleServer/EdgeLookup.h"
#include "Transaction/Methods.h"
#include "Transaction/Helpers.h"
#include "Logger/LogMacros.h"
#include "VocBase/vocbase.h"
#include "VocBase/LogicalCollection.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Basics/StaticStrings.h"

using namespace arangodb;

namespace arangodb::graph {

auto EdgeLookup::doAppendEdge(EdgeDocumentToken const& idToken,
                              IndexIterator::DocumentCallback const& cb) const
    -> bool {
  auto col = _trx->vocbase().lookupCollection(idToken.cid());

  if (ADB_UNLIKELY(col == nullptr)) {
    // collection gone... should not happen
    LOG_TOPIC("c4d78", ERR, arangodb::Logger::GRAPHS)
        << "Could not extract indexed edge document. collection not found";
    TRI_ASSERT(col != nullptr);  // for maintainer mode
    return false;
  }

  auto res =
      col->getPhysical()->lookup(_trx, idToken.localDocumentId(), cb, {}).ok();

  if (ADB_UNLIKELY(!res)) {
    // We already had this token, inconsistent state. Return NULL in Production
    LOG_TOPIC("daac5", ERR, arangodb::Logger::GRAPHS)
        << "Could not extract indexed edge document, return 'null' instead. "
        << "This is most likely a caching issue. Try: 'db." << col->name()
        << ".unload(); db." << col->name()
        << ".load()' in arangosh to fix this.";
  }

  return res;
}

auto EdgeLookup::insertEdgeIntoResult(EdgeDocumentToken const& idToken,
                                      VPackBuilder& result) const -> void {
  if (!doAppendEdge(idToken, [&](LocalDocumentId, aql::DocumentData&& data,
                                 VPackSlice edge) {
        _projections.toVelocyPackFromDocumentFull(result, edge, _trx);
        return true;
      })) {
    result.add(VPackSlice::nullSlice());
  }
}

auto EdgeLookup::insertEdgeIdIntoResult(EdgeDocumentToken const& idToken,
                                        VPackBuilder& result) const -> void {
  if (!doAppendEdge(
          idToken,

          [&](LocalDocumentId, aql::DocumentData&& data, VPackSlice edge) {
            result.add(edge.get(StaticStrings::IdString).translate());
            return true;
          })) {
    result.add(VPackSlice::nullSlice());
  }
}

auto EdgeLookup::insertEdgeIntoLookupMap(EdgeDocumentToken const& idToken,
                                         VPackBuilder& result) const -> void {
  if (!doAppendEdge(idToken, [&](LocalDocumentId, aql::DocumentData&& data,
                                 VPackSlice edge) {
        TRI_ASSERT(result.isOpenObject());
        TRI_ASSERT(edge.isObject());
        // Extract and Translate the _key value
        result.add(VPackValue(transaction::helpers::extractIdString(
            _trx->resolver(), edge, VPackSlice::noneSlice())));
        _projections.toVelocyPackFromDocumentFull(result, edge, _trx);
        return true;
      })) {
    // The IDToken has been expanded by an index used on for the edges.
    // The invariant is that an index only delivers existing edges so this
    // case should never happen in production. If it shows up we have
    // inconsistencies in index and data.
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "GraphEngine attempt to read details of a non-existing edge. This "
        "indicates index inconsistency.");
  }
}

auto EdgeLookup::getEdgeId(EdgeDocumentToken const& idToken) const
    -> std::string {
  std::string result;
  if (!doAppendEdge(idToken, [&](LocalDocumentId, aql::DocumentData&& data,
                                 VPackSlice edge) {
        // If we want to expose the ID, we need to translate the
        // custom type Unfortunately we cannot do this in slice only
        // manner, as there is no complete slice with the _id.
        result = transaction::helpers::extractIdString(_trx->resolver(), edge,
                                                       VPackSlice::noneSlice());
        return true;
      })) {
    result = "null";
  }
  return result;
}

}  // namespace arangodb::graph
