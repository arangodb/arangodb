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
#include "Aql/ExecutionNode.h"
#include "Utils/transactions.h"

// #include "V8/v8.h"

using namespace triagens::basics;

struct TRI_json_s;

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                    ExecutionBlock
// -----------------------------------------------------------------------------

    class ExecutionBlock {
      public:
        ExecutionBlock (ExecutionNode const* ep)
          : _exePlan(ep), _done(false), _depth(0), _varOverview(nullptr) { 
        }

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
            if (! getBlock(1, 1000)) {
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

          // Here, _buffer.size() is > 0 and _pos points to a valid place
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

        bool skip (int number);

        virtual int64_t count () {
          return 0;
        }

        virtual int64_t remaining () {
          return 0;
        }

        ExecutionNode const* getPlanNode () {
          return _exePlan;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief our corresponding ExecutionNode node
////////////////////////////////////////////////////////////////////////////////

      protected:

        ExecutionNode const* _exePlan;

////////////////////////////////////////////////////////////////////////////////
/// @brief our dependent nodes
////////////////////////////////////////////////////////////////////////////////

        std::vector<ExecutionBlock*> _dependencies;

////////////////////////////////////////////////////////////////////////////////
/// @brief this is our buffer for the items, it is a deque of AqlItemBlocks
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

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                    SingletonBlock
// -----------------------------------------------------------------------------

    class SingletonBlock : public ExecutionBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        SingletonBlock (SingletonNode const* ep)
          : ExecutionBlock(ep) {
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

    };

// -----------------------------------------------------------------------------
// --SECTION--                                          EnumerateCollectionBlock
// -----------------------------------------------------------------------------

    class EnumerateCollectionBlock : public ExecutionBlock {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        EnumerateCollectionBlock (EnumerateCollectionNode const* ep)
          : ExecutionBlock(ep), _posInAllDocs(0) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~EnumerateCollectionBlock () {
          if (! _allDocs.empty()) {
            for (auto it = _allDocs.begin(); it != _allDocs.end(); ++it) {
              delete *it;
            }
            _allDocs.clear();
          }
        }
        
////////////////////////////////////////////////////////////////////////////////
/// @brief initialize, here we fetch all docs from the database
////////////////////////////////////////////////////////////////////////////////

        int initialize () {
          // TODO: this is very very inefficient
          // it must be implemented properly for production

          int res = ExecutionBlock::initialize();
          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          auto p = reinterpret_cast<EnumerateCollectionNode const*>(_exePlan);

          V8ReadTransaction trx(p->_vocbase, p->_collname);
  
          res = trx.begin();

          if (res != TRI_ERROR_NO_ERROR) {
            THROW_ARANGO_EXCEPTION(res);
          }

          std::string collectionName = trx.resolver()->getCollectionName(trx.cid());
          collectionName.push_back('/');
          
          vector<TRI_doc_mptr_t*> docs;
          res = trx.read(docs);
          size_t const n = docs.size();

          auto shaper = trx.documentCollection()->getShaper();

          // TODO: if _allDocs is not empty, its contents will leak
          _allDocs.clear();
          for (size_t i = 0; i < n; ++i) {
            TRI_doc_mptr_t const* mptr = docs[i];
            TRI_shaped_json_t shaped;
            TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, mptr->getDataPtr());

            Json* json = new Json(shaper->_memoryZone, TRI_JsonShapedJson(shaper, &shaped));

            try {
              std::string id(collectionName);
              id += std::string(TRI_EXTRACT_MARKER_KEY(mptr));
              (*json)("_id", Json(id));
              (*json)("_rev", Json(std::to_string(mptr->_rid))); 
              (*json)("_key", Json(TRI_EXTRACT_MARKER_KEY(mptr)));

              _allDocs.push_back(json);
            }
            catch (...) {
              delete json;
              throw;
            }
          }
          
          res = trx.finish(res);

          if (_allDocs.empty()) {
            _done = true;
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute, here we release our docs from this collection
////////////////////////////////////////////////////////////////////////////////

        int execute () {
          int res = ExecutionBlock::execute();
          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }
          if (_allDocs.empty()) {
            _done = true;
          }
          return TRI_ERROR_NO_ERROR;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown, here we release our docs from this collection
////////////////////////////////////////////////////////////////////////////////

        int shutdown () {
          int res = ExecutionBlock::shutdown();  // Tell all dependencies
          for (auto it = _allDocs.begin(); it != _allDocs.end(); ++it) {
            delete *it;
          }
          _allDocs.clear();
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
            if (! ExecutionBlock::getBlock(1, 1000)) {
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
            res->setValue(0, i, cur->getValue(_pos, i)->clone());
          }

          // The result is in the first variable of this depth,
          // we do not need to do a lookup in _varOverview->varInfo,
          // but can just take cur->getNrRegs() as registerId:
          res->setValue(0, cur->getNrRegs(), 
              new AqlValue( new Json(_allDocs[_posInAllDocs++]->copy()) ) );

          // Advance read position:
          if (_posInAllDocs >= _allDocs.size()) {
            _posInAllDocs = 0;
            if (++_pos >= cur->size()) {
              _buffer.pop_front();
              delete cur;
              _pos = 0;
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
            if (! ExecutionBlock::getBlock(1000, 1000)) {
              _done = true;
              return nullptr;
            }
            _pos = 0;           // this is in the first block
            _posInAllDocs = 0;  // Note that we know _allDocs.size() > 0,
                                // otherwise _done would be true already
          }

          // If we get here, we do have _buffer.front()
          AqlItemBlock* cur = _buffer.front();

          size_t available = _allDocs.size() - _posInAllDocs;
          size_t toSend = std::min(atMost, available);

          auto res = new AqlItemBlock(toSend, _varOverview->nrRegs[_depth]);
          TRI_ASSERT(cur->getNrRegs() <= res->getNrRegs());
          for (size_t j = 0; j < toSend; j++) {
            for (RegisterId i = 0; i < cur->getNrRegs(); i++) {
              res->setValue(j, i, cur->getValue(_pos, i)->clone());
            }
            // The result is in the first variable of this depth,
            // we do not need to do a lookup in _varOverview->varInfo,
            // but can just take cur->getNrRegs() as registerId:
            res->setValue(j, cur->getNrRegs(), 
                new AqlValue( new Json(_allDocs[_posInAllDocs++]->copy()) ) );
          }

          // Advance read position:
          if (_posInAllDocs >= _allDocs.size()) {
            _posInAllDocs = 0;
            if (++_pos >= cur->size()) {
              _buffer.pop_front();
              delete cur;
              _pos = 0;
            }
          }
          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief all documents of this collection
////////////////////////////////////////////////////////////////////////////////

      private:

        std::vector<Json*> _allDocs;

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

        CalculationBlock (CalculationNode const* en)
          : ExecutionBlock(en), _expression(en->expression()), _outReg(0) {

        }

        ~CalculationBlock () {
        } 

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

        void doEvaluation (AqlItemBlock* result) {
          TRI_ASSERT(result != nullptr);

          AqlValue** data = result->getData();
          TRI_ASSERT(data != nullptr);

          RegisterId nrRegs = result->getNrRegs();

          for (size_t i = 0; i < result->size(); i++) {
            AqlValue* res = _expression->execute(data + nrRegs * i,
                                                 _inVars, _inRegs);
            result->setValue(i, _outReg, res);
          }
        }

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
        std::vector<Variable*> _inVars;
        std::vector<RegisterId> _inRegs;
        RegisterId _outReg;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                       ReturnBlock
// -----------------------------------------------------------------------------

    class ReturnBlock : public ExecutionBlock {

      public:

        ReturnBlock (ReturnNode const* ep)
          : ExecutionBlock(ep) {

        }

        ~ReturnBlock () {
        } 

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
          res->setValue(0, registerId, nullptr);
          delete res;
          return stripped;
        }

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
            res->setValue(i, registerId, nullptr);
          }
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


