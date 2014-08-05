////////////////////////////////////////////////////////////////////////////////
/// @brief Infrastructure for ExecutionBlocks (the execution engine)
///
/// @file arangod/Aql/ExecutionBlock.h
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_EXECUTION_BLOCK_H
#define ARANGODB_AQL_EXECUTION_BLOCK_H 1

#include <Basics/JsonHelper.h>
#include <ShapedJson/shaped-json.h>

#include "Aql/Types.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode.h"
#include "Utils/AqlTransaction.h"
#include "Utils/transactions.h"
#include "Utils/V8TransactionContext.h"

struct TRI_json_s;

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                    ExecutionBlock
// -----------------------------------------------------------------------------

    class ExecutionBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ExecutionBlock (AQL_TRANSACTION_V8* trx,
                        ExecutionNode const* ep)
          : _trx(trx), _exeNode(ep), _done(false), _depth(0), 
            _varOverview(nullptr) { 
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        virtual ~ExecutionBlock ();
          
////////////////////////////////////////////////////////////////////////////////
/// @brief add a dependency
////////////////////////////////////////////////////////////////////////////////

        void addDependency (ExecutionBlock* ep) {
          _dependencies.push_back(ep);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get all dependencies
////////////////////////////////////////////////////////////////////////////////

        vector<ExecutionBlock*> getDependencies () {
          return _dependencies;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a dependency, returns true if the pointer was found and 
/// removed, please note that this does not delete ep!
////////////////////////////////////////////////////////////////////////////////

        bool removeDependency (ExecutionBlock* ep) {
          auto it = _dependencies.begin();
          while (it != _dependencies.end()) {
            if (*it == ep) {
              _dependencies.erase(it);
              return true;
            }
            ++it;
          }
          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief access the pos-th dependency
////////////////////////////////////////////////////////////////////////////////

        ExecutionBlock* operator[] (size_t pos) {
          if (pos > _dependencies.size()) {
            return nullptr;
          }
          else {
            return _dependencies.at(pos);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief functionality to walk an execution block recursively
////////////////////////////////////////////////////////////////////////////////

        class WalkerWorker {
          public:
            WalkerWorker () {};
            virtual ~WalkerWorker () {};
            virtual void before (ExecutionBlock* eb) {};
            virtual void after (ExecutionBlock* eb) {};
            virtual bool enterSubquery (ExecutionBlock* super, 
                                        ExecutionBlock* sub) {
              return true;  // indicates that we enter the subquery
            };
            virtual void leaveSubquery (ExecutionBlock* super,
                                        ExecutionBlock* sub) {};
            bool done (ExecutionBlock* eb) {
              if (_done.find(eb) == _done.end()) {
                _done.insert(eb);
                return false;
              }
              else {
                return true;
              }
            }
            void reset () {
              _done.clear();
            }
          private:
            std::unordered_set<ExecutionBlock*> _done;
        };

        void walk (WalkerWorker* worker);

////////////////////////////////////////////////////////////////////////////////
/// @brief static analysis, walker class and information collector
////////////////////////////////////////////////////////////////////////////////

        struct VarInfo {
          unsigned int depth;
          RegisterId registerId;
          VarInfo(int depth, int registerId) : depth(depth), registerId(registerId) {}
        };

        struct VarOverview : public WalkerWorker {
          // The following are collected for global usage in the ExecutionBlock:

          // map VariableIds to their depth and registerId:
          std::unordered_map<VariableId, VarInfo> varInfo;

          // number of variables in the frame of the current depth:
          std::vector<RegisterId>                 nrRegsHere;

          // number of variables in this and all outer frames together,
          // the entry with index i here is always the sum of all values
          // in nrRegsHere from index 0 to i (inclusively) and the two
          // have the same length:
          std::vector<RegisterId>                 nrRegs;

          // Local for the walk:
          unsigned int depth;
          unsigned int totalNrRegs;

          // This is used to tell all Blocks and share a pointer to ourselves
          shared_ptr<VarOverview>* me;

          VarOverview () 
            : depth(0), totalNrRegs(0), me(nullptr) {
            nrRegsHere.push_back(0);
            nrRegs.push_back(0);
          };

          void setSharedPtr (shared_ptr<VarOverview>* shared) {
            me = shared;
          }

          // Copy constructor used for a subquery:
          VarOverview (VarOverview const& v) 
            : varInfo(v.varInfo), nrRegsHere(v.nrRegs), nrRegs(v.nrRegs),
              depth(v.depth+1), totalNrRegs(v.totalNrRegs), me(nullptr) {
            nrRegsHere.push_back(0);
            nrRegs.push_back(0);
          }

          ~VarOverview () {};

          virtual bool enterSubquery (ExecutionBlock* super,
                                      ExecutionBlock* sub) {
            shared_ptr<VarOverview> vv(new VarOverview(*this));
            vv->setSharedPtr(&vv);
            sub->walk(vv.get());
            vv->reset();
            return false;  // do not walk into subquery
          }

          virtual void after (ExecutionBlock *eb) {
            switch (eb->getPlanNode()->getType()) {
              case ExecutionNode::ENUMERATE_COLLECTION: {
                depth++;
                nrRegsHere.push_back(1);
                nrRegs.push_back(1 + nrRegs.back());
                auto ep = static_cast<EnumerateCollectionNode const*>(eb->getPlanNode());
                varInfo.insert(make_pair(ep->_outVariable->id,
                                         VarInfo(depth, totalNrRegs)));
                totalNrRegs++;
                break;
              }
              case ExecutionNode::ENUMERATE_LIST: {
                depth++;
                nrRegsHere.push_back(1);
                nrRegs.push_back(1 + nrRegs.back());
                auto ep = static_cast<EnumerateListNode const*>(eb->getPlanNode());
                varInfo.insert(make_pair(ep->_outVariable->id,
                                         VarInfo(depth, totalNrRegs)));
                totalNrRegs++;
                break;
              }
              case ExecutionNode::CALCULATION: {
                nrRegsHere[depth]++;
                nrRegs[depth]++;
                auto ep = static_cast<CalculationNode const*>(eb->getPlanNode());
                varInfo.insert(make_pair(ep->_outVariable->id,
                                         VarInfo(depth, totalNrRegs)));
                totalNrRegs++;
                break;
              }
              case ExecutionNode::PROJECTION: {
                nrRegsHere[depth]++;
                nrRegs[depth]++;
                auto ep = static_cast<ProjectionNode const*>(eb->getPlanNode());
                varInfo.insert(make_pair(ep->_outVariable->id,
                                         VarInfo(depth, totalNrRegs)));
                totalNrRegs++;
                break;
              }
              case ExecutionNode::SUBQUERY: {
                nrRegsHere[depth]++;
                nrRegs[depth]++;
                auto ep = static_cast<SubqueryNode const*>(eb->getPlanNode());
                varInfo.insert(make_pair(ep->_outVariable->id,
                                         VarInfo(depth, totalNrRegs)));
                totalNrRegs++;
                break;
              }
              // TODO: potentially more cases
              default:
                break;
            }
            eb->_depth = depth;
            eb->_varOverview = *me;
          }

        };

        void staticAnalysis () {
          shared_ptr<VarOverview> v(new VarOverview());
          v->setSharedPtr(&v);
          walk(v.get());
          v->reset();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief Methods for execution
/// Lifecycle is:
///    staticAnalysis()
///    initialize()
///    possibly repeat many times:
///      bind(...)
///      execute(...)
///      // use cursor functionality
///    shutdown()
/// It should be possible to perform the sequence from initialize to shutdown
/// multiple times.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

        virtual int initialize () {
          int res;
          for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
            res = (*it)->initialize();
            if (res != TRI_ERROR_NO_ERROR) {
              return res;
            }
          }
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief bind
////////////////////////////////////////////////////////////////////////////////

        virtual int bind (std::map<std::string, struct TRI_json_s*>* params);

////////////////////////////////////////////////////////////////////////////////
/// @brief getParameters
////////////////////////////////////////////////////////////////////////////////

        std::map<std::string, struct TRI_json_s*>* getParameters ();

////////////////////////////////////////////////////////////////////////////////
/// @brief execute
////////////////////////////////////////////////////////////////////////////////

        virtual int execute () {
          int res;
          for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
            res = (*it)->execute();
            if (res != TRI_ERROR_NO_ERROR) {
              return res;
            }
          }
          _done = false;
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown
////////////////////////////////////////////////////////////////////////////////

        virtual int shutdown () {
          int res;
          for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
            res = (*it)->shutdown();
            if (res != TRI_ERROR_NO_ERROR) {
              return res;
            }
          }
          for (auto it = _buffer.begin(); it != _buffer.end(); ++it) {
            delete *it;
          }
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the following is internal to pull one more block and append it to
/// our _buffer deque. Returns true if a new block was appended and false if
/// the dependent node is exhausted.
////////////////////////////////////////////////////////////////////////////////

      protected:

        bool getBlock (size_t atLeast, size_t atMost) {
          AqlItemBlock* docs = _dependencies[0]->getSome(atLeast, atMost);
          if (docs == nullptr) {
            return false;
          }
          _buffer.push_back(docs);
          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getOne, gets one more document
////////////////////////////////////////////////////////////////////////////////

      public:

        virtual AqlItemBlock* getOne () {
          if (_done) {
            return nullptr;
          }

          if (_buffer.empty()) {
            if (! getBlock(1, DefaultBatchSize)) {
              _done = true;
              return nullptr;
            }
            _pos = 0;
          }

          TRI_ASSERT(! _buffer.empty());

          // Here, _buffer.size() is > 0 and _pos points to a valid place
          // in it.

          AqlItemBlock* res;
          AqlItemBlock* cur;
          cur = _buffer.front();
          res = cur->slice(_pos, _pos + 1);
          _pos++;
          if (_pos >= cur->size()) {
            _buffer.pop_front();
            delete cur;
            _pos = 0;
          }
          return res;
        }

        virtual AqlItemBlock* getSome (size_t atLeast, 
                                       size_t atMost) {
          if (_done) {
            return nullptr;
          }

          // Here, if _buffer.size() is > 0 then _pos points to a valid place
          // in it.
          size_t total = 0;
          vector<AqlItemBlock*> collector;
          AqlItemBlock* res;
          while (total < atLeast) {
            if (_buffer.empty()) {
              if (! getBlock(atLeast - total, atMost - total)) {
                _done = true;
                break;
              }
              _pos = 0;
            }
            AqlItemBlock* cur = _buffer.front();
            if (cur->size() - _pos + total > atMost) {
              // The current block is too large for atMost:
              collector.push_back(cur->slice(_pos, _pos + (atMost - total)));
              _pos += atMost - total;
              total = atMost;
            }
            else if (_pos > 0) {
              // The current block fits into our result, but it is already
              // half-eaten:
              collector.push_back(cur->slice(_pos, cur->size()));
              total += cur->size() - _pos;
              delete cur;
              _buffer.pop_front();
              _pos = 0;
            } 
            else {
              // The current block fits into our result and is fresh:
              collector.push_back(cur);
              total += cur->size();
              _buffer.pop_front();
              _pos = 0;
            }
          }
          if (collector.empty()) {
            return nullptr;
          }
          else if (collector.size() == 1) {
            return collector[0];
          }
          else {
            res = AqlItemBlock::splice(collector);
            for (auto it = collector.begin(); 
                 it != collector.end(); ++it) {
              delete (*it);
            }
            return res;
          }
        }

        virtual bool hasMore () {
          if (_done) {
            return false;
          }
          if (! _buffer.empty()) {
            return true;
          }
          if (getBlock(DefaultBatchSize, DefaultBatchSize)) {
            return true;
          }

          return ! _done;
        }

        virtual bool skip (size_t number) {
          if (_done) {
            return false;
          }
          
          // Here, if _buffer.size() is > 0 then _pos points to a valid place
          // in it.

          size_t skipped = 0;
          vector<AqlItemBlock*> collector;
          while (skipped < number) {
            if (_buffer.empty()) {
              if (! getBlock(number - skipped, number - skipped)) {
                _done = true;
                return false;
              }
              _pos = 0;
            }
            AqlItemBlock* cur = _buffer.front();
            if (cur->size() - _pos + skipped > number) {
              // The current block is too large:
              _pos += number - skipped;
              skipped = number;
            }
            else {
              // The current block fits into our result, but it might be
              // half-eaten already:
              skipped += cur->size() - _pos;
              delete cur;
              _buffer.pop_front();
              _pos = 0;
            } 
          }

          // When we get here, we have skipped enough documents and
          // kept our data structures OK.
          // If _buffer.size() == 0, we have to try to get another block
          // just to be sure to get the return value right:
          if (! _buffer.empty()) {
            return true;
          }

          if (! getBlock(DefaultBatchSize, DefaultBatchSize)) {
            _done = true;
            return false;
          }
          return true;
        }

        virtual int64_t count () {
          return _dependencies[0]->count();
        }

        virtual int64_t remaining () {
          int64_t sum = 0;
          for (auto it = _buffer.begin(); it != _buffer.end(); ++it) {
            sum += (*it)->size();
          }
          return sum + _dependencies[0]->remaining();
        }

        ExecutionNode const* getPlanNode () {
          return _exeNode;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the transaction for this query
////////////////////////////////////////////////////////////////////////////////

      protected:

        AQL_TRANSACTION_V8* _trx;

////////////////////////////////////////////////////////////////////////////////
/// @brief our corresponding ExecutionNode node
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode const* _exeNode;

////////////////////////////////////////////////////////////////////////////////
/// @brief our dependent nodes
////////////////////////////////////////////////////////////////////////////////

        std::vector<ExecutionBlock*> _dependencies;

////////////////////////////////////////////////////////////////////////////////
/// @brief this is our buffer for the items, it is a deque of AqlItemBlocks.
/// We keep the following invariant between this and the other two variables
/// _pos and _done: If _buffer.size() != 0, then 0 <= _pos < _buffer[0]->size()
/// and _buffer[0][_pos] is the next item to be handed on. If _done is true,
/// then no more documents will ever be returned. _done will be set to
/// true if and only if we have no more data ourselves (i.e. _buffer.size()==0)
/// and we have unsuccessfully tried to get another block from our dependency.
////////////////////////////////////////////////////////////////////////////////

        std::deque<AqlItemBlock*> _buffer;

////////////////////////////////////////////////////////////////////////////////
/// @brief current working position in the first entry of _buffer
////////////////////////////////////////////////////////////////////////////////

        size_t _pos;

////////////////////////////////////////////////////////////////////////////////
/// @brief if this is set, we are done, this is reset to false by execute()
////////////////////////////////////////////////////////////////////////////////

        bool _done;

////////////////////////////////////////////////////////////////////////////////
/// @brief depth of frames (number of FOR statements here or above)
////////////////////////////////////////////////////////////////////////////////

        int _depth;  // will be filled in by staticAnalysis

////////////////////////////////////////////////////////////////////////////////
/// @brief info about variables, filled in by staticAnalysis
////////////////////////////////////////////////////////////////////////////////

        std::shared_ptr<VarOverview> _varOverview;

////////////////////////////////////////////////////////////////////////////////
/// @brief batch size value
////////////////////////////////////////////////////////////////////////////////
  
        static size_t const DefaultBatchSize = 1000;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                    SingletonBlock
// -----------------------------------------------------------------------------

    class SingletonBlock : public ExecutionBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        SingletonBlock (AQL_TRANSACTION_V8* trx, SingletonNode const* ep)
          : ExecutionBlock(trx, ep) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~SingletonBlock () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize, just call base class
////////////////////////////////////////////////////////////////////////////////

        int initialize () {
          ExecutionBlock::initialize();
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute, just call base class
////////////////////////////////////////////////////////////////////////////////

        int execute () {
          ExecutionBlock::execute();
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown, just call base class
////////////////////////////////////////////////////////////////////////////////

        int shutdown () {
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getOne
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* getOne () {
          if (_done) {
            return nullptr;
          }

          AqlItemBlock* res(new AqlItemBlock(1, _varOverview->nrRegs[_depth]));
          _done = true;
          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* getSome (size_t atLeast, size_t atMost) {
          if (_done) {
            return nullptr;
          }

          AqlItemBlock* res(new AqlItemBlock(1, _varOverview->nrRegs[_depth]));
          _done = true;
          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief hasMore
////////////////////////////////////////////////////////////////////////////////

        bool hasMore () {
          return ! _done;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief skip
////////////////////////////////////////////////////////////////////////////////

        bool skip (size_t number) {
          if (_done) {
            return true;
          }
          if (number > 0) {
            _done = true;
          }
          return _done;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief count
////////////////////////////////////////////////////////////////////////////////

        int64_t count () {
          return 1;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief remaining
////////////////////////////////////////////////////////////////////////////////

        int64_t remaining () {
          return _done ? 0 : 1;
        }

    };


// -----------------------------------------------------------------------------
// --SECTION--                                          EnumerateCollectionBlock
// -----------------------------------------------------------------------------

    class EnumerateCollectionBlock : public ExecutionBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        EnumerateCollectionBlock (AQL_TRANSACTION_V8* trx,
                                  EnumerateCollectionNode const* ep) 
          : ExecutionBlock(trx, ep), _trx(nullptr), _posInAllDocs(0) {
          
          auto p = reinterpret_cast<EnumerateCollectionNode const*>(_exeNode);

          // create an embedded transaction for the collection access
          _trx = new triagens::arango::SingleCollectionReadOnlyTransaction<triagens::arango::V8TransactionContext<true>>(p->_vocbase, p->_collname);

          int res = _trx->begin();

          if (res != TRI_ERROR_NO_ERROR) {
            // transaction failure
            delete _trx;
            THROW_ARANGO_EXCEPTION(res);
          }

        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~EnumerateCollectionBlock () {
          if (_trx != nullptr) {
            // finalize our own transaction
            delete _trx;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize fetching of documents
////////////////////////////////////////////////////////////////////////////////

        void initDocuments () {
          _internalSkip = 0;
          if (! moreDocuments()) {
            _done = true;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief continue fetching of documents
////////////////////////////////////////////////////////////////////////////////

        bool moreDocuments () {
          if (_documents.empty()) {
            _documents.reserve(DefaultBatchSize);
          }

          _documents.clear();

          int res = _trx->readOffset(_documents, 
                                     _internalSkip, 
                                     static_cast<TRI_voc_size_t>(DefaultBatchSize), 
                                     0, 
                                     TRI_QRY_NO_LIMIT, 
                                     &_totalCount);

          if (res != TRI_ERROR_NO_ERROR) {
            THROW_ARANGO_EXCEPTION(res);
          }

          return (! _documents.empty());
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief initialize, here we fetch all docs from the database
////////////////////////////////////////////////////////////////////////////////

        int initialize () {
          int res = ExecutionBlock::initialize();

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          auto p = reinterpret_cast<EnumerateCollectionNode const*>(_exeNode);

          std::string collectionName(p->_collname);
          collectionName.push_back('/');

          initDocuments();

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute, here we release our docs from this collection
////////////////////////////////////////////////////////////////////////////////

        int execute () {
          int res = ExecutionBlock::execute();

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          if (_totalCount == 0) {
            _done = true;
          }

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown, here we release our docs from this collection
////////////////////////////////////////////////////////////////////////////////

        int shutdown () {
          int res = ExecutionBlock::shutdown();  // Tell all dependencies

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getOne
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* getOne () {
          if (_done) {
            return nullptr;
          }

          if (_buffer.empty()) {
            if (! ExecutionBlock::getBlock(1, DefaultBatchSize)) {
              _done = true;
              return nullptr;
            }
            _pos = 0;           // this is in the first block
            _posInAllDocs = 0;  // Note that we know _allDocs.size() > 0,
                                // otherwise _done would be true already
          }

          // If we get here, we do have _buffer.front()
          AqlItemBlock* cur = _buffer.front();

          // Copy stuff from frames above:
          auto res = new AqlItemBlock(1, _varOverview->nrRegs[_depth]);
          TRI_ASSERT(cur->getNrRegs() <= res->getNrRegs());
          for (RegisterId i = 0; i < cur->getNrRegs(); i++) {
            res->setValue(0, i, cur->getValue(_pos, i).clone());
          }

          // The result is in the first variable of this depth,
          // we do not need to do a lookup in _varOverview->varInfo,
          // but can just take cur->getNrRegs() as registerId:
          res->setValue(0, cur->getNrRegs(), AqlValue(reinterpret_cast<TRI_df_marker_t const*>(_documents[_posInAllDocs++].getDataPtr())));
          res->getDocumentCollections().at(cur->getNrRegs()) 
            = _trx->documentCollection();

          // Advance read position:
          if (_posInAllDocs >= _documents.size()) {
            // we have exhausted our local documents buffer
            _posInAllDocs = 0;
            
            // fetch more documents into our buffer
            if (! moreDocuments()) {
              initDocuments();
              if (++_pos >= cur->size()) {
                _buffer.pop_front();
                delete cur;
                _pos = 0;
              }
            }
          }
          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* getSome (size_t atLeast, size_t atMost) {
          if (_done) {
            return nullptr;
          }

          if (_buffer.empty()) {
            if (! ExecutionBlock::getBlock(DefaultBatchSize, DefaultBatchSize)) {
              _done = true;
              return nullptr;
            }
            _pos = 0;           // this is in the first block
            _posInAllDocs = 0;  // Note that we know _allDocs.size() > 0,
                                // otherwise _done would be true already
          }

          // If we get here, we do have _buffer.front()
          AqlItemBlock* cur = _buffer.front();
          size_t const curRegs = cur->getNrRegs();

          size_t available = _documents.size() - _posInAllDocs;
          size_t toSend = std::min(atMost, available);
            
          auto res = new AqlItemBlock(toSend, _varOverview->nrRegs[_depth]);
          TRI_ASSERT(curRegs <= res->getNrRegs());

          // only copy 1st row of registers inherited from previous frame(s)
          for (RegisterId i = 0; i < curRegs; i++) {
            res->setValue(0, i, cur->getValue(_pos, i).clone());
          }
          res->getDocumentCollections().at(curRegs)
            = _trx->documentCollection();

          for (size_t j = 0; j < toSend; j++) {
            if (j > 0) {
              // re-use already copied aqlvalues
              for (RegisterId i = 0; i < curRegs; i++) {
                res->setValue(j, i, res->getValue(0, i));
              }
            }

            // The result is in the first variable of this depth,
            // we do not need to do a lookup in _varOverview->varInfo,
            // but can just take cur->getNrRegs() as registerId:
            res->setValue(j, curRegs, AqlValue(reinterpret_cast<TRI_df_marker_t const*>(_documents[_posInAllDocs++].getDataPtr())));
          }

          // Advance read position:
          if (_posInAllDocs >= _documents.size()) {
            // we have exhausted our local documents buffer
            _posInAllDocs = 0;

            // fetch more documents into our buffer
            if (! moreDocuments()) {
              // nothing more to read, re-initialize fetching of documents
              initDocuments();
              if (++_pos >= cur->size()) {
                _buffer.pop_front();
                delete cur;
                _pos = 0;
              }
            }
          }
          return res;
        }

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief currently ongoing transaction
////////////////////////////////////////////////////////////////////////////////
                                  
        triagens::arango::SingleCollectionReadOnlyTransaction<triagens::arango::V8TransactionContext<true>>* _trx;

////////////////////////////////////////////////////////////////////////////////
/// @brief total number of documents in the collection
////////////////////////////////////////////////////////////////////////////////
  
        uint32_t _totalCount;

////////////////////////////////////////////////////////////////////////////////
/// @brief internal skip value
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_size_t _internalSkip;

////////////////////////////////////////////////////////////////////////////////
/// @brief document buffer
////////////////////////////////////////////////////////////////////////////////

        std::vector<TRI_doc_mptr_copy_t> _documents;

////////////////////////////////////////////////////////////////////////////////
/// @brief current position in _allDocs
////////////////////////////////////////////////////////////////////////////////

        size_t _posInAllDocs;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                  CalculationBlock
// -----------------------------------------------------------------------------

    class CalculationBlock : public ExecutionBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        CalculationBlock (AQL_TRANSACTION_V8* trx,
                          CalculationNode const* en)
          : ExecutionBlock(trx, en), _expression(en->expression()), _outReg(0) {

        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~CalculationBlock () {
        } 

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

        int initialize () {
          int res = ExecutionBlock::initialize();
          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          // We know that staticAnalysis has been run, so _varOverview is set up

          auto en = static_cast<CalculationNode const*>(getPlanNode());

          std::unordered_set<Variable*> inVars = _expression->variables();

          for (auto it = inVars.begin(); it != inVars.end(); ++it) {
            _inVars.push_back(*it);
            auto it2 = _varOverview->varInfo.find((*it)->id);
            TRI_ASSERT(it2 != _varOverview->varInfo.end());
            _inRegs.push_back(it2->second.registerId);
          }

          auto it3 = _varOverview->varInfo.find(en->_outVariable->id);
          TRI_ASSERT(it3 != _varOverview->varInfo.end());
          _outReg = it3->second.registerId;
          
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief doEvaluation, private helper to do the work
////////////////////////////////////////////////////////////////////////////////

      private:

        void doEvaluation (AqlItemBlock* result) {
          TRI_ASSERT(result != nullptr);

          vector<AqlValue>& data(result->getData());
          vector<TRI_document_collection_t const*> 
              docColls(result->getDocumentCollections());

          RegisterId nrRegs = result->getNrRegs();

          for (size_t i = 0; i < result->size(); i++) {
            AqlValue res = _expression->execute(_trx, docColls, data, nrRegs * i,
                                                _inVars, _inRegs);
            result->setValue(i, _outReg, res);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getOne
////////////////////////////////////////////////////////////////////////////////

      public:

        virtual AqlItemBlock* getOne () {
          AqlItemBlock* res = ExecutionBlock::getOne();

          if (res == nullptr) {
            return nullptr;
          }

          try {
            doEvaluation(res);
            return res;
          }
          catch (...) {
            delete res;
            throw;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

        virtual AqlItemBlock* getSome (size_t atLeast, 
                                       size_t atMost) {
          AqlItemBlock* res = ExecutionBlock::getSome(atLeast, atMost);
          
          if (res == nullptr) {
            return nullptr;
          }

          try {
            doEvaluation(res);
            return res;
          }
          catch (...) {
            delete res;
            throw;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief we hold a pointer to the expression in the plan
////////////////////////////////////////////////////////////////////////////////

      private:
        Expression* _expression;

////////////////////////////////////////////////////////////////////////////////
/// @brief info about input variables
////////////////////////////////////////////////////////////////////////////////

        std::vector<Variable*> _inVars;

////////////////////////////////////////////////////////////////////////////////
/// @brief info about input registers
////////////////////////////////////////////////////////////////////////////////

        std::vector<RegisterId> _inRegs;

////////////////////////////////////////////////////////////////////////////////
/// @brief output register
////////////////////////////////////////////////////////////////////////////////

        RegisterId _outReg;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                       FilterBlock
// -----------------------------------------------------------------------------

    class FilterBlock : public ExecutionBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        FilterBlock (AQL_TRANSACTION_V8* trx, FilterNode const* ep)
          : ExecutionBlock(trx, ep) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~FilterBlock () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

        int initialize () {
          int res = ExecutionBlock::initialize();
          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          // We know that staticAnalysis has been run, so _varOverview is set up
          
          auto en = static_cast<FilterNode const*>(getPlanNode());

          auto it = _varOverview->varInfo.find(en->_inVariable->id);
          TRI_ASSERT(it != _varOverview->varInfo.end());
          _inReg = it->second.registerId;
          
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to actually decide
////////////////////////////////////////////////////////////////////////////////

      private:

        bool takeItem (AqlItemBlock* items, size_t index) {
          AqlValue v = items->getValue(index, _inReg);
          return v.isTrue();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to get another block
////////////////////////////////////////////////////////////////////////////////

        bool getBlock (size_t atLeast, size_t atMost) {
          bool res;
          while (true) {  // will be left by break or return
            res = ExecutionBlock::getBlock(atLeast, atMost);
            if (res == false) {
              return false;
            }
            if (_buffer.size() > 1) {
              break;  // Already have a current block
            }
            // Now decide about these docs:
            _chosen.clear();
            AqlItemBlock* cur = _buffer.front();
            _chosen.reserve(cur->size());
            for (size_t i = 0; i < cur->size(); ++i) {
              if (takeItem(cur, i)) {
                _chosen.push_back(i);
              }
            }
            if (_chosen.size() > 0) {
              break;   // OK, there are some docs in the result
            }
            _buffer.pop_front();  // Block was useless, just try again
          }
          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getOne
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* getOne () {
          if (_done) {
            return nullptr;
          }

          if (_buffer.empty()) {
            if (! getBlock(1, DefaultBatchSize)) {
              _done = true;
              return nullptr;
            }
            _pos = 0;
          }

          TRI_ASSERT(_buffer.size() > 0);

          // Here, _buffer.size() is > 0 and _pos points to a valid place
          // in it.

          AqlItemBlock* res;
          AqlItemBlock* cur;
          cur = _buffer.front();
          res = cur->slice(_chosen[_pos], _chosen[_pos+ 1]);
          _pos++;
          if (_pos >= _chosen.size()) {
            _buffer.pop_front();
            delete cur;
            _pos = 0;
          }
          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* getSome (size_t atLeast, size_t atMost) {
          if (_done) {
            return nullptr;
          }

          // Here, if _buffer.size() is > 0 then _pos points to a valid place
          // in it.
          size_t total = 0;
          vector<AqlItemBlock*> collector;
          AqlItemBlock* res;
          while (total < atLeast) {
            if (_buffer.empty()) {
              if (! getBlock(atLeast - total, atMost - total)) {
                _done = true;
                break;
              }
              _pos = 0;
            }
            // If we get here, then _buffer.size() > 0 and _pos points to a
            // valid place in it.
            AqlItemBlock* cur = _buffer.front();
            if (_chosen.size() - _pos + total > atMost) {
              // The current block of chosen ones is too large for atMost:
              collector.push_back(cur->slice(_chosen, 
                                  _pos, _pos + (atMost - total)));
              _pos += atMost - total;
              total = atMost;
            }
            else if (_pos > 0 || _chosen.size() < cur->size()) {
              // The current block fits into our result, but it is already
              // half-eaten or needs to be copied anyway:
              collector.push_back(cur->slice(_chosen, _pos, _chosen.size()));
              total += _chosen.size() - _pos;
              delete cur;
              _buffer.pop_front();
              _chosen.clear();
              _pos = 0;
            } 
            else {
              // The current block fits into our result and is fresh and
              // takes them all, so we can just hand it on:
              collector.push_back(cur);
              total += cur->size();
              _buffer.pop_front();
              _chosen.clear();
              _pos = 0;
            }
          }
          if (collector.empty()) {
            return nullptr;
          }
          else if (collector.size() == 1) {
            return collector[0];
          }
          else {
            res = AqlItemBlock::splice(collector);
            for (auto it = collector.begin(); 
                 it != collector.end(); ++it) {
              delete (*it);
            }
            return res;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief hasMore
////////////////////////////////////////////////////////////////////////////////

        bool hasMore () {
          if (_done) {
            return false;
          }

          if (_buffer.empty()) {
            if (! getBlock(1, DefaultBatchSize)) {
              _done = true;
              return false;
            }
            _pos = 0;
          }

          TRI_ASSERT(! _buffer.empty());

          // Here, _buffer.size() is > 0 and _pos points to a valid place
          // in it.

          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief skip
////////////////////////////////////////////////////////////////////////////////

        bool skip (size_t number) {
          // FIXME: to be implemented
          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief count
////////////////////////////////////////////////////////////////////////////////

        int64_t count () {
          return -1;   // refuse to work
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief remaining
////////////////////////////////////////////////////////////////////////////////

        int64_t remaining () {
          return -1;   // refuse to work
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief input register
////////////////////////////////////////////////////////////////////////////////

      private:

        RegisterId _inReg;

////////////////////////////////////////////////////////////////////////////////
/// @brief vector of indices of those documents in the current block
/// that are chosen
////////////////////////////////////////////////////////////////////////////////

        vector<size_t> _chosen;

    };


// -----------------------------------------------------------------------------
// --SECTION--                                                       LimitBlock
// -----------------------------------------------------------------------------

    class LimitBlock : public ExecutionBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        LimitBlock (AQL_TRANSACTION_V8* trx, LimitNode const* ep)
          : ExecutionBlock(trx, ep), _offset(ep->_offset), _limit(ep->_limit),
            _state(0) {  // start in the beginning
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~LimitBlock () {
        } 

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

        int initialize () {
          int res = ExecutionBlock::initialize();
          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }
          _state = 0;
          _count = 0;
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getOne
////////////////////////////////////////////////////////////////////////////////

        virtual AqlItemBlock* getOne () {
          if (_state == 2) {
            return nullptr;
          }

          if (_state == 0) {
            if (_offset > 0) {
              ExecutionBlock::skip(_offset);
              _state = 1;
              _count = 0;
              if (_limit == 0) {
                _state = 2;
                return nullptr;
              }
            }
          }

          // If we get to here, _state == 1 and _count < _limit

          // Fetch one from above, possibly using our _buffer:
          auto res = ExecutionBlock::getOne();
          if (res == nullptr) {
            _state = 2;
            return res;
          }
          TRI_ASSERT(res->size() == 1);

          if (++_count >= _limit) {
            _state = 2;
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

        virtual AqlItemBlock* getSome (size_t atLeast, 
                                       size_t atMost) {
          if (_state == 2) {
            return nullptr;
          }

          if (_state == 0) {
            if (_offset > 0) {
              ExecutionBlock::skip(_offset);
              _state = 1;
              _count = 0;
              if (_limit == 0) {
                _state = 2;
                return nullptr;
              }
            }
          }

          // If we get to here, _state == 1 and _count < _limit

          if (atMost > _limit - _count) {
            atMost = _limit - _count;
            if (atLeast > atMost) {
              atLeast = atMost;
            }
          }

          auto res = ExecutionBlock::getSome(atLeast, atMost);
          if (res == nullptr) {
            return res;
          }
          _count += res->size();
          if (_count >= _limit) {
            _state = 2;
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief _offset
////////////////////////////////////////////////////////////////////////////////

        size_t _offset;

////////////////////////////////////////////////////////////////////////////////
/// @brief _limit
////////////////////////////////////////////////////////////////////////////////

        size_t _limit;

////////////////////////////////////////////////////////////////////////////////
/// @brief _count, number of items already handed on
////////////////////////////////////////////////////////////////////////////////

        size_t _count;

////////////////////////////////////////////////////////////////////////////////
/// @brief _state, 0 is beginning, 1 is after offset, 2 is done
////////////////////////////////////////////////////////////////////////////////

        int _state;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                       ReturnBlock
// -----------------------------------------------------------------------------

    class ReturnBlock : public ExecutionBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ReturnBlock (AQL_TRANSACTION_V8* trx, ReturnNode const* ep)
          : ExecutionBlock(trx, ep) {

        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~ReturnBlock () {
        } 

////////////////////////////////////////////////////////////////////////////////
/// @brief getOne
////////////////////////////////////////////////////////////////////////////////

        virtual AqlItemBlock* getOne () {
          // Fetch one from above, possibly using our _buffer:
          auto res = ExecutionBlock::getOne();
          if (res == nullptr) {
            return res;
          }
          TRI_ASSERT(res->size() == 1);

          // Let's steal the actual result and throw away the vars:
          AqlItemBlock* stripped = new AqlItemBlock(1, 1);
          auto ep = static_cast<ReturnNode const*>(getPlanNode());
          auto it = _varOverview->varInfo.find(ep->_inVariable->id);
          TRI_ASSERT(it != _varOverview->varInfo.end());
          RegisterId registerId = it->second.registerId;
          stripped->setValue(0, 0, res->getValue(0, registerId));
          res->eraseValue(0, registerId);
          stripped->getDocumentCollections().at(0)
              = res->getDocumentCollections().at(registerId);
          delete res;
          return stripped;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

        virtual AqlItemBlock* getSome (size_t atLeast, 
                                       size_t atMost) {
          auto res = ExecutionBlock::getSome(atLeast, atMost);
          if (res == nullptr) {
            return res;
          }

          // Let's steal the actual result and throw away the vars:
          auto ep = static_cast<ReturnNode const*>(getPlanNode());
          auto it = _varOverview->varInfo.find(ep->_inVariable->id);
          TRI_ASSERT(it != _varOverview->varInfo.end());
          RegisterId registerId = it->second.registerId;
          AqlItemBlock* stripped = new AqlItemBlock(res->size(), 1);
          for (size_t i = 0; i < res->size(); i++) {
            stripped->setValue(i, 0, res->getValue(i, registerId));
            res->eraseValue(i, registerId);
          }
          stripped->getDocumentCollections().at(0)
              = res->getDocumentCollections().at(registerId);
          delete res;
          return stripped;
        }

    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


