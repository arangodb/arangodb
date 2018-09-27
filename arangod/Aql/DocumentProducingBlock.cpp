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
#include "Basics/StaticStrings.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Helpers.h"
      
using namespace arangodb;
using namespace arangodb::aql;

namespace {
static inline void handleProjections(DocumentProducingNode const* node, 
                                     transaction::Methods const* trxPtr,
                                     VPackSlice slice, 
                                     VPackBuilder& b, 
                                     bool useRawDocumentPointers) {
  for (auto const& it : node->projections()) {
    if (it == StaticStrings::IdString) {
      VPackSlice found = transaction::helpers::extractIdFromDocument(slice);
      if (found.isCustom()) {
        // _id as a custom type needs special treatment
        b.add(it, VPackValue(transaction::helpers::extractIdString(trxPtr->resolver(), found, slice)));
      } else {
        b.add(it, found);
      }
    } else if (it == StaticStrings::KeyString) {
      VPackSlice found = transaction::helpers::extractKeyFromDocument(slice);
      if (useRawDocumentPointers) {
        b.add(VPackValue(it));
        b.addExternal(found.begin());
      } else {
        b.add(it, found);
      }
    } else {
      VPackSlice found = slice.get(it);
      if (found.isNone()) {
        // attribute not found
        b.add(it, VPackValue(VPackValueType::Null));
      } else {
        if (useRawDocumentPointers) {
          b.add(VPackValue(it));
          b.addExternal(found.begin());
        } else {
          b.add(it, found);
        }
      }
    }
  }
}

} // namespace

DocumentProducingBlock::DocumentProducingBlock(DocumentProducingNode const* node, transaction::Methods* trx)
    : _trxPtr(trx),
      _node(node),
      _produceResult(dynamic_cast<ExecutionNode const*>(_node)->isVarUsedLater(_node->outVariable())),
      _useRawDocumentPointers(EngineSelectorFeature::ENGINE->useRawDocumentPointers()),
      _allowCoveringIndexOptimization(true) {}

void DocumentProducingBlock::buildCallback() {
  if (!_produceResult) {
    // no result needed
    _documentProducer = [](AqlItemBlock* res, VPackSlice, size_t registerId, size_t& row, size_t fromRow) {
      if (row != fromRow) {
        // re-use already copied AQLValues
        res->copyValuesFromRow(row, static_cast<RegisterId>(registerId), fromRow);
      }
      ++row;
    };
    return;
  }

  if (!_node->projections().empty()) {
    // return a projection
    if (!_node->coveringIndexAttributePositions().empty()) {
      // projections from an index value (covering index)
      _documentProducer = [this](AqlItemBlock* res, VPackSlice slice, size_t registerId, size_t& row, size_t fromRow) {
        transaction::BuilderLeaser b(_trxPtr);
        b->openObject(true);

        if (_allowCoveringIndexOptimization) {
          // a potential call by a covering index iterator... 
          bool const isArray = slice.isArray();
          size_t i = 0;
          VPackSlice found;
          for (auto const& it : _node->projections()) {
            if (isArray) {
              // we will get a Slice with an array of index values. now we need to 
              // look up the array values from the correct positions to populate the
              // result with the projection values
              // this case will be triggered for indexes that can be set up on any
              // number of attributes (hash/skiplist)
              found = slice.at(_node->coveringIndexAttributePositions()[i]);
              ++i;
            } else {
              // no array Slice... this case will be triggered for indexes that contain
              // simple string values, such as the primary index or the edge index
              found = slice;
            }
            if (found.isNone()) {
              // attribute not found
              b->add(it, VPackValue(VPackValueType::Null));
            } else {
              if (_useRawDocumentPointers) {
                b->add(VPackValue(it));
                b->addExternal(found.begin());
              } else {
                b->add(it, found);
              }
            }
          }
        } else {
          // projections from a "real" document
          handleProjections(_node, _trxPtr, slice, *b.get(), _useRawDocumentPointers);
        }

        b->close();
              
        res->emplaceValue(row, static_cast<arangodb::aql::RegisterId>(registerId), b.get());
        
        if (row != fromRow) {
          // re-use already copied AQLValues
          res->copyValuesFromRow(row, static_cast<RegisterId>(registerId), fromRow);
        }
        ++row;
      };
      return;
    }
      
    // projections from a "real" document
    _documentProducer = [this](AqlItemBlock* res, VPackSlice slice, size_t registerId, size_t& row, size_t fromRow) {
      transaction::BuilderLeaser b(_trxPtr);
      b->openObject(true);
      handleProjections(_node, _trxPtr, slice, *b.get(), _useRawDocumentPointers);
      b->close();
            
      res->emplaceValue(row, static_cast<arangodb::aql::RegisterId>(registerId), b.get());
      
      if (row != fromRow) {
        // re-use already copied AQLValues
        res->copyValuesFromRow(row, static_cast<RegisterId>(registerId), fromRow);
      }
      ++row;
    };
    return;
  }

  // return the document as is
  _documentProducer = [this](AqlItemBlock* res, VPackSlice slice, size_t registerId, size_t& row, size_t fromRow) {
    uint8_t const* vpack = slice.begin();
    if (_useRawDocumentPointers) {
      res->emplaceValue(row, static_cast<arangodb::aql::RegisterId>(registerId),
                        AqlValueHintDocumentNoCopy(vpack));
    } else {
      res->emplaceValue(row, static_cast<arangodb::aql::RegisterId>(registerId),
                        AqlValueHintCopy(vpack));
    }
    if (row != fromRow) {
      // re-use already copied AQLValues
      res->copyValuesFromRow(row, static_cast<RegisterId>(registerId), fromRow);
    }
    ++row;
  };
}
