////////////////////////////////////////////////////////////////////////////////
/// @brief AQL ModificationBlock(s)
///
/// @file 
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/ModificationBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/json-utilities.h"
#include "Basics/Exceptions.h"
#include "Cluster/ClusterMethods.h"
#include "V8/v8-globals.h"
#include "VocBase/vocbase.h"

using namespace std;
using namespace triagens::arango;
using namespace triagens::aql;

using Json = triagens::basics::Json;
using JsonHelper = triagens::basics::JsonHelper;

// -----------------------------------------------------------------------------
// --SECTION--                                           class ModificationBlock
// -----------------------------------------------------------------------------

ModificationBlock::ModificationBlock (ExecutionEngine* engine,
                                      ModificationNode const* ep)
  : ExecutionBlock(engine, ep),
    _outRegOld(ExecutionNode::MaxRegisterId),
    _outRegNew(ExecutionNode::MaxRegisterId),
    _collection(ep->_collection),
    _isDBServer(false),
    _usesDefaultSharding(true),
    _buffer(TRI_UNKNOWN_MEM_ZONE) {
  
  auto trxCollection = _trx->trxCollection(_collection->cid());
  if (trxCollection != nullptr) {
    _trx->orderDitch(trxCollection);
  }

  auto const& registerPlan = ep->getRegisterPlan()->varInfo;

  if (ep->_outVariableOld != nullptr) {
    auto it = registerPlan.find(ep->_outVariableOld->id);
    TRI_ASSERT(it != registerPlan.end());
    _outRegOld = (*it).second.registerId;
  }
  if (ep->_outVariableNew != nullptr) {
    auto it = registerPlan.find(ep->_outVariableNew->id);
    TRI_ASSERT(it != registerPlan.end());
    _outRegNew = (*it).second.registerId;
  }
            
  // check if we're a DB server in a cluster
  _isDBServer = triagens::arango::ServerState::instance()->isDBServer();

  if (_isDBServer) {
    _usesDefaultSharding = _collection->usesDefaultSharding();
  }
}

ModificationBlock::~ModificationBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get some - this accumulates all input and calls the work() method
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* ModificationBlock::getSome (size_t atLeast,
                                          size_t atMost) {
  std::vector<AqlItemBlock*> blocks;
  std::unique_ptr<AqlItemBlock>replyBlocks;

  auto freeBlocks = [](std::vector<AqlItemBlock*>& blocks) {
    for (auto it = blocks.begin(); it != blocks.end(); ++it) {
      if ((*it) != nullptr) {
        delete (*it);
      }
    }
    blocks.clear();
  };

  // loop over input until it is exhausted
  try {
    if (static_cast<ModificationNode const*>(_exeNode)->_options.readCompleteInput) {
      // read all input into a buffer first
      while (true) { 
        std::unique_ptr<AqlItemBlock> res(ExecutionBlock::getSomeWithoutRegisterClearout(atLeast, atMost));

        if (res.get() == nullptr) {
          break;
        }
          
        TRI_IF_FAILURE("ModificationBlock::getSome") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
       
        blocks.emplace_back(res.get());
        res.release();
      }

      // now apply the modifications for the complete input
      replyBlocks.reset(work(blocks));
    }
    else {
      // read input in chunks, and process it in chunks
      // this reduces the amount of memory used for storing the input
      while (true) {
        freeBlocks(blocks);
        std::unique_ptr<AqlItemBlock> res(ExecutionBlock::getSomeWithoutRegisterClearout(atLeast, atMost));

        if (res.get() == nullptr) {
          break;
        }
        
        TRI_IF_FAILURE("ModificationBlock::getSome") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
       
        blocks.emplace_back(res.get());
        res.release();

        replyBlocks.reset(work(blocks));

        if (replyBlocks.get() != nullptr) {
          break;
        }
      }
    }
  }
  catch (...) {
    freeBlocks(blocks);
    throw;
  }

  freeBlocks(blocks);
        
  return replyBlocks.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a key from the AqlValue passed
////////////////////////////////////////////////////////////////////////////////
          
int ModificationBlock::extractKey (AqlValue const& value,
                                   TRI_document_collection_t const* document,
                                   std::string& key) {
  if (value.isShaped()) {
    key = TRI_EXTRACT_MARKER_KEY(value.getMarker());
    return TRI_ERROR_NO_ERROR;
  }

  if (value.isObject()) {
    Json member(value.extractObjectMember(_trx, document, TRI_VOC_ATTRIBUTE_KEY, false, _buffer));

    TRI_json_t const* json = member.json();

    if (TRI_IsStringJson(json)) {
      key.assign(json->_value._string.data, json->_value._string.length - 1);
      return TRI_ERROR_NO_ERROR;
    }
  }
  else if (value.isString()) {
    key = value.toString();
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a master pointer from the marker passed
////////////////////////////////////////////////////////////////////////////////

void ModificationBlock::constructMptr (TRI_doc_mptr_copy_t* dst,
                                       TRI_df_marker_t const* marker) const { 
  dst->_rid = TRI_EXTRACT_MARKER_RID(marker);
  dst->_fid = 0;
  dst->_hash = 0;
  dst->_prev = nullptr;
  dst->_next = nullptr;
  dst->setDataPtr(marker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a shard key value has changed
////////////////////////////////////////////////////////////////////////////////
                
bool ModificationBlock::isShardKeyChange (TRI_json_t const* oldJson,
                                          TRI_json_t const* newJson,
                                          bool isPatch) const {
  TRI_ASSERT(_isDBServer);

  auto planId = _collection->documentCollection()->_info._planId;
  auto vocbase = static_cast<ModificationNode const*>(_exeNode)->_vocbase;
  return triagens::arango::shardKeysChanged(vocbase->_name, std::to_string(planId), oldJson, newJson, isPatch);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the _key attribute is specified when it must not be
/// specified
////////////////////////////////////////////////////////////////////////////////
              
bool ModificationBlock::isShardKeyError (TRI_json_t const* json) const {
  TRI_ASSERT(_isDBServer);
              
  if (_usesDefaultSharding) {
    return false;
  }

  return (TRI_LookupObjectJson(json, TRI_VOC_ATTRIBUTE_KEY) != nullptr);
}                 

////////////////////////////////////////////////////////////////////////////////
/// @brief process the result of a data-modification operation
////////////////////////////////////////////////////////////////////////////////

void ModificationBlock::handleResult (int code,
                                      bool ignoreErrors,
                                      std::string const* errorMessage) {
  if (code == TRI_ERROR_NO_ERROR) {
    // update the success counter
    ++_engine->_stats.writesExecuted;
    return;
  }
    
  if (ignoreErrors) {
    // update the ignored counter
    ++_engine->_stats.writesIgnored;
    return;
  }
      
  // bubble up the error
  if (errorMessage != nullptr && ! errorMessage->empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(code, *errorMessage);
  }
        
  THROW_ARANGO_EXCEPTION(code);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 class RemoveBlock
// -----------------------------------------------------------------------------

RemoveBlock::RemoveBlock (ExecutionEngine* engine,
                          RemoveNode const* ep)
  : ModificationBlock(engine, ep) {
}

RemoveBlock::~RemoveBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for removing data
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* RemoveBlock::work (std::vector<AqlItemBlock*>& blocks) {
  size_t const count = countBlocksRows(blocks);
  
  if (count == 0) {
    return nullptr;
  }
  
  std::unique_ptr<AqlItemBlock> result;

  auto ep = static_cast<RemoveNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;

  TRI_doc_mptr_copy_t nptr;
  auto trxCollection = _trx->trxCollection(_collection->cid());
  
  bool const ignoreDocumentNotFound = ep->getOptions().ignoreDocumentNotFound;
  bool const producesOutput = (ep->_outVariableOld != nullptr);

  result.reset(new AqlItemBlock(count,
                                getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  if (producesOutput) {
    result->setDocumentCollection(_outRegOld, trxCollection->_collection->_collection);
  }

  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);
    auto document = res->getDocumentCollection(registerId);
    
    throwIfKilled(); // check if we were aborted
      
    size_t const n = res->size();
    
    // loop over the complete block
    for (size_t i = 0; i < n; ++i) {
      AqlValue a = res->getValue(i, registerId);
    
      // only copy 1st row of registers inherited from previous frame(s)
      inheritRegisters(res, result.get(), i, dstRow);

      std::string key;
      int errorCode = TRI_ERROR_NO_ERROR;

      if (a.isObject()) {
        // value is an array. now extract the _key attribute
        errorCode = extractKey(a, document, key);
      }
      else if (a.isString()) {
        // value is a string
        key = a.toChar();
      }
      else {
        errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
      }

      if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
        // read "old" version
        if (a.isShaped()) {
          // already have a ShapedJson, no need to fetch the old document again
          constructMptr(&nptr, a.getMarker());
        }
        else {
          // need to fetch the old document
          errorCode = _trx->readSingle(trxCollection, &nptr, key);
        }
      }

      if (errorCode == TRI_ERROR_NO_ERROR) {
        // no error. we expect to have a key
                  
        // all exceptions are caught in _trx->remove()
        errorCode = _trx->remove(trxCollection, 
                                key,
                                0,
                                TRI_DOC_UPDATE_LAST_WRITE,
                                0, 
                                nullptr,
                                ep->_options.waitForSync);

        if (errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && 
            _isDBServer &&
            ignoreDocumentNotFound) {
          // Ignore document not found on the DBserver:
          errorCode = TRI_ERROR_NO_ERROR;
        }

        if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
          result->setValue(dstRow,
                           _outRegOld,
                           AqlValue(reinterpret_cast<TRI_df_marker_t const*>(nptr.getDataPtr())));

        }
      }
        
      handleResult(errorCode, ep->_options.ignoreErrors); 
      ++dstRow;
    }
    // done with a block

    // now free it already
    (*it) = nullptr;  
    delete res;
  }

  return result.release();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 class InsertBlock
// -----------------------------------------------------------------------------

InsertBlock::InsertBlock (ExecutionEngine* engine,
                          InsertNode const* ep)
  : ModificationBlock(engine, ep) {
}

InsertBlock::~InsertBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for inserting data
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* InsertBlock::work (std::vector<AqlItemBlock*>& blocks) {
  size_t const count = countBlocksRows(blocks);

  if (count == 0) {
    return nullptr;
  }
  
  std::unique_ptr<AqlItemBlock> result;

  auto ep = static_cast<InsertNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;

  auto trxCollection = _trx->trxCollection(_collection->cid());

  TRI_doc_mptr_copy_t nptr;
  bool const isEdgeCollection = _collection->isEdgeCollection();
  bool const producesOutput = (ep->_outVariableNew != nullptr);
         
  // initialize an empty edge container
  TRI_document_edge_t edge = { 0, nullptr, 0, nullptr };

  std::string from;
  std::string to;

  result.reset(new AqlItemBlock(count, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  if (producesOutput) {
    result->setDocumentCollection(_outRegNew, trxCollection->_collection->_collection);
  }

  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);
    auto document = res->getDocumentCollection(registerId);
    size_t const n = res->size();
    
    throwIfKilled(); // check if we were aborted
    
    // loop over the complete block
    for (size_t i = 0; i < n; ++i) {
      AqlValue a = res->getValue(i, registerId);
      
      // only copy 1st row of registers inherited from previous frame(s)
      inheritRegisters(res, result.get(), i, dstRow);

      int errorCode = TRI_ERROR_NO_ERROR;

      if (a.isObject()) {
        // value is an array
        
        if (isEdgeCollection) {
          // array must have _from and _to attributes
          TRI_json_t const* json;

          Json member(a.extractObjectMember(_trx, document, TRI_VOC_ATTRIBUTE_FROM, false, _buffer));
          json = member.json();

          if (TRI_IsStringJson(json)) {
            errorCode = resolve(json->_value._string.data, edge._fromCid, from);
          }
          else {
            errorCode = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
          }
         
          if (errorCode == TRI_ERROR_NO_ERROR) { 
            Json member(a.extractObjectMember(_trx, document, TRI_VOC_ATTRIBUTE_TO, false, _buffer));
            json = member.json();
            if (TRI_IsStringJson(json)) {
              errorCode = resolve(json->_value._string.data, edge._toCid, to);
            }
            else {
              errorCode = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
            }
          }
        }
      }
      else {
        errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
      }

      if (errorCode == TRI_ERROR_NO_ERROR) {
        TRI_doc_mptr_copy_t mptr;
        auto json = a.toJson(_trx, document, false);

        if (isEdgeCollection) {
          // edge
          edge._fromKey = (TRI_voc_key_t) from.c_str();
          edge._toKey = (TRI_voc_key_t) to.c_str();
          errorCode = _trx->create(trxCollection, &mptr, json.json(), &edge, ep->_options.waitForSync);
        }
        else {
          // document
          errorCode = _trx->create(trxCollection, &mptr, json.json(), nullptr, ep->_options.waitForSync);
        }

        if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
          result->setValue(dstRow,
                           _outRegNew,
                           AqlValue(reinterpret_cast<TRI_df_marker_t const*>(mptr.getDataPtr())));
        }
      }

      handleResult(errorCode, ep->_options.ignoreErrors);
      ++dstRow; 
    }
    // done with a block

    // now free it already
    (*it) = nullptr;  
    delete res;
  }

  return result.release();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 class UpdateBlock
// -----------------------------------------------------------------------------

UpdateBlock::UpdateBlock (ExecutionEngine* engine,
                          UpdateNode const* ep)
  : ModificationBlock(engine, ep) {
}

UpdateBlock::~UpdateBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for inserting data
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* UpdateBlock::work (std::vector<AqlItemBlock*>& blocks) {
  size_t const count = countBlocksRows(blocks);

  if (count == 0) {
    return nullptr;
  }

  std::unique_ptr<AqlItemBlock> result;
  auto ep = static_cast<UpdateNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inDocVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const docRegisterId = it->second.registerId;
  RegisterId keyRegisterId = 0; // default initialization
  
  bool const ignoreDocumentNotFound = ep->getOptions().ignoreDocumentNotFound;
  bool const producesOutput = (ep->_outVariableOld != nullptr || ep->_outVariableNew != nullptr);

  TRI_doc_mptr_copy_t nptr;
  bool const hasKeyVariable = (ep->_inKeyVariable != nullptr);
  std::string errorMessage;

  if (hasKeyVariable) {
    it = ep->getRegisterPlan()->varInfo.find(ep->_inKeyVariable->id);
    TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
    keyRegisterId = it->second.registerId;
  }
  
  auto trxCollection = _trx->trxCollection(_collection->cid());

  result.reset(new AqlItemBlock(count, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  if (ep->_outVariableOld != nullptr) {
    result->setDocumentCollection(_outRegOld, trxCollection->_collection->_collection);
  }
  if (ep->_outVariableNew != nullptr) {
    result->setDocumentCollection(_outRegNew, trxCollection->_collection->_collection);
  }

  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);   // This is intentionally a copy!
    auto document = res->getDocumentCollection(docRegisterId);
    decltype(document) keyDocument = nullptr;

    throwIfKilled(); // check if we were aborted

    if (hasKeyVariable) {
      keyDocument = res->getDocumentCollection(keyRegisterId);
    }

    size_t const n = res->size();
    
    // loop over the complete block
    for (size_t i = 0; i < n; ++i) {
      AqlValue a = res->getValue(i, docRegisterId);

      int errorCode = TRI_ERROR_NO_ERROR;
      std::string key;

      if (a.isObject()) {
        // value is an object
        if (hasKeyVariable) {
          // seperate key specification
          AqlValue k = res->getValue(i, keyRegisterId);
          errorCode = extractKey(k, keyDocument, key);
        }
        else {
          errorCode = extractKey(a, document, key);
        }
      }
      else {
        errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
        errorMessage += std::string("expecting 'object', got: ") +
          a.getTypeString() + 
          std::string(" while handling: ") + 
          _exeNode->getTypeString();
      }

      if (errorCode == TRI_ERROR_NO_ERROR) {
        TRI_doc_mptr_copy_t mptr;
        auto json = a.toJson(_trx, document, true);

        // read old document
        TRI_doc_mptr_copy_t oldDocument;
        if (! hasKeyVariable && a.isShaped()) {
          // "old" is already ShapedJson. no need to fetch the old document first
          constructMptr(&oldDocument, a.getMarker());
        }
        else {
          // "old" is no ShapedJson. now fetch old version from database
          errorCode = _trx->readSingle(trxCollection, &oldDocument, key);
        }

        if (! json.isObject()) {
          errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
        }

        if (errorCode == TRI_ERROR_NO_ERROR) {
          if (oldDocument.getDataPtr() != nullptr) {

            if (json.members() > 0) {
              // only update the document if the update value is not empty
              TRI_shaped_json_t shapedJson;
              TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, oldDocument.getDataPtr()); // PROTECTED by trx here
              std::unique_ptr<TRI_json_t> old(TRI_JsonShapedJson(_collection->documentCollection()->getShaper(), &shapedJson));

              // the default
              errorCode = TRI_ERROR_OUT_OF_MEMORY;

              if (old.get() != nullptr) {
                std::unique_ptr<TRI_json_t> patchedJson(TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, old.get(), json.json(), ep->_options.nullMeansRemove, ep->_options.mergeObjects));

                if (patchedJson.get() != nullptr) {
                  if (_isDBServer && 
                      isShardKeyChange(old.get(), patchedJson.get(), true)) {
                    errorCode = TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES;
                  }
                  else {
                    // all exceptions are caught in _trx->update()
                    errorCode = _trx->update(trxCollection, key, 0, &mptr, patchedJson.get(), TRI_DOC_UPDATE_LAST_WRITE, 0, nullptr, ep->_options.waitForSync);
                  }
                }
              }
            }
            else {
              // copy the existing master pointer for OLD into NEW
              mptr = oldDocument;
            }
          }
          else {
            errorCode = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
          }
        }

        if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
          if (ep->_outVariableOld != nullptr) {
            // store $OLD
            result->setValue(dstRow,
                             _outRegOld,
                             AqlValue(reinterpret_cast<TRI_df_marker_t const*>(oldDocument.getDataPtr())));
          }

          if (ep->_outVariableNew != nullptr) {
            // store $NEW
            result->setValue(dstRow,
                             _outRegNew,
                             AqlValue(reinterpret_cast<TRI_df_marker_t const*>(mptr.getDataPtr())));
          }
        }

        if (errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && 
            _isDBServer &&
            ignoreDocumentNotFound) {
          // Ignore document not found on the DBserver:
          errorCode = TRI_ERROR_NO_ERROR;
        }
      }

      handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
      ++dstRow; 
    }
    // done with a block

    // now free it already
    (*it) = nullptr;  
    delete res;
  }

  return result.release();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 class UpsertBlock
// -----------------------------------------------------------------------------

UpsertBlock::UpsertBlock (ExecutionEngine* engine,
                          UpsertNode const* ep)
  : ModificationBlock(engine, ep) {
}

UpsertBlock::~UpsertBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for inserting data
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* UpsertBlock::work (std::vector<AqlItemBlock*>& blocks) {
  size_t const count = countBlocksRows(blocks);

  if (count == 0) {
    return nullptr;
  }

  std::unique_ptr<AqlItemBlock> result;
  auto ep = static_cast<UpsertNode const*>(getPlanNode());
  
  auto const& registerPlan = ep->getRegisterPlan()->varInfo;
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inDocVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const docRegisterId = it->second.registerId;
  
  it= registerPlan.find(ep->_insertVariable->id);
  TRI_ASSERT(it != registerPlan.end());
  RegisterId const insertRegisterId = it->second.registerId;

  it = registerPlan.find(ep->_updateVariable->id);
  TRI_ASSERT(it != registerPlan.end());
  RegisterId const updateRegisterId = it->second.registerId;
  
  bool const producesOutput = (ep->_outVariableNew != nullptr);
  
  // initialize an empty edge container
  TRI_document_edge_t edge = { 0, nullptr, 0, nullptr };

  std::string from;
  std::string to;

  TRI_doc_mptr_copy_t nptr;
  std::string errorMessage;

  auto trxCollection = _trx->trxCollection(_collection->cid());
  bool const isEdgeCollection = _collection->isEdgeCollection();

  result.reset(new AqlItemBlock(count, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  if (ep->_outVariableNew != nullptr) {
    result->setDocumentCollection(_outRegNew, trxCollection->_collection->_collection);
  }
         
  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);   // This is intentionally a copy!
    auto document = res->getDocumentCollection(docRegisterId);

    throwIfKilled(); // check if we were aborted

    decltype(document) keyDocument = res->getDocumentCollection(docRegisterId);
    decltype(document) updateDocument = res->getDocumentCollection(updateRegisterId);
    decltype(document) insertDocument = res->getDocumentCollection(insertRegisterId);

    size_t const n = res->size();
    
    // loop over the complete block
    for (size_t i = 0; i < n; ++i) {
      AqlValue a = res->getValue(i, docRegisterId);

      // only copy 1st row of registers inherited from previous frame(s)
      inheritRegisters(res, result.get(), i, dstRow);

      std::string key;
        
      int errorCode = TRI_ERROR_NO_ERROR;

      if (a.isObject()) {

        // old document present => update case
        errorCode = extractKey(a, keyDocument, key);

        if (errorCode == TRI_ERROR_NO_ERROR) {
          AqlValue updateDoc = res->getValue(i, updateRegisterId);

          if (updateDoc.isObject()) {
            auto const updateJson = updateDoc.toJson(_trx, updateDocument, false);
            auto searchJson = a.toJson(_trx, keyDocument, true);

            if (! searchJson.isObject()) {
              errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
            }

            if (errorCode == TRI_ERROR_NO_ERROR && 
                updateJson.isObject()) {
              TRI_doc_mptr_copy_t mptr;
              
              // use default value 
              errorCode = TRI_ERROR_OUT_OF_MEMORY;
              bool wasEmpty = false;

              // check for shard key change
              if (_isDBServer && 
                  isShardKeyChange(searchJson.json(), updateJson.json(), ! ep->_isReplace)) {
                // a shard key value has changed. this is not allowed!
                errorCode = TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES;
              }
              else if (ep->_isReplace) {
                // replace

                errorCode = _trx->update(trxCollection, key, 0, &mptr, updateJson.json(), TRI_DOC_UPDATE_LAST_WRITE, 0, nullptr, ep->_options.waitForSync);
              }
              else {
                // update
                if (updateJson.members() == 0) {
                  // empty object. nothing to do
                  errorCode = TRI_ERROR_NO_ERROR;

                  if (producesOutput) {
                    // copy OLD into NEW
                    result->setValue(dstRow,
                                      _outRegNew,
                                      AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, searchJson.steal())));
                    wasEmpty = true;
                  }
                }
                else {
                  std::unique_ptr<TRI_json_t> mergedJson(TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, searchJson.json(), updateJson.json(), ep->_options.nullMeansRemove, ep->_options.mergeObjects));
              
                  if (mergedJson.get() != nullptr) {
                    // all exceptions are caught in _trx->update()
                    errorCode = _trx->update(trxCollection, key, 0, &mptr, mergedJson.get(), TRI_DOC_UPDATE_LAST_WRITE, 0, nullptr, ep->_options.waitForSync);
                  }
                }
              }

              if (producesOutput && 
                  errorCode == TRI_ERROR_NO_ERROR && 
                  ! wasEmpty) {
                // store $NEW
                result->setValue(dstRow,
                                  _outRegNew,
                                  AqlValue(reinterpret_cast<TRI_df_marker_t const*>(mptr.getDataPtr())));
              }
            }
          }
          else {
            errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
          }
        }

      }
      else {
        // no document found => insert case
        AqlValue insertDoc = res->getValue(i, insertRegisterId);

        if (insertDoc.isObject()) {
          if (isEdgeCollection) {
            // array must have _from and _to attributes
            Json member(insertDoc.extractObjectMember(_trx, insertDocument, TRI_VOC_ATTRIBUTE_FROM, false, _buffer));
            TRI_json_t const* json = member.json();

            if (TRI_IsStringJson(json)) {
              errorCode = resolve(json->_value._string.data, edge._fromCid, from);
            }
            else {
              errorCode = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
            }
          
            if (errorCode == TRI_ERROR_NO_ERROR) { 
              Json member(insertDoc.extractObjectMember(_trx, document, TRI_VOC_ATTRIBUTE_TO, false, _buffer));
              json = member.json();
              if (TRI_IsStringJson(json)) {
                errorCode = resolve(json->_value._string.data, edge._toCid, to);
              }
              else {
                errorCode = TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
              }
            }
          }

          if (errorCode == TRI_ERROR_NO_ERROR) {
            auto const insertJson = insertDoc.toJson(_trx, insertDocument, true);

            // use default value
            errorCode = TRI_ERROR_OUT_OF_MEMORY;

            if (insertJson.isObject()) {
              // now insert
              if (_isDBServer && isShardKeyError(insertJson.json())) {
                errorCode = TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY;
              }
              else {
                TRI_doc_mptr_copy_t mptr;

                if (isEdgeCollection) {
                  // edge
                  edge._fromKey = (TRI_voc_key_t) from.c_str();
                  edge._toKey = (TRI_voc_key_t) to.c_str();
                  errorCode = _trx->create(trxCollection, &mptr, insertJson.json(), &edge, ep->_options.waitForSync);
                }
                else {
                  // document
                  errorCode = _trx->create(trxCollection, &mptr, insertJson.json(), nullptr, ep->_options.waitForSync);
                }
          
                if (producesOutput && errorCode == TRI_ERROR_NO_ERROR) {
                  result->setValue(dstRow,
                                    _outRegNew,
                                    AqlValue(reinterpret_cast<TRI_df_marker_t const*>(mptr.getDataPtr())));
                }
              }
            }
          }

        }
        else {
          errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
        }

      }

      handleResult(errorCode, ep->_options.ignoreErrors, &errorMessage);
      ++dstRow; 
    }
    // done with a block

    // now free it already
    (*it) = nullptr;  
    delete res;
  }

  return result.release();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                class ReplaceBlock
// -----------------------------------------------------------------------------

ReplaceBlock::ReplaceBlock (ExecutionEngine* engine,
                            ReplaceNode const* ep)
  : ModificationBlock(engine, ep) {
}

ReplaceBlock::~ReplaceBlock () {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual work horse for replacing data
////////////////////////////////////////////////////////////////////////////////

AqlItemBlock* ReplaceBlock::work (std::vector<AqlItemBlock*>& blocks) {
  size_t const count = countBlocksRows(blocks);

  if (count == 0) {
    return nullptr;
  }
 
  std::unique_ptr<AqlItemBlock> result;
  auto ep = static_cast<ReplaceNode const*>(getPlanNode());
  auto it = ep->getRegisterPlan()->varInfo.find(ep->_inDocVariable->id);
  TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
  RegisterId const registerId = it->second.registerId;
  RegisterId keyRegisterId = 0; // default initialization
  
  TRI_doc_mptr_copy_t nptr;
          
  bool const ignoreDocumentNotFound = ep->getOptions().ignoreDocumentNotFound;
  bool const hasKeyVariable = (ep->_inKeyVariable != nullptr);
  
  if (hasKeyVariable) {
    it = ep->getRegisterPlan()->varInfo.find(ep->_inKeyVariable->id);
    TRI_ASSERT(it != ep->getRegisterPlan()->varInfo.end());
    keyRegisterId = it->second.registerId;
  }

  auto trxCollection = _trx->trxCollection(_collection->cid());

  result.reset(new AqlItemBlock(count, getPlanNode()->getRegisterPlan()->nrRegs[getPlanNode()->getDepth()]));

  if (ep->_outVariableOld != nullptr) {
    result->setDocumentCollection(_outRegOld, trxCollection->_collection->_collection);
  }
  if (ep->_outVariableNew != nullptr) {
    result->setDocumentCollection(_outRegNew, trxCollection->_collection->_collection);
  }

  // loop over all blocks
  size_t dstRow = 0;
  for (auto it = blocks.begin(); it != blocks.end(); ++it) {
    auto* res = (*it);  // This is intentionally a copy
    auto document = res->getDocumentCollection(registerId);
    decltype(document) keyDocument = nullptr;

    if (hasKeyVariable) {
      keyDocument = res->getDocumentCollection(keyRegisterId);
    }
    
    throwIfKilled(); // check if we were aborted
      
    size_t const n = res->size();
    
    // loop over the complete block
    for (size_t i = 0; i < n; ++i) {
      AqlValue a = res->getValue(i, registerId);

      int errorCode = TRI_ERROR_NO_ERROR;
      int readErrorCode = TRI_ERROR_NO_ERROR;
      std::string key;

      if (a.isObject()) {
        // value is an object
        if (hasKeyVariable) {
          // seperate key specification
          AqlValue k = res->getValue(i, keyRegisterId);
          errorCode = extractKey(k, keyDocument, key);
        }
        else {
          errorCode = extractKey(a, document, key);
        }
      }
      else {
        errorCode = TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
      }

      if (errorCode == TRI_ERROR_NO_ERROR && (ep->_outVariableOld != nullptr || _isDBServer)) {
        if (! hasKeyVariable && a.isShaped()) {
          // "old" is already ShapedJson. no need to fetch the old document first
          constructMptr(&nptr, a.getMarker());
        }
        else {
          // "old" is no ShapedJson. now fetch old version from database
          readErrorCode = _trx->readSingle(trxCollection, &nptr, key);
        }
      }

      if (errorCode == TRI_ERROR_NO_ERROR) {
        TRI_doc_mptr_copy_t mptr;
        auto const json = a.toJson(_trx, document, true);
              
        if (_isDBServer) {
          TRI_shaped_json_t shapedJson;
          TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, nptr.getDataPtr()); // PROTECTED by trx here
          std::unique_ptr<TRI_json_t> old(TRI_JsonShapedJson(_collection->documentCollection()->getShaper(), &shapedJson));

          if (isShardKeyChange(old.get(), json.json(), false)) {
            errorCode = TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES;
          }
        }

        if (errorCode == TRI_ERROR_NO_ERROR) {
          // all exceptions are caught in _trx->update()
          errorCode = _trx->update(trxCollection, key, 0, &mptr, json.json(), TRI_DOC_UPDATE_LAST_WRITE, 0, nullptr, ep->_options.waitForSync);
        }

        if (errorCode == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && _isDBServer) { 
          if (ignoreDocumentNotFound) {
            // Note that this is coded here for the sake of completeness, 
            // but it will intentionally never happen, since this flag is
            // not set in the REPLACE case, because we will always use
            // a DistributeNode rather than a ScatterNode:
            errorCode = TRI_ERROR_NO_ERROR;
          }
          else {
            errorCode = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND_OR_SHARDING_ATTRIBUTES_CHANGED;
          }
        }

        if (errorCode == TRI_ERROR_NO_ERROR &&
            readErrorCode == TRI_ERROR_NO_ERROR) {

          if (ep->_outVariableOld != nullptr) {
            result->setValue(dstRow,
                             _outRegOld,
                             AqlValue(reinterpret_cast<TRI_df_marker_t const*>(nptr.getDataPtr())));
          }

          if (ep->_outVariableNew != nullptr) {
            result->setValue(dstRow,
                             _outRegNew,
                             AqlValue(reinterpret_cast<TRI_df_marker_t const*>(mptr.getDataPtr())));
          }
        }
      }

      handleResult(errorCode, ep->_options.ignoreErrors); 
      ++dstRow;
    }
    // done with a block

    // now free it already
    (*it) = nullptr;  
    delete res;
  }

  return result.release();
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
