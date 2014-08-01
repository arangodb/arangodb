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
          : _exePlan(ep), _done(false), _depth(0) { }

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
          unsigned int index;
          VarInfo(int depth, int index) : depth(depth), index(index) {}
        };

        struct VarOverview : public WalkerWorker {
          // The following are collected for global usage in the ExecutionBlock:

          // map VariableIds to their depth and index:
          std::unordered_map<VariableId, VarInfo> varInfo;

          // number of variables in the frame of the current depth:
          std::vector<VariableId>                 nrVarsHere;

          // number of variables in this and all outer frames together,
          // the entry with index i here is always the sum of all values
          // in nrVarsHere from index 0 to i (inclusively) and the two
          // have the same length:
          std::vector<VariableId>                 nrVars;

          // Local for the walk:
          unsigned int depth;
          unsigned int totalNrVars;

          // This is used to tell all Blocks and share a pointer to ourselves
          shared_ptr<VarOverview> me;

          VarOverview () 
            : depth(0), totalNrVars(0) {
            nrVarsHere.push_back(0);
            nrVars.push_back(0);
            me.reset(this);
          };

          // Copy constructor used for a subquery:
          VarOverview (VarOverview const& v) 
            : varInfo(v.varInfo), nrVarsHere(v.nrVars), nrVars(v.nrVars),
              depth(v.depth+1), totalNrVars(v.totalNrVars) {
            nrVarsHere.push_back(0);
            nrVars.push_back(0);
          }

          ~VarOverview () {};

          virtual bool enterSubquery (ExecutionBlock* super,
                                      ExecutionBlock* sub) {
            auto vv = new VarOverview(*this);
            sub->walk(vv);
            vv->reset();
            return false;  // do not walk into subquery
          }

          virtual void after (ExecutionBlock *eb) {
            switch (eb->getPlanNode()->getType()) {
              case ExecutionNode::ENUMERATE_COLLECTION: {
                depth++;
                nrVarsHere.push_back(1);
                nrVars.push_back(1 + nrVars.back());
                auto ep = static_cast<EnumerateCollectionNode const*>(eb->getPlanNode());
                varInfo.insert(make_pair(ep->_outVariable->id,
                                         VarInfo(depth, totalNrVars)));
                totalNrVars++;
                break;
              }
              case ExecutionNode::ENUMERATE_LIST: {
                depth++;
                nrVarsHere.push_back(1);
                nrVars.push_back(1 + nrVars.back());
                auto ep = static_cast<EnumerateListNode const*>(eb->getPlanNode());
                varInfo.insert(make_pair(ep->_outVariable->id,
                                         VarInfo(depth, totalNrVars)));
                totalNrVars++;
                break;
              }
              case ExecutionNode::CALCULATION: {
                nrVarsHere[depth]++;
                nrVars[depth]++;
                auto ep = static_cast<CalculationNode const*>(eb->getPlanNode());
                varInfo.insert(make_pair(ep->_outVariable->id,
                                         VarInfo(depth, totalNrVars)));
                totalNrVars++;
                break;
              }
              case ExecutionNode::PROJECTION: {
                nrVarsHere[depth]++;
                nrVars[depth]++;
                auto ep = static_cast<ProjectionNode const*>(eb->getPlanNode());
                varInfo.insert(make_pair(ep->_outVariable->id,
                                         VarInfo(depth, totalNrVars)));
                totalNrVars++;
                break;
              }
              case ExecutionNode::SUBQUERY: {
                nrVarsHere[depth]++;
                nrVars[depth]++;
                auto ep = static_cast<SubqueryNode const*>(eb->getPlanNode());
                varInfo.insert(make_pair(ep->_outVariable->id,
                                         VarInfo(depth, totalNrVars)));
                totalNrVars++;
                break;
              }
              // TODO: potentially more cases
              default:
                break;
            }
            eb->_depth = depth;
            eb->_varOverview = me;
          }

        };

        void staticAnalysis () {
          auto v = new VarOverview();
          walk(v);
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

          if (_buffer.size() == 0) {
            if (! getBlock(1, 1000)) {
              _done = true;
              return nullptr;
            }
            _pos = 0;
          }

          // Here, _buffer.size() is > 0 and _pos points to a valid place
          // in it.

          AqlItemBlock* res;
          AqlItemBlock* cur;
          cur = _buffer.front();
          res = cur->slice(_pos, _pos+1);
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
            if (_buffer.size() == 0) {
              if (! getBlock(atLeast-total, atMost-total)) {
                _done = true;
                break;
              }
            }
            AqlItemBlock* cur = _buffer.front();
            if (cur->size() - _pos + total > atMost) {
              // The current block is too large for atMost:
              collector.push_back(cur->slice(_pos, _pos + (atMost-total)));
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
          if (collector.size() == 0) {
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

          AqlItemBlock* res(new AqlItemBlock(1, _varOverview->nrVars[_depth]));
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

          AqlItemBlock* res(new AqlItemBlock(1, _varOverview->nrVars[_depth]));
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
          if (_allDocs.size() > 0) {
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
          
          vector<TRI_doc_mptr_t*> docs;
          res = trx.read(docs);
          size_t const n = docs.size();

          auto shaper = trx.documentCollection()->getShaper();

          _allDocs.clear();
          for (size_t i = 0; i < n; ++i) {
            TRI_shaped_json_t shaped;
            TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, docs[i]->getDataPtr());
            _allDocs.push_back(new Json(shaper->_memoryZone,
                                        TRI_JsonShapedJson(shaper, &shaped)));
          }
          
          res = trx.finish(res);

          if (_allDocs.size() == 0) {
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
          if (_allDocs.size() == 0) {
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
          if (_buffer.size() == 0) {
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
          auto res = new AqlItemBlock(1, _varOverview->nrVars[_depth]);
          TRI_ASSERT(cur->getNrVars() <= res->getNrVars());
          for (VariableId i = 0; i < cur->getNrVars(); i++) {
            res->setValue(0, i, cur->getValue(_pos, i)->clone());
          }

          // The result is in the first variable of this depth,
          // we do not need to do a lookup in _varOverview->varInfo,
          // but can just take cur->getNrVars() as index:
          res->setValue(0, cur->getNrVars(), 
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
          if (_buffer.size() == 0) {
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

          auto res = new AqlItemBlock(toSend, _varOverview->nrVars[_depth]);
          TRI_ASSERT(cur->getNrVars() <= res->getNrVars());
          for (size_t j = 0; j < toSend; j++) {
            for (VariableId i = 0; i < cur->getNrVars(); i++) {
              res->setValue(j, i, cur->getValue(_pos, i)->clone());
            }
            // The result is in the first variable of this depth,
            // we do not need to do a lookup in _varOverview->varInfo,
            // but can just take cur->getNrVars() as index:
            res->setValue(j, cur->getNrVars(), 
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

        vector<Json*> _allDocs;

////////////////////////////////////////////////////////////////////////////////
/// @brief current position in _allDocs
////////////////////////////////////////////////////////////////////////////////

        size_t _posInAllDocs;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                         RootBlock
// -----------------------------------------------------------------------------

    class RootBlock : public ExecutionBlock {

      public:

        RootBlock (RootNode const* ep)
          : ExecutionBlock(ep) {

        }

        ~RootBlock () {
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
          auto ep = static_cast<RootNode const*>(getPlanNode());
          auto it = _varOverview->varInfo.find(ep->_inVariable->id);
          TRI_ASSERT(it != _varOverview->varInfo.end());
          unsigned int index = it->second.index;
          stripped->setValue(0, 0, res->getValue(0, index));
          res->setValue(0, index, nullptr);
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
          auto ep = static_cast<RootNode const*>(getPlanNode());
          auto it = _varOverview->varInfo.find(ep->_inVariable->id);
          TRI_ASSERT(it != _varOverview->varInfo.end());
          unsigned int index = it->second.index;
          AqlItemBlock* stripped = new AqlItemBlock(res->size(), 1);
          for (size_t i = 0; i < res->size(); i++) {
            stripped->setValue(i, 0, res->getValue(i, index));
            res->setValue(i, index, nullptr);
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


