
%define api.pure
%name-prefix="Ahuacatl"
%locations 
%defines
%parse-param { TRI_aql_parse_context_t* const context }
%lex-param { void* scanner } 
%error-verbose

%{

#include <stdio.h>
#include <stdlib.h>

#include <BasicsC/common.h>
#include <BasicsC/conversions.h>
#include <BasicsC/strings.h>

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-parser.h"
#include "Ahuacatl/ahuacatl-error.h"

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

void Ahuacatlerror (YYLTYPE* locp, TRI_aql_parse_context_t* const context, const char* err) {
  TRI_SetParseErrorAql(context, err, locp->first_line, locp->first_column);
}

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
%token T_NUMBER "number" 
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
%left FUNCCALL
%left REFERENCE
%left INDEXED

/* define token return types */
%type <strval> T_STRING
%type <strval> T_QUOTED_STRING
%type <strval> T_NUMBER
%type <strval> T_PARAMETER; 

%type <node> query;
%type <node> for_statement;
%type <node> filter_statement;
%type <node> let_statement;
%type <node> collect_statement;
%type <node> sort_statement;
%type <node> sort_list;
%type <node> sort_element;
%type <boolval> sort_direction;
%type <node> limit_statement;
%type <node> return_statement;
%type <node> collect_list;
%type <node> collect_element;
%type <strval> optional_into;
%type <node> expression;
%type <node> operator_unary;
%type <node> operator_binary;
%type <node> operator_ternary;
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
%type <node> expansion;
%type <node> atomic_value;
%type <node> value_literal;
%type <node> bind_parameter;
%type <strval> variable_name;
%type <intval> signed_number;


/* define start token of language */
%start query

%%

query: 
    {
      // a query or a sub-query always starts a new scope
      if (!TRI_StartScopeParseContextAql(context)) {
        YYABORT;
      }
    } optional_statement_block_statements return_statement {
      // end the scope
      TRI_AddStatementAql(context, $3);
      
      $$ = (TRI_aql_node_t*) TRI_GetFirstStatementAql(context);

      TRI_EndScopeParseContextAql(context);
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
      TRI_AddStatementAql(context, $1);
    }
  | let_statement {
      TRI_AddStatementAql(context, $1);
    }
  | filter_statement {
      TRI_AddStatementAql(context, $1);
    }
  | collect_statement {
      TRI_AddStatementAql(context, $1);
    }
  | sort_statement {
      TRI_AddStatementAql(context, $1);
    }
  | limit_statement {
      TRI_AddStatementAql(context, $1);
    }
    
for_statement:
    T_FOR variable_name T_IN expression {
      TRI_aql_node_t* node = TRI_CreateNodeForAql(context, $2, $4);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  ;

filter_statement:
    T_FILTER expression {
      TRI_aql_node_t* node = TRI_CreateNodeFilterAql(context, $2);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  ;

let_statement:
    T_LET variable_name {
      if (!TRI_PushStackAql(context, $2) || !TRI_StartScopeParseContextAql(context)) {
        YYABORT;
      }
    } T_ASSIGN T_OPEN expression T_CLOSE {
      TRI_aql_node_t* node;

      TRI_EndScopeParseContextAql(context);

      node = TRI_CreateNodeAssignAql(context, TRI_PopStackAql(context), $6);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  ;

collect_statement: 
    T_COLLECT {
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      
      if (!node) {
        YYABORT;
      }

      TRI_PushStackAql(context, node);
    } collect_list optional_into {
      TRI_aql_node_t* node = TRI_CreateNodeCollectAql(context, TRI_PopStackAql(context), $4);
      if (!node) {
        YYABORT;
      }

      $$ = node;
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
      if (!node) {
        YYABORT;
      }

      if (!TRI_PushListAql(context, node)) {
        YYABORT;
      }
    }
  | expression {
      if (!TRI_PushListAql(context, $1)) {
        YYABORT;
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
      
      if (!node) {
        YYABORT;
      }

      TRI_PushStackAql(context, node);
    } sort_list {
      TRI_aql_node_t* list = TRI_PopStackAql(context);
      TRI_aql_node_t* node = TRI_CreateNodeSortAql(context, list);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  ;

sort_list: 
    sort_element {
      if (!TRI_PushListAql(context, $1)) {
        YYABORT;
      }
    }
  | sort_list T_COMMA sort_element {
      if (!TRI_PushListAql(context, $3)) {
        YYABORT;
      }
    }
  ;

sort_element:
    expression sort_direction {
      TRI_aql_node_t* node = TRI_CreateNodeSortElementAql(context, $1, $2);
      if (!node) {
        YYABORT;
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
    T_LIMIT signed_number {
      TRI_aql_node_t* node = TRI_CreateNodeLimitAql(context, TRI_CreateNodeValueIntAql(context, 0), TRI_CreateNodeValueIntAql(context, $2));
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | T_LIMIT signed_number T_COMMA signed_number {
      TRI_aql_node_t* node = TRI_CreateNodeLimitAql(context, TRI_CreateNodeValueIntAql(context, $2), TRI_CreateNodeValueIntAql(context, $4));
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  ;

return_statement:
    T_RETURN expression {
      TRI_aql_node_t* node = TRI_CreateNodeReturnAql(context, $2);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  ;


expression:
    T_OPEN expression T_CLOSE {
      $$ = $2;
    }
  | T_OPEN query T_CLOSE {
      TRI_aql_node_t* node = TRI_CreateNodeSubqueryAql(context, $2);
      if (!node) {
        YYABORT;
      }

      $$ = node;
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
  | T_STRING {
      TRI_aql_node_t* node;

      if (!TRI_PushStackAql(context, $1)) {
        YYABORT;
      }

      node = TRI_CreateNodeListAql(context);
      if (!node) {
        YYABORT;
      }

      TRI_PushStackAql(context, node);
    } T_OPEN optional_function_call_arguments T_CLOSE %prec FUNCCALL {
      TRI_aql_node_t* list = TRI_PopStackAql(context);
      TRI_aql_node_t* node = TRI_CreateNodeFcallAql(context, TRI_PopStackAql(context), list);
      if (!node) {
        YYABORT;
      }

      $$ = node;
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

operator_unary:
    T_PLUS expression %prec UPLUS {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryPlusAql(context, $2);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | T_MINUS expression %prec UMINUS {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryMinusAql(context, $2);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | T_NOT expression %prec T_NOT { 
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryNotAql(context, $2);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  ;

operator_binary:
    expression T_OR expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryOrAql(context, $1, $3);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | expression T_AND expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryAndAql(context, $1, $3);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | expression T_PLUS expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryPlusAql(context, $1, $3);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | expression T_MINUS expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryMinusAql(context, $1, $3);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | expression T_TIMES expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryTimesAql(context, $1, $3);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | expression T_DIV expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryDivAql(context, $1, $3);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | expression T_MOD expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryModAql(context, $1, $3);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | expression T_EQ expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryEqAql(context, $1, $3);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | expression T_NE expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryNeAql(context, $1, $3);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | expression T_LT expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryLtAql(context, $1, $3);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | expression T_GT expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryGtAql(context, $1, $3);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    } 
  | expression T_LE expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryLeAql(context, $1, $3);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | expression T_GE expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryGeAql(context, $1, $3);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | expression T_IN expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryInAql(context, $1, $3);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  ;

operator_ternary:
    expression T_QUESTION expression T_COLON expression {
      TRI_aql_node_t* node = TRI_CreateNodeOperatorTernaryAql(context, $1, $3, $5);
      if (!node) {
        YYABORT;
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
      if (!node) {
        YYABORT;
      }

      TRI_PushStackAql(context, node);
    } optional_list_elements T_LIST_CLOSE {
      $$ = TRI_PopStackAql(context);
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
      if (!TRI_PushListAql(context, $1)) {
        YYABORT;
      }
    }
  | list_elements_list T_COMMA expression {
      if (!TRI_PushListAql(context, $3)) {
        YYABORT;
      }
    }
  ;

array:
    T_DOC_OPEN {
      TRI_aql_node_t* node = TRI_CreateNodeArrayAql(context);
      if (!node) {
        YYABORT;
      }

      TRI_PushStackAql(context, node);
    } optional_array_elements T_DOC_CLOSE {
      $$ = TRI_PopStackAql(context);
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
      if (!TRI_PushArrayAql(context, $1, $3)) {
        YYABORT;
      }
    }
  ;

reference:
    T_STRING {
      // variable or collection
      TRI_aql_node_t* node;
     
      if (TRI_VariableExistsAql(context, $1)) {
        node = TRI_CreateNodeReferenceAql(context, $1);
      }
      else {
        node = TRI_CreateNodeCollectionAql(context, $1);
      }

      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | reference T_LIST_OPEN expression T_LIST_CLOSE %prec INDEXED {
      // variable[]
      $$ = TRI_CreateNodeIndexedAql(context, $1, $3);
      if (!$$) {
        YYABORT;
      }
    }
  | reference T_LIST_OPEN T_TIMES T_LIST_CLOSE '.' expansion %prec INDEXED {
      // variable[*]
      $$ = TRI_CreateNodeExpandAql(context, $1, $6);
      if (!$$) {
        YYABORT;
      }
    }
  | reference '.' T_STRING %prec REFERENCE {
      // variable.reference
      $$ = TRI_CreateNodeAttributeAccessAql(context, $1, $3);
      if (!$$) {
        YYABORT;
      }
    }
  ;

expansion:
    T_STRING {
      // reference
      $$ = TRI_CreateNodeAttributeAql(context, $1);
      if (!$$) {
        YYABORT;
      }
    }
  | expansion T_LIST_OPEN expression T_LIST_CLOSE %prec INDEXED {
      // variable[]
      $$ = TRI_CreateNodeIndexedAql(context, $1, $3);
      if (!$$) {
        YYABORT;
      }
    }
  | expansion '.' T_STRING %prec REFERENCE {
      // variable.variable
      $$ = TRI_CreateNodeAttributeAccessAql(context, $1, $3);
      if (!$$) {
        YYABORT;
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
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | T_NUMBER {
      TRI_aql_node_t* node;

      if (!$1) {
        YYABORT;
      }
      
      node = TRI_CreateNodeValueDoubleAql(context, TRI_DoubleString($1));
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | T_NULL {
      TRI_aql_node_t* node = TRI_CreateNodeValueNullAql(context);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | T_TRUE {
      TRI_aql_node_t* node = TRI_CreateNodeValueBoolAql(context, true);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  | T_FALSE {
      TRI_aql_node_t* node = TRI_CreateNodeValueBoolAql(context, false);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  ;

bind_parameter:
    T_PARAMETER {
      TRI_aql_node_t* node = TRI_CreateNodeParameterAql(context, $1);
      if (!node) {
        YYABORT;
      }

      $$ = node;
    }
  ;

array_element_name:
    T_STRING {
      if (!$1) {
        YYABORT;
      }

      $$ = $1;
    }
  | T_QUOTED_STRING {
      if (!$1) {
        YYABORT;
      }

      $$ = $1;
    }

variable_name:
    T_STRING {
      $$ = $1;
    }
  ;

signed_number: 
    T_NUMBER {
      if (!$1) {
        YYABORT;
      }

      $$ = TRI_Int64String($1);
    }
  | '-' T_NUMBER {
      if (!$2) {
        YYABORT;
      }

      $$ = - TRI_Int64String($2);
    }
  ;

