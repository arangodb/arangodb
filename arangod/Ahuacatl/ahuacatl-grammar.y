
%define api.pure
%name-prefix="Ahuacatl"
%locations 
%defines
%parse-param { TRI_aql_context_t* const context }
%lex-param { void* scanner } 
%error-verbose

%{
#include <stdio.h>
#include <stdlib.h>

#include <BasicsC/common.h>
#include <BasicsC/conversions.h>
#include <BasicsC/tri-strings.h>

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-context.h"
#include "Ahuacatl/ahuacatl-error.h"
#include "Ahuacatl/ahuacatl-parser.h"
#include "Ahuacatl/ahuacatl-parser-functions.h"
#include "Ahuacatl/ahuacatl-scope.h"
%}

%union {
  TRI_aql_node_t* node;
  char* strval;
  bool boolval;
  int64_t intval;
}

%{

////////////////////////////////////////////////////////////////////////////////
/// @brief forward for lexer function defined in ahuacatl-tokens.l
////////////////////////////////////////////////////////////////////////////////

int Ahuacatllex (YYSTYPE*, YYLTYPE*, void*);
 
////////////////////////////////////////////////////////////////////////////////
/// @brief register parse error
////////////////////////////////////////////////////////////////////////////////

void Ahuacatlerror (YYLTYPE* locp, TRI_aql_context_t* const context, const char* err) {
  TRI_SetErrorParseAql(context, err, locp->first_line, locp->first_column);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut macro for signalling out of memory
////////////////////////////////////////////////////////////////////////////////

#define ABORT_OOM                                                              \
  TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);              \
  YYABORT;

#define scanner context->_parser->_scanner

%}

/* define tokens and "nice" token names */
%token T_FOR "FOR declaration"
%token T_LET "LET declaration" 
%token T_FILTER "FILTER declaration" 
%token T_RETURN "RETURN declaration" 
%token T_COLLECT "COLLECT declaration" 
%token T_SORT "SORT declaration" 
%token T_LIMIT "LIMIT declaration"

%token T_ASC "ASC keyword"
%token T_DESC "DESC keyword"
%token T_IN "IN keyword"
%token T_INTO "INTO keyword"

%token T_NULL "null" 
%token T_TRUE "true" 
%token T_FALSE "false"
%token T_STRING "identifier" 
%token T_QUOTED_STRING "quoted string" 
%token T_INTEGER "integer number" 
%token T_DOUBLE "number" 
%token T_PARAMETER "bind parameter"

%token T_ASSIGN "assignment"

%token T_NOT "not operator"
%token T_AND "and operator"
%token T_OR "or operator"

%token T_EQ "== operator"
%token T_NE "!= operator"
%token T_LT "< operator"
%token T_GT "> operator"
%token T_LE "<= operator"
%token T_GE ">= operator"

%token T_PLUS "+ operator"
%token T_MINUS "- operator"
%token T_TIMES "* operator"
%token T_DIV "/ operator"
%token T_MOD "% operator"
%token T_EXPAND "[*] operator"

%token T_QUESTION "?"
%token T_COLON ":"

%token T_COMMA ","
%token T_OPEN "("
%token T_CLOSE ")"
%token T_DOC_OPEN "{"
%token T_DOC_CLOSE "}"
%token T_LIST_OPEN "["
%token T_LIST_CLOSE "]"

%token T_END 0 "end of query string"


/* define operator precedence */
%left T_COMMA        
%right T_ASSIGN
%right T_QUESTION T_COLON
%left T_OR 
%left T_AND
%left T_EQ T_NE 
%left T_IN
%left T_LT T_GT T_LE T_GE
%left T_PLUS T_MINUS
%left T_TIMES T_DIV T_MOD
%right UMINUS UPLUS T_NOT
%left T_EXPAND
%left FUNCCALL
%left REFERENCE
%left INDEXED

/* define token return types */
%type <strval> T_STRING
%type <strval> T_QUOTED_STRING
%type <strval> T_INTEGER
%type <strval> T_DOUBLE
%type <strval> T_PARAMETER; 
%type <node> sort_list;
%type <node> sort_element;
%type <boolval> sort_direction;
%type <node> collect_list;
%type <node> collect_element;
%type <strval> optional_into;
%type <node> expression;
%type <node> operator_unary;
%type <node> operator_binary;
%type <node> operator_ternary;
%type <node> function_call;
%type <node> optional_function_call_arguments;
%type <node> function_arguments_list;
%type <node> compound_type;
%type <node> list;
%type <node> optional_list_elements;
%type <node> list_elements_list;
%type <node> array;
%type <node> optional_array_elements;
%type <node> array_elements_list;
%type <node> array_element;
%type <strval> array_element_name;
%type <node> reference;
%type <node> single_reference;
%type <node> expansion;
%type <node> atomic_value;
%type <node> value_literal;
%type <node> bind_parameter;
%type <strval> variable_name;
%type <node> integer_value;


/* define start token of language */
%start query

%%

query: 
    optional_statement_block_statements return_statement {
    }
  ;

optional_statement_block_statements:
    /* empty */ {
    }
  | optional_statement_block_statements statement_block_statement {
    }
  ;
  
statement_block_statement: 
    for_statement {
    }
  | let_statement {
    }
  | filter_statement {
    }
  | collect_statement {
    }
  | sort_statement {
    }
  | limit_statement {
    }
  ;

for_statement:
    T_FOR variable_name T_IN expression {
      TRI_aql_node_t* node;
      
      if (! TRI_StartScopeAql(context, TRI_AQL_SCOPE_FOR)) {
        ABORT_OOM
      }
      
      node = TRI_CreateNodeForAql(context, $2, $4);
      if (! node) {
        ABORT_OOM
      }

      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
  ;

filter_statement:
    T_FILTER expression {
      TRI_aql_node_t* node = TRI_CreateNodeFilterAql(context, $2);
      if (! node) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
  ;

let_statement:
    T_LET variable_name T_ASSIGN expression {
      TRI_aql_node_t* node = TRI_CreateNodeLetAql(context, $2, $4);
      if (! node) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
  ;

collect_statement: 
    T_COLLECT {
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      
      if (! node) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    } collect_list optional_into {
      TRI_aql_node_t* node = TRI_CreateNodeCollectAql(context, TRI_PopStackParseAql(context), $4);
      if (! node) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
  ;

collect_list: 
    collect_element {
    }
  | collect_list T_COMMA collect_element {
    }
  ;

collect_element:
    variable_name T_ASSIGN expression {
      TRI_aql_node_t* node = TRI_CreateNodeAssignAql(context, $1, $3);
      if (! node) {
        ABORT_OOM
      }

      if (! TRI_PushListAql(context, node)) {
        ABORT_OOM
      }
    }
  ;

optional_into: 
    /* empty */ {
      $$ = NULL;
    }
  | T_INTO variable_name {
      $$ = $2;
    }
  ;

sort_statement:
    T_SORT {
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      
      if (! node) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    } sort_list {
      TRI_aql_node_t* list = TRI_PopStackParseAql(context);
      TRI_aql_node_t* node = TRI_CreateNodeSortAql(context, list);
      if (! node) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
  ;

sort_list: 
    sort_element {
      if (! TRI_PushListAql(context, $1)) {
        ABORT_OOM
      }
    }
  | sort_list T_COMMA sort_element {
      if (! TRI_PushListAql(context, $3)) {
        ABORT_OOM
      }
    }
  ;

sort_element:
    expression sort_direction {
      TRI_aql_node_t* node = TRI_CreateNodeSortElementAql(context, $1, $2);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  ;

sort_direction:
    /* empty */ {
      $$ = true;
    }
  | T_ASC {
      $$ = true;
    } 
  | T_DESC {
      $$ = false;
    }
  ;

limit_statement: 
    T_LIMIT integer_value {
      TRI_aql_node_t* node = TRI_CreateNodeLimitAql(context, TRI_CreateNodeValueIntAql(context, 0), $2); 
      if (! node) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
    }
  | T_LIMIT integer_value T_COMMA integer_value {
      TRI_aql_node_t* node = TRI_CreateNodeLimitAql(context, $2, $4);
      if (! node) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
  ;

return_statement:
    T_RETURN expression {
      TRI_aql_node_t* node = TRI_CreateNodeReturnAql(context, $2);
      if (! node) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
      if (! TRI_EndScopeByReturnAql(context)) {
        ABORT_OOM
      }
      
      // $$ = node;
    }
  ;


expression:
    T_OPEN expression T_CLOSE {
      $$ = $2;
    }
  | T_OPEN {
      if (! TRI_StartScopeAql(context, TRI_AQL_SCOPE_SUBQUERY)) {
        ABORT_OOM
      }
      
    } query T_CLOSE {
      TRI_aql_node_t* result;
      TRI_aql_node_t* subQuery;
      TRI_aql_node_t* nameNode;
      
      if (! TRI_EndScopeAql(context)) {
        ABORT_OOM
      }

      subQuery = TRI_CreateNodeSubqueryAql(context);
      if (! subQuery) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, subQuery)) {
        ABORT_OOM
      }

      nameNode = TRI_AQL_NODE_MEMBER(subQuery, 0);
      if (! nameNode) {
        ABORT_OOM
      }

      result = TRI_CreateNodeReferenceAql(context, TRI_AQL_NODE_STRING(nameNode));
      if (! result) {
        ABORT_OOM
      }

      // return the result
      $$ = result;
    } 
  | operator_unary {
      $$ = $1;
    }
  | operator_binary {
      $$ = $1;
    }
  | operator_ternary {
      $$ = $1;
    }
  | compound_type {
      $$ = $1;
    }
  | atomic_value {
      $$ = $1;
    }
  | reference {
      $$ = $1;
    }
  ;

function_call:
    T_STRING {
      TRI_aql_node_t* node;
      /* function call */

      if (! TRI_PushStackParseAql(context, $1)) {
        ABORT_OOM
      }

      node = TRI_CreateNodeListAql(context);
      if (! node) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    } T_OPEN optional_function_call_arguments T_CLOSE %prec FUNCCALL {
      TRI_aql_node_t* list = TRI_PopStackParseAql(context);
      TRI_aql_node_t* node = TRI_CreateNodeFcallAql(context, TRI_PopStackParseAql(context), list);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  ;

operator_unary:
    T_PLUS expression %prec UPLUS {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryPlusAql(context, $2);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | T_MINUS expression %prec UMINUS {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryMinusAql(context, $2);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | T_NOT expression %prec T_NOT { 
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryNotAql(context, $2);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  ;

operator_binary:
    expression T_OR expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryOrAql(context, $1, $3);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_AND expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryAndAql(context, $1, $3);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_PLUS expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryPlusAql(context, $1, $3);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_MINUS expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryMinusAql(context, $1, $3);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_TIMES expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryTimesAql(context, $1, $3);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_DIV expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryDivAql(context, $1, $3);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_MOD expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryModAql(context, $1, $3);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_EQ expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryEqAql(context, $1, $3);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_NE expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryNeAql(context, $1, $3);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_LT expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryLtAql(context, $1, $3);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_GT expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryGtAql(context, $1, $3);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    } 
  | expression T_LE expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryLeAql(context, $1, $3);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_GE expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryGeAql(context, $1, $3);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_IN expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryInAql(context, $1, $3);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  ;

operator_ternary:
    expression T_QUESTION expression T_COLON expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorTernaryAql(context, $1, $3, $5);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  ;

optional_function_call_arguments: 
    /* empty */ {
    }
  | function_arguments_list {
    }
  ;

function_arguments_list:
    expression {
      TRI_PushListAql(context, $1);
    }
  | function_arguments_list T_COMMA expression {
      TRI_PushListAql(context, $3);
    }
  ;

compound_type:
    list {
      $$ = $1;
    }
  | array {
      $$ = $1;
    }
  ;

list: 
    T_LIST_OPEN {
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      if (! node) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    } optional_list_elements T_LIST_CLOSE {
      $$ = TRI_PopStackParseAql(context);
    }
  ;

optional_list_elements:
    /* empty */ {
    }
  | list_elements_list {
    }
  ;

list_elements_list:
    expression {
      if (! TRI_PushListAql(context, $1)) {
        ABORT_OOM
      }
    }
  | list_elements_list T_COMMA expression {
      if (! TRI_PushListAql(context, $3)) {
        ABORT_OOM
      }
    }
  ;

array:
    T_DOC_OPEN {
      TRI_aql_node_t* node = TRI_CreateNodeArrayAql(context);
      if (! node) {
        ABORT_OOM
      }

      TRI_PushStackParseAql(context, node);
    } optional_array_elements T_DOC_CLOSE {
      $$ = TRI_PopStackParseAql(context);
    }
  ;

optional_array_elements:
    /* empty */ {
    }
  | array_elements_list {
    }
  ;

array_elements_list:
    array_element {
    }
  | array_elements_list T_COMMA array_element {
    }
  ;

array_element: 
    array_element_name T_COLON expression {
      if (! TRI_PushArrayAql(context, $1, $3)) {
        ABORT_OOM
      }
    }
  ;

reference:
    single_reference {
      // start of reference (collection or variable name)
      $$ = $1;
    }
  | reference {
      // expanded variable access, e.g. variable[*]
      TRI_aql_node_t* node;
      char* varname = TRI_GetNameParseAql(context);

      if (! varname) {
        ABORT_OOM
      }
      
      // push the varname onto the stack
      TRI_PushStackParseAql(context, varname);
      
      // push on the stack what's going to be expanded (will be popped when we come back) 
      TRI_PushStackParseAql(context, $1);

      // create a temporary variable for the row iterator (will be popped by "expansion" rule")
      node = TRI_CreateNodeReferenceAql(context, varname);

      if (! node) {
        ABORT_OOM
      }

      // push the variable
      TRI_PushStackParseAql(context, node);
    } T_EXPAND expansion {
      // return from the "expansion" subrule
      TRI_aql_node_t* expanded = TRI_PopStackParseAql(context);
      TRI_aql_node_t* expand;
      TRI_aql_node_t* nameNode;
      char* varname = TRI_PopStackParseAql(context);

      // push the actual expand node into the statement list
      expand = TRI_CreateNodeExpandAql(context, varname, expanded, $4);
      
      if (! TRI_AppendStatementListAql(context->_statements, expand)) {
        ABORT_OOM
      }

      nameNode = TRI_AQL_NODE_MEMBER(expand, 1);
      if (! nameNode) {
        ABORT_OOM
      }

      // return a reference only
      $$ = TRI_CreateNodeReferenceAql(context, TRI_AQL_NODE_STRING(nameNode));

      if (! $$) {
        ABORT_OOM
      }
    }
  ;

single_reference:
    T_STRING {
      // variable or collection
      TRI_aql_node_t* node;
      
      if (TRI_VariableExistsScopeAql(context, $1)) {
        node = TRI_CreateNodeReferenceAql(context, $1);
      }
      else {
        node = TRI_CreateNodeCollectionAql(context, $1);
      }

      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | function_call {
      $$ = $1;
      
      if (! $$) {
        ABORT_OOM
      }
    }
  | single_reference '.' T_STRING %prec REFERENCE {
      // named variable access, e.g. variable.reference
      $$ = TRI_CreateNodeAttributeAccessAql(context, $1, $3);
      
      if (! $$) {
        ABORT_OOM
      }
    }
  | single_reference T_LIST_OPEN expression T_LIST_CLOSE %prec INDEXED {
      // indexed variable access, e.g. variable[index]
      $$ = TRI_CreateNodeIndexedAql(context, $1, $3);
      
      if (! $$) {
        ABORT_OOM
      }
    }
  ;

expansion:
    '.' T_STRING %prec REFERENCE {
      // named variable access, continuation from * expansion, e.g. [*].variable.reference
      TRI_aql_node_t* node = TRI_PopStackParseAql(context);

      $$ = TRI_CreateNodeAttributeAccessAql(context, node, $2);

      if (! $$) {
        ABORT_OOM
      }
    }
  | T_LIST_OPEN expression T_LIST_CLOSE %prec INDEXED {
      // indexed variable access, continuation from * expansion, e.g. [*].variable[index]
      TRI_aql_node_t* node = TRI_PopStackParseAql(context);

      $$ = TRI_CreateNodeIndexedAql(context, node, $2);

      if (! $$) {
        ABORT_OOM
      }
    }
  | expansion '.' T_STRING %prec REFERENCE {
      // named variable access, continuation from * expansion, e.g. [*].variable.xx.reference
      $$ = TRI_CreateNodeAttributeAccessAql(context, $1, $3);
      if (! $$) {
        ABORT_OOM
      }
    }
  | expansion T_LIST_OPEN expression T_LIST_CLOSE %prec INDEXED {
      // indexed variable access, continuation from * expansion, e.g. [*].variable.xx.[index]
      $$ = TRI_CreateNodeIndexedAql(context, $1, $3);
      if (! $$) {
        ABORT_OOM
      }
    }
  ;

atomic_value:
    value_literal {
      $$ = $1;
    }
  | bind_parameter {
      $$ = $1;
    }
  ;
  
value_literal: 
    T_QUOTED_STRING {
      TRI_aql_node_t* node = TRI_CreateNodeValueStringAql(context, $1);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | integer_value {
    $$ = $1;
    }
  | T_DOUBLE {
      TRI_aql_node_t* node;
      double value;

      if (! $1) {
        ABORT_OOM
      }
      
      value = TRI_DoubleString($1);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, NULL);
        YYABORT;
      }
      
      node = TRI_CreateNodeValueDoubleAql(context, value);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | T_NULL {
      TRI_aql_node_t* node = TRI_CreateNodeValueNullAql(context);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | T_TRUE {
      TRI_aql_node_t* node = TRI_CreateNodeValueBoolAql(context, true);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  | T_FALSE {
      TRI_aql_node_t* node = TRI_CreateNodeValueBoolAql(context, false);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  ;

bind_parameter:
    T_PARAMETER {
      TRI_aql_node_t* node = TRI_CreateNodeParameterAql(context, $1);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  ;

array_element_name:
    T_STRING {
      if (! $1) {
        ABORT_OOM
      }

      $$ = $1;
    }
  | T_QUOTED_STRING {
      if (! $1) {
        ABORT_OOM
      }

      $$ = $1;
    }

variable_name:
    T_STRING {
      $$ = $1;
    }
  ;

integer_value:
    T_INTEGER {
      TRI_aql_node_t* node;
      int64_t value;

      value = TRI_Int64String($1);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, NULL);
        YYABORT;
      }

      node = TRI_CreateNodeValueIntAql(context, value);
      if (! node) {
        ABORT_OOM
      }

      $$ = node;
    }
  ;

