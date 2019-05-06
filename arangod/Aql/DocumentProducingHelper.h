////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_DOCUMENT_PRODUCING_HELPER_H
#define ARANGOD_AQL_DOCUMENT_PRODUCING_HELPER_H 1

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Basics/Common.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Slice.h>

namespace arangodb {
namespace aql {

using DocumentProducingFunction =
    std::function<void(InputAqlItemRow&, OutputAqlItemRow&, arangodb::velocypack::Slice, RegisterId)>;

inline void handleProjections(std::vector<std::string> const& projections,
                              transaction::Methods const* trxPtr, VPackSlice slice,
                              VPackBuilder& b, bool useRawDocumentPointers) {
  for (auto const& it : projections) {
    if (it == StaticStrings::IdString) {
      VPackSlice found = transaction::helpers::extractIdFromDocument(slice);
      if (found.isCustom()) {
        // _id as a custom type needs special treatment
        b.add(it, VPackValue(transaction::helpers::extractIdString(trxPtr->resolver(),
                                                                   found, slice)));
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

struct DocumentProducingFunctionContext {
 public:
  DocumentProducingFunctionContext(bool produceResult,
                                   std::vector<std::string> const& projections,
                                   transaction::Methods* trxPtr,
                                   std::vector<size_t> const& coveringIndexAttributePositions,
                                   bool allowCoveringIndexOptimization, bool useRawDocumentPointers)
      : _produceResult(produceResult),
        _projections(projections),
        _trxPtr(trxPtr),
        _coveringIndexAttributePositions(coveringIndexAttributePositions),
        _allowCoveringIndexOptimization(allowCoveringIndexOptimization),
        _useRawDocumentPointers(useRawDocumentPointers) {}

  DocumentProducingFunctionContext() = delete;

  bool getProduceResult() const noexcept { return _produceResult; }

  std::vector<std::string> const& getProjections() const noexcept {
    return _projections;
  }

  transaction::Methods* getTrxPtr() const noexcept { return _trxPtr; }

  std::vector<size_t> const& getCoveringIndexAttributePositions() const noexcept {
    return _coveringIndexAttributePositions;
  }

  bool getAllowCoveringIndexOptimization() const noexcept {
    return _allowCoveringIndexOptimization;
  }

  bool getUseRawDocumentPointers() const noexcept {
    return _useRawDocumentPointers;
  }

  void setAllowCoveringIndexOptimization(bool allowCoveringIndexOptimization) noexcept {
    _allowCoveringIndexOptimization = allowCoveringIndexOptimization;
  }

 private:
  bool const _produceResult;
  std::vector<std::string> const& _projections;
  transaction::Methods* const _trxPtr;
  std::vector<size_t> const& _coveringIndexAttributePositions;
  bool _allowCoveringIndexOptimization;
  bool const _useRawDocumentPointers;
};

inline DocumentProducingFunction buildCallback(DocumentProducingFunctionContext& context) {
  DocumentProducingFunction documentProducer;
  if (!context.getProduceResult()) {
    // no result needed
    documentProducer = [](InputAqlItemRow& input, OutputAqlItemRow& output,
                          VPackSlice, RegisterId registerId) {
      // TODO: optimize this within the register planning mechanism?
      TRI_ASSERT(!output.isFull());
      output.cloneValueInto(registerId, input, AqlValue(AqlValueHintNull()));
      TRI_ASSERT(output.produced());
      output.advanceRow();
    };
    return documentProducer;
  }

  if (!context.getProjections().empty()) {
    // return a projection
    if (!context.getCoveringIndexAttributePositions().empty()) {
      // projections from an index value (covering index)
      documentProducer = [&context](InputAqlItemRow& input, OutputAqlItemRow& output,
                                    VPackSlice slice, RegisterId registerId) {
        transaction::BuilderLeaser b(context.getTrxPtr());
        b->openObject(true);

        if (context.getAllowCoveringIndexOptimization()) {
          // a potential call by a covering index iterator...
          bool const isArray = slice.isArray();
          size_t i = 0;
          VPackSlice found;
          for (auto const& it : context.getProjections()) {
            if (isArray) {
              // we will get a Slice with an array of index values. now we need
              // to look up the array values from the correct positions to
              // populate the result with the projection values this case will
              // be triggered for indexes that can be set up on any number of
              // attributes (hash/skiplist)
              found = slice.at(context.getCoveringIndexAttributePositions()[i]);
              ++i;
            } else {
              // no array Slice... this case will be triggered for indexes that
              // contain simple string values, such as the primary index or the
              // edge index
              found = slice;
            }
            if (found.isNone()) {
              // attribute not found
              b->add(it, VPackValue(VPackValueType::Null));
            } else {
              if (context.getUseRawDocumentPointers()) {
                b->add(VPackValue(it));
                b->addExternal(found.begin());
              } else {
                b->add(it, found);
              }
            }
          }
        } else {
          // projections from a "real" document
          handleProjections(context.getProjections(), context.getTrxPtr(), slice,
                            *b.get(), context.getUseRawDocumentPointers());
        }

        b->close();

        AqlValue v(b.get());
        AqlValueGuard guard{v, true};
        TRI_ASSERT(!output.isFull());
        output.moveValueInto(registerId, input, guard);
        TRI_ASSERT(output.produced());
        output.advanceRow();
      };
      return documentProducer;
    }

    // projections from a "real" document
    documentProducer = [context](InputAqlItemRow& input, OutputAqlItemRow& output,
                                 VPackSlice slice, RegisterId registerId) {
      transaction::BuilderLeaser b(context.getTrxPtr());
      b->openObject(true);
      handleProjections(context.getProjections(), context.getTrxPtr(), slice,
                        *b.get(), context.getUseRawDocumentPointers());
      b->close();

      AqlValue v(b.get());
      AqlValueGuard guard{v, true};
      TRI_ASSERT(!output.isFull());
      output.moveValueInto(registerId, input, guard);
      TRI_ASSERT(output.produced());
      output.advanceRow();
    };
    return documentProducer;
  }

  // return the document as is
  if (context.getUseRawDocumentPointers()) {
    documentProducer = [](InputAqlItemRow& input, OutputAqlItemRow& output,
                          VPackSlice slice, RegisterId registerId) {
      uint8_t const* vpack = slice.begin();
      // With NoCopy we do not clone anyways
      TRI_ASSERT(!output.isFull());
      AqlValue v{AqlValueHintDocumentNoCopy{vpack}};
      AqlValueGuard guard{v, false};
      output.moveValueInto(registerId, input, guard);
      TRI_ASSERT(output.produced());
      output.advanceRow();
    };
  } else {
    documentProducer = [](InputAqlItemRow& input, OutputAqlItemRow& output,
                          VPackSlice slice, RegisterId registerId) {
      uint8_t const* vpack = slice.begin();

      // Here we do a clone, so clone once, then move into
      AqlValue v{AqlValueHintCopy{vpack}};
      AqlValueGuard guard{v, true};
      TRI_ASSERT(!output.isFull());
      output.moveValueInto(registerId, input, guard);
      TRI_ASSERT(output.produced());
      output.advanceRow();
    };
  }

  return documentProducer;
}

}  // namespace aql
}  // namespace arangodb

#endif
