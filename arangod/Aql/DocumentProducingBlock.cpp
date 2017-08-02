////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "DocumentProducingBlock.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/DocumentProducingNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/IndexNode.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Helpers.h"
      
using namespace arangodb;
using namespace arangodb::aql;

DocumentProducingBlock::DocumentProducingBlock(DocumentProducingNode const* node, transaction::Methods* trx)
    : _trxPtr(trx),
      _node(node),
      _produceResult(dynamic_cast<ExecutionNode const*>(_node)->isVarUsedLater(_node->outVariable())),
      _useRawDocumentPointers(EngineSelectorFeature::ENGINE->useRawDocumentPointers()),
      _documentProducer(buildCallback()) {
}

DocumentProducingBlock::DocumentProducingFunction DocumentProducingBlock::buildCallback() const {
  if (!_produceResult) {
    // no result needed
    return [](AqlItemBlock* res, VPackSlice, size_t registerId, size_t& row,
              size_t fromRow) {
      if (row != fromRow) {
        // re-use already copied AQLValues
        res->copyValuesFromRow(row, static_cast<RegisterId>(registerId), fromRow);
      }
      ++row;
    };
  }

  auto const& projection = _node->projection();

  if (projection.size() == 1) {
    if (projection[0] == "_id") {
      // return _id attribute
      return [this](AqlItemBlock* res, VPackSlice slice, size_t registerId, size_t& row, size_t fromRow) {
        VPackSlice found = transaction::helpers::extractIdFromDocument(slice);
        if (found.isCustom()) {
          // _id as a custom type needs special treatment
          res->setValue(row, static_cast<arangodb::aql::RegisterId>(registerId),
                        AqlValue(transaction::helpers::extractIdString(_trxPtr->resolver(), found, slice)));
        } else {
          res->setValue(row, static_cast<arangodb::aql::RegisterId>(registerId),
                        AqlValue(AqlValueHintCopy(found.begin())));
        }
        if (row != fromRow) {
          // re-use already copied AQLValues
          res->copyValuesFromRow(row, static_cast<RegisterId>(registerId), fromRow);
        }
        ++row;
      };
    } else if (projection[0] == "_key") {
      // return _key attribute
      return [this](AqlItemBlock* res, VPackSlice slice, size_t registerId, size_t& row, size_t fromRow) {
        VPackSlice found = transaction::helpers::extractKeyFromDocument(slice);
        if (_useRawDocumentPointers) {
          res->setValue(row, static_cast<arangodb::aql::RegisterId>(registerId),
                        AqlValue(AqlValueHintNoCopy(found.begin())));
        } else {
          res->setValue(row, static_cast<arangodb::aql::RegisterId>(registerId),
                        AqlValue(AqlValueHintCopy(found.begin())));
        }
        if (row != fromRow) {
          // re-use already copied AQLValues
          res->copyValuesFromRow(row, static_cast<RegisterId>(registerId), fromRow);
        }
        ++row;
      };
    }
  }

  if (!projection.empty()) {
    // return another projection
    return [this](AqlItemBlock* res, VPackSlice slice, size_t registerId, size_t& row, size_t fromRow) {
      slice = slice.get(_node->projection());
      if (slice.isNone()) {
        // attribute not found
        res->setValue(row, static_cast<arangodb::aql::RegisterId>(registerId),
                      AqlValue(VPackSlice::nullSlice()));
      } else {
        uint8_t const* vpack = slice.begin();
        if (_useRawDocumentPointers) {
          res->setValue(row, static_cast<arangodb::aql::RegisterId>(registerId),
                        AqlValue(AqlValueHintNoCopy(vpack)));
        } else {
          res->setValue(row, static_cast<arangodb::aql::RegisterId>(registerId),
                        AqlValue(AqlValueHintCopy(vpack)));
        }
      }
      if (row != fromRow) {
        // re-use already copied AQLValues
        res->copyValuesFromRow(row, static_cast<RegisterId>(registerId), fromRow);
      }
      ++row;
    };
  }

  // return the document as is
  return [this](AqlItemBlock* res, VPackSlice slice, size_t registerId, size_t& row, size_t fromRow) {
    uint8_t const* vpack = slice.begin();
    if (_useRawDocumentPointers) {
      res->setValue(row, static_cast<arangodb::aql::RegisterId>(registerId),
                    AqlValue(AqlValueHintNoCopy(vpack)));
    } else {
      res->setValue(row, static_cast<arangodb::aql::RegisterId>(registerId),
                    AqlValue(AqlValueHintCopy(vpack)));
    }
    if (row != fromRow) {
      // re-use already copied AQLValues
      res->copyValuesFromRow(row, static_cast<RegisterId>(registerId), fromRow);
    }
    ++row;
  };
}
