////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, query context
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_QUERY_H
#define ARANGODB_AQL_QUERY_H 1

#include "Basics/Common.h"
#include "Basics/JsonHelper.h"
#include "Aql/BindParameters.h"
#include "Aql/Collections.h"
#include "Aql/QueryResultV8.h"
#include "Aql/ShortStringStorage.h"
#include "Aql/types.h"
#include "Utils/AqlTransaction.h"
#include "Utils/V8TransactionContext.h"
#include "VocBase/voc-types.h"
#include "V8Server/ApplicationV8.h"

struct TRI_json_t;
struct TRI_vocbase_s;

namespace triagens {
  namespace arango {
    class TransactionContext;
  }

  namespace aql {

    struct AstNode;
    class Ast;
    class ExecutionEngine;
    class ExecutionPlan;
    class Executor;
    class Expression;
    class Parser;
    class Query;
    class QueryRegistry;
    struct Variable;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief equery part
////////////////////////////////////////////////////////////////////////////////

    enum QueryPart {
      PART_MAIN,
      PART_DEPENDENT
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief the type of query to execute
////////////////////////////////////////////////////////////////////////////////

    enum QueryType {
      AQL_QUERY_READ,
      AQL_QUERY_REMOVE,
      AQL_QUERY_INSERT,
      AQL_QUERY_UPDATE,
      AQL_QUERY_REPLACE,
      AQL_QUERY_UPSERT
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief execution states
////////////////////////////////////////////////////////////////////////////////

    enum ExecutionState {
      INITIALIZATION        = 0,
      PARSING,
      AST_OPTIMIZATION,
      PLAN_INSTANCIATION,
      PLAN_OPTIMIZATION,
      EXECUTION,
      FINALIZATION,

      INVALID_STATE
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                    struct Profile
// -----------------------------------------------------------------------------

    struct Profile {
      Profile (Profile const&) = delete;
      Profile& operator= (Profile const&) = delete;

      explicit Profile (Query*);

      ~Profile ();

      void enter (ExecutionState);

      TRI_json_t* toJson (TRI_memory_zone_t*);

      Query*                                         query;
      std::vector<std::pair<ExecutionState, double>> results;
      double                                         stamp;
      bool                                           tracked;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                       class Query
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief an AQL query
////////////////////////////////////////////////////////////////////////////////

    class Query {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        Query (triagens::arango::ApplicationV8*,
               bool,
               struct TRI_vocbase_s*,
               char const*,
               size_t,
               struct TRI_json_t*,
               struct TRI_json_t*,
               QueryPart);

        Query (triagens::arango::ApplicationV8*,
               bool,
               struct TRI_vocbase_s*,
               triagens::basics::Json queryStruct,
               struct TRI_json_t*,
               QueryPart);

        ~Query ();

////////////////////////////////////////////////////////////////////////////////
/// @brief clone a query
/// note: as a side-effect, this will also create and start a transaction for
/// the query
////////////////////////////////////////////////////////////////////////////////

        Query* clone (QueryPart, bool);

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the query is killed
////////////////////////////////////////////////////////////////////////////////

        inline bool killed () const {
          return _killed;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the query to killed
////////////////////////////////////////////////////////////////////////////////
        
        inline void killed (bool) {
          _killed = true;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief the part of the query
////////////////////////////////////////////////////////////////////////////////

        inline QueryPart part () const {
          return _part;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the vocbase
////////////////////////////////////////////////////////////////////////////////

        inline struct TRI_vocbase_s* vocbase () const {
          return _vocbase;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief collections
////////////////////////////////////////////////////////////////////////////////

        inline Collections* collections () {
          return &_collections;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the names of collections used in the query
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::string> collectionNames () const {
          return _collections.collectionNames();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the query's id
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_tick_t id () const {
          return _id;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the query string
////////////////////////////////////////////////////////////////////////////////

        char const* queryString () const {
          return _queryString;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the length of the query string
////////////////////////////////////////////////////////////////////////////////

        size_t queryLength () const {
          return _queryLength;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for _ast
////////////////////////////////////////////////////////////////////////////////

        Ast* ast () const {
          return _ast;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief add a node to the list of nodes
////////////////////////////////////////////////////////////////////////////////

        void addNode (AstNode*);

////////////////////////////////////////////////////////////////////////////////
/// @brief should we return verbose plans?
////////////////////////////////////////////////////////////////////////////////

        bool verbosePlans () const {  
          return getBooleanOption("verbosePlans", false);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief should we return all plans?
////////////////////////////////////////////////////////////////////////////////

        bool allPlans () const {  
          return getBooleanOption("allPlans", false);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief should the execution be profiled?
////////////////////////////////////////////////////////////////////////////////

        bool profiling () const {  
          return getBooleanOption("profile", false);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum number of plans to produce
////////////////////////////////////////////////////////////////////////////////

        size_t maxNumberOfPlans () const { 
          double value = getNumericOption("maxNumberOfPlans", 0.0);
          if (value > 0) {
            return static_cast<size_t>(value);
          }
          return 0;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a region from the query
////////////////////////////////////////////////////////////////////////////////

        std::string extractRegion (int, 
                                   int) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error, with an optional parameter inserted into printf
/// this also makes the query abort
////////////////////////////////////////////////////////////////////////////////

        void registerError (int,
                            char const* = nullptr);

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error with a custom error message
/// this also makes the query abort
////////////////////////////////////////////////////////////////////////////////
        
        void registerErrorCustom (int,
                                  char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief register a warning
////////////////////////////////////////////////////////////////////////////////

        void registerWarning (int,
                              char const* = nullptr);

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare an AQL query, this is a preparation for execute, but 
/// execute calls it internally. The purpose of this separate method is
/// to be able to only prepare a query from JSON and then store it in the
/// QueryRegistry.
////////////////////////////////////////////////////////////////////////////////

        QueryResult prepare (QueryRegistry*);

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an AQL query 
////////////////////////////////////////////////////////////////////////////////

        QueryResult execute (QueryRegistry*);

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an AQL query 
/// may only be called with an active V8 handle scope
////////////////////////////////////////////////////////////////////////////////

        QueryResultV8 executeV8 (v8::Isolate* isolate, QueryRegistry*);


////////////////////////////////////////////////////////////////////////////////
/// @brief parse an AQL query
////////////////////////////////////////////////////////////////////////////////

        QueryResult parse ();

////////////////////////////////////////////////////////////////////////////////
/// @brief explain an AQL query 
////////////////////////////////////////////////////////////////////////////////

        QueryResult explain ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get v8 executor
////////////////////////////////////////////////////////////////////////////////

        Executor* executor ();

////////////////////////////////////////////////////////////////////////////////
/// @brief register a string
/// the string is freed when the query is destroyed
////////////////////////////////////////////////////////////////////////////////

        char* registerString (char const*,
                              size_t,
                              bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief register a string
/// the string is freed when the query is destroyed
////////////////////////////////////////////////////////////////////////////////

        char* registerString (std::string const&,
                              bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the engine, if prepared
////////////////////////////////////////////////////////////////////////////////

        ExecutionEngine* engine () {
          return _engine;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief inject the engine
////////////////////////////////////////////////////////////////////////////////

        void engine (ExecutionEngine* engine) {
          _engine = engine;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the transaction, if prepared
////////////////////////////////////////////////////////////////////////////////

        inline triagens::arango::AqlTransaction* trx () {
          return _trx;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the plan for the query
////////////////////////////////////////////////////////////////////////////////

        ExecutionPlan* plan () const {
          return _plan;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the query returns verbose error messages
////////////////////////////////////////////////////////////////////////////////

        bool verboseErrors () const {
          return getBooleanOption("verboseErrors", false);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the plan for the query
////////////////////////////////////////////////////////////////////////////////

        void setPlan (ExecutionPlan *plan);

////////////////////////////////////////////////////////////////////////////////
/// @brief enter a V8 context
////////////////////////////////////////////////////////////////////////////////

        void enterContext ();

////////////////////////////////////////////////////////////////////////////////
/// @brief exits a V8 context
////////////////////////////////////////////////////////////////////////////////

        void exitContext ();

////////////////////////////////////////////////////////////////////////////////
/// @brief returns statistics for current query.
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Json getStats();

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch a boolean value from the options
////////////////////////////////////////////////////////////////////////////////

        bool getBooleanOption (char const*, 
                               bool) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief convert the list of warnings to JSON
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t* warningsToJson (TRI_memory_zone_t*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the global query tracking value
////////////////////////////////////////////////////////////////////////////////

        static bool DisableQueryTracking () {
          return DoDisableQueryTracking;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief turn off tracking globally
////////////////////////////////////////////////////////////////////////////////
        
        static void DisableQueryTracking (bool value) {
          DoDisableQueryTracking = value;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch a numeric value from the options
////////////////////////////////////////////////////////////////////////////////

        double getNumericOption (char const*, 
                                 double) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief read the "optimizer.inspectSimplePlans" section from the options
////////////////////////////////////////////////////////////////////////////////

        bool inspectSimplePlans () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief read the "optimizer.rules" section from the options
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::string> getRulesFromOptions () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief neatly format transaction errors to the user.
////////////////////////////////////////////////////////////////////////////////

        QueryResult transactionError (int errorCode) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief enter a new state
////////////////////////////////////////////////////////////////////////////////

        void enterState (ExecutionState);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a description of the query's current state
////////////////////////////////////////////////////////////////////////////////

        std::string getStateString () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup plan and engine for current query
////////////////////////////////////////////////////////////////////////////////

        void cleanupPlanAndEngine (int);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a TransactionContext
////////////////////////////////////////////////////////////////////////////////

        triagens::arango::TransactionContext* createTransactionContext ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief query id
////////////////////////////////////////////////////////////////////////////////

        TRI_voc_tick_t const _id;

////////////////////////////////////////////////////////////////////////////////
/// @brief application v8 used in the query, we need this for V8 context access
////////////////////////////////////////////////////////////////////////////////

        triagens::arango::ApplicationV8*  _applicationV8;
        
////////////////////////////////////////////////////////////////////////////////
/// @brief all nodes created in the AST - will be used for freeing them later
////////////////////////////////////////////////////////////////////////////////

        std::vector<AstNode*>             _nodes;

////////////////////////////////////////////////////////////////////////////////
/// @brief pointer to vocbase the query runs in
////////////////////////////////////////////////////////////////////////////////

        struct TRI_vocbase_s*             _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 code executor
////////////////////////////////////////////////////////////////////////////////
        
        Executor*                         _executor;

////////////////////////////////////////////////////////////////////////////////
/// @brief the currently used V8 context
////////////////////////////////////////////////////////////////////////////////

        triagens::arango::ApplicationV8::V8Context* _context;

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual query string
////////////////////////////////////////////////////////////////////////////////

        char const*                       _queryString;

////////////////////////////////////////////////////////////////////////////////
/// @brief length of the query string in bytes
////////////////////////////////////////////////////////////////////////////////

        size_t const                      _queryLength;

////////////////////////////////////////////////////////////////////////////////
/// @brief query in a JSON structure
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Json const      _queryJson;

////////////////////////////////////////////////////////////////////////////////
/// @brief bind parameters for the query
////////////////////////////////////////////////////////////////////////////////

        BindParameters                    _bindParameters;

////////////////////////////////////////////////////////////////////////////////
/// @brief query options
////////////////////////////////////////////////////////////////////////////////

        TRI_json_t*                       _options;

////////////////////////////////////////////////////////////////////////////////
/// @brief collections used in the query
////////////////////////////////////////////////////////////////////////////////

        Collections                       _collections;

////////////////////////////////////////////////////////////////////////////////
/// @brief strings created in the query - used for easy memory deallocation
////////////////////////////////////////////////////////////////////////////////

        std::vector<char const*>          _strings;

////////////////////////////////////////////////////////////////////////////////
/// @brief short string storage. uses less memory allocations for short strings
////////////////////////////////////////////////////////////////////////////////

        ShortStringStorage                _shortStringStorage;

////////////////////////////////////////////////////////////////////////////////
/// @brief _ast, we need an ast to manage the memory for AstNodes, even
/// if we do not have a parser, because AstNodes occur in plans and engines
////////////////////////////////////////////////////////////////////////////////

        Ast*                              _ast;

////////////////////////////////////////////////////////////////////////////////
/// @brief query execution profile
////////////////////////////////////////////////////////////////////////////////

        Profile*                          _profile;

////////////////////////////////////////////////////////////////////////////////
/// @brief current state the query is in (used for profiling and error messages)
////////////////////////////////////////////////////////////////////////////////

        ExecutionState                    _state;

////////////////////////////////////////////////////////////////////////////////
/// @brief the ExecutionPlan object, if the query is prepared
////////////////////////////////////////////////////////////////////////////////

        ExecutionPlan*                    _plan;

////////////////////////////////////////////////////////////////////////////////
/// @brief the Parser object, if the query is prepared
////////////////////////////////////////////////////////////////////////////////

        Parser*                           _parser;

////////////////////////////////////////////////////////////////////////////////
/// @brief the transaction object, in a distributed query every part of
/// the query has its own transaction object. The transaction object is
/// created in the prepare method.
////////////////////////////////////////////////////////////////////////////////

        triagens::arango::AqlTransaction* _trx;

////////////////////////////////////////////////////////////////////////////////
/// @brief the ExecutionEngine object, if the query is prepared
////////////////////////////////////////////////////////////////////////////////

        ExecutionEngine*                  _engine;

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum number of warnings
////////////////////////////////////////////////////////////////////////////////

        size_t                            _maxWarningCount;

////////////////////////////////////////////////////////////////////////////////
/// @brief warnings collected during execution
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::pair<int, std::string>> _warnings;

////////////////////////////////////////////////////////////////////////////////
/// @brief the query part
////////////////////////////////////////////////////////////////////////////////

        QueryPart const                   _part;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not someone else has acquired a V8 context for us
////////////////////////////////////////////////////////////////////////////////

        bool const                        _contextOwnedByExterior;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the query is killed
////////////////////////////////////////////////////////////////////////////////

        bool                              _killed;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not query tracking is disabled globally
////////////////////////////////////////////////////////////////////////////////
          
        static bool DoDisableQueryTracking;

    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
