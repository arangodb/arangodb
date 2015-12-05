////////////////////////////////////////////////////////////////////////////////
/// @brief Cluster execution nodes
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_CLUSTER_NODES_H
#define ARANGODB_AQL_CLUSTER_NODES_H 1

#include "Basics/Common.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Query.h"
#include "Aql/Variable.h"
#include "Basics/JsonHelper.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

namespace triagens {
  namespace aql {
    class ExecutionBlock;
    class ExecutionPlan;
    struct Collection;

// -----------------------------------------------------------------------------
// --SECTION--                                                  class RemoteNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class RemoteNode
////////////////////////////////////////////////////////////////////////////////

    class RemoteNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class RemoteBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with an id
////////////////////////////////////////////////////////////////////////////////

      public:
 
        RemoteNode (ExecutionPlan* plan, 
                    size_t id,
                    TRI_vocbase_t* vocbase,
                    Collection const* collection,
                    std::string const& server,
                    std::string const& ownName,
                    std::string const& queryId) 
          : ExecutionNode(plan, id),
            _vocbase(vocbase),
            _collection(collection),
            _server(server),
            _ownName(ownName),
            _queryId(queryId),
            _isResponsibleForInitCursor(true) {
          // note: server, ownName and queryId may be empty and filled later
        }
 
////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not this node will forward initializeCursor or shutDown
/// requests
////////////////////////////////////////////////////////////////////////////////

        void isResponsibleForInitCursor (bool value) {
          _isResponsibleForInitCursor = value;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not this node will forward initializeCursor or shutDown
/// requests
////////////////////////////////////////////////////////////////////////////////

        bool isResponsibleForInitCursor () const {
          return _isResponsibleForInitCursor;
        }

        RemoteNode (ExecutionPlan*, triagens::basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override final {
          return REMOTE;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        void toJsonHelper (triagens::basics::Json&,
                           TRI_memory_zone_t*,
                           bool) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* clone (ExecutionPlan* plan,
                              bool withDependencies,
                              bool withProperties) const override final {
          auto c = new RemoteNode(plan, _id, _vocbase, _collection, _server, _ownName, _queryId);

          cloneHelper(c, plan, withDependencies, withProperties);

          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief estimateCost
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost (size_t&) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief return the database
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* vocbase () const {
          return _vocbase;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection
////////////////////////////////////////////////////////////////////////////////

        Collection const* collection () const {
          return _collection;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the server name
////////////////////////////////////////////////////////////////////////////////

        std::string server () const {
          return _server;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the server name
////////////////////////////////////////////////////////////////////////////////

        void server (std::string const& server) {
          _server = server;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return our own name
////////////////////////////////////////////////////////////////////////////////
        
        std::string ownName () const {
          return _ownName;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set our own name
////////////////////////////////////////////////////////////////////////////////
        
        void ownName (std::string const& ownName) {
          _ownName = ownName;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the query id
////////////////////////////////////////////////////////////////////////////////

        std::string queryId () const {
          return _queryId;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the query id
////////////////////////////////////////////////////////////////////////////////

        void queryId (std::string const& queryId) {
          _queryId = queryId;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the query id
////////////////////////////////////////////////////////////////////////////////

        void queryId (QueryId queryId) {
          _queryId = triagens::basics::StringUtils::itoa(queryId);
        }

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the underlying database
////////////////////////////////////////////////////////////////////////////////
                                 
        TRI_vocbase_t* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief the underlying collection
////////////////////////////////////////////////////////////////////////////////

        Collection const* _collection;

////////////////////////////////////////////////////////////////////////////////
/// @brief our server, can be like "shard:S1000" or like "server:Claus"
////////////////////////////////////////////////////////////////////////////////

        std::string _server;

////////////////////////////////////////////////////////////////////////////////
/// @brief our own identity, in case of the coordinator this is empty,
/// in case of the DBservers, this is the shard ID as a string
////////////////////////////////////////////////////////////////////////////////

        std::string _ownName;

////////////////////////////////////////////////////////////////////////////////
/// @brief the ID of the query on the server as a string
////////////////////////////////////////////////////////////////////////////////

        std::string _queryId;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not this node will forward initializeCursor and shutDown
/// requests
////////////////////////////////////////////////////////////////////////////////

        bool _isResponsibleForInitCursor;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                                 class ScatterNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class ScatterNode
////////////////////////////////////////////////////////////////////////////////

    class ScatterNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class ScatterBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with an id
////////////////////////////////////////////////////////////////////////////////

      public:
 
        ScatterNode (ExecutionPlan* plan, 
                     size_t id,
                     TRI_vocbase_t* vocbase,
                     Collection const* collection) 
          : ExecutionNode(plan, id),
            _vocbase(vocbase),
            _collection(collection) {
        }

        ScatterNode (ExecutionPlan*, 
                     triagens::basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override final {
          return SCATTER;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        void toJsonHelper (triagens::basics::Json&,
                           TRI_memory_zone_t*,
                           bool) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* clone (ExecutionPlan* plan,
                              bool withDependencies,
                              bool withProperties) const override final {
          auto c = new ScatterNode(plan, _id, _vocbase, _collection);

          cloneHelper(c, plan, withDependencies, withProperties);

          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief estimateCost
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost (size_t&) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief return the database
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* vocbase () const {
          return _vocbase;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection
////////////////////////////////////////////////////////////////////////////////

        Collection const* collection () const {
          return _collection;
        }

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the underlying database
////////////////////////////////////////////////////////////////////////////////
                                 
        TRI_vocbase_t* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief the underlying collection
////////////////////////////////////////////////////////////////////////////////

        Collection const* _collection;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                              class DistributeNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class DistributeNode
////////////////////////////////////////////////////////////////////////////////

    class DistributeNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class DistributeBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with an id
////////////////////////////////////////////////////////////////////////////////

      public:
 
        DistributeNode (ExecutionPlan* plan, 
                        size_t id,
                        TRI_vocbase_t* vocbase,
                        Collection const* collection, 
                        VariableId const varId,
                        VariableId const alternativeVarId,
                        bool createKeys,
                        bool allowKeyConversionToObject)
          : ExecutionNode(plan, id),
            _vocbase(vocbase),
            _collection(collection),
            _varId(varId),
            _alternativeVarId(alternativeVarId),
            _createKeys(createKeys),
            _allowKeyConversionToObject(allowKeyConversionToObject) {
        }
        
        DistributeNode (ExecutionPlan* plan, 
                        size_t id,
                        TRI_vocbase_t* vocbase,
                        Collection const* collection, 
                        VariableId const varId,
                        bool createKeys,
                        bool allowKeyConversionToObject)
          : DistributeNode(plan, id, vocbase, collection, varId, varId, createKeys, allowKeyConversionToObject) {
          // just delegates to the other constructor
        }

        DistributeNode (ExecutionPlan*, 
                        triagens::basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override final {
          return DISTRIBUTE;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        void toJsonHelper (triagens::basics::Json&,
                           TRI_memory_zone_t*,
                           bool) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* clone (ExecutionPlan* plan,
                              bool withDependencies,
                              bool withProperties) const override final {
          auto c = new DistributeNode(plan, _id, _vocbase, _collection, _varId, _alternativeVarId, _createKeys, _allowKeyConversionToObject);
          
          cloneHelper(c, plan, withDependencies, withProperties);

          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief estimateCost
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost (size_t&) const override final;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief return the database
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* vocbase () const {
          return _vocbase;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection
////////////////////////////////////////////////////////////////////////////////

        Collection const* collection () const {
          return _collection;
        }

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the underlying database
////////////////////////////////////////////////////////////////////////////////
                                 
        TRI_vocbase_t* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief the underlying collection
////////////////////////////////////////////////////////////////////////////////

        Collection const* _collection;

////////////////////////////////////////////////////////////////////////////////
/// @brief the variable we must inspect to know where to distribute
////////////////////////////////////////////////////////////////////////////////

        VariableId const _varId;

////////////////////////////////////////////////////////////////////////////////
/// @brief an optional second variable we must inspect to know where to 
/// distribute
////////////////////////////////////////////////////////////////////////////////

        VariableId const _alternativeVarId;

////////////////////////////////////////////////////////////////////////////////
/// @brief the node is responsible for creating document keys
////////////////////////////////////////////////////////////////////////////////

        bool const _createKeys;

////////////////////////////////////////////////////////////////////////////////
/// @brief allow conversion of key to object
////////////////////////////////////////////////////////////////////////////////

        bool const _allowKeyConversionToObject;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                  class GatherNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class GatherNode
////////////////////////////////////////////////////////////////////////////////

    class GatherNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class GatherBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with an id
////////////////////////////////////////////////////////////////////////////////

      public:
 
        GatherNode (ExecutionPlan* plan, 
                    size_t id,
                    TRI_vocbase_t* vocbase,
                    Collection const* collection)
          : ExecutionNode(plan, id),
            _vocbase(vocbase),
            _collection(collection) {
        }

        GatherNode (ExecutionPlan*,
                    triagens::basics::Json const& base,
                    SortElementVector const& elements);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override final {
          return GATHER;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        void toJsonHelper (triagens::basics::Json&,
                           TRI_memory_zone_t*,
                           bool) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* clone (ExecutionPlan* plan,
                              bool withDependencies,
                              bool withProperties) const override final {
          auto c = new GatherNode(plan, _id, _vocbase, _collection);

          cloneHelper(c, plan, withDependencies, withProperties);

          return static_cast<ExecutionNode*>(c);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief estimateCost
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost (size_t&) const override final;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, returning a vector
////////////////////////////////////////////////////////////////////////////////

        std::vector<Variable const*> getVariablesUsedHere () const override final {
          std::vector<Variable const*> v;
          v.reserve(_elements.size());

          for (auto const& p : _elements) {
            v.emplace_back(p.first);
          }
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, modifying the set in-place
////////////////////////////////////////////////////////////////////////////////

        void getVariablesUsedHere (std::unordered_set<Variable const*>& vars) const override final {
          for (auto const& p : _elements) {
            vars.emplace(p.first);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get Variables Used Here including ASC/DESC
////////////////////////////////////////////////////////////////////////////////

        SortElementVector const & getElements () const {
          return _elements;
        }

        void setElements (SortElementVector const & src) {
          _elements = src;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the database
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* vocbase () const {
          return _vocbase;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection
////////////////////////////////////////////////////////////////////////////////

        Collection const* collection () const {
          return _collection;
        }

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief pairs, consisting of variable and sort direction
/// (true = ascending | false = descending)
////////////////////////////////////////////////////////////////////////////////

        SortElementVector _elements;

////////////////////////////////////////////////////////////////////////////////
/// @brief the underlying database
////////////////////////////////////////////////////////////////////////////////
                                 
        TRI_vocbase_t* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief the underlying collection
////////////////////////////////////////////////////////////////////////////////

        Collection const* _collection;

    };

  }   // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


