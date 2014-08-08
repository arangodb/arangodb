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

        std::vector<ExecutionBlock*> getDependencies () {
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
          
          // We collect the subquery blocks to deal with them at the end:
          std::vector<ExecutionBlock*>            subQueries;

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
          VarOverview (VarOverview const& v, unsigned int newdepth) 
            : varInfo(v.varInfo), nrRegsHere(v.nrRegsHere), nrRegs(v.nrRegs),
              subQueries(), depth(newdepth+1), 
              totalNrRegs(v.nrRegs[newdepth]), me(nullptr) {
            nrRegs.resize(depth);
            nrRegsHere.resize(depth);
            nrRegsHere.push_back(0);
            nrRegs.push_back(nrRegs.back());
          }

          ~VarOverview () {};

          virtual bool enterSubquery (ExecutionBlock* super,
                                      ExecutionBlock* sub) {
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
                subQueries.push_back(eb);
                break;
              }
              case ExecutionNode::AGGREGATE: {
                depth++;
                nrRegsHere.push_back(0);
                nrRegs.push_back(nrRegs.back());

                auto ep = static_cast<AggregateNode const*>(eb->getPlanNode());
                for (auto p : ep->_aggregateVariables) {
                  // p is std::pair<Variable const*,Variable const*>
                  // and the first is the to be assigned output variable
                  // for which we need to create a register in the current
                  // frame:
                  nrRegsHere[depth]++;
                  nrRegs[depth]++;
                  varInfo.insert(make_pair(p.first->id,
                                           VarInfo(depth, totalNrRegs)));
                  totalNrRegs++;
                }
                if (ep->_outVariable != nullptr) {
                  nrRegsHere[depth]++;
                  nrRegs[depth]++;
                  varInfo.insert(make_pair(ep->_outVariable->id,
                                           VarInfo(depth, totalNrRegs)));
                  totalNrRegs++;
                }
                break;
              }
              case ExecutionNode::SORT: {
                // sort sorts in place and does not produce new registers
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
          shared_ptr<VarOverview> v;
          if (_varOverview.get() == nullptr) {
            v.reset(new VarOverview());
          }
          else {
            v.reset(new VarOverview(*_varOverview, _depth));
          }
          v->setSharedPtr(&v);
          walk(v.get());
          // Now handle the subqueries:
          for (auto s : v->subQueries) {
            s->staticAnalysis();
          }
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

        virtual int bind (AqlItemBlock* items, size_t pos);

////////////////////////////////////////////////////////////////////////////////
/// @brief execute
////////////////////////////////////////////////////////////////////////////////

        virtual int execute () {
          for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
            int res = (*it)->execute();
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
          for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
            int res = (*it)->shutdown();

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

////////////////////////////////////////////////////////////////////////////////
/// @brief copy register data from one block (src) into another (dst)
/// register values are either copied/cloned or stolen
////////////////////////////////////////////////////////////////////////////////
          
        void inheritRegisters (AqlItemBlock const* src,
                               AqlItemBlock* dst,
                               size_t row) {
          RegisterId const n = src->getNrRegs();

          for (RegisterId i = 0; i < n; i++) {
            dst->setValue(0, i, src->getValue(row, i).clone());

            // copy collection
            dst->setDocumentCollection(i, src->getDocumentCollection(i));
          }
        }


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
          return getSome(1, 1);
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
              if (! getBlock(atLeast - total, std::max(atMost - total, DefaultBatchSize))) {
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
          _done = true;
          return false;
        }

        virtual bool skip (size_t number) {
          if (_done) {
            return false;
          }
          
          // Here, if _buffer.size() is > 0 then _pos points to a valid place
          // in it.

          size_t skipped = 0;
          std::vector<AqlItemBlock*> collector;
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
  
        static size_t const DefaultBatchSize;
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
          : ExecutionBlock(trx, ep), _inputRegisterValues(nullptr) {
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
          _inputRegisterValues = nullptr;   // just in case
          return ExecutionBlock::initialize();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief bind, store a copy of the register values coming from above
////////////////////////////////////////////////////////////////////////////////

        int bind (AqlItemBlock* items, size_t pos) {
          // Create a deep copy of the register values given to us:
          if (_inputRegisterValues != nullptr) {
            delete _inputRegisterValues;
          }
          _inputRegisterValues = items->slice(pos, pos+1);
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute, just call base class
////////////////////////////////////////////////////////////////////////////////

        int execute () {
          return ExecutionBlock::execute();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown, just call base class
////////////////////////////////////////////////////////////////////////////////

        int shutdown () {
          int res = ExecutionBlock::shutdown();
          if (_inputRegisterValues != nullptr) {
            delete _inputRegisterValues;
            _inputRegisterValues = nullptr;
          }
          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* getSome (size_t, size_t) {
          if (_done) {
            return nullptr;
          }

          AqlItemBlock* res = new AqlItemBlock(1, _varOverview->nrRegs[_depth]);
          if (_inputRegisterValues != nullptr) {
            for (RegisterId reg = 0; reg < _inputRegisterValues->getNrRegs(); ++reg) {
              res->setValue(0, reg, _inputRegisterValues->getValue(0, reg));
              _inputRegisterValues->eraseValue(0, reg);
              res->setDocumentCollection(reg,
                       _inputRegisterValues->getDocumentCollection(reg));

            }
          }
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

////////////////////////////////////////////////////////////////////////////////
/// @brief the bind data coming from outside
////////////////////////////////////////////////////////////////////////////////

      private:

        AqlItemBlock* _inputRegisterValues;

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
          inheritRegisters(cur, res, _pos);

          // set our collection for our output register
          res->setDocumentCollection(curRegs, _trx->documentCollection());

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
// --SECTION--                                          EnumerateListBlock
// -----------------------------------------------------------------------------

    class EnumerateListBlock : public ExecutionBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        EnumerateListBlock (AQL_TRANSACTION_V8* trx,
                                  EnumerateListNode const* ep) 
          : ExecutionBlock(trx, ep) {

        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~EnumerateListBlock () {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize, here we get the inVariable
////////////////////////////////////////////////////////////////////////////////

        int initialize () {
          int res = ExecutionBlock::initialize();

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          auto en = reinterpret_cast<EnumerateListNode const*>(_exeNode);
          
          // get the inVariable register id . . .
          // staticAnalysis has been run, so _varOverview is set up
          
          auto it = _varOverview->varInfo.find(en->_inVariable->id);
          if (it == _varOverview->varInfo.end()){
            THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
          }
          
          _inVarRegId = (*it).second.registerId;
          
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

          // handle local data (if any) 
           _index = 0;     // index in _inVariable for next run
           _thisblock = 0; // the current block in the _inVariable DOCVEC
           _seen = 0;      // the sum of the sizes of the blocks in the _inVariable
                           // DOCVEC that preceed _thisblock

          return TRI_ERROR_NO_ERROR;
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
          }

          // if we make it here, then _buffer.front() exists
          AqlItemBlock* cur = _buffer.front();
          
          // get the thing we are looping over 
          AqlValue inVarReg = cur->getValue(_pos, _inVarRegId);
          size_t sizeInVar;

          switch (inVarReg._type) {
            case AqlValue::JSON: {
              if(!inVarReg._json->isList()){
                THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
              }
              sizeInVar = inVarReg._json->size();
              break;
            }
            case AqlValue::RANGE: {
              sizeInVar = inVarReg._range->_high - inVarReg._range->_low;
              break;
            }
            case AqlValue::DOCVEC: {
                if( _index == 0){// this is a (maybe) new DOCVEC
                  _DOCVECsize = 0;
                  //we require the total number of items 
                  for (size_t i = 0; i < inVarReg._vector->size(); i++) {
                    _DOCVECsize += inVarReg._vector->at(i)->size();
                  }
                }
                sizeInVar = _DOCVECsize;
              break;
            }
            default: {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
            }
          }
         
          size_t toSend = std::min(atMost, sizeInVar - _index); 

          //create the result
          auto res = new AqlItemBlock(toSend, _varOverview->nrRegs[_depth]);
          
          inheritRegisters(cur, res, _pos);

          // we don't have a collection
          res->setDocumentCollection(cur->getNrRegs(), nullptr);

          for (size_t j = 0; j < toSend; j++) {
            if (j > 0) {
              // re-use already copied aqlvalues
              for (RegisterId i = 0; i < cur->getNrRegs(); i++) {
                res->setValue(j, i, res->getValue(0, i));
              }
            }
            // add the new register value . . .
            res->setValue(j, cur->getNrRegs(), getAqlValue(inVarReg));
            // deep copy of the inVariable.at(_pos) with correct memory
            // requirements
            ++_index; 
          }
          if (_index == sizeInVar) {
            _index = 0;
            _thisblock = 0;
            _seen = 0;
            // advance read position in the current block . . .
            if (++_pos == cur->size() ) {
              delete cur;
              _buffer.pop_front();
              _pos = 0;
            }
          }
          return res;
        } 

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AqlValue from the inVariable using the current _index
////////////////////////////////////////////////////////////////////////////////

      private:

        AqlValue getAqlValue (AqlValue inVarReg) {
          switch (inVarReg._type) {
            case AqlValue::JSON: {
              return AqlValue(new basics::Json(inVarReg._json->at(_index).copy()));
            }
            case AqlValue::RANGE: {
              return AqlValue(new 
                  basics::Json(static_cast<double>(inVarReg._range->_low + _index)));
            }
            case AqlValue::DOCVEC: { // incoming doc vec has a single column 
                AqlValue out = inVarReg._vector->at(_thisblock)->getValue(_index -
                    _seen, 0).clone();
                if(++_index == (inVarReg._vector->at(_thisblock)->size() + _seen)){
                  _seen += inVarReg._vector->at(_thisblock)->size();
                  _thisblock++;
                }
            }
            default: {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
            }
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief current position in the _inVariable
////////////////////////////////////////////////////////////////////////////////
        
        size_t _index, _thisblock, _seen, _DOCVECsize;

////////////////////////////////////////////////////////////////////////////////
/// @brief the register index containing the inVariable of the EnumerateListNode
////////////////////////////////////////////////////////////////////////////////
                                  
        RegisterId _inVarRegId;

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
// --SECTION--                                                     SubqueryBlock
// -----------------------------------------------------------------------------

    class SubqueryBlock : public ExecutionBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        SubqueryBlock (AQL_TRANSACTION_V8* trx,
                       SubqueryNode const* en,
                       ExecutionBlock* subquery)
                : ExecutionBlock(trx, en), _outReg(0),
           _subquery(subquery) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~SubqueryBlock () {
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

          auto en = static_cast<SubqueryNode const*>(getPlanNode());

          auto it3 = _varOverview->varInfo.find(en->_outVariable->id);
          TRI_ASSERT(it3 != _varOverview->varInfo.end());
          _outReg = it3->second.registerId;

          return TRI_ERROR_NO_ERROR;
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
          for (size_t i = 0; i < res->size(); i++) {
            _subquery->initialize();
            _subquery->bind(res, i);
            _subquery->execute();
            auto results = new std::vector<AqlItemBlock*>;
            do {
              auto tmp = _subquery->getSome(DefaultBatchSize, DefaultBatchSize);
              if (tmp == nullptr) {
                break;
              }
              results->push_back(tmp);
            } 
            while(true);

            res->setValue(i, _outReg, AqlValue(results));
            _subquery->shutdown();
          }
          return res;
        }

        ExecutionBlock* getSubquery() {
                return _subquery;
        }
////////////////////////////////////////////////////////////////////////////////
/// @brief we hold a pointer to the expression in the plan
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief output register
////////////////////////////////////////////////////////////////////////////////

        RegisterId _outReg;

////////////////////////////////////////////////////////////////////////////////
/// @brief we need to have an executionblock and where to write the result
////////////////////////////////////////////////////////////////////////////////

        ExecutionBlock* _subquery;
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
          while (true) {  // will be left by break or return
            if (! ExecutionBlock::getBlock(atLeast, atMost)) {
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

            if (! _chosen.empty()) {
              break;   // OK, there are some docs in the result
            }

            _buffer.pop_front();  // Block was useless, just try again
          }

          return true;
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

        std::vector<size_t> _chosen;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                    AggregateBlock
// -----------------------------------------------------------------------------

    class AggregateBlock : public ExecutionBlock  {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        AggregateBlock (AQL_TRANSACTION_V8* trx,
                        ExecutionNode const* ep)
          : ExecutionBlock(trx, ep),
            _collectDetails(false) { 
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        virtual ~AggregateBlock () {};

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

        int initialize () {
          int res = ExecutionBlock::initialize();

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          auto en = static_cast<AggregateNode const*>(getPlanNode());

          for (auto p : en->_aggregateVariables){
            //We know that staticAnalysis has been run, so _varOverview is set up
            auto itOut = _varOverview->varInfo.find(p.first->id);
            TRI_ASSERT(itOut != _varOverview->varInfo.end());
            
            auto itIn = _varOverview->varInfo.find(p.second->id);
            TRI_ASSERT(itIn != _varOverview->varInfo.end());
            _aggregateRegisters.push_back(make_pair((*itOut).second.registerId, (*itIn).second.registerId));
          }

          if (en->_outVariable != nullptr) {
            auto it = _varOverview->varInfo.find(en->_outVariable->id);
            TRI_ASSERT(it != _varOverview->varInfo.end());
            _groupRegister = (*it).second.registerId;
            _collectDetails = true;
          }
         
          // reserve space for the current row 
          _currentGroup.groupValues.reserve(_aggregateRegisters.size());
          _currentGroup.collections.reserve(_aggregateRegisters.size());

          for (size_t i = 0; i < _aggregateRegisters.size(); ++i) {
            _currentGroup.groupValues[i] = AqlValue();
            _currentGroup.collections[i] = nullptr;
          }
          
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getSome
////////////////////////////////////////////////////////////////////////////////

        AqlItemBlock* getSome (size_t atLeast, size_t atMost) {
          if (_done) {
            return nullptr;
          }

std::cout << "AGGREGATE::GETSOME\n";
          if (_buffer.empty()) {
            if (! ExecutionBlock::getBlock(DefaultBatchSize, DefaultBatchSize)) {

                // return the last group if we're running out of input
                // TODO: last group is lost at the moment
//                auto res = new AqlItemBlock(1, _varOverview->nrRegs[_depth]);
//                emitRow(res, 0, _previousRow);
//                clearGroup();
std::cout << "AGGREGATE::GETSOME - DONE\n";

              _done = true;
              return nullptr;
            }
            _pos = 0;           // this is in the first block
          }

          // If we get here, we do have _buffer.front()
          AqlItemBlock* cur = _buffer.front();
          size_t const curRegs = cur->getNrRegs();

          auto res = new AqlItemBlock(atMost, _varOverview->nrRegs[_depth]);
          TRI_ASSERT(curRegs <= res->getNrRegs());

std::cout << "POS: " << _pos << "\n";
          inheritRegisters(cur, res, _pos);
           
          size_t j = 0;

          while (j < atMost) {
            // read the next input tow

            bool newGroup = false;
            if (_currentGroup.groupValues[0].isEmpty()) {
              // we never had any previous group
              newGroup = true;
              std::cout << "NEED TO CREATE NEW GROUP\n";
            }
            else {
              // we already had a group, check if the group has changed
              std::cout << "HAVE A GROUP\n";
              size_t i = 0;

              for (auto it = _aggregateRegisters.begin(); it != _aggregateRegisters.end(); ++it) {
                int res = CompareAqlValues(_currentGroup.groupValues[i], 
                                           _currentGroup.collections[i],
                                           cur->getValue(_pos, (*it).second), 
                                           cur->getDocumentCollection((*it).second));
                if (res != 0) {
                  // group change
                  newGroup = true;
                  break;
                }
                ++i;
              }
            }

            if (newGroup) {
              std::cout << "CREATING GROUP...\n";
              if (! _currentGroup.groupValues[0].isEmpty()) {
                // need to emit the current group first
              std::cout << "EMITTING OLD GROUP INTO ROW #" << j << "\n";
                size_t i = 0;
                for (auto it = _aggregateRegisters.begin(); it != _aggregateRegisters.end(); ++it) {
              std::cout << "REGISTER #" << (*it).first << "\n";
                  res->setValue(j, (*it).first, _currentGroup.groupValues[i]);
                  ++i;
                }
            
                ++j;
              }

              // construct the new group
              size_t i = 0;
              for (auto it = _aggregateRegisters.begin(); it != _aggregateRegisters.end(); ++it) {
                _currentGroup.groupValues[i] = cur->getValue(_pos, (*it).second).clone();
                _currentGroup.collections[i] = cur->getDocumentCollection((*it).second);
                std::cout << "GROUP VALUE #" << i << ": " << _currentGroup.groupValues[i].toString(_currentGroup.collections[i]) << "\n";
                ++i;
              }
            }

            if (_collectDetails) {
//              _currentGroup.groupDetails.
            }

            if (++_pos >= cur->size()) {
              _buffer.pop_front();
              delete cur;
              _pos = 0;
std::cout << "SHRINKING BLOCK TO " << j << " ROWS\n";
          res->shrink(j);
              return res;
            }
          }

std::cout << "SHRINKING BLOCK TO " << j << " ROWS\n";
          res->shrink(j);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief pairs, consisting of out register and in register
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::pair<RegisterId, RegisterId>> _aggregateRegisters;

        struct {
          std::vector<AqlValue> groupValues;
          std::vector<TRI_document_collection_t const*> collections;
        }
        _currentGroup;

////////////////////////////////////////////////////////////////////////////////
/// @brief the optional register that contains the values for each group
////////////////////////////////////////////////////////////////////////////////
        
        RegisterId _groupRegister;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not elements should be collected for each group
////////////////////////////////////////////////////////////////////////////////

        bool _collectDetails;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                         SortBlock
// -----------------------------------------------------------------------------

    class SortBlock : public ExecutionBlock  {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        SortBlock (AQL_TRANSACTION_V8* trx,
                   ExecutionNode const* ep)
          : ExecutionBlock(trx, ep),
            _stable(false) { 
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        virtual ~SortBlock () {};

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

        int initialize () {
          int res = ExecutionBlock::initialize();

          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          auto en = static_cast<SortNode const*>(getPlanNode());

          for( auto p: en->_elements){
            //We know that staticAnalysis has been run, so _varOverview is set up
            auto it = _varOverview->varInfo.find(p.first->id);
            TRI_ASSERT(it != _varOverview->varInfo.end());
            _sortRegisters.push_back(make_pair(it->second.registerId, p.second));
          }

          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute
////////////////////////////////////////////////////////////////////////////////

        virtual int execute () {
          int res = ExecutionBlock::execute();
          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }
          // suck all blocks into _buffer
          while (getBlock(DefaultBatchSize, DefaultBatchSize)) {
          }

          if (_buffer.empty()) {
            _done = true;
            return TRI_ERROR_NO_ERROR;
          }

          // coords[i][j] is the <j>th row of the <i>th block
          std::vector<std::pair<size_t, size_t>> coords;

          size_t sum = 0;
          for (auto block : _buffer){
            sum += block->size();
          }

          coords.reserve(sum);

          // install the coords
          size_t count = 0;

          for (auto block :_buffer) {
            for (size_t i = 0; i < block->size(); i++) {
              coords.push_back(std::make_pair(count, i));
            }
            count++;
          }

          std::vector<TRI_document_collection_t const*> colls;
          for (RegisterId i = 0; i < _sortRegisters.size(); i++) {
            colls.push_back(_buffer.front()->getDocumentCollection(_sortRegisters[i].first));
          }

          // comparison function
          OurLessThan ourLessThan(_buffer, _sortRegisters, colls);
          
          // sort coords
          if (_stable) {
            std::stable_sort(coords.begin(), coords.end(), ourLessThan);
          }
          else {
            std::sort(coords.begin(), coords.end(), ourLessThan);
          }

          //copy old blocks from _buffer into newbuf
          std::vector<AqlItemBlock*> newbuf;
          
          // reserve enough space in newbuf here because we know the target size
          newbuf.reserve(_buffer.size());

          for (auto x : _buffer) {
            newbuf.push_back(x);
          }
          _buffer.clear();
          
          count = 0;
          RegisterId nrregs = newbuf.front()->getNrRegs();
          
          //install the rearranged values from <newbuf> into <_buffer>
          while (count < sum) {
            size_t size_next 
              = (sum - count > DefaultBatchSize ?  DefaultBatchSize : sum - count);
            AqlItemBlock* next = new AqlItemBlock(size_next, nrregs);
            for (size_t i = 0; i < size_next; i++) {
              for (RegisterId j = 0; j < nrregs; j++) {
                next->setValue(i, j, 
                    newbuf[coords[count].first]->getValue(coords[count].second, j));
                newbuf[coords[count].first]->eraseValue(coords[count].second, j);
              }
              count++;
            }
            for (RegisterId j = 0; j < nrregs; j++) {
              next->setDocumentCollection(j, 
                               newbuf.front()->getDocumentCollection(j));
            }
            _buffer.push_back(next);
          }
           
          for (auto x : newbuf){
            delete x;
          }

          _done = false;
          _pos = 0;

          return TRI_ERROR_NO_ERROR;
        }

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief OurLessThan
////////////////////////////////////////////////////////////////////////////////
        
        class OurLessThan {
          public:
            OurLessThan (std::deque<AqlItemBlock*>& buffer,  
                         std::vector<std::pair<RegisterId, bool>>& sortRegisters,
                         std::vector<TRI_document_collection_t const*>& colls) 
              : _buffer(buffer), _sortRegisters(sortRegisters), _colls(colls) {
            }

            bool operator() (std::pair<size_t, size_t> const& a, 
                             std::pair<size_t, size_t> const& b) {

              size_t i = 0;
              for(auto reg: _sortRegisters){
                int cmp = CompareAqlValues(_buffer[a.first]->getValue(a.second, reg.first),
                                           _colls[i],
                                           _buffer[b.first]->getValue(b.second, reg.first),
                                           _colls[i]);
                if(cmp == -1) {
                  return reg.second;
                } 
                else if (cmp == 1) {
                  return !reg.second;
                }
                i++;
              }

              return false;
            }

          private:
            std::deque<AqlItemBlock*>& _buffer;
            std::vector<std::pair<RegisterId, bool>>& _sortRegisters;
            std::vector<TRI_document_collection_t const*>& _colls;
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief pairs, consisting of variable and sort direction
/// (true = ascending | false = descending)
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::pair<RegisterId, bool>> _sortRegisters;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the sort should be stable
////////////////////////////////////////////////////////////////////////////////
            
        bool _stable;

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
          stripped->setDocumentCollection(0, res->getDocumentCollection(registerId));
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


