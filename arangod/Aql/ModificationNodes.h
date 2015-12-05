////////////////////////////////////////////////////////////////////////////////
/// @brief Modification nodes
///
/// @file arangod/Aql/ExecutionNode.h
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
/// @author Jan Steemann
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_MODIFICATION_NODES_H
#define ARANGODB_AQL_MODIFICATION_NODES_H 1

#include "Basics/Common.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ModificationOptions.h"
#include "Aql/types.h"
#include "Aql/Variable.h"
#include "Basics/JsonHelper.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

namespace triagens {
  namespace aql {
    struct Collection;
    class ExecutionBlock;
    class ExecutionPlan;

// -----------------------------------------------------------------------------
// --SECTION--                                            class ModificationNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base class for modification operations
////////////////////////////////////////////////////////////////////////////////

    class ModificationNode : public ExecutionNode {
      
      friend class ExecutionBlock;
      friend class ModificationBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with a vocbase and a collection and options
////////////////////////////////////////////////////////////////////////////////

      protected:

        ModificationNode (ExecutionPlan* plan,
                          size_t id,
                          TRI_vocbase_t* vocbase, 
                          Collection* collection,
                          ModificationOptions const& options,
                          Variable const* outVariableOld,
                          Variable const* outVariableNew)
          : ExecutionNode(plan, id), 
            _vocbase(vocbase), 
            _collection(collection),
            _options(options),
            _outVariableOld(outVariableOld),
            _outVariableNew(outVariableNew) {

          TRI_ASSERT(_vocbase != nullptr);
          TRI_ASSERT(_collection != nullptr);
        }

        ModificationNode (ExecutionPlan*,
                          triagens::basics::Json const& json);

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        virtual void toJsonHelper (triagens::basics::Json& json,
                                   TRI_memory_zone_t* zone,
                                   bool) const override;

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

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
/// @brief estimateCost
/// Note that all the modifying nodes use this estimateCost method which is
/// why we can make it final here.
////////////////////////////////////////////////////////////////////////////////
        
        double estimateCost (size_t&) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief getOptions
////////////////////////////////////////////////////////////////////////////////
        
        ModificationOptions const& getOptions () const {
          return _options;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getOptions
////////////////////////////////////////////////////////////////////////////////
        
        ModificationOptions& getOptions () {
          return _options;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        std::vector<Variable const*> getVariablesSetHere () const override final {
          std::vector<Variable const*> v;
          if (_outVariableOld != nullptr) {
            v.emplace_back(_outVariableOld);
          }
          if (_outVariableNew != nullptr) {
            v.emplace_back(_outVariableNew);
          }
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the "$OLD" out variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* getOutVariableOld () const {
          return _outVariableOld;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the "$NEW" out variable
////////////////////////////////////////////////////////////////////////////////
        
        Variable const* getOutVariableNew () const {
          return _outVariableNew;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief clear the "$OLD" out variable
////////////////////////////////////////////////////////////////////////////////

        void clearOutVariableOld () {
          _outVariableOld = nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief clear the "$NEW" out variable
////////////////////////////////////////////////////////////////////////////////
        
        void clearOutVariableNew () {
          _outVariableNew = nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the node is a data modification node
////////////////////////////////////////////////////////////////////////////////

        bool isModificationNode () const override {
          return true;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief _vocbase, the database
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection
////////////////////////////////////////////////////////////////////////////////

        Collection* _collection;

////////////////////////////////////////////////////////////////////////////////
/// @brief modification operation options
////////////////////////////////////////////////////////////////////////////////

        ModificationOptions _options;

////////////////////////////////////////////////////////////////////////////////
/// @brief output variable ($OLD)
////////////////////////////////////////////////////////////////////////////////

        Variable const* _outVariableOld;

////////////////////////////////////////////////////////////////////////////////
/// @brief output variable ($NEW)
////////////////////////////////////////////////////////////////////////////////

        Variable const* _outVariableNew;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                  class RemoveNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class RemoveNode
////////////////////////////////////////////////////////////////////////////////

    class RemoveNode : public ModificationNode {
      
      friend class ExecutionNode;
      friend class ExecutionBlock;
      friend class RemoveBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor 
////////////////////////////////////////////////////////////////////////////////

      public:

        RemoveNode (ExecutionPlan* plan,
                    size_t id,
                    TRI_vocbase_t* vocbase, 
                    Collection* collection,
                    ModificationOptions const& options,
                    Variable const* inVariable,
                    Variable const* outVariableOld)
          : ModificationNode(plan, id, vocbase, collection, options, outVariableOld, nullptr),
            _inVariable(inVariable) {

          TRI_ASSERT(_inVariable != nullptr);
        }
        
        RemoveNode (ExecutionPlan*, 
                    triagens::basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override final {
          return REMOVE;
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
                              bool withProperties) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, returning a vector
////////////////////////////////////////////////////////////////////////////////

        std::vector<Variable const*> getVariablesUsedHere () const override final {
          return std::vector<Variable const*>{ _inVariable };
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, modifying the set in-place
////////////////////////////////////////////////////////////////////////////////

        void getVariablesUsedHere (std::unordered_set<Variable const*>& vars) const override final {
          vars.emplace(_inVariable);
        } 

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief input variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inVariable;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                  class InsertNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class InsertNode
////////////////////////////////////////////////////////////////////////////////

    class InsertNode : public ModificationNode {
      
      friend class ExecutionNode;
      friend class ExecutionBlock;
      friend class InsertBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor 
////////////////////////////////////////////////////////////////////////////////

      public:

        InsertNode (ExecutionPlan* plan,
                    size_t id,
                    TRI_vocbase_t* vocbase, 
                    Collection* collection,
                    ModificationOptions const& options,
                    Variable const* inVariable,
                    Variable const* outVariableNew)
          : ModificationNode(plan, id, vocbase, collection, options, nullptr, outVariableNew),
            _inVariable(inVariable) {

          TRI_ASSERT(_inVariable != nullptr);
          // _outVariable might be a nullptr
        }
        
        InsertNode (ExecutionPlan*, 
                    triagens::basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override final {
          return INSERT;
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
                              bool withProperties) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, returning a vector
////////////////////////////////////////////////////////////////////////////////

        std::vector<Variable const*> getVariablesUsedHere () const override final {
          return std::vector<Variable const*>{ _inVariable };
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, modifying the set in-place
////////////////////////////////////////////////////////////////////////////////

        void getVariablesUsedHere (std::unordered_set<Variable const*>& vars) const override final {
          vars.emplace(_inVariable);
        } 

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief input variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inVariable;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                  class UpdateNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class UpdateNode
////////////////////////////////////////////////////////////////////////////////

    class UpdateNode : public ModificationNode {
      
      friend class ExecutionNode;
      friend class ExecutionBlock;
      friend class UpdateBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with a vocbase and a collection name
////////////////////////////////////////////////////////////////////////////////

      public:

        UpdateNode (ExecutionPlan* plan,
                    size_t id, 
                    TRI_vocbase_t* vocbase, 
                    Collection* collection,
                    ModificationOptions const& options,
                    Variable const* inDocVariable,
                    Variable const* inKeyVariable,
                    Variable const* outVariableOld,
                    Variable const* outVariableNew) 
          : ModificationNode(plan, id, vocbase, collection, options, outVariableOld, outVariableNew),
            _inDocVariable(inDocVariable),
            _inKeyVariable(inKeyVariable) { 

          TRI_ASSERT(_inDocVariable != nullptr);
          // _inKeyVariable might be a nullptr
        }
        
        UpdateNode (ExecutionPlan*, 
                    triagens::basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override final {
          return UPDATE;
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
                              bool withProperties) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, returning a vector
////////////////////////////////////////////////////////////////////////////////

        std::vector<Variable const*> getVariablesUsedHere () const override final {
          // Please do not change the order here without adjusting the 
          // optimizer rule distributeInCluster as well!
          std::vector<Variable const*> v{ _inDocVariable };

          if (_inKeyVariable != nullptr) {
            v.emplace_back(_inKeyVariable);
          }
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, modifying the set in-place
////////////////////////////////////////////////////////////////////////////////

        void getVariablesUsedHere (std::unordered_set<Variable const*>& vars) const override final {
          vars.emplace(_inDocVariable);

          if (_inKeyVariable != nullptr) {
            vars.emplace(_inKeyVariable);
          }
        } 

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief input variable for documents
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inDocVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief input variable for keys
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inKeyVariable;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                 class ReplaceNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class ReplaceNode
////////////////////////////////////////////////////////////////////////////////

    class ReplaceNode : public ModificationNode {
      
      friend class ExecutionNode;
      friend class ExecutionBlock;
      friend class ReplaceBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with a vocbase and a collection name
////////////////////////////////////////////////////////////////////////////////

      public:

        ReplaceNode (ExecutionPlan* plan,
                     size_t id,
                     TRI_vocbase_t* vocbase, 
                     Collection* collection,
                     ModificationOptions const& options,
                     Variable const* inDocVariable,
                     Variable const* inKeyVariable,
                     Variable const* outVariableOld,
                     Variable const* outVariableNew)
          : ModificationNode(plan, id, vocbase, collection, options, outVariableOld, outVariableNew),
            _inDocVariable(inDocVariable),
            _inKeyVariable(inKeyVariable) {

          TRI_ASSERT(_inDocVariable != nullptr);
          // _inKeyVariable might be a nullptr
        }

        ReplaceNode (ExecutionPlan*, 
                     triagens::basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override final {
          return REPLACE;
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
                              bool withProperties) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, returning a vector
////////////////////////////////////////////////////////////////////////////////

        std::vector<Variable const*> getVariablesUsedHere () const override final {
          // Please do not change the order here without adjusting the 
          // optimizer rule distributeInCluster as well!
          std::vector<Variable const*> v{ _inDocVariable };

          if (_inKeyVariable != nullptr) {
            v.emplace_back(_inKeyVariable);
          }
          return v;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, modifying the set in-place
////////////////////////////////////////////////////////////////////////////////

        void getVariablesUsedHere (std::unordered_set<Variable const*>& vars) const override final {
          vars.emplace(_inDocVariable);

          if (_inKeyVariable != nullptr) {
            vars.emplace(_inKeyVariable);
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief input variable for documents
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inDocVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief input variable for keys
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inKeyVariable;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                  class UpsertNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class UpsertNode
////////////////////////////////////////////////////////////////////////////////

    class UpsertNode : public ModificationNode {
      
      friend class ExecutionNode;
      friend class ExecutionBlock;
      friend class UpsertBlock;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with a vocbase and a collection name
////////////////////////////////////////////////////////////////////////////////

      public:

        UpsertNode (ExecutionPlan* plan,
                    size_t id, 
                    TRI_vocbase_t* vocbase, 
                    Collection* collection,
                    ModificationOptions const& options,
                    Variable const* inDocVariable,
                    Variable const* insertVariable,
                    Variable const* updateVariable,
                    Variable const* outVariableNew,
                    bool isReplace) 
          : ModificationNode(plan, id, vocbase, collection, options, nullptr, outVariableNew),
            _inDocVariable(inDocVariable),
            _insertVariable(insertVariable),
            _updateVariable(updateVariable),
            _isReplace(isReplace) {

          TRI_ASSERT(_inDocVariable != nullptr);
          TRI_ASSERT(_insertVariable != nullptr);
          TRI_ASSERT(_updateVariable != nullptr);

          TRI_ASSERT(_outVariableOld == nullptr);
        }
        
        UpsertNode (ExecutionPlan*, 
                    triagens::basics::Json const& base);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override final {
          return UPSERT;
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
                              bool withProperties) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, returning a vector
////////////////////////////////////////////////////////////////////////////////

        std::vector<Variable const*> getVariablesUsedHere () const override final {
          // Please do not change the order here without adjusting the 
          // optimizer rule distributeInCluster as well!
          return std::vector<Variable const*>({ _inDocVariable, _insertVariable, _updateVariable });
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, modifying the set in-place
////////////////////////////////////////////////////////////////////////////////

        void getVariablesUsedHere (std::unordered_set<Variable const*>& vars) const override final {
          vars.emplace(_inDocVariable);
          vars.emplace(_insertVariable);
          vars.emplace(_updateVariable);
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief input variable for the search document
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inDocVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief insert case expression
////////////////////////////////////////////////////////////////////////////////

        Variable const* _insertVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief update case expression
////////////////////////////////////////////////////////////////////////////////

        Variable const* _updateVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether to perform a REPLACE (or an UPDATE alternatively)
////////////////////////////////////////////////////////////////////////////////

        bool const _isReplace;
    };

  }   // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


