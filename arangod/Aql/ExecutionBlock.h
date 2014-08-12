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
#include "Aql/AqlItemBlock.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode.h"
#include "Utils/AqlTransaction.h"
#include "Utils/transactions.h"
#include "Utils/V8TransactionContext.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                   AggregatorGroup
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief details about the current group
////////////////////////////////////////////////////////////////////////////////

    struct AggregatorGroup {
      std::vector<AqlValue> groupValues;
      std::vector<TRI_document_collection_t const*> collections;

      std::vector<AqlItemBlock*> groupBlocks;

      size_t firstRow;
      size_t lastRow;
      bool rowsAreValid;

      AggregatorGroup () 
        : firstRow(0),
          lastRow(0),
          rowsAreValid(false) {
      }

      ~AggregatorGroup () {
        reset();
      }
          
      void initialize (size_t capacity) {
        groupValues.reserve(capacity);
        collections.reserve(capacity);

        for (size_t i = 0; i < capacity; ++i) {
          groupValues[i] = AqlValue();
          collections[i] = nullptr;
        }
      }

      void reset () {
        for (auto it = groupBlocks.begin(); it != groupBlocks.end(); ++it) {
          delete (*it);
        }
        groupBlocks.clear();
        groupValues[0].erase();
      }

      void setFirstRow (size_t value) {
        firstRow = value;
        rowsAreValid = true;
      }
      
      void setLastRow (size_t value) {
        lastRow = value;
        rowsAreValid = true;
      }

      void addValues (AqlItemBlock const* src,
                      RegisterId groupRegister) {
        if (groupRegister == 0) {
          // nothing to do
          return;
        }

        if (rowsAreValid) {
          // emit group details 
          TRI_ASSERT(firstRow <= lastRow);

          auto block = src->slice(firstRow, lastRow + 1);
          try {
            groupBlocks.push_back(block);
          }
          catch (...) {
            delete block;
            throw;
          }
        }

        firstRow = lastRow = 0;
        // the next statement ensures we don't add the same value (row) twice
        rowsAreValid = false;
      }
            

    };

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
          if (pos >= _dependencies.size()) {
            return nullptr;
          }

          return _dependencies.at(pos);
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
          unsigned int const depth;
          RegisterId const registerId;

          VarInfo () = delete;
          VarInfo (int depth, int registerId) : depth(depth), registerId(registerId) {
          }
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
          std::vector<ExecutionBlock*>            subQueryBlocks;

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
              subQueryBlocks(), depth(newdepth+1), 
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
                TRI_ASSERT(ep != nullptr);
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
                TRI_ASSERT(ep != nullptr);
                varInfo.insert(make_pair(ep->_outVariable->id,
                                         VarInfo(depth, totalNrRegs)));
                totalNrRegs++;
                break;
              }
              case ExecutionNode::CALCULATION: {
                nrRegsHere[depth]++;
                nrRegs[depth]++;
                auto ep = static_cast<CalculationNode const*>(eb->getPlanNode());
                TRI_ASSERT(ep != nullptr);
                varInfo.insert(make_pair(ep->_outVariable->id,
                                         VarInfo(depth, totalNrRegs)));
                totalNrRegs++;
                break;
              }
              case ExecutionNode::PROJECTION: {
                nrRegsHere[depth]++;
                nrRegs[depth]++;
                auto ep = static_cast<ProjectionNode const*>(eb->getPlanNode());
                TRI_ASSERT(ep != nullptr);
                varInfo.insert(make_pair(ep->_outVariable->id,
                                         VarInfo(depth, totalNrRegs)));
                totalNrRegs++;
                break;
              }
              case ExecutionNode::SUBQUERY: {
                nrRegsHere[depth]++;
                nrRegs[depth]++;
                auto ep = static_cast<SubqueryNode const*>(eb->getPlanNode());
                TRI_ASSERT(ep != nullptr);
                varInfo.insert(make_pair(ep->_outVariable->id,
                                         VarInfo(depth, totalNrRegs)));
                totalNrRegs++;
                subQueryBlocks.push_back(eb);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief static analysis
////////////////////////////////////////////////////////////////////////////////

        void staticAnalysis (ExecutionBlock* super = nullptr);

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
            if (! src->getValue(row, i).isEmpty()) {
              dst->setValue(0, i, src->getValue(row, i).clone());
            }

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
            res = AqlItemBlock::concatenate(collector);
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

        // skip between atLeast and atMost using the same setup as getSome,
        // returns the number actually skipped . . . 
        // will only return less than atLeast if there aren't atLeast many
        // things to skip overall.
        virtual size_t skipSome (size_t atLeast, size_t atMost) {
          size_t skipped = 0;
          
          if (_done) {
            return skipped;
          }
          
          while (skipped < atLeast) {
            if (_buffer.empty()) {
              if (! getBlock(atLeast - skipped, 
                    std::max(atMost - skipped, DefaultBatchSize))) {
                _done = true;
                break;
              }
              _pos = 0;
            }
            // get the current block 
            AqlItemBlock* cur = _buffer.front();
            
            if (cur->size() - _pos + skipped > atMost) {
              // eat just enough of the current block . . .
              _pos += atMost - skipped;
              skipped = atMost;
            }
            else { 
              // eat the rest of the current block and then proceed to the next
              // course if any . . .
              skipped += cur->size() - _pos;
              delete cur;
              _buffer.pop_front();
              _pos = 0;
            } 
          }
          return skipped;
        }
        
        // skip exactly <number> outputs, returns <true> if _done after
        // skipping, and <false> otherwise . . .
        bool skip (size_t number) {
          size_t skipped = skipSome(number, number);
          size_t nr = skipped;
          while ( nr != 0 && skipped < number ){
            nr = skipSome(number - skipped, number - skipped);
            skipped += nr;
          }
          if (nr == 0) {
            return true;
          } 
          return ! hasMore();
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

      public:

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

        size_t skipSome (size_t atLeast, size_t atMost) {
          if (_done) {
            return 0; //nothing to skip
          }
          if (atLeast > 0) {
            _done = true;
            return 1; // 1 thing to skip
          }
          return 0; // atLeast = 0, skip nothing . . . 
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

////////////////////////////////////////////////////////////////////////////////
// skip between atLeast and atMost, returns the number actually skipped . . . 
// will only return less than atLeast if there aren't atLeast many
// things to skip overall.
////////////////////////////////////////////////////////////////////////////////

        size_t skipSome (size_t atLeast, size_t atMost) {
          
          size_t skipped = 0;

          if (_done) {
            return skipped;
          }
          
          while (skipped < atLeast) {
            if (_buffer.empty()) {
              if (! getBlock(DefaultBatchSize, DefaultBatchSize)) {
                _done = true;
                return skipped;
              }
              _pos = 0;           // this is in the first block
              _posInAllDocs = 0;  // Note that we know _allDocs.size() > 0,
                                  // otherwise _done would be true already
            }

            // if we get here, then _buffer.front() exists
            AqlItemBlock* cur = _buffer.front();
            
            if (atMost >= skipped + _documents.size() - _posInAllDocs) {
              skipped += _documents.size() - _posInAllDocs;
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
            else {
              _posInAllDocs += atMost - skipped;
              skipped = atMost;
            }
          }
        return skipped;
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
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "variable not found");
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

          AqlItemBlock* res;
         
          do {
            // repeatedly try to get more stuff from upstream
            // note that the value of the variable we have to loop over
            // can contain zero entries, in which case we have to 
            // try again!

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

            // get the size of the thing we are looping over
            collection = nullptr;
            switch (inVarReg._type) {
              case AqlValue::JSON: {
                if(! inVarReg._json->isList()) {
                  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                      "EnumerateListBlock: JSON is not a list");
                }
                sizeInVar = inVarReg._json->size();
                break;
              }

              case AqlValue::RANGE: {
                sizeInVar = inVarReg._range->size();
                break;
              }

              case AqlValue::DOCVEC: {
                if( _index == 0) { // this is a (maybe) new DOCVEC
                  _DOCVECsize = 0;
                  //we require the total number of items 

                  for (size_t i = 0; i < inVarReg._vector->size(); i++) {
                    _DOCVECsize += inVarReg._vector->at(i)->size();
                  }
                }
                sizeInVar = _DOCVECsize;
                if (sizeInVar > 0) {
                  collection = inVarReg._vector->at(0)->getDocumentCollection(0);
                }
                break;
              }

              case AqlValue::SHAPED: {
                THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                    "EnumerateListBlock: cannot iterate over shaped value");
              }
              
              case AqlValue::EMPTY: {
                THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                    "EnumerateListBlock: cannot iterate over empty value");
              }
            }

            if (sizeInVar == 0) { 
              res = nullptr;
            }
            else {
              size_t toSend = std::min(atMost, sizeInVar - _index); 

              // create the result
              res = new AqlItemBlock(toSend, _varOverview->nrRegs[_depth]);
              
              inheritRegisters(cur, res, _pos);

              // we don't have a collection
              res->setDocumentCollection(cur->getNrRegs(), collection);

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
                // Note that _index has been increased by 1 by getAqlValue!
              }
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
          } 
          while (res == nullptr);

          return res;
        } 

////////////////////////////////////////////////////////////////////////////////
// skip between atLeast and atMost returns the number actually skipped . . . 
// will only return less than atLeast if there aren't atLeast many
// things to skip overall.
////////////////////////////////////////////////////////////////////////////////

        size_t skipSome (size_t atLeast, size_t atMost) {
          
          if (_done) {
            return 0;
          }
          
          size_t skipped = 0;

          while ( skipped < atLeast ) {
            if (_buffer.empty()) {
              if (! ExecutionBlock::getBlock(DefaultBatchSize, DefaultBatchSize)) {
                _done = true;
                return skipped;
              }
              _pos = 0;           // this is in the first block
            }

            // if we make it here, then _buffer.front() exists
            AqlItemBlock* cur = _buffer.front();
            
            // get the thing we are looping over 
            AqlValue inVarReg = cur->getValue(_pos, _inVarRegId);
            size_t sizeInVar;

            // get the size of the thing we are looping over
            switch (inVarReg._type) {
              case AqlValue::JSON: {
                if(! inVarReg._json->isList()) {
                  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                      "EnumerateListBlock: JSON is not a list");
                }
                sizeInVar = inVarReg._json->size();
                break;
              }
              case AqlValue::RANGE: {
                sizeInVar = inVarReg._range->_high - inVarReg._range->_low + 1;
                break;
              }
              case AqlValue::DOCVEC: {
                if( _index == 0) { // this is a (maybe) new DOCVEC
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
                THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                    "EnumerateListBlock: unexpected type in register");
              }
            }
           
            if (atMost < sizeInVar - _index) {
              // eat just enough of inVariable . . .
              _index += atMost;
              skipped = atMost;
            } 
            else {
              // eat the whole of the current inVariable and proceed . . .
              skipped += (sizeInVar - _index);
              _index = 0;
              _thisblock = 0;
              _seen = 0;
              delete cur;
              _buffer.pop_front();
              _pos = 0;
            }
          }
          return skipped;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief create an AqlValue from the inVariable using the current _index
////////////////////////////////////////////////////////////////////////////////

      private:

        AqlValue getAqlValue (AqlValue inVarReg) {
          switch (inVarReg._type) {
            case AqlValue::JSON: {
              return AqlValue(new basics::Json(inVarReg._json->at(_index++).copy()));
            }
            case AqlValue::RANGE: {
              return AqlValue(new 
                  basics::Json(static_cast<double>(inVarReg._range->_low + _index++)));
            }
            case AqlValue::DOCVEC: { // incoming doc vec has a single column 
              AqlValue out = inVarReg._vector->at(_thisblock)->getValue(_index -
                  _seen, 0).clone();
              if(++_index == (inVarReg._vector->at(_thisblock)->size() + _seen)){
                _seen += inVarReg._vector->at(_thisblock)->size();
                _thisblock++;
              }
              return out;
            }
            
            case AqlValue::SHAPED: 
            case AqlValue::EMPTY: {
              // error
              break;
            }
          }
              
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected value in variable to iterate over");
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief current position in the _inVariable
////////////////////////////////////////////////////////////////////////////////
        
        size_t _index;
        
////////////////////////////////////////////////////////////////////////////////
/// @brief current block in DOCVEC
////////////////////////////////////////////////////////////////////////////////
        
        size_t _thisblock;
        
////////////////////////////////////////////////////////////////////////////////
/// @brief number of elements in DOCVEC before the current block
////////////////////////////////////////////////////////////////////////////////
        
        size_t _seen;
        
////////////////////////////////////////////////////////////////////////////////
/// @brief total number of elements in DOCVEC
////////////////////////////////////////////////////////////////////////////////
        
        size_t _DOCVECsize;

////////////////////////////////////////////////////////////////////////////////
/// @brief document collection from DOCVEC
////////////////////////////////////////////////////////////////////////////////
        
        TRI_document_collection_t const* collection;

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
          : ExecutionBlock(trx, en), 
            _expression(en->expression()), 
            _outReg(0) {

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
          _inVars.clear();
          _inRegs.clear();

          for (auto it = inVars.begin(); it != inVars.end(); ++it) {            
            _inVars.push_back(*it);
            auto it2 = _varOverview->varInfo.find((*it)->id);

            TRI_ASSERT(it2 != _varOverview->varInfo.end());
            _inRegs.push_back(it2->second.registerId);
          }
          
          // check if the expression is "only" a reference to another variable
          // this allows further optimizations inside doEvaluation
          _isReference = (_expression->node()->type == NODE_TYPE_REFERENCE);

          if (_isReference) {
            TRI_ASSERT(_inRegs.size() == 1);
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

          size_t const n = result->size();
          if (_isReference) {
            // the expression is a reference to a variable only.
            // no need to execute the expression at all
            result->setDocumentCollection(_outReg, result->getDocumentCollection(_inRegs[0]));

            for (size_t i = 0; i < n; i++) {
              // must clone, otherwise all the results become invalid
              result->setValue(i, _outReg, result->getValue(i, _inRegs[0]).clone());
            }
          }
          else {
            vector<AqlValue>& data(result->getData());
            vector<TRI_document_collection_t const*> docColls(result->getDocumentCollections());

            RegisterId nrRegs = result->getNrRegs();
            result->setDocumentCollection(_outReg, nullptr);
            for (size_t i = 0; i < n; i++) {
              // need to execute the expression
              result->setValue(i, _outReg, _expression->execute(_trx, docColls, data, nrRegs * i, _inVars, _inRegs));
            }
          }
        }

      public:

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

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the expression is a simple variable reference
////////////////////////////////////////////////////////////////////////////////

        bool _isReference;

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
            int ret;
            ret = _subquery->initialize();
            if (ret == TRI_ERROR_NO_ERROR) {
              ret = _subquery->bind(res, i);
            }
            if (ret == TRI_ERROR_NO_ERROR) {
              ret = _subquery->execute();
            }
            if (ret != TRI_ERROR_NO_ERROR) {
              delete res;
              THROW_ARANGO_EXCEPTION(ret);
            }

            auto results = new std::vector<AqlItemBlock*>;
            try {
              do {
                auto tmp = _subquery->getSome(DefaultBatchSize, DefaultBatchSize);
                if (tmp == nullptr) {
                  break;
                }
                results->push_back(tmp);
              }
              while(true);
            }
            catch (...) {
              delete res;
              delete results;
              throw;
            }

            res->setValue(i, _outReg, AqlValue(results));

            ret = _subquery->shutdown();
            if (ret != TRI_ERROR_NO_ERROR) {
              delete res;
              THROW_ARANGO_EXCEPTION(ret);
            }

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
          try {
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
                collector.push_back(cur->steal(_chosen, _pos, _chosen.size()));
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
          }
          catch (...) {
            for (auto c : collector) {
              delete c;
            }
            throw;
          }
          if (collector.empty()) {
            return nullptr;
          }
          else if (collector.size() == 1) {
            return collector[0];
          }
          else {
            res = AqlItemBlock::concatenate(collector);
            for (auto it = collector.begin(); 
                 it != collector.end(); ++it) {
              delete (*it);
            }
            return res;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief skipSome
////////////////////////////////////////////////////////////////////////////////

        size_t skipSome (size_t atLeast, size_t atMost) {
          if (_done) {
            return 0;
          }

          // Here, if _buffer.size() is > 0 then _pos points to a valid place
          // in it.
          size_t skipped = 0;

          while (skipped < atLeast) {
            if (_buffer.empty()) {
              if (! getBlock(atLeast - skipped, atMost - skipped)) {
                _done = true;
                break;
              }
              _pos = 0;
            }
            // If we get here, then _buffer.size() > 0 and _pos points to a
            // valid place in it.
            AqlItemBlock* cur = _buffer.front();
            if (_chosen.size() - _pos + skipped > atMost) {
              // The current block of chosen ones is too large for atMost:
              _pos += atMost - skipped;
              skipped = atMost;
            }
            else if (_pos > 0 || _chosen.size() < cur->size()) {
              // The current block fits into our result, but it is already
              // half-eaten or needs to be copied anyway:
              skipped += _chosen.size() - _pos;
              delete cur;
              _buffer.pop_front();
              _chosen.clear();
              _pos = 0;
            } 
            else {
              // The current block fits into our result and is fresh and
              // takes them all, so we can just hand it on:
              skipped += cur->size();
              _buffer.pop_front();
              _chosen.clear();
              _pos = 0;
            }
          }
          return skipped;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief hasMore
////////////////////////////////////////////////////////////////////////////////

        bool hasMore () {
          if (_done) {
            return false;
          }

          if (_buffer.empty()) {
            if (! getBlock(DefaultBatchSize, DefaultBatchSize)) {
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
            _groupRegister(0),
            _variableNames() {
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

          // Reinitialise if we are called a second time:
          _aggregateRegisters.clear();
          _variableNames.clear();

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

            TRI_ASSERT(_groupRegister > 0);

            // construct a mapping of all register ids to variable names
            // we need this mapping to generate the grouped output

            for (size_t i = 0; i < _varOverview->varInfo.size(); ++i) {
              _variableNames.push_back(""); // init with some default value
            }

            // iterate over all our variables
            for (auto it = _varOverview->varInfo.begin(); it != _varOverview->varInfo.end(); ++it) {
              // find variable in the global variable map
              auto itVar = en->_variableMap.find((*it).first);

              if (itVar != en->_variableMap.end()) {
                _variableNames[(*it).second.registerId] = (*itVar).second;
              }
            }
          }
         
          // reserve space for the current row 
          _currentGroup.initialize(_aggregateRegisters.size());
          
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
            if (! ExecutionBlock::getBlock(atLeast, atMost)) {
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

          inheritRegisters(cur, res, _pos);
           
          size_t j = 0;

          while (j < atMost) {
            // read the next input tow
          
            bool newGroup = false;
            if (_currentGroup.groupValues[0].isEmpty()) {
              // we never had any previous group
              newGroup = true;
            }
            else {
              // we already had a group, check if the group has changed
              size_t i = 0;

              for (auto it = _aggregateRegisters.begin(); it != _aggregateRegisters.end(); ++it) {
                int cmp = AqlValue::Compare(_currentGroup.groupValues[i], 
                                            _currentGroup.collections[i],
                                            cur->getValue(_pos, (*it).second), 
                                            cur->getDocumentCollection((*it).second));
                if (cmp != 0) {
                  // group change
                  newGroup = true;
                  break;
                }
                ++i;
              }
            }

            if (newGroup) {
              if (! _currentGroup.groupValues[0].isEmpty()) {
                // need to emit the current group first

                emitGroup(cur, res, j);

                // increase output row count
                ++j;
                
                if (j == atMost) {
                  // output is full

                  // do NOT advance input pointer
                  return res;
                }
              }

              // still space left in the output to create a new group

              // construct the new group
              size_t i = 0;
              for (auto it = _aggregateRegisters.begin(); it != _aggregateRegisters.end(); ++it) {
                _currentGroup.groupValues[i] = cur->getValue(_pos, (*it).second).clone();
                _currentGroup.collections[i] = cur->getDocumentCollection((*it).second);
                ++i;
              }
              
              _currentGroup.setFirstRow(_pos);
            }
              
            _currentGroup.setLastRow(_pos);

            if (++_pos >= cur->size()) {
              _buffer.pop_front();
              _pos = 0;
          
              bool hasMore = ! _buffer.empty();
              if (! hasMore) {
                hasMore = ExecutionBlock::getBlock(atLeast, atMost);
              }

              if (! hasMore) {
                // no more input. we're done
                try {
                  // emit last buffered group
                  emitGroup(cur, res, j);
                  ++j;
                  delete cur;
                  cur = nullptr;

                  TRI_ASSERT(j > 0);
                  res->shrink(j);
                
                  _done = true;
                  return res;
                }
                catch (...) {
                  delete cur;
                  throw;
                }
              }

              // hasMore

              // move over the last group details into the group before we delete the block
              _currentGroup.addValues(cur, _groupRegister);

              delete cur;
              cur = _buffer.front();
            }
          }


          TRI_ASSERT(j > 0);
          res->shrink(j);

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief skipSome
////////////////////////////////////////////////////////////////////////////////

        size_t skipSome (size_t atLeast, size_t atMost) {
          if (_done) {
            return 0;
          }

          if (_buffer.empty()) {
            if (! ExecutionBlock::getBlock(DefaultBatchSize, DefaultBatchSize)) {
              _done = true;
              return 0;
            }
            _pos = 0;           // this is in the first block
          }

          // If we get here, we do have _buffer.front()
          AqlItemBlock* cur = _buffer.front();
          size_t skipped = 0;

          while (skipped < atLeast) {
            // read the next input tow

            bool newGroup = false;
            if (_currentGroup.groupValues[0].isEmpty()) {
              // we never had any previous group
              newGroup = true;
            }
            else {
              // we already had a group, check if the group has changed
              size_t i = 0;

              for (auto it = _aggregateRegisters.begin(); it != _aggregateRegisters.end(); ++it) {
                int cmp = AqlValue::Compare(_currentGroup.groupValues[i], 
                                            _currentGroup.collections[i],
                                            cur->getValue(_pos, (*it).second), 
                                            cur->getDocumentCollection((*it).second));
                if (cmp != 0) {
                  // group change
                  newGroup = true;
                  break;
                }
                ++i;
              }
            }

            if (newGroup) {
              if (! _currentGroup.groupValues[0].isEmpty()) {
                // increase output row count
                ++skipped;
                if ( skipped == atMost) {
                  return skipped;
                }
              }

              // construct the new group
              size_t i = 0;
              for (auto it = _aggregateRegisters.begin(); it != _aggregateRegisters.end(); ++it) {
                _currentGroup.groupValues[i] = cur->getValue(_pos, (*it).second).clone();
                _currentGroup.collections[i] = cur->getDocumentCollection((*it).second);
                ++i;
              }
              
            }

            if (++_pos >= cur->size()) {
              _buffer.pop_front();
              _pos = 0;
           
              bool hasMore = ! _buffer.empty();
              if (!hasMore) {
                hasMore = ExecutionBlock::getBlock(atLeast, atMost);
              }
              if (! hasMore) {
                try {
                  ++skipped;
                  delete cur;
                  cur = nullptr;
                  _done = true;
                  return skipped;
                }
                catch (...) {
                  delete cur;
                  throw;
                }
              }

              // hasMore

              delete cur;
              cur = _buffer.front();
            }
          }

          return skipped;
        }


      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief writes the current group data into the result
////////////////////////////////////////////////////////////////////////////////

        void emitGroup (AqlItemBlock const* cur,
                        AqlItemBlock* res, 
                        size_t row) {

          size_t i = 0;
          for (auto it = _aggregateRegisters.begin(); it != _aggregateRegisters.end(); ++it) {
            res->setValue(row, (*it).first, _currentGroup.groupValues[i]);
            ++i;
          }
            
          if (_groupRegister > 0) {
            // set the group values
            _currentGroup.addValues(cur, _groupRegister);

            res->setValue(row, _groupRegister, AqlValue::CreateFromBlocks(_currentGroup.groupBlocks, _variableNames));
          }
           
          // reset the group so a new one can start
          _currentGroup.reset();
        }

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief pairs, consisting of out register and in register
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::pair<RegisterId, RegisterId>> _aggregateRegisters;

////////////////////////////////////////////////////////////////////////////////
/// @brief details about the current group
////////////////////////////////////////////////////////////////////////////////

        AggregatorGroup _currentGroup;

////////////////////////////////////////////////////////////////////////////////
/// @brief the optional register that contains the values for each group
/// if no values should be returned, then this has a value of 0
////////////////////////////////////////////////////////////////////////////////
        
        RegisterId _groupRegister;

////////////////////////////////////////////////////////////////////////////////
/// @brief list of variables names for the registers
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::string> _variableNames;

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

          _sortRegisters.clear();

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
          for (auto block : _buffer) {
            sum += block->size();
          }

          coords.reserve(sum);

          // install the coords
          size_t count = 0;

          for (auto block : _buffer) {
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

          // here we collect the new blocks (later swapped into _buffer):
          std::deque<AqlItemBlock*> newbuffer;
          
          try {  // If we throw from here, the catch will delete the new
                 // blocks in newbuffer
            _buffer.clear();
            
            count = 0;
            RegisterId const nrregs = _buffer.front()->getNrRegs();
            
            // install the rearranged values from _buffer into newbuffer

            while (count < sum) {
              size_t sizeNext = std::min(sum - count, DefaultBatchSize);
              AqlItemBlock* next = new AqlItemBlock(sizeNext, nrregs);
              try {
                newbuffer.push_back(next);
              }
              catch (...) {
                delete next;
                throw;
              }
              std::unordered_map<AqlValue, AqlValue> cache;  
                  // only copy as much as needed!
              for (size_t i = 0; i < sizeNext; i++) {
                for (RegisterId j = 0; j < nrregs; j++) {
                  AqlValue a = _buffer[coords[count].first]->getValue(coords[count].second, j);
                  // If we have already dealt with this value for the next
                  // block, then we just put the same value again:
                  if (! a.isEmpty()) {
                    auto it = cache.find(a);
                    if (it != cache.end()) {
                      AqlValue b = it->second;
                      // If one of the following throws, all is well, because
                      // the new block already has either a copy or stolen
                      // the AqlValue:
                      _buffer[coords[count].first]->eraseValue(coords[count].second, j);
                      next->setValue(i, j, b);
                    }
                    else {
                      // We need to copy a, if it has already been stolen from
                      // its original buffer, which we know by looking at the
                      // valueCount there.
                      auto vCount = _buffer[coords[count].first]->valueCount(a);
                      if (vCount == 0) {
                        // Was already stolen for another block
                        AqlValue b = a.clone();
                        try {
                          cache.insert(make_pair(a,b));
                        }
                        catch (...) {
                          b.destroy();
                          throw;
                        }
                        try {
                          next->setValue(i, j, b);
                        }
                        catch (...) {
                          b.destroy();
                          cache.erase(a);
                          throw;
                        }
                        // It does not matter whether the following works or not,
                        // since the original block keeps its responsibility 
                        // for a:
                        _buffer[coords[count].first]->eraseValue(coords[count].second, j);
                      }
                      else {
                        // Here we are the first to want to inherit a, so we
                        // steal it:
                        _buffer[coords[count].first]->steal(a);
                        // If this has worked, responsibility is now with the
                        // new block or indeed with us!
                        try {
                          next->setValue(i, j, a);
                        }
                        catch (...) {
                          a.destroy();
                          throw;
                        }
                        _buffer[coords[count].first]->eraseValue(coords[count].second, j);
                        // This might throw as well, however, the responsibility
                        // is already with the new block.

                        // If the following does not work, we will create a
                        // few unnecessary copies, but this does not matter:
                        cache.insert(make_pair(a,a));
                      }
                    }
                  }
                }
                count++;
              }
              cache.clear();
              for (RegisterId j = 0; j < nrregs; j++) {
                next->setDocumentCollection(j, _buffer.front()->getDocumentCollection(j));
              }
            }
          }
          catch (...) {
            for (auto x : newbuffer) {
              delete x;
            }
          }
          _buffer.swap(newbuffer);  // does not throw since allocators
                                    // are the same
          for (auto x : newbuffer) {
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
              : _buffer(buffer), 
                _sortRegisters(sortRegisters), 
                _colls(colls) {
            }

            bool operator() (std::pair<size_t, size_t> const& a, 
                             std::pair<size_t, size_t> const& b) {

              size_t i = 0;
              for (auto reg : _sortRegisters) {
                int cmp = AqlValue::Compare(_buffer[a.first]->getValue(a.second, reg.first),
                                            _colls[i],
                                            _buffer[b.first]->getValue(b.second, reg.first),
                                            _colls[i]);
                if (cmp == -1) {
                  return reg.second;
                } 
                else if (cmp == 1) {
                  return ! reg.second;
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
              // TODO: here we're calling getSome to skip over elements
              // this must be implemented properly using skip() when it works
              // ATM skip() doesn't work here in the following case:
              // FOR i IN 0..99 LIMIT 10,50 LIMIT 1,20 RETURN i (returns wrong rows ATM)
              // ExecutionBlock::_dependencies[0]->skip(_offset);
              auto tmp = ExecutionBlock::_dependencies[0]->getSome(_offset, _offset);
              if (tmp != nullptr) {
                delete tmp;
              }

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

          size_t const n = res->size();

          // Let's steal the actual result and throw away the vars:
          auto ep = static_cast<ReturnNode const*>(getPlanNode());
          auto it = _varOverview->varInfo.find(ep->_inVariable->id);
          TRI_ASSERT(it != _varOverview->varInfo.end());
          RegisterId const registerId = it->second.registerId;
          AqlItemBlock* stripped = new AqlItemBlock(n, 1);

          try {
            for (size_t i = 0; i < n; i++) {
              AqlValue a = res->getValue(i, registerId);
              if (! a.isEmpty()) {
                res->steal(a);
                try {
                  stripped->setValue(i, 0, a);
                }
                catch (...) {
                  a.destroy();
                }
                // If the following does not go well, we do not care, since
                // the value is already stolen and installed in stripped
                res->eraseValue(i, registerId);
              }
            }
          }
          catch (...) {
            delete stripped;
            delete res;
            throw;
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


