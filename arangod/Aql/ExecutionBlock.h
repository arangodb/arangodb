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
          
        // Methods for execution:
        int initialise () {
          return TRI_ERROR_NO_ERROR;
        }

        int bind (std::map<std::string, struct TRI_json_s*>* params);

        std::map<std::string, struct TRI_json_s*>* getParameters ();

        int execute () {
          return TRI_ERROR_NO_ERROR;
        }

        int shutdown () {
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
    };


    class EnumerateCollectionBlock : public ExecutionBlock {

      public:

        EnumerateCollectionBlock (EnumerateCollectionPlan const* ep)
          : ExecutionBlock(ep) {

        }

        ~EnumerateCollectionBlock () {
          std::cout << "ENUMERATECOLLECTIONBLOCK DTOR\n";
        } 

        AqlValue* getOne () {
          if (_buffer.empty()) {
              
          }

          auto value = _buffer.front();
          _buffer.pop_front();
          return value;
        }
        
    };

  }  // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


