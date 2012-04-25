////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, AST dump functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <BasicsC/logging.h>

#include "Ahuacatl/ast-codegen-js.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief register a function name for easier later disposal
////////////////////////////////////////////////////////////////////////////////

static bool RegisterString (TRI_aql_codegen_t* const generator, 
                            const char* const name) {
  assert(generator);

  TRI_PushBackVectorPointer(&generator->_strings, (char*) name);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a function name using the rule "f" + number
////////////////////////////////////////////////////////////////////////////////

static char* GetIndexedFunctionName (TRI_aql_codegen_t* const generator, 
                                     const size_t funcIndex) {
  char* numberString;
  char* functionName;

  numberString = TRI_StringUInt64((uint64_t) funcIndex);
  if (!numberString) {
    return NULL;
  }

  functionName = TRI_Concatenate2String("f", numberString); 
  if (functionName) {
    RegisterString(generator, functionName);
  }

  TRI_Free(numberString);

  return functionName; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the next function name (using the rule "f" + number, number will
/// be increased on each call)
////////////////////////////////////////////////////////////////////////////////

static char* GetNextFunctionName (TRI_aql_codegen_t* const generator) {
  char* functionName;

  assert(generator);

  functionName = GetIndexedFunctionName(generator, ++generator->_funcIndex);

  return functionName; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a code scope
////////////////////////////////////////////////////////////////////////////////

static void FreeScope (TRI_aql_codegen_scope_t* const scope) {
  assert(scope);

  // note: scope->_funcName is freed globally

  if (scope->_buffer) {
   TRI_FreeStringBuffer(scope->_buffer);
  }

  TRI_Free(scope);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a code scope
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_codegen_scope_t* CreateScope (TRI_aql_codegen_t* const generator,
                                             const char* const funcName, 
                                             const TRI_aql_scope_type_e type) {
  TRI_aql_codegen_scope_t* scope;
  
  scope = (TRI_aql_codegen_scope_t*) TRI_Allocate(sizeof(TRI_aql_codegen_scope_t));

  if (!scope) {
    return NULL;
  }
  
  scope->_funcName = TRI_DuplicateString(funcName);
  if (!scope->_funcName) {
    FreeScope(scope);
    return NULL;
  }

  RegisterString(generator, scope->_funcName);
  scope->_variablePrefix = NULL;
  scope->_indent = 0;
  scope->_forLoops = 0;
  scope->_type = type;

  scope->_buffer = TRI_CreateStringBuffer();
  if (!scope->_buffer) {
    FreeScope(scope);
    return NULL;
  }

  return scope;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a new code scope
////////////////////////////////////////////////////////////////////////////////

static bool StartScope (TRI_aql_codegen_t* const generator, 
                        const TRI_aql_scope_type_e type, 
                        const char* const funcName) {
  TRI_aql_codegen_scope_t* scope;
  
  assert(generator);
  assert(funcName);

  scope = CreateScope(generator, funcName, type);

  if (!scope) {
    return false;
  }

  TRI_PushBackVectorPointer(&generator->_scopes, (void*) scope);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the scope currently used
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_codegen_scope_t* GetCurrentScope (TRI_aql_codegen_t* const generator) {
  TRI_aql_codegen_scope_t* scope;
  size_t length;

  assert(generator);

  length = generator->_scopes._length;

  assert(length > 0);

  scope = (TRI_aql_codegen_scope_t*) generator->_scopes._buffer[length -1];

  return scope;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the indent level in the current scope
////////////////////////////////////////////////////////////////////////////////

static void Indent (TRI_aql_codegen_t* const generator) {
  TRI_aql_codegen_scope_t* scope;
  assert(generator);

  scope = GetCurrentScope(generator);
  assert(scope);

  scope->_indent++;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decrease the indent level in the current scope
////////////////////////////////////////////////////////////////////////////////

static void Outdent (TRI_aql_codegen_t* const generator) {
  TRI_aql_codegen_scope_t* scope;
  assert(generator);

  scope = GetCurrentScope(generator);
  assert(scope);
  assert(scope->_indent > 0);

  --scope->_indent;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add the current indentation to the output buffer in the current scope
////////////////////////////////////////////////////////////////////////////////

static void AppendIndent (TRI_aql_codegen_t* const generator) {
  TRI_aql_codegen_scope_t* scope;
  size_t i;

  assert(generator);

  scope = GetCurrentScope(generator);

  for (i = 0; i <= scope->_forLoops + scope->_indent; ++i) {
    TRI_AppendStringStringBuffer(scope->_buffer, "  ");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove the current scope from the stack
////////////////////////////////////////////////////////////////////////////////

static void RemoveCurrentScope (TRI_aql_codegen_t* const generator) {
  TRI_aql_codegen_scope_t* scope;
  size_t length;

  assert(generator);

  length = generator->_scopes._length;

  assert(length > 0);

  scope = (TRI_aql_codegen_scope_t*) TRI_RemoveVectorPointer(&generator->_scopes, length - 1);
  FreeScope(scope);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief close all for loops in current scope
////////////////////////////////////////////////////////////////////////////////

static void CloseForLoops (TRI_aql_codegen_t* const generator) {
  TRI_aql_codegen_scope_t* scope;
  size_t i;
  size_t n;

  assert(generator);

  scope = GetCurrentScope(generator);

  assert(scope);

  n = scope->_forLoops;
  for (i = 0; i < n; ++i) {
    scope->_forLoops--;

    AppendIndent(generator);
    TRI_AppendStringStringBuffer(scope->_buffer, "}\n");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write a function definition to the output buffer
////////////////////////////////////////////////////////////////////////////////

static bool AppendFunction (TRI_aql_codegen_t* const generator, 
                            const TRI_aql_scope_type_e type, 
                            const char* const name, 
                            const char* const body) {
  assert(generator);
  assert(name);
  assert(body);

  TRI_AppendStringStringBuffer(&generator->_buffer, "\nfunction ");
  TRI_AppendStringStringBuffer(&generator->_buffer, name);
  TRI_AppendStringStringBuffer(&generator->_buffer, "(");
  if (type == AQL_SCOPE_COMPARE) {
    TRI_AppendStringStringBuffer(&generator->_buffer, "l, r");
  }
  else {
    TRI_AppendStringStringBuffer(&generator->_buffer, "previous");
  }

  TRI_AppendStringStringBuffer(&generator->_buffer, ") {\n");
  if (type != AQL_SCOPE_COMPARE) {
    TRI_AppendStringStringBuffer(&generator->_buffer, "  var $ = AHUACATL_CLONE(previous);\n");
  }
  TRI_AppendStringStringBuffer(&generator->_buffer, body);
  TRI_AppendStringStringBuffer(&generator->_buffer, "\n");
  TRI_AppendStringStringBuffer(&generator->_buffer, "}\n");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief end a scope. this will also write the scope's function to the
/// output buffer
////////////////////////////////////////////////////////////////////////////////

static char* EndScope (TRI_aql_codegen_t* const generator) {
  TRI_aql_codegen_scope_t* scope;
  TRI_string_buffer_t* body;
  char* funcName;

  assert(generator);

  scope = GetCurrentScope(generator);

  assert(scope);

  funcName = scope->_funcName;

  body = TRI_CreateStringBuffer();
  if (!body) {
    // oom
    generator->_error = true;
  }
  else {
    if (scope->_type == AQL_SCOPE_RESULT) {
      TRI_AppendStringStringBuffer(body, "  var result = [];\n");
      TRI_AppendStringStringBuffer(body, scope->_buffer->_buffer);
      TRI_AppendStringStringBuffer(body, "  return result;");
    }
    else {
      TRI_AppendStringStringBuffer(body, scope->_buffer->_buffer);
    }

    AppendFunction(generator, scope->_type, funcName, body->_buffer);
    TRI_FreeStringBuffer(body);
  }

  generator->_funcName = funcName;

  RemoveCurrentScope(generator);

  return funcName;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the number of for loops in the current scope by one
////////////////////////////////////////////////////////////////////////////////
  
static void IncreaseForCount (TRI_aql_codegen_t* const generator) {
  TRI_aql_codegen_scope_t* scope;
  size_t length;

  assert(generator);

  length = generator->_scopes._length;

  assert(length > 0);

  scope = (TRI_aql_codegen_scope_t*) generator->_scopes._buffer[length - 1];
  assert(scope);

  scope->_forLoops++;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append code the current scope's output buffer
////////////////////////////////////////////////////////////////////////////////

static void AppendCode (TRI_aql_codegen_t* const generator, const char* const code) {
  TRI_aql_codegen_scope_t* scope;

  assert(generator);

  scope = GetCurrentScope(generator);

  assert(scope);

  TRI_AppendStringStringBuffer(scope->_buffer, code);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the value of a value node to the current scope's output buffer
////////////////////////////////////////////////////////////////////////////////

static bool AppendValue (TRI_aql_codegen_t* const generator, 
                         const TRI_aql_node_t* const node) {
  TRI_aql_codegen_scope_t* scope;

  assert(generator);

  scope = GetCurrentScope(generator);

  assert(scope);

  switch (node->_value._type) {
    case AQL_TYPE_FAIL:
      TRI_AppendStringStringBuffer(scope->_buffer, "fail");
      break;
    case AQL_TYPE_NULL:
      TRI_AppendStringStringBuffer(scope->_buffer, "null");
      break;
    case AQL_TYPE_BOOL:
      if (node->_value._value._bool) {
        TRI_AppendStringStringBuffer(scope->_buffer, "true");
      }
      else {
        TRI_AppendStringStringBuffer(scope->_buffer, "false");
      }
      break;
    case AQL_TYPE_INT:
      TRI_AppendInt64StringBuffer(scope->_buffer, node->_value._value._int);
      break;
    case AQL_TYPE_DOUBLE:
      TRI_AppendDoubleStringBuffer(scope->_buffer, node->_value._value._double);
      break;
    case AQL_TYPE_STRING: {
      char* escapedString;
      size_t outLength;

      TRI_AppendStringStringBuffer(scope->_buffer, "'");
      escapedString = TRI_EscapeUtf8String(node->_value._value._string, strlen(node->_value._value._string), false, &outLength);
      if (escapedString) {
        TRI_AppendStringStringBuffer(scope->_buffer, escapedString);
        TRI_Free(escapedString); 
      }
      TRI_AppendStringStringBuffer(scope->_buffer, "'");
      break;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set a variable prefix for all variables to be printed
////////////////////////////////////////////////////////////////////////////////

static void SetVariablePrefix (TRI_aql_codegen_t* const generator, 
                               const char* const prefix) {
  TRI_aql_codegen_scope_t* scope;

  assert(generator);

  scope = GetCurrentScope(generator);

  assert(scope);
 
  scope->_variablePrefix = (char*) prefix; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print a variable name in the current scope, enclosed in $['...']
////////////////////////////////////////////////////////////////////////////////

static bool AppendVarname (TRI_aql_codegen_t* const generator, 
                           const TRI_aql_node_t* const node) {
  AppendCode(generator, "$['");
  AppendCode(generator, TRI_AQL_NODE_STRING(node));
  AppendCode(generator, "']");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print a variable name in the current scope, without modifications
////////////////////////////////////////////////////////////////////////////////

static bool AppendVarname0 (TRI_aql_codegen_t* const generator, 
                            const TRI_aql_node_t* const node) {
  AppendCode(generator, TRI_AQL_NODE_STRING(node));
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print a variable name in the current scope, prepended by "_"
////////////////////////////////////////////////////////////////////////////////

static bool AppendVarname1 (TRI_aql_codegen_t* const generator, 
                            const TRI_aql_node_t* const node) {
  AppendCode(generator, "_");
  AppendCode(generator, TRI_AQL_NODE_STRING(node));
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print a variable name in the current scope, prepended by "__"
////////////////////////////////////////////////////////////////////////////////

static bool AppendVarname2 (TRI_aql_codegen_t* const generator, 
                            const TRI_aql_node_t* const node) {
  AppendCode(generator, "__");
  AppendCode(generator, TRI_AQL_NODE_STRING(node));
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print hasOwnProperty(...) if construct in current scope
////////////////////////////////////////////////////////////////////////////////

static bool AppendOwnPropertyName (TRI_aql_codegen_t* const generator, 
                                   const char* const name) {
  AppendIndent(generator);
  AppendCode(generator, "if (!__");
  AppendCode(generator, name);
  AppendCode(generator, ".hasOwnProperty(_");
  AppendCode(generator, name);
  AppendCode(generator, ")) {\n");
  Indent(generator);
  AppendIndent(generator);
  AppendCode(generator, "continue;\n");
  Outdent(generator);
  AppendIndent(generator);
  AppendCode(generator, "}\n");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print hasOwnProperty(...) if construct in current scope
////////////////////////////////////////////////////////////////////////////////

static bool AppendOwnPropertyVar (TRI_aql_codegen_t* const generator, 
                                  const TRI_aql_node_t* const data) {
  AppendIndent(generator);
  AppendCode(generator, "if (!");
  AppendVarname2(generator, data);
  AppendCode(generator, ".hasOwnProperty(");
  AppendVarname1(generator, data);
  AppendCode(generator, ")) {\n");
  Indent(generator);
  AppendIndent(generator);
  AppendCode(generator, "continue;\n");
  Outdent(generator);
  AppendIndent(generator);
  AppendCode(generator, "}\n");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate the code for an individual node and all subnodes
////////////////////////////////////////////////////////////////////////////////

static void GenerateCode (TRI_aql_codegen_t* const generator, 
                          const TRI_aql_node_t* const data) {
  TRI_aql_node_t* node;

  if (!data) {
    return;
  }

  node = (TRI_aql_node_t*) data;

  while (node) {
    switch (node->_type) {
      case AQL_NODE_VALUE:
        AppendValue(generator, node);
        break;
      case AQL_NODE_LIST: {
        size_t i, n;

        AppendCode(generator, "[ ");
        n = node->_subNodes._length;
        for (i = 0; i < n; ++i) {
          if (i > 0) {
            AppendCode(generator, ", ");
          }
          GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, i));
        }
        AppendCode(generator, " ]");
        break;
      }
      case AQL_NODE_ARRAY: {
        size_t i, n;

        AppendCode(generator, "{ ");
        n = node->_subNodes._length;
        for (i = 0; i < n; ++i) {
          if (i > 0) {
            AppendCode(generator, ", ");
          }
          GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, i));
        }
        AppendCode(generator, " }");
        break;
      }
      case AQL_NODE_ARRAY_ELEMENT: {
        AppendCode(generator, "'");
        AppendCode(generator, TRI_AQL_NODE_STRING(node));
        AppendCode(generator, "' : ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        break;
      }
      case AQL_NODE_FOR:
        AppendIndent(generator);
        AppendCode(generator, "var ");
        AppendVarname2(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, " = AHUACATL_LIST(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ");\n");

        AppendIndent(generator);
        AppendCode(generator, "for (var ");
        AppendVarname1(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, " in ");
        AppendVarname2(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ") {\n");

        IncreaseForCount(generator);
        AppendOwnPropertyVar(generator, TRI_AQL_NODE_SUBNODE(node, 0));

        AppendIndent(generator);
        AppendVarname(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, " = ");
        AppendVarname2(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, "[");
        AppendVarname1(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, "];\n");
        break;
      case AQL_NODE_COLLECT: {
        char* previousFunction; 
        char* sortFuncName;
        char* groupFuncName;
        size_t i, n;
        TRI_aql_node_t* list;

        sortFuncName = GetNextFunctionName(generator);

        StartScope(generator, AQL_SCOPE_COMPARE, sortFuncName);
        AppendIndent(generator);
        AppendCode(generator, "var lhs, rhs;\n");

        list = TRI_AQL_NODE_SUBNODE(node, 0);
        n = list->_subNodes._length;
        for (i = 0; i < n; ++i) {
          TRI_aql_node_t* element = TRI_AQL_NODE_SUBNODE(list, i);

          AppendIndent(generator);
          AppendCode(generator, "lhs = ");
          SetVariablePrefix(generator, "l");
          GenerateCode(generator, TRI_AQL_NODE_SUBNODE(element, 1));
          AppendCode(generator, ";\n");
          
          AppendIndent(generator);
          AppendCode(generator, "rhs = ");
          SetVariablePrefix(generator, "r");
          GenerateCode(generator, TRI_AQL_NODE_SUBNODE(element, 1));
          AppendCode(generator, ";\n");
          SetVariablePrefix(generator, NULL);

          AppendIndent(generator);
          AppendCode(generator, "if (AHUACATL_RELATIONAL_LESS(lhs, rhs)) {\n");
          Indent(generator);
          AppendIndent(generator);
          AppendCode(generator, "return -1;\n");
          Outdent(generator);
          AppendIndent(generator);
          AppendCode(generator, "}\n");
          AppendIndent(generator);
          AppendCode(generator, "if (AHUACATL_RELATIONAL_GREATER(lhs, rhs)) {\n");
          Indent(generator);
          AppendIndent(generator);
          AppendCode(generator, "return 1;\n");
          Outdent(generator);
          AppendIndent(generator);
          AppendCode(generator, "}\n");
        }

        AppendIndent(generator);
        AppendCode(generator, "return 0;");
        EndScope(generator);


        groupFuncName = GetNextFunctionName(generator);

        StartScope(generator, AQL_SCOPE_RESULT, groupFuncName);
        AppendIndent(generator);

        AppendCode(generator, "return { ");

        n = list->_subNodes._length;
        for (i = 0; i < n; ++i) {
          TRI_aql_node_t* element = TRI_AQL_NODE_SUBNODE(list, i);
          TRI_aql_node_t* varNode = TRI_AQL_NODE_SUBNODE(element, 0);
          if (i > 0) {
            AppendCode(generator, ", ");
          }
          AppendCode(generator, "'");
          AppendCode(generator, TRI_AQL_NODE_STRING(varNode));
          AppendCode(generator, "' : ");
          GenerateCode(generator, TRI_AQL_NODE_SUBNODE(element, 1));
        }
        AppendCode(generator, " };\n");
        EndScope(generator);

        AppendIndent(generator);
        AppendCode(generator, "result.push(AHUACATL_CLONE($));\n");
        CloseForLoops(generator);
        AppendCode(generator, "  result = AHUACATL_GROUP(result, ");
        AppendCode(generator, sortFuncName);
        AppendCode(generator, ", ");
        AppendCode(generator, groupFuncName);

        if (TRI_AQL_NODE_SUBNODE(node, 1)) {
          AppendCode(generator, ", '");
          AppendVarname0(generator, TRI_AQL_NODE_SUBNODE(node, 1));
          AppendCode(generator, "'");
        }
        AppendCode(generator, ");\n");
        previousFunction = EndScope(generator);

        StartScope(generator, AQL_SCOPE_RESULT, GetNextFunctionName(generator));
        AppendIndent(generator);
        AppendCode(generator, "var __group = ");
        AppendCode(generator, previousFunction);
        AppendCode(generator, "($);\n");
        AppendIndent(generator);
        AppendCode(generator, "for (var _group in __group) {\n");
        IncreaseForCount(generator);
        AppendIndent(generator);
        AppendOwnPropertyName(generator, "group");
        AppendIndent(generator);
        AppendCode(generator, "var $ = __group[_group];\n");
        break;
      }
      case AQL_NODE_EXPAND: { 
        char* funcName = GetNextFunctionName(generator);
        
        if (!funcName) {
          assert(false);
          // TODO: oom
        }
        
        StartScope(generator, AQL_SCOPE_RESULT, funcName);
        AppendIndent(generator);
        AppendCode(generator, "var __e = AHUACATL_LIST($);\n");
        AppendIndent(generator);
        AppendCode(generator, "for (var _e in __e) {\n");
        Indent(generator);
        AppendOwnPropertyName(generator, "e");

        AppendIndent(generator);
        AppendCode(generator, "result.push(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ");\n");
        Outdent(generator);
        AppendIndent(generator);
        AppendCode(generator, "}\n");
        EndScope(generator);
        
        AppendCode(generator, funcName);
        AppendCode(generator, "(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ")");
        break;
      }
      case AQL_NODE_ASSIGN: 
        AppendIndent(generator);
        AppendVarname(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, " = ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ";\n");
        break;
      case AQL_NODE_SUBQUERY: {
        char* funcName = GetNextFunctionName(generator);
          
        if (!funcName) {
          assert(false);
          // TODO: oom
        }

        StartScope(generator, AQL_SCOPE_RESULT, funcName);
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));

        AppendCode(generator, funcName);
        AppendCode(generator, "($)");
        break;
      }
      case AQL_NODE_FILTER:
        AppendIndent(generator);
        AppendCode(generator, "if (!(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ")) {\n");
        Indent(generator);
        AppendIndent(generator);
        AppendCode(generator, "continue;\n");
        Outdent(generator);
        AppendIndent(generator);
        AppendCode(generator, "}\n");
        break;
      case AQL_NODE_RETURN:
        AppendIndent(generator);
        AppendCode(generator, "result.push(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ");\n");
        CloseForLoops(generator);
        EndScope(generator);
        break;
      case AQL_NODE_LIMIT: {
        char* previousFunction; 

        AppendIndent(generator);
        AppendCode(generator, "result.push($);\n");
        CloseForLoops(generator);
        AppendCode(generator, "  result = AHUACATL_LIMIT(result, ");

        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ");\n");
        previousFunction = EndScope(generator);

        StartScope(generator, AQL_SCOPE_RESULT, GetNextFunctionName(generator));
        AppendIndent(generator);
        AppendCode(generator, "var __limit = ");
        AppendCode(generator, previousFunction);
        AppendCode(generator, "($);\n");
        AppendCode(generator, "for (var _limit in __limit) {\n");
        IncreaseForCount(generator);
        AppendIndent(generator);
        AppendOwnPropertyName(generator, "limit");
        AppendIndent(generator);
        AppendCode(generator, "var $ = __limit[_limit];\n");
        break;
      }
      case AQL_NODE_SORT: {
        TRI_aql_codegen_scope_t* scope = GetCurrentScope(generator);
        TRI_aql_node_t* list;
        char* previousFunction; 
        char* sortFuncName;
        size_t i;
        size_t n;

        previousFunction = scope->_funcName;
        // todo: OOM

        sortFuncName = GetNextFunctionName(generator);

        StartScope(generator, AQL_SCOPE_COMPARE, sortFuncName);
        AppendIndent(generator);
        AppendCode(generator, "var lhs, rhs;\n");

        list = TRI_AQL_NODE_SUBNODE(node, 0);
        n = list->_subNodes._length;
        for (i = 0; i < n; ++i) {
          TRI_aql_node_t* element = TRI_AQL_NODE_SUBNODE(list, i);

          AppendIndent(generator);
          AppendCode(generator, "lhs = ");
          SetVariablePrefix(generator, "l");
          GenerateCode(generator, TRI_AQL_NODE_SUBNODE(element, 0));
          AppendCode(generator, ";\n");
          
          AppendIndent(generator);
          AppendCode(generator, "rhs = ");
          SetVariablePrefix(generator, "r");
          GenerateCode(generator, TRI_AQL_NODE_SUBNODE(element, 0));
          AppendCode(generator, ";\n");
          SetVariablePrefix(generator, NULL);

          AppendIndent(generator);
          AppendCode(generator, "if (AHUACATL_RELATIONAL_LESS(lhs, rhs)) {\n");
          Indent(generator);
          AppendIndent(generator);
          if (TRI_AQL_NODE_BOOL(element)) {
            AppendCode(generator, "return -1;\n");
          } 
          else {
            AppendCode(generator, "return 1;\n");
          }
          Outdent(generator);
          AppendIndent(generator);
          AppendCode(generator, "}\n");
          AppendIndent(generator);
          AppendCode(generator, "if (AHUACATL_RELATIONAL_GREATER(lhs, rhs)) {\n");
          Indent(generator);
          AppendIndent(generator);
          if (TRI_AQL_NODE_BOOL(element)) {
            AppendCode(generator, "return 1;\n");
          }
          else {
            AppendCode(generator, "return -1;\n");
          }
          Outdent(generator);
          AppendIndent(generator);
          AppendCode(generator, "}\n");
        }

        AppendIndent(generator);
        AppendCode(generator, "return 0;");
        EndScope(generator);


        AppendIndent(generator);
        AppendCode(generator, "result.push(AHUACATL_CLONE($));\n");
        CloseForLoops(generator);
        AppendCode(generator, "  AHUACATL_SORT(result, ");
        AppendCode(generator, sortFuncName);
        AppendCode(generator, ");\n");
        EndScope(generator);

        StartScope(generator, AQL_SCOPE_RESULT, GetNextFunctionName(generator));
        AppendIndent(generator);
        AppendCode(generator, "var __sort = ");
        AppendCode(generator, previousFunction);
        AppendCode(generator, "($);\n");

        AppendIndent(generator);
        AppendCode(generator, "for (var _sort in __sort) {\n");
        IncreaseForCount(generator);
        AppendOwnPropertyName(generator, "sort");
        AppendIndent(generator);
        AppendCode(generator, "var $ = __sort[_sort];\n");
        break;
      }
      case AQL_NODE_VARIABLE:
        AppendVarname(generator, node);
        break;
      case AQL_NODE_COLLECTION:
        AppendCode(generator, "AHUACATL_GET_DOCUMENTS('");
        AppendCode(generator, TRI_AQL_NODE_STRING(node));
        AppendCode(generator, "')");
        break;
      case AQL_NODE_REFERENCE: {
        TRI_aql_codegen_scope_t* scope= GetCurrentScope(generator);
  
        if (scope->_variablePrefix) {
          AppendCode(generator, scope->_variablePrefix);
          AppendCode(generator, "['");
          AppendCode(generator, TRI_AQL_NODE_STRING(node));
          AppendCode(generator, "']");
        }
        else {
          AppendCode(generator, "$['");
          AppendCode(generator, TRI_AQL_NODE_STRING(node));
          AppendCode(generator, "']");
        }
        break;
      }
      case AQL_NODE_PARAMETER:
        AppendCode(generator, "AHUACATL_GET_PARAMETER('");
        AppendCode(generator, TRI_AQL_NODE_STRING(node));
        AppendCode(generator, "')");
        break;
      case AQL_NODE_ATTRIBUTE_ACCESS:
        AppendCode(generator, "AHUACATL_DOCUMENT_MEMBER(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", '");
        AppendCode(generator, TRI_AQL_NODE_STRING(node));
        AppendCode(generator, "')");
        break;
      case AQL_NODE_INDEXED:
        AppendCode(generator, "AHUACATL_GET_INDEX(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_ATTRIBUTE:
        AppendCode(generator, "AHUACATL_DOCUMENT_MEMBER(__e[_e], '");
        AppendCode(generator, TRI_AQL_NODE_STRING(node));
        AppendCode(generator, "')");
        break;
      case AQL_NODE_OPERATOR_UNARY_NOT:
        AppendCode(generator, "AHUACATL_LOGICAL_NOT(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_OPERATOR_UNARY_PLUS:
        AppendCode(generator, "AHUACATL_UNARY_PLUS(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_OPERATOR_UNARY_MINUS:
        AppendCode(generator, "AHUACATL_UNARY_MINUS(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_OPERATOR_BINARY_AND:
        AppendCode(generator, "AHUACATL_LOGICAL_AND(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_OPERATOR_BINARY_OR:
        AppendCode(generator, "AHUACATL_LOGICAL_OR(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_OPERATOR_BINARY_PLUS:
        AppendCode(generator, "AHUACATL_ARITHMETIC_PLUS(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_OPERATOR_BINARY_MINUS:
        AppendCode(generator, "AHUACATL_ARITHMETIC_MINUS(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_OPERATOR_BINARY_TIMES:
        AppendCode(generator, "AHUACATL_ARITHMETIC_TIMES(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_OPERATOR_BINARY_DIV:
        AppendCode(generator, "AHUACATL_ARITHMETIC_DIVIDE(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_OPERATOR_BINARY_MOD:
        AppendCode(generator, "AHUACATL_ARITHMETIC_MODULUS(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_OPERATOR_BINARY_EQ:
        AppendCode(generator, "AHUACATL_RELATIONAL_EQUAL(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_OPERATOR_BINARY_NE:
        AppendCode(generator, "AHUACATL_RELATIONAL_UNEQUAL(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_OPERATOR_BINARY_LT:
        AppendCode(generator, "AHUACATL_RELATIONAL_LESS(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_OPERATOR_BINARY_LE:
        AppendCode(generator, "AHUACATL_RELATIONAL_LESSEQUAL(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_OPERATOR_BINARY_GT:
        AppendCode(generator, "AHUACATL_RELATIONAL_GREATER(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_OPERATOR_BINARY_GE:
        AppendCode(generator, "AHUACATL_RELATIONAL_GREATEREQUAL(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_OPERATOR_BINARY_IN:
        AppendCode(generator, "AHUACATL_RELATIONAL_IN(");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 1));
        AppendCode(generator, ")");
        break;
      case AQL_NODE_FCALL: {
        AppendCode(generator, "AHUACATL_FCALL(");
        AppendCode(generator, TRI_AQL_NODE_STRING(node));
        AppendCode(generator, ", ");
        GenerateCode(generator, TRI_AQL_NODE_SUBNODE(node, 0));
        AppendCode(generator, ")");
      }
      default:
        break;
    }
    
    node = node->_next;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @} 
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a code generator 
////////////////////////////////////////////////////////////////////////////////

TRI_aql_codegen_t* TRI_CreateCodegenAql (void) {
  TRI_aql_codegen_t* generator;
  
  generator = (TRI_aql_codegen_t*) TRI_Allocate(sizeof(TRI_aql_codegen_t));

  if (!generator) {
    return NULL;
  }

  generator->_error = false;
  generator->_funcIndex = 0;
  generator->_funcName = NULL;

  TRI_InitStringBuffer(&generator->_buffer);
  
  TRI_InitVectorPointer(&generator->_strings);

  TRI_InitVectorPointer(&generator->_scopes);
  
  if (!StartScope(generator, AQL_SCOPE_RESULT, GetNextFunctionName(generator))) { 
    TRI_FreeCodegenAql(generator);
    return NULL;
  }
  
  return generator;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the code generator 
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCodegenAql (TRI_aql_codegen_t* const generator) {
  size_t i;

  assert(generator);
  
  TRI_DestroyStringBuffer(&generator->_buffer);
  TRI_DestroyVectorPointer(&generator->_scopes);

  // free strings
  i = generator->_strings._length;
  while (i--) {
    void* string = generator->_strings._buffer[i];
    if (string) {
      TRI_Free(string);
    }
  }
  TRI_DestroyVectorPointer(&generator->_strings);

  TRI_Free(generator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate Javascript code for the AST nodes recursively
////////////////////////////////////////////////////////////////////////////////

char* TRI_GenerateCodeAql (const void* const data) {
  TRI_aql_node_t* node = (TRI_aql_node_t*) data;
  TRI_aql_codegen_t* generator;
  char* code = NULL;

  if (!node) {
    // todo: handle errors
    return code;
  }

  generator = TRI_CreateCodegenAql();
  if (!generator) {
    // todo: handle errors
    return code;
  }

  TRI_AppendStringStringBuffer(&generator->_buffer, "(function() {\n");

  GenerateCode(generator, node);
  if (!generator->_funcName) {
    generator->_error = true;
  }

  if (!generator->_error) {
    char* funcName = generator->_funcName;

    TRI_AppendStringStringBuffer(&generator->_buffer, "try {\n");
    TRI_AppendStringStringBuffer(&generator->_buffer, "  return ");
    TRI_AppendStringStringBuffer(&generator->_buffer, funcName);
    TRI_AppendStringStringBuffer(&generator->_buffer, "( { } );\n");
    TRI_AppendStringStringBuffer(&generator->_buffer, "}\n");
    TRI_AppendStringStringBuffer(&generator->_buffer, "catch (e) {\n");
    TRI_AppendStringStringBuffer(&generator->_buffer, "print(e);\n");
    TRI_AppendStringStringBuffer(&generator->_buffer, "}\n");

    TRI_AppendStringStringBuffer(&generator->_buffer, "})();\n");

    code = TRI_DuplicateString(generator->_buffer._buffer);
    LOG_TRACE("generated code:\n%s\n",code);
    // printf("generated code:\n%s\n",code);
  }

  TRI_FreeCodegenAql(generator);

  return code;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
