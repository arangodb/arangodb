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

#include "Ahuacatl/ahuacatl-codegen-js.h"
#include "Ahuacatl/ahuacatl-functions.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

#define REGISTER_PREFIX "r"
#define FUNCTION_PREFIX "f"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief append the value of a value node to the current scope's output buffer
////////////////////////////////////////////////////////////////////////////////

static void DumpNode (TRI_aql_codegen_js_t* const generator, const TRI_aql_node_t* const node);

static TRI_string_buffer_t* GetBuffer (TRI_aql_codegen_js_t* const generator) {
  TRI_aql_codegen_function_t* function;
  size_t n;
  
  n = generator->_functions._length;
  assert(n > 0);

  function = (TRI_aql_codegen_function_t*) generator->_functions._buffer[n - 1];
  assert(function);

  return &function->_buffer;
}

static size_t NextRegister (TRI_aql_codegen_js_t* const generator) {
  return ++generator->_registerIndex;
}

static size_t NextFunction (TRI_aql_codegen_js_t* const generator) {
  return ++generator->_functionIndex;
}

static TRI_aql_codegen_function_t* CreateFunction (TRI_aql_codegen_js_t* const generator) {   
  TRI_aql_codegen_function_t* function;

  function = (TRI_aql_codegen_function_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_codegen_function_t), false);
  if (!function) {
    generator->_error = true;
    return NULL;
  }

  function->_index = NextFunction(generator);
  function->_forCount = 0;
  function->_prefix = NULL;

  TRI_InitStringBuffer(&function->_buffer, TRI_UNKNOWN_MEM_ZONE);

  return function;
}

static void FreeFunction (TRI_aql_codegen_function_t* const function) {
  assert(function);

  TRI_DestroyStringBuffer(&function->_buffer);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, function);
}

static void AppendString (TRI_aql_codegen_js_t* const generator, 
                          const char* const value) {
  if (!value) {
    generator->_error = true;
    return;
  }

  TRI_AppendStringStringBuffer(GetBuffer(generator), value);
}

static void AppendInt (TRI_aql_codegen_js_t* const generator, 
                       const int64_t value) {
  TRI_AppendInt64StringBuffer(GetBuffer(generator), value);
}

static void AppendQuoted (TRI_aql_codegen_js_t* const generator, 
                          const char* name) {
  if (!name) {
    generator->_error = true;
    return;
  }

  TRI_AppendCharStringBuffer(GetBuffer(generator), '\'');
  AppendString(generator, name);
  TRI_AppendCharStringBuffer(GetBuffer(generator), '\'');
}

static void AppendResultVariable (TRI_aql_codegen_js_t* const generator,
                                  const TRI_aql_node_t* const nameNode) {
  if (!nameNode) {
    generator->_error = true;
    return;
  }

  TRI_AppendStringStringBuffer(GetBuffer(generator), "$[");
  AppendQuoted(generator, nameNode->_value._value._string);
  TRI_AppendStringStringBuffer(GetBuffer(generator), "]");
}

static void AppendFuncName (TRI_aql_codegen_js_t* const generator, 
                            const size_t funcIndex) {
  AppendString(generator, FUNCTION_PREFIX);
  AppendInt(generator, (int64_t) funcIndex);
}

static void AppendRegisterName (TRI_aql_codegen_js_t* const generator, 
                                const size_t registerIndex) {
  AppendString(generator, REGISTER_PREFIX);
  AppendInt(generator, (int64_t) registerIndex);
}

static void AppendOwnPropertyCheck (TRI_aql_codegen_js_t* const generator, 
                                    const size_t listRegister,
                                    const size_t keyRegister) {
  AppendString(generator, "if (!");
  AppendRegisterName(generator, listRegister);
  AppendString(generator, ".hasOwnProperty(");
  AppendRegisterName(generator, keyRegister); 
  AppendString(generator, ")) {\n");
  AppendString(generator, "continue;\n");
  AppendString(generator, "}\n");
}

static void StartFunction (TRI_aql_codegen_js_t* const generator,
                           const TRI_aql_codegen_function_type_e type) {
  TRI_aql_codegen_function_t* function = CreateFunction(generator);

  if (!function) {
    generator->_error = true;
    return;
  }
  
  TRI_PushBackVectorPointer(&generator->_functions, function);
  
  AppendString(generator, "function ");
  AppendFuncName(generator, function->_index);
  if (type == AQL_FUNCTION_COMPARE) {
    AppendString(generator, "(l, r) {\n");
  } 
  else {
    AppendString(generator, "($) {\n");
  }
}

static size_t EndFunction (TRI_aql_codegen_js_t* const generator) {
  TRI_aql_codegen_function_t* function;
  size_t n;
  size_t funcIndex;

  AppendString(generator, "}\n");

  n = generator->_functions._length;
  assert(n > 0);
  function = TRI_RemoveVectorPointer(&generator->_functions, n -1);
  assert(function);

  TRI_AppendStringStringBuffer(&generator->_buffer, function->_buffer._buffer);
  funcIndex = function->_index;

  FreeFunction(function);

  return funcIndex;
}

static TRI_aql_codegen_function_t* CurrentFunction (TRI_aql_codegen_js_t* const generator) {
  TRI_aql_codegen_function_t* function;
  size_t n;

  n = generator->_functions._length;
  assert(n > 0);

  function = (TRI_aql_codegen_function_t*) generator->_functions._buffer[n - 1];
  assert(function);

  return function;
}
  
static void IncreaseForCount (TRI_aql_codegen_js_t* const generator) {
  TRI_aql_codegen_function_t* function = CurrentFunction(generator);

  if (!function) {
    return;
  }

  ++function->_forCount;
}

static void CloseForLoops (TRI_aql_codegen_js_t* const generator) {
  TRI_aql_codegen_function_t* function = CurrentFunction(generator);
  size_t i;

  if (!function) {
    return;
  }

  for (i = 0; i < function->_forCount; ++i) {
    AppendString(generator, "}\n");
  }

  function->_forCount = 0;
}

static void AppendForStart (TRI_aql_codegen_js_t* const generator, 
                            const size_t listRegister,
                            const size_t keyRegister) {
  AppendString(generator, "for (var "); 
  AppendRegisterName(generator, keyRegister);
  AppendString(generator, " in ");
  AppendRegisterName(generator, listRegister);
  AppendString(generator, ") {\n");
  IncreaseForCount(generator);
  AppendOwnPropertyCheck(generator, listRegister, keyRegister);
}

static size_t HandleSortNode (TRI_aql_codegen_js_t* const generator, 
                              const TRI_aql_node_t* const node,
                              const size_t idx) {
  TRI_aql_node_t* list;
  TRI_aql_codegen_function_t* function;
  size_t funcIndex;
  size_t i;
  size_t n;

  StartFunction(generator, AQL_FUNCTION_COMPARE);
  AppendString(generator, "var lhs, rhs;\n");

  function = CurrentFunction(generator);

  list = TRI_AQL_NODE_MEMBER(node, 0);
  n = list->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* element = TRI_AQL_NODE_MEMBER(list, i);

    AppendString(generator, "lhs = ");
    function->_prefix = "l";
    DumpNode(generator, TRI_AQL_NODE_MEMBER(element, idx));
    AppendString(generator, ";\n");
          
    AppendString(generator, "rhs = ");
    function->_prefix = "r";
    DumpNode(generator, TRI_AQL_NODE_MEMBER(element, idx));
    AppendString(generator, ";\n");
    function->_prefix = NULL;

    AppendString(generator, "if (AHUACATL_RELATIONAL_LESS(lhs, rhs)) {\n");
    AppendString(generator, (idx || TRI_AQL_NODE_BOOL(element)) ? "return -1;\n" : "return 1;\n");
    AppendString(generator, "}\n");
    AppendString(generator, "if (AHUACATL_RELATIONAL_GREATER(lhs, rhs)) {\n");
    AppendString(generator, (idx || TRI_AQL_NODE_BOOL(element)) ? "return 1;\n" : "return -1;\n");
    AppendString(generator, "}\n");
  }
  AppendString(generator, "return 0;\n");

  funcIndex = EndFunction(generator);

  return funcIndex;
}

static void HandleValue (TRI_aql_codegen_js_t* const generator, 
                         const TRI_aql_node_t* const node) {
  if (!TRI_ValueJavascriptAql(GetBuffer(generator), &node->_value, node->_value._type)) {
    generator->_error = true;
  }
}

static void HandleList (TRI_aql_codegen_js_t* const generator, 
                        const TRI_aql_node_t* const node) {
  size_t i, n;

  AppendString(generator, "[ ");
  n = node->_members._length;
  for (i = 0; i < n; ++i) {
    if (i > 0) {
      AppendString(generator, ", ");
    }
    DumpNode(generator, TRI_AQL_NODE_MEMBER(node, i));
  }
  AppendString(generator, " ]");
}

static void HandleArray (TRI_aql_codegen_js_t* const generator, 
                         const TRI_aql_node_t* const node) {
  size_t i, n;

  AppendString(generator, "{ ");
  n = node->_members._length;
  for (i = 0; i < n; ++i) {
    if (i > 0) {
      AppendString(generator, ", ");
    }
    DumpNode(generator, TRI_AQL_NODE_MEMBER(node, i));
  }
  AppendString(generator, " }");
} 

static void HandleArrayElement (TRI_aql_codegen_js_t* const generator, 
                                const TRI_aql_node_t* const node) {
  TRI_ValueJavascriptAql(GetBuffer(generator), &node->_value, AQL_TYPE_STRING);
  AppendString(generator, " : ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
}

static void HandleCollection (TRI_aql_codegen_js_t* const generator,
                              const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_GET_DOCUMENTS('");
  AppendString(generator, TRI_AQL_NODE_STRING(node));
  AppendString(generator, "')");
}

static void HandleReference (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) { 
  TRI_aql_codegen_function_t* function;
  
  function = CurrentFunction(generator);
  if (function && function->_prefix) {
    AppendString(generator, function->_prefix);
    AppendString(generator, "['");
    AppendString(generator, TRI_AQL_NODE_STRING(node));
    AppendString(generator, "']");
    return;
  }

  AppendString(generator, "$[");
  AppendQuoted(generator, TRI_AQL_NODE_STRING(node));
  AppendString(generator, "]");
}

static void HandleAttributeAccess (TRI_aql_codegen_js_t* const generator,
                                   const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_DOCUMENT_MEMBER(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", '");
  AppendString(generator, TRI_AQL_NODE_STRING(node));
  AppendString(generator, "')");
}

static void HandleAttribute (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_DOCUMENT_MEMBER(__e[_e], '");
  AppendString(generator, TRI_AQL_NODE_STRING(node));
  AppendString(generator, "')");
}

static void HandleIndexed (TRI_aql_codegen_js_t* const generator,
                           const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_GET_INDEX(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleExpand (TRI_aql_codegen_js_t* const generator,
                          const TRI_aql_node_t* const node) {
  size_t r1 = NextRegister(generator);
  size_t r2 = NextRegister(generator);
  size_t funcIndex;

  StartFunction(generator, AQL_FUNCTION_STANDALONE);

  AppendString(generator, "var result = [];\n");
  AppendString(generator, "var "); 
  AppendRegisterName(generator, r1);
  AppendString(generator, " = AHUACATL_LIST($);\n");
  AppendString(generator, "for (var "); 
  AppendRegisterName(generator, r2);
  AppendString(generator, " in "); 
  AppendRegisterName(generator, r1);
  AppendString(generator, ") {\n"); 
  AppendOwnPropertyCheck(generator, r1, r2);
  AppendString(generator, "result.push(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ");\n");
  AppendString(generator, "}\n");
  AppendString(generator, "return result;\n");

  funcIndex = EndFunction(generator);
  AppendFuncName(generator, funcIndex);
  AppendString(generator, "(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ")");
}

static void HandleUnaryNot (TRI_aql_codegen_js_t* const generator,
                            const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_LOGICAL_NOT(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ")");
}

static void HandleUnaryMinus (TRI_aql_codegen_js_t* const generator,
                            const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_UNARY_MINUS(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ")");
}

static void HandleUnaryPlus (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_UNARY_PLUS(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ")");
}

static void HandleBinaryAnd (TRI_aql_codegen_js_t* const generator,
                             const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_LOGICAL_AND(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryOr (TRI_aql_codegen_js_t* const generator,
                            const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_LOGICAL_OR(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryPlus (TRI_aql_codegen_js_t* const generator,
                              const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_ARITHMETIC_PLUS(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryMinus (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_ARITHMETIC_MINUS(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryTimes (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_ARITHMETIC_TIMES(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryDivide (TRI_aql_codegen_js_t* const generator,
                                const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_ARITHMETIC_DIVIDE(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryModulus(TRI_aql_codegen_js_t* const generator,
                                const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_ARITHMETIC_MODULUS(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryEqual (TRI_aql_codegen_js_t* const generator,
                               const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_RELATIONAL_EQUAL(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryUnequal (TRI_aql_codegen_js_t* const generator,
                                 const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_RELATIONAL_UNEQUAL(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryLessEqual (TRI_aql_codegen_js_t* const generator,
                                   const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_RELATIONAL_LESSEQUAL(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryLess (TRI_aql_codegen_js_t* const generator,
                              const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_RELATIONAL_LESS(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryGreaterEqual (TRI_aql_codegen_js_t* const generator,
                                      const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_RELATIONAL_GREATEREQUAL(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryGreater (TRI_aql_codegen_js_t* const generator,
                                 const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_RELATIONAL_GREATER(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleBinaryIn (TRI_aql_codegen_js_t* const generator,
                            const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_RELATIONAL_IN(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ")");
}

static void HandleFcall (TRI_aql_codegen_js_t* const generator,
                         const TRI_aql_node_t* const node) {
  AppendString(generator, "AHUACATL_FCALL(");
  AppendString(generator, TRI_GetInternalNameFunctionAql((TRI_aql_function_t*) TRI_AQL_NODE_DATA(node)));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ")");
}

static void HandleFor (TRI_aql_codegen_js_t* const generator, 
                       const TRI_aql_node_t* const node) {
  size_t r1 = NextRegister(generator);
  size_t r2 = NextRegister(generator);

  AppendString(generator, "var ");
  AppendRegisterName(generator, r1); 
  AppendString(generator, " = AHUACATL_LIST(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ");\n");
        
  AppendForStart(generator, r1, r2);
  AppendResultVariable(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, " = ");
  AppendRegisterName(generator, r1);
  AppendString(generator, "[");
  AppendRegisterName(generator, r2); 
  AppendString(generator, "];\n");
}

static void HandleReturn (TRI_aql_codegen_js_t* const generator, 
                          const TRI_aql_node_t* const node) {
  AppendString(generator, "result.push(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ");\n");

  CloseForLoops(generator);
}

static void HandleFilter (TRI_aql_codegen_js_t* const generator, 
                          const TRI_aql_node_t* const node) {
  AppendString(generator, "if (!(");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ")) {\n");
  AppendString(generator, "continue;\n");
  AppendString(generator, "}\n");
}

static void HandleSort (TRI_aql_codegen_js_t* const generator, 
                        const TRI_aql_node_t* const node) {
  size_t r1 = NextRegister(generator);
  size_t r2 = NextRegister(generator);
  size_t funcIndex;

  funcIndex = HandleSortNode(generator, node, 0);
        
  AppendString(generator, "result.push(AHUACATL_CLONE($));\n");
  CloseForLoops(generator);
  AppendString(generator, "AHUACATL_SORT(result, ");
  AppendFuncName(generator, funcIndex);
  AppendString(generator, ");\n");
  AppendString(generator, "return result;\n");
  funcIndex = EndFunction(generator);

  StartFunction(generator, AQL_FUNCTION_STANDALONE);
  AppendString(generator, "var result = [ ];\n");
  AppendString(generator, "var ");
  AppendRegisterName(generator, r1);
  AppendString(generator, " = ");
  AppendFuncName(generator, funcIndex);
  AppendString(generator, "($);\n");

  AppendForStart(generator, r1, r2);
  AppendString(generator, "var $ = ");
  AppendRegisterName(generator, r1);
  AppendString(generator, "[");
  AppendRegisterName(generator, r2);
  AppendString(generator, "];\n");
}

static void HandleLimit (TRI_aql_codegen_js_t* const generator, 
                         const TRI_aql_node_t* const node) {
  size_t r1 = NextRegister(generator);
  size_t r2 = NextRegister(generator);
  size_t funcIndex;

  AppendString(generator, "result.push($);\n");
  CloseForLoops(generator);
  AppendString(generator, "result = AHUACATL_LIMIT(result, ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, ", ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ");\n");
  AppendString(generator, "return result;\n");

  funcIndex = EndFunction(generator);

  StartFunction(generator, AQL_FUNCTION_STANDALONE);
  AppendString(generator, "var result = [];\n");
  AppendString(generator, "var ");
  AppendRegisterName(generator, r1);
  AppendString(generator, " = ");
  AppendFuncName(generator, funcIndex);
  AppendString(generator, "($);\n");
  AppendString(generator, "for (var ");
  AppendRegisterName(generator, r2);
  AppendString(generator, " in ");
  AppendRegisterName(generator, r1);
  AppendString(generator, ") {\n");
  IncreaseForCount(generator);
  AppendOwnPropertyCheck(generator, r1, r2);
  AppendString(generator, "var $ = ");
  AppendRegisterName(generator, r1);
  AppendString(generator, "[");
  AppendRegisterName(generator, r2);
  AppendString(generator, "];\n");
}

static void HandleAssign (TRI_aql_codegen_js_t* const generator, 
                          const TRI_aql_node_t* const node) {
  AppendResultVariable(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, " = ");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 1));
  AppendString(generator, ";\n");
}

static void HandleCollect (TRI_aql_codegen_js_t* const generator, 
                           const TRI_aql_node_t* const node) {
  TRI_aql_node_t* list;
  size_t r1 = NextRegister(generator);
  size_t r2 = NextRegister(generator);
  size_t sortFuncIndex;
  size_t groupFuncIndex;
  size_t funcIndex;
  size_t i;
  size_t n;

  sortFuncIndex = HandleSortNode(generator, node, 1);

  StartFunction(generator, AQL_FUNCTION_STANDALONE);
  AppendString(generator, "return { ");
  list = TRI_AQL_NODE_MEMBER(node, 0);
  n = list->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* element = TRI_AQL_NODE_MEMBER(list, i);
    TRI_aql_node_t* varNode = TRI_AQL_NODE_MEMBER(element, 0);
    if (i > 0) {
      AppendString(generator, ", ");
    }
    AppendQuoted(generator, TRI_AQL_NODE_STRING(varNode));
    AppendString(generator, " : ");
    DumpNode(generator, TRI_AQL_NODE_MEMBER(element, 1));
  }
  AppendString(generator, " };\n");
  groupFuncIndex = EndFunction(generator);
        
  AppendString(generator, "result.push(AHUACATL_CLONE($));\n");
  CloseForLoops(generator);
  AppendString(generator, "result = AHUACATL_GROUP(result, ");
  AppendFuncName(generator, sortFuncIndex);
  AppendString(generator, ", ");
  AppendFuncName(generator, groupFuncIndex);

  if (TRI_AQL_NODE_MEMBER(node, 1)) {
    TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(node, 1);
    AppendString(generator, ", ");
    AppendQuoted(generator, TRI_AQL_NODE_STRING(nameNode));
  }
  AppendString(generator, ");\n");
  AppendString(generator, "return result;\n");
  funcIndex = EndFunction(generator);
 
  StartFunction(generator, AQL_FUNCTION_STANDALONE); 
  AppendString(generator, "var result = [ ];\n");
  AppendString(generator, "var ");
  AppendRegisterName(generator, r1);
  AppendString(generator, " = ");
  AppendFuncName(generator, funcIndex);
  AppendString(generator, "($);\n");
  AppendForStart(generator, r1, r2);
  AppendString(generator, "var $ = ");
  AppendRegisterName(generator, r1);
  AppendString(generator, "[\n");
  AppendRegisterName(generator, r2);
  AppendString(generator, "];\n");
}

static void HandleSubquery (TRI_aql_codegen_js_t* const generator, 
                            const TRI_aql_node_t* const node) {
  size_t funcIndex;

  StartFunction(generator, AQL_FUNCTION_STANDALONE);
  AppendString(generator, "var result = [ ];\n");
  DumpNode(generator, TRI_AQL_NODE_MEMBER(node, 0));
  AppendString(generator, "return result;\n");
  funcIndex = EndFunction(generator);

  AppendFuncName(generator, funcIndex);
  AppendString(generator, "($)");
}

static void DumpNode (TRI_aql_codegen_js_t* generator, const TRI_aql_node_t* const data) {
  TRI_aql_node_t* node = (TRI_aql_node_t*) data;

  assert(generator);

  if (!node) {
    return;
  }

  while (node) {
    switch (node->_type) {
      case AQL_NODE_VALUE:
        HandleValue(generator, node);
        break;
      case AQL_NODE_LIST: 
        HandleList(generator, node);
        break;
      case AQL_NODE_ARRAY: 
        HandleArray(generator, node);
        break;
      case AQL_NODE_ARRAY_ELEMENT: 
        HandleArrayElement(generator, node);
        break;
      case AQL_NODE_COLLECTION:
        HandleCollection(generator, node);
        break;
      case AQL_NODE_REFERENCE: 
        HandleReference(generator, node);
        break;
      case AQL_NODE_ATTRIBUTE_ACCESS:
        HandleAttributeAccess(generator, node);
        break;
      case AQL_NODE_ATTRIBUTE:
        HandleAttribute(generator, node);
        break;
      case AQL_NODE_INDEXED:
        HandleIndexed(generator, node);
        break;
      case AQL_NODE_EXPAND:
        HandleExpand(generator, node);
        break; 
      case AQL_NODE_OPERATOR_UNARY_NOT:
        HandleUnaryNot(generator, node);
        break;
      case AQL_NODE_OPERATOR_UNARY_PLUS:
        HandleUnaryPlus(generator, node);
        break;
      case AQL_NODE_OPERATOR_UNARY_MINUS:
        HandleUnaryMinus(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_AND:
        HandleBinaryAnd(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_OR:
        HandleBinaryOr(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_PLUS:
        HandleBinaryPlus(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_MINUS:
        HandleBinaryMinus(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_TIMES:
        HandleBinaryTimes(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_DIV:
        HandleBinaryDivide(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_MOD:
        HandleBinaryModulus(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_EQ:
        HandleBinaryEqual(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_NE:
        HandleBinaryUnequal(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_LT:
        HandleBinaryLess(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_LE:
        HandleBinaryLessEqual(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_GT:
        HandleBinaryGreater(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_GE:
        HandleBinaryGreaterEqual(generator, node);
        break;
      case AQL_NODE_OPERATOR_BINARY_IN:
        HandleBinaryIn(generator, node);
        break;
      case AQL_NODE_FCALL:
        HandleFcall(generator, node);
        break;
      case AQL_NODE_FOR:
        HandleFor(generator, node);
        break;
      case AQL_NODE_COLLECT: 
        HandleCollect(generator, node);
        break;
      case AQL_NODE_ASSIGN:
        HandleAssign(generator, node);
        break;
      case AQL_NODE_FILTER:
        HandleFilter(generator, node);
        break;
      case AQL_NODE_LIMIT:
        HandleLimit(generator, node);
        break;
      case AQL_NODE_SORT:
        HandleSort(generator, node);
        break;
      case AQL_NODE_RETURN:
        HandleReturn(generator, node);
        break;
      case AQL_NODE_SUBQUERY:
        HandleSubquery(generator, node);
        break;
      default: {
      }
    }

    node = node->_next;
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief free memory associated with a code generator
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeGeneratorAql (TRI_aql_codegen_js_t* const generator) {
  TRI_DestroyVectorPointer(&generator->_functions);
  TRI_DestroyStringBuffer(&generator->_buffer);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, generator);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a code generator
////////////////////////////////////////////////////////////////////////////////

TRI_aql_codegen_js_t* TRI_CreateGeneratorAql (void) {
  TRI_aql_codegen_js_t* generator;

  generator = (TRI_aql_codegen_js_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_codegen_js_t), false);
  if (!generator) {
    return NULL;
  }

  TRI_InitStringBuffer(&generator->_buffer, TRI_UNKNOWN_MEM_ZONE);
  TRI_InitVectorPointer(&generator->_functions, TRI_UNKNOWN_MEM_ZONE);
  generator->_error = false;
  generator->_registerIndex = 0;
  generator->_functionIndex = 0;

  return generator;
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
/// @brief generate Javascript code for the AST nodes recursively
////////////////////////////////////////////////////////////////////////////////

TRI_aql_codegen_js_t* TRI_GenerateCodeAql (const void* const data) {
  TRI_aql_codegen_js_t* generator;
  size_t funcIndex;

  generator = TRI_CreateGeneratorAql();
  if (!generator) {
    return NULL;
  }

  StartFunction(generator, AQL_FUNCTION_STANDALONE);
  AppendString(generator, "var result = [];\n");
  DumpNode(generator, (TRI_aql_node_t*) data);
  AppendString(generator, "return result;\n");
  funcIndex = EndFunction(generator);

  TRI_AppendStringStringBuffer(&generator->_buffer, FUNCTION_PREFIX);
  TRI_AppendInt64StringBuffer(&generator->_buffer, (int64_t) funcIndex);
  TRI_AppendStringStringBuffer(&generator->_buffer, "({ });");

  LOG_DEBUG("generated code: %s", generator->_buffer._buffer);
  return generator;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
