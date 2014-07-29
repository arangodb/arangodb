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
#include "Aql/ExecutionPlan.h"
#include "Utils/transactions.h"

using namespace triagens::basics;

struct TRI_json_s;

namespace triagens {
  namespace aql {

    class ExecutionBlock {
      public:
        ExecutionBlock (ExecutionPlan const* ep)
          : _exePlan(ep), _done(false) { }

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


        // Methods for execution:
        virtual int initialize () {
          for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
            (*it)->initialize();
          }
          // FIXME: report errors from above
          return TRI_ERROR_NO_ERROR;
        }

        virtual int bind (std::map<std::string, struct TRI_json_s*>* params);

        std::map<std::string, struct TRI_json_s*>* getParameters ();

        virtual int execute () {
          for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
            (*it)->execute();
          }
          // FIXME: report errors from above
          _done = false;
          return TRI_ERROR_NO_ERROR;
        }

        virtual int shutdown () {
          for (auto it = _dependencies.begin(); it != _dependencies.end(); ++it) {
            (*it)->shutdown();
          }
          // FIXME: report errors from above
          return TRI_ERROR_NO_ERROR;
        }

      protected:
        bool bufferMore (size_t atLeast, size_t atMost) {
          std::vector<shared_ptr<AqlItem> > docs 
            = _dependencies[0]->getSome(atLeast, atMost);
          if (docs.size() == 0) {
            return false;
          }
          for (auto it = docs.begin(); it != docs.end(); ++it) {
            _buffer.push_back(*it);
          }
          return true;
        }

      public:
        virtual shared_ptr<AqlItem> getOne () {
          shared_ptr<AqlItem> res;
          if (_buffer.size() != 0) {
            res = _buffer.front();
            _buffer.pop_front();
            return res;
          }
          return _dependencies[0]->getOne();
        }

        virtual std::vector<shared_ptr<AqlItem> > getSome (size_t atLeast, 
                                                           size_t atMost) {
          std::vector<shared_ptr<AqlItem> > res;
          if (_done) {
            return res;
          }
          if (_buffer.size() < atLeast) {
            if (! bufferMore(atLeast-_buffer.size(), atMost-_buffer.size()) &&
                _buffer.size() == 0) {
              _done = true;
              return res;
            }
          }
          size_t nr = _buffer.size();
          if (nr > atMost) {
            nr = atMost;
          }
          while (nr-- > 0) {
            res.push_back(_buffer.front());
            _buffer.pop_front();
          }
          return res;
        }


        bool skip (int number);

        virtual int64_t count () {
          return _dependencies[0]->count();
        }

        virtual int64_t remaining () {
          return _dependencies[0]->remaining() + _buffer.size();
        }

      protected:
        ExecutionPlan const* _exePlan;
        std::vector<ExecutionBlock*> _dependencies;
        std::deque<shared_ptr<AqlItem> > _buffer;
        bool _done;

      public:

        static ExecutionBlock* instanciatePlan (ExecutionPlan const* ep);

    };


    class SingletonBlock : public ExecutionBlock {

      public:

        SingletonBlock (SingletonPlan const* ep)
          : ExecutionBlock(ep) {
        }

        ~SingletonBlock () {
        }

        int initialize () {
          ExecutionBlock::initialize();
          return TRI_ERROR_NO_ERROR;
        }

        int execute () {
          ExecutionBlock::execute();
          return TRI_ERROR_NO_ERROR;
        }

        int shutdown () {
          return TRI_ERROR_NO_ERROR;
        }

        shared_ptr<AqlItem> getOne () {
          if (_done) {
            return nullptr;
          }

          auto p = reinterpret_cast<SingletonPlan const*>(_exePlan);
          shared_ptr<AqlItem> res(new AqlItem(p->_nrVars));
          _done = true;
          return res;
        }

        vector<shared_ptr<AqlItem> > getSome (size_t atLeast, size_t atMost) {
          vector<shared_ptr<AqlItem> > res;

          if (_done) {
            return res;
          }

          auto p = reinterpret_cast<SingletonPlan const*>(_exePlan);
          res.emplace_back(new AqlItem(p->_nrVars));
          _done = true;
          return res;
        }

    };

    class EnumerateCollectionBlock : public ExecutionBlock {

      public:

        EnumerateCollectionBlock (EnumerateCollectionPlan const* ep)
          : ExecutionBlock(ep) {
        }

        ~EnumerateCollectionBlock () {
        }
        
        int initialize () {
          // TODO: this is very very inefficient
          // it must be implemented properly for production

          int res = ExecutionBlock::initialize();
          if (res != TRI_ERROR_NO_ERROR) {
            return res;
          }

          auto p = reinterpret_cast<EnumerateCollectionPlan const*>(_exePlan);

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
          _buffer.clear();

          return res;
        }

        int shutdown () {
          int res = ExecutionBlock::shutdown();  // Tell all dependencies
          _allDocs.clear();
          return res;
        }

        shared_ptr<AqlItem> getOne () {
          if (_done) {
            return nullptr;
          }
          if (_buffer.size() == 0) {
            if (! bufferMore(1,1)) {
              _done = true;
              return nullptr;
            }
            _pos = 0;
            if (_allDocs.size() == 0) {  // just in case
              _done = true;
              return nullptr;
            }
          }
          shared_ptr<AqlItem> res(new AqlItem(_buffer.front(), 1));
          res->setValue(0,0,new AqlValue( new Json(_allDocs[_pos]->copy())));
          if (++_pos >= _allDocs.size()) {
            _buffer.pop_front();
          }
          return res;
        }

      private:

        vector<Json*> _allDocs;
        size_t _pos;
    };

    class RootBlock : public ExecutionBlock {

      public:

        RootBlock (RootPlan const* ep)
          : ExecutionBlock(ep) {

        }

        ~RootBlock () {
        } 

    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


