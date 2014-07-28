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

#include "Aql/Types.h"
#include "Aql/ExecutionPlan.h"
#include "Utils/transactions.h"

struct TRI_json_s;

namespace triagens {
  namespace aql {

    class ExecutionBlock {
      public:
        ExecutionBlock (ExecutionPlan const* ep)
          : _exePlan(ep) { }

        virtual ~ExecutionBlock () { 
          std::cout << "EXECUTIONBLOCK DTOR\n";
          for (auto i = _dependencies.begin(); i != _dependencies.end(); ++i) {
            delete *i;
          }
        }
          
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
        virtual int initialise () {
          return TRI_ERROR_NO_ERROR;
        }

        virtual int bind (std::map<std::string, struct TRI_json_s*>* params);

        std::map<std::string, struct TRI_json_s*>* getParameters ();

        virtual int execute () {
          return TRI_ERROR_NO_ERROR;
        }

        virtual int shutdown () {
          return TRI_ERROR_NO_ERROR;
        }

        virtual AqlValue* getOne () = 0;

        std::vector<AqlValue*> getSome (int atLeast, int atMost);

        bool skip (int number);

        int64_t count ();

        int64_t remaining ();

      protected:
        ExecutionPlan const* _exePlan;
        std::vector<ExecutionBlock*> _dependencies;
        std::deque<AqlValue*> _buffer;

      public:

        static ExecutionBlock* instanciatePlan (ExecutionPlan const* ep);

    };


    class EnumerateCollectionBlock : public ExecutionBlock {

      public:

        EnumerateCollectionBlock (EnumerateCollectionPlan const* ep)
          : ExecutionBlock(ep) {
        }

        ~EnumerateCollectionBlock () {
          std::cout << "ENUMERATECOLLECTIONBLOCK DTOR\n";
        }
        
        int initialize () {
          // TODO: this is very very inefficient
          // it must be implemented properly for production
          auto p = reinterpret_cast<EnumerateCollectionPlan const*>(_exePlan);

          V8ReadTransaction trx(p->_vocbase, p->_collname);
  
          int res = trx.begin();
          
          vector<TRI_doc_mptr_t*> docs;
          res = trx.read(docs);
          size_t const n = docs.size();

          for (size_t i = 0; i < n; ++i) {
            _buffer.push_back(new AqlValue(docs[i]));
          }
          
          res = trx.finish(res);
          return res;
        }

        int shutdown () {
          return TRI_ERROR_NO_ERROR;
        }

        AqlValue* getOne () {
          if (_buffer.empty()) {
            return nullptr;  
          }

          auto value = _buffer.front();
          _buffer.pop_front();
          return value;
        }
        
    };

    class RootBlock : public ExecutionBlock {

      public:

        RootBlock (RootPlan const* ep)
          : ExecutionBlock(ep) {

        }

        ~RootBlock () {
          std::cout << "ROOTBLOCK DTOR\n";
        } 

        AqlValue* getOne () {
          return _dependencies[0]->getOne();
        }
        
    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


