////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, expression
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

#include "Aql/Expression.h"
#include "Aql/AqlValue.h"
#include "Aql/Ast.h"
#include "Aql/Types.h"
#include "Aql/V8Executor.h"
#include "Aql/V8Expression.h"
#include "Aql/Variable.h"
#include "Basics/JsonHelper.h"
#include "BasicsC/json.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/document-collection.h"
#include "Utils/Exception.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;
using JsonHelper = triagens::basics::JsonHelper;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the expression
////////////////////////////////////////////////////////////////////////////////

Expression::Expression (V8Executor* executor,
                        AstNode const* node)
  : _executor(executor),
    _node(node),
    _type(UNPROCESSED),
    _canThrow(true) {

  TRI_ASSERT(_executor != nullptr);
  TRI_ASSERT(_node != nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the expression
////////////////////////////////////////////////////////////////////////////////

Expression::~Expression () {
  if (_type == V8) {
    delete _func;
  }
  else if (_type == JSON) {
    TRI_ASSERT(_data != nullptr);
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _data);
    // _json is freed automatically by AqlItemBlock
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return all variables used in the expression
////////////////////////////////////////////////////////////////////////////////

std::unordered_set<Variable*> Expression::variables () const {
  return Ast::getReferencedVariables(_node);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the expression
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::execute (AQL_TRANSACTION_V8* trx,
                              std::vector<TRI_document_collection_t const*>& docColls,
                              std::vector<AqlValue>& argv,
                              size_t startPos,
                              std::vector<Variable*> const& vars,
                              std::vector<RegisterId> const& regs) {

  if (_type == UNPROCESSED) {
    analyzeExpression();
  }

  // and execute
  switch (_type) {
    case JSON: {
      TRI_ASSERT(_data != nullptr);
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, _data, Json::NOFREE));
    }

    case V8: {
      TRI_ASSERT(_func != nullptr);
      return _func->execute(trx, docColls, argv, startPos, vars, regs);
    }

    case SIMPLE: {
      return executeSimpleExpression(_node, nullptr, trx, docColls, argv, startPos, vars, regs);
    }
 
    case UNPROCESSED: {
      // fall-through to exception
    }
  }
      
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief analyze the expression (and, if appropriate, compile it into 
/// executable code)
////////////////////////////////////////////////////////////////////////////////

void Expression::analyzeExpression () {
  TRI_ASSERT(_type == UNPROCESSED);

  if (_node->isConstant()) {
    // generate a constant value
    _data = _node->toJsonValue(TRI_UNKNOWN_MEM_ZONE);

    if (_data == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    _type = JSON;
    _canThrow = false;
  }
  else if (_node->isSimple()) {
    _type = SIMPLE;
    _canThrow = _node->canThrow();
  }
  else {
    // generate a V8 expression
    _func = _executor->generateExpression(_node);
    _type = V8;
    _canThrow = _node->canThrow();
  }

  std::cout << "CAN THROW: " << _canThrow << "\n";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an expression of type SIMPLE
////////////////////////////////////////////////////////////////////////////////

AqlValue Expression::executeSimpleExpression (AstNode const* node,
                                              TRI_document_collection_t const** collection, 
                                              AQL_TRANSACTION_V8* trx,
                                              std::vector<TRI_document_collection_t const*>& docColls,
                                              std::vector<AqlValue>& argv,
                                              size_t startPos,
                                              std::vector<Variable*> const& vars,
                                              std::vector<RegisterId> const& regs) {

  if (node->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
    TRI_ASSERT(node->numMembers() == 1);

    auto member = node->getMember(0);
    auto name = static_cast<char const*>(node->getData());

    TRI_document_collection_t const* myCollection = nullptr;
    AqlValue result = executeSimpleExpression(member, &myCollection, trx, docColls, argv, startPos, vars, regs);

    if (result._type == AqlValue::SHAPED) {
      TRI_ASSERT(myCollection != nullptr);

      auto shaper = myCollection->getShaper();

      // look for the attribute name in the shape
      if (*name == '_') {
        if (strcmp(name, "_key") == 0) {
          // _key value is copied into JSON
          return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, TRI_EXTRACT_MARKER_KEY(result._marker)));
        }
        else if (strcmp(name, "_id") == 0) {
          std::string id(trx->resolver()->getCollectionName(myCollection->_info._cid));
          id.push_back('/');
          id.append(TRI_EXTRACT_MARKER_KEY(result._marker));
          return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, id));
        }
        else if (strcmp(name, "_rev") == 0) {
          TRI_voc_rid_t rid = TRI_EXTRACT_MARKER_RID(result._marker);
          return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, JsonHelper::uint64String(TRI_UNKNOWN_MEM_ZONE, rid)));
        }
        else if (strcmp(name, "_from") == 0) {
          std::string from(trx->resolver()->getCollectionName(TRI_EXTRACT_MARKER_FROM_CID(result._marker)));
          from.push_back('/');
          from.append(TRI_EXTRACT_MARKER_FROM_KEY(result._marker));
          return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, from));
        }
        else if (strcmp(name, "_to") == 0) {
          std::string to(trx->resolver()->getCollectionName(TRI_EXTRACT_MARKER_TO_CID(result._marker)));
          to.push_back('/');
          to.append(TRI_EXTRACT_MARKER_TO_KEY(result._marker));
          return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, to));
        }
      }

      TRI_shape_pid_t pid = shaper->lookupAttributePathByName(shaper, name);
      if (pid != 0) {
        // attribute exists
        TRI_shaped_json_t document;
        TRI_EXTRACT_SHAPED_JSON_MARKER(document, result._marker);

        TRI_shaped_json_t json;
        TRI_shape_t const* shape;

        bool ok = TRI_ExtractShapedJsonVocShaper(shaper, &document, 0, pid, &json, &shape);
        if (ok && shape != nullptr) {
          return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, TRI_JsonShapedJson(shaper, &json)));
        }
      }

      // attribute does not exist or something went wrong - fall-through to returning null below
    }

    else if (result._type == AqlValue::JSON) {
      TRI_json_t const* json = result._json->json();

      if (TRI_IsArrayJson(json)) {
        TRI_json_t const* found = TRI_LookupArrayJson(json, name);

        if (found != nullptr) {
          return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, found)));
        }
      }
      
      // attribute does not exist or something went wrong - fall-through to returning null below
    }
      
    return AqlValue(new Json(Json::Null));
  }
  
  else if (node->type == NODE_TYPE_LIST) {
    auto list = new Json(Json::List);

    try {
      size_t const n = node->numMembers();
      for (size_t i = 0; i < n; ++i) {
        auto member = node->getMember(i);
        TRI_document_collection_t const* myCollection = nullptr;

        AqlValue result = executeSimpleExpression(member, &myCollection, trx, docColls, argv, startPos, vars, regs);
        list->add(result.toJson(trx, myCollection));
      }
      return AqlValue(list);
    }
    catch (...) {
      delete list;
      throw;
    }
  }

  else if (node->type == NODE_TYPE_ARRAY) {
    auto resultArray = new Json(Json::Array);

    try {
      size_t const n = node->numMembers();
      for (size_t i = 0; i < n; ++i) {
        auto member = node->getMember(i);
        TRI_document_collection_t const* myCollection = nullptr;

        TRI_ASSERT(member->type == NODE_TYPE_ARRAY_ELEMENT);
        auto key = member->getStringValue();
        member = member->getMember(0);

        AqlValue result = executeSimpleExpression(member, &myCollection, trx, docColls, argv, startPos, vars, regs);
        resultArray->set(key, result.toJson(trx, myCollection));
      }
      return AqlValue(resultArray);
    }
    catch (...) {
      delete resultArray;
      throw;
    }
  }

  else if (node->type == NODE_TYPE_REFERENCE) {
    auto v = static_cast<Variable*>(node->getData());

    size_t i = 0;
    for (auto it = vars.begin(); it != vars.end(); ++it, ++i) {
      if ((*it)->name == v->name) {
        TRI_ASSERT(collection != nullptr);

        // save the collection info
        *collection = docColls[regs[i]]; 
        return argv[startPos + regs[i]];
      }
    }
    // fall-through to exception
  }

  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
