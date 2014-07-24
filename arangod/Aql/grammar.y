
%define api.pure
%name-prefix "Aql"
%locations 
%defines
%parse-param { triagens::aql::Parser* parser }
%lex-param { void* scanner } 
%error-verbose

%{
#include <stdio.h>
#include <stdlib.h>

#include <Basics/Common.h>
#include <BasicsC/conversions.h>
#include <BasicsC/tri-strings.h>

#include "Ahuacatl/ahuacatl-node.h"
#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-context.h"
#include "Aql/Parser.h"
%}

%union {
  struct TRI_aql_node_t* node;
  char*                  strval;
  bool                   boolval;
  int64_t                intval;
}

%{

////////////////////////////////////////////////////////////////////////////////
/// @brief forward for lexer function defined in Aql/tokens.l
////////////////////////////////////////////////////////////////////////////////

int Aqllex (YYSTYPE*, 
            YYLTYPE*, 
            void*);
 
////////////////////////////////////////////////////////////////////////////////
/// @brief register parse error
////////////////////////////////////////////////////////////////////////////////

void Aqlerror (YYLTYPE* locp, 
               triagens::aql::Parser* parser,
               const char* message) {
  parser->registerError(message, locp->first_line, locp->first_column);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut macro for signalling out of memory
////////////////////////////////////////////////////////////////////////////////

#define ABORT_OOM                                                                     \
  /* TRI_SetErrorContextAql(__FILE__, __LINE__, context, TRI_ERROR_OUT_OF_MEMORY, NULL); */ \
  YYABORT;

#define scanner parser->scanner()

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
%token T_WITH "WITH keyword"
%token T_INTO "INTO keyword"

%token T_REMOVE "REMOVE command"
%token T_INSERT "INSERT command"
%token T_UPDATE "UPDATE command"
%token T_REPLACE "REPLACE command"

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
%token T_SCOPE "::"
%token T_RANGE ".."

%token T_COMMA ","
%token T_OPEN "("
%token T_CLOSE ")"
%token T_DOC_OPEN "{"
%token T_DOC_CLOSE "}"
%token T_LIST_OPEN "["
%token T_LIST_CLOSE "]"

%token T_END 0 "end of query string"


/* define operator precedence */
%left T_COMMA T_RANGE
%right T_QUESTION T_COLON
%right T_ASSIGN
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
%left T_SCOPE

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
%type <strval> function_name;
%type <node> optional_function_call_arguments;
%type <node> function_arguments_list;
%type <node> compound_type;
%type <node> list;
%type <node> optional_list_elements;
%type <node> list_elements_list;
%type <node> array;
%type <node> query_options;
%type <node> optional_array_elements;
%type <node> array_elements_list;
%type <node> array_element;
%type <strval> array_element_name;
%type <node> reference;
%type <node> single_reference;
%type <node> expansion;
%type <node> atomic_value;
%type <node> value_literal;
%type <node> collection_name;
%type <node> in_or_into_collection;
%type <node> bind_parameter;
%type <strval> variable_name;
%type <node> numeric_value;
%type <node> integer_value;


/* define start token of language */
%start query

%%

query: 
    optional_statement_block_statements return_statement {
      parser->type(triagens::aql::AQL_QUERY_READ);
    }
  | optional_statement_block_statements remove_statement {
      parser->type(triagens::aql::AQL_QUERY_REMOVE);
    }
  | optional_statement_block_statements insert_statement {
      parser->type(triagens::aql::AQL_QUERY_INSERT);
    }
  | optional_statement_block_statements update_statement {
      parser->type(triagens::aql::AQL_QUERY_UPDATE);
    }
  | optional_statement_block_statements replace_statement {
      parser->type(triagens::aql::AQL_QUERY_REPLACE);
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
      TRI_aql_context_t* context = nullptr;
      parser->scopes()->start(triagens::aql::AQL_SCOPE_FOR);
      
      TRI_aql_node_t* node = TRI_CreateNodeForAql(context, $2, $4);
      if (node == nullptr) {
        ABORT_OOM
      }

      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
  ;

filter_statement:
    T_FILTER expression {
      TRI_aql_context_t* context = nullptr;

      TRI_aql_node_t* node = TRI_CreateNodeFilterAql(context, $2);
      if (node == nullptr) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
  ;

let_statement:
    T_LET let_list {
    }
  ;

let_list: 
    let_element {
    }
  | let_list T_COMMA let_element {
    }
  ;

let_element:
    variable_name T_ASSIGN expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeLetAql(context, $1, $3);

      if (node == nullptr) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
  ;

collect_statement: 
    T_COLLECT {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      
      if (node == nullptr) {
        ABORT_OOM
      }

      parser->pushStack(node);
    } collect_list optional_into {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeCollectAql(context, static_cast<const TRI_aql_node_t* const>(parser->popStack()), $4);
      if (node == nullptr) {
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
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeAssignAql(context, $1, $3);
      if (node == nullptr) {
        ABORT_OOM
      }

      if (! TRI_PushListAql(context, node)) {
        ABORT_OOM
      }
    }
  ;

optional_into: 
    /* empty */ {
      $$ = nullptr;
    }
  | T_INTO variable_name {
      $$ = $2;
    }
  ;

sort_statement:
    T_SORT {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      
      if (node == nullptr) {
        ABORT_OOM
      }

      parser->pushStack(node);
    } sort_list {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* list = static_cast<TRI_aql_node_t*>(parser->popStack());
      TRI_aql_node_t* node = TRI_CreateNodeSortAql(context, list);
      if (node == nullptr) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
  ;

sort_list: 
    sort_element {
      TRI_aql_context_t* context = nullptr;
      if (! TRI_PushListAql(context, $1)) {
        ABORT_OOM
      }
    }
  | sort_list T_COMMA sort_element {
      TRI_aql_context_t* context = nullptr;
      if (! TRI_PushListAql(context, $3)) {
        ABORT_OOM
      }
    }
  ;

sort_element:
    expression sort_direction {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeSortElementAql(context, $1, $2);
      if (node == nullptr) {
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
    T_LIMIT atomic_value {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeLimitAql(context, TRI_CreateNodeValueIntAql(context, 0), $2);
      if (node == nullptr) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
    }
  | T_LIMIT atomic_value T_COMMA atomic_value {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeLimitAql(context, $2, $4);
      if (node == nullptr) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
    }
  ;

return_statement:
    T_RETURN expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeReturnAql(context, $2);
      if (node == nullptr) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
      parser->scopes()->endNested();
    }
  ;

in_or_into_collection:
    T_IN collection_name {
      $$ = $2;
    }
  | T_INTO collection_name {
      $$ = $2;
    }
  ; 

remove_statement:
    T_REMOVE expression in_or_into_collection query_options {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeRemoveAql(context, $2, $3, $4);
      if (node == nullptr) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
      parser->scopes()->endNested();
    }
  ;

insert_statement:
    T_INSERT expression in_or_into_collection query_options {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeInsertAql(context, $2, $3, $4);
      if (node == nullptr) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
      parser->scopes()->endNested();
    }
  ;

update_statement:
    T_UPDATE expression in_or_into_collection query_options {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeUpdateAql(context, nullptr, $2, $3, $4);
      if (node == nullptr) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
      parser->scopes()->endNested();
    }
  | T_UPDATE expression T_WITH expression in_or_into_collection query_options {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeUpdateAql(context, $2, $4, $5, $6);
      if (node == nullptr) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
      parser->scopes()->endNested();
    }
  ;

replace_statement:
    T_REPLACE expression in_or_into_collection query_options {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeReplaceAql(context, nullptr, $2, $3, $4);
      if (node == nullptr) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
      parser->scopes()->endNested();
    }
  | T_REPLACE expression T_WITH expression in_or_into_collection query_options {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeReplaceAql(context, $2, $4, $5, $6);
      if (node == nullptr) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, node)) {
        ABORT_OOM
      }
      
      parser->scopes()->endNested();
    }
  ;

expression:
    T_OPEN expression T_CLOSE {
      $$ = $2;
    }
  | T_OPEN {
      TRI_aql_context_t* context = nullptr;
      parser->scopes()->start(triagens::aql::AQL_SCOPE_SUBQUERY);

      context->_subQueries++;

    } query T_CLOSE {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* result;
      TRI_aql_node_t* subQuery;
      TRI_aql_node_t* nameNode;
      
      context->_subQueries--;

      if (! parser->scopes()->endCurrent()) {
        ABORT_OOM
      }

      subQuery = TRI_CreateNodeSubqueryAql(context);

      if (subQuery == nullptr) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, subQuery)) {
        ABORT_OOM
      }

      nameNode = TRI_AQL_NODE_MEMBER(subQuery, 0);
      if (nameNode == nullptr) {
        ABORT_OOM
      }

      result = TRI_CreateNodeReferenceAql(context, TRI_AQL_NODE_STRING(nameNode));
      if (result == nullptr) {
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
  | expression T_RANGE expression {
      TRI_aql_context_t* context = nullptr;

      if ($1 == nullptr || $3 == nullptr) {
        ABORT_OOM
      }
      
      TRI_aql_node_t* list = TRI_CreateNodeListAql(context);
      if (list == nullptr) {
        ABORT_OOM
      }
       
      if (TRI_ERROR_NO_ERROR != TRI_PushBackVectorPointer(&list->_members, (void*) $1)) {
        ABORT_OOM
      }
      if (TRI_ERROR_NO_ERROR != TRI_PushBackVectorPointer(&list->_members, (void*) $3)) {
        ABORT_OOM
      }
      
      TRI_aql_node_t* node = TRI_CreateNodeFcallAql(context, "RANGE", list);

      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  ;

function_name:
    T_STRING {
      $$ = $1;

      if ($$ == nullptr) {
        ABORT_OOM
      }
    }
  | function_name T_SCOPE T_STRING {
      if ($1 == nullptr || $3 == nullptr) {
        ABORT_OOM
      }

      std::string temp($1);
      temp.append("::");
      temp.append($3);
      $$ = parser->registerString(temp.c_str(), temp.size(), false);

      if ($$ == nullptr) {
        ABORT_OOM
      }
    }
  ;

function_call:
    function_name {
      TRI_aql_context_t* context = nullptr;
      parser->pushStack($1);

      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      if (node == nullptr) {
        ABORT_OOM
      }

      parser->pushStack(node);
    } T_OPEN optional_function_call_arguments T_CLOSE %prec FUNCCALL {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* list = static_cast<TRI_aql_node_t*>(parser->popStack());
      TRI_aql_node_t* node = TRI_CreateNodeFcallAql(context, static_cast<char const* const>(parser->popStack()), list);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  ;

operator_unary:
    T_PLUS expression %prec UPLUS {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryPlusAql(context, $2);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | T_MINUS expression %prec UMINUS {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryMinusAql(context, $2);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | T_NOT expression %prec T_NOT { 
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorUnaryNotAql(context, $2);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  ;

operator_binary:
    expression T_OR expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryOrAql(context, $1, $3);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_AND expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryAndAql(context, $1, $3);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_PLUS expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryPlusAql(context, $1, $3);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_MINUS expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryMinusAql(context, $1, $3);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_TIMES expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryTimesAql(context, $1, $3);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_DIV expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryDivAql(context, $1, $3);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_MOD expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryModAql(context, $1, $3);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_EQ expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryEqAql(context, $1, $3);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_NE expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryNeAql(context, $1, $3);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_LT expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryLtAql(context, $1, $3);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_GT expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryGtAql(context, $1, $3);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    } 
  | expression T_LE expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryLeAql(context, $1, $3);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_GE expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryGeAql(context, $1, $3);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | expression T_IN expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorBinaryInAql(context, $1, $3);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  ;

operator_ternary:
    expression T_QUESTION expression T_COLON expression {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeOperatorTernaryAql(context, $1, $3, $5);

      if (node == nullptr) {
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
      TRI_aql_context_t* context = nullptr;
      if (! TRI_PushListAql(context, $1)) {
        ABORT_OOM
      }
    }
  | function_arguments_list T_COMMA expression {
      TRI_aql_context_t* context = nullptr;
      if (! TRI_PushListAql(context, $3)) {
        ABORT_OOM
      }
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
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeListAql(context);
      if (node == nullptr) {
        ABORT_OOM
      }

      parser->pushStack(node);
    } optional_list_elements T_LIST_CLOSE {
      $$ = static_cast<TRI_aql_node_t*>(parser->popStack());
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
      TRI_aql_context_t* context = nullptr;
      if (! TRI_PushListAql(context, $1)) {
        ABORT_OOM
      }
    }
  | list_elements_list T_COMMA expression {
      TRI_aql_context_t* context = nullptr;
      if (! TRI_PushListAql(context, $3)) {
        ABORT_OOM
      }
    }
  ;

query_options:
    /* empty */ {
      $$ = nullptr;
    }
  | T_STRING array {
      if ($1 == nullptr || $2 == nullptr) {
        ABORT_OOM
      }

      if (! TRI_CaseEqualString($1, "OPTIONS")) {
        parser->registerError(TRI_ERROR_QUERY_PARSE);
        YYABORT;
      }

      $$ = $2;
    }
  ;

array:
    T_DOC_OPEN {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeArrayAql(context);
      if (node == nullptr) {
        ABORT_OOM
      }

      parser->pushStack(node);
    } optional_array_elements T_DOC_CLOSE {
      $$ = static_cast<TRI_aql_node_t*>(parser->popStack());
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
      TRI_aql_context_t* context = nullptr;
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
      TRI_aql_context_t* context = nullptr;
      char* varname = parser->generateName();

      if (varname == nullptr) {
        ABORT_OOM
      }
      
      // push the varname onto the stack
      parser->pushStack(varname);
      
      // push on the stack what's going to be expanded (will be popped when we come back) 
      parser->pushStack($1);

      // create a temporary variable for the row iterator (will be popped by "expansion" rule")
      TRI_aql_node_t* node = TRI_CreateNodeReferenceAql(context, varname);

      if (node == nullptr) {
        ABORT_OOM
      }

      // push the variable
      parser->pushStack(node);
    } T_EXPAND expansion {
      // return from the "expansion" subrule
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* expanded = static_cast<TRI_aql_node_t*>(parser->popStack());
      char* varname = static_cast<char*>(parser->popStack());

      // push the actual expand node into the statement list
      TRI_aql_node_t* expand = TRI_CreateNodeExpandAql(context, varname, expanded, $4);

      if (expand == nullptr) {
        ABORT_OOM
      }
      
      if (! TRI_AppendStatementListAql(context->_statements, expand)) {
        ABORT_OOM
      }

      TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(expand, 1);

      if (nameNode == nullptr) {
        ABORT_OOM
      }

      // return a reference only
      $$ = TRI_CreateNodeReferenceAql(context, TRI_AQL_NODE_STRING(nameNode));

      if ($$ == nullptr) {
        ABORT_OOM
      }
    }
  ;

single_reference:
    T_STRING {
      // variable or collection
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node;
      
      if (parser->scopes()->existsVariable($1)) {
        node = TRI_CreateNodeReferenceAql(context, $1);
      }
      else {
        node = TRI_CreateNodeCollectionAql(context, $1);
      }

      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | function_call {
      $$ = $1;
      
      if ($$ == nullptr) {
        ABORT_OOM
      }
    }
  | single_reference '.' T_STRING %prec REFERENCE {
      // named variable access, e.g. variable.reference
      TRI_aql_context_t* context = nullptr;
      $$ = TRI_CreateNodeAttributeAccessAql(context, $1, $3);
      
      if ($$ == nullptr) {
        ABORT_OOM
      }
    }
  | single_reference '.' bind_parameter %prec REFERENCE {
      // named variable access, e.g. variable.@reference
      TRI_aql_context_t* context = nullptr;
      $$ = TRI_CreateNodeBoundAttributeAccessAql(context, $1, $3);
      
      if ($$ == nullptr) {
        ABORT_OOM
      }
    }
  | single_reference T_LIST_OPEN expression T_LIST_CLOSE %prec INDEXED {
      // indexed variable access, e.g. variable[index]
      TRI_aql_context_t* context = nullptr;
      $$ = TRI_CreateNodeIndexedAql(context, $1, $3);
      
      if ($$ == nullptr) {
        ABORT_OOM
      }
    }
  ;

expansion:
    '.' T_STRING %prec REFERENCE {
      // named variable access, continuation from * expansion, e.g. [*].variable.reference
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = static_cast<TRI_aql_node_t*>(parser->popStack());

      $$ = TRI_CreateNodeAttributeAccessAql(context, node, $2);

      if ($$ == nullptr) {
        ABORT_OOM
      }
    }
  | '.' bind_parameter %prec REFERENCE {
      // named variable access w/ bind parameter, continuation from * expansion, e.g. [*].variable.@reference
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = static_cast<TRI_aql_node_t*>(parser->popStack());

      $$ = TRI_CreateNodeBoundAttributeAccessAql(context, node, $2);

      if ($$ == nullptr) {
        ABORT_OOM
      }
    }
  | T_LIST_OPEN expression T_LIST_CLOSE %prec INDEXED {
      // indexed variable access, continuation from * expansion, e.g. [*].variable[index]
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = static_cast<TRI_aql_node_t*>(parser->popStack());

      $$ = TRI_CreateNodeIndexedAql(context, node, $2);

      if ($$ == nullptr) {
        ABORT_OOM
      }
    }
  | expansion '.' T_STRING %prec REFERENCE {
      // named variable access, continuation from * expansion, e.g. [*].variable.xx.reference
      TRI_aql_context_t* context = nullptr;
      $$ = TRI_CreateNodeAttributeAccessAql(context, $1, $3);

      if ($$ == nullptr) {
        ABORT_OOM
      }
    }
  | expansion '.' bind_parameter %prec REFERENCE {
      // named variable access w/ bind parameter, continuation from * expansion, e.g. [*].variable.xx.@reference
      TRI_aql_context_t* context = nullptr;
      $$ = TRI_CreateNodeBoundAttributeAccessAql(context, $1, $3);

      if ($$ == nullptr) {
        ABORT_OOM
      }
    }
  | expansion T_LIST_OPEN expression T_LIST_CLOSE %prec INDEXED {
      // indexed variable access, continuation from * expansion, e.g. [*].variable.xx.[index]
      TRI_aql_context_t* context = nullptr;
      $$ = TRI_CreateNodeIndexedAql(context, $1, $3);

      if ($$ == nullptr) {
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

numeric_value:
    integer_value {
      $$ = $1;
    }
  | T_DOUBLE {
      TRI_aql_context_t* context = nullptr;
      if ($1 == nullptr) {
        ABORT_OOM
      }
      
      double value = TRI_DoubleString($1);

      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        parser->registerError(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE);
        YYABORT;
      }
      
      TRI_aql_node_t* node = TRI_CreateNodeValueDoubleAql(context, value);

      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    };
  
value_literal: 
    T_QUOTED_STRING {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeValueStringAql(context, $1);

      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | numeric_value {
      $$ = $1;
    }
  | T_NULL {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeValueNullAql(context);

      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | T_TRUE {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeValueBoolAql(context, true);

      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | T_FALSE {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeValueBoolAql(context, false);

      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  ;

collection_name:
    T_STRING {
      TRI_aql_context_t* context = nullptr;

      if ($1 == nullptr) {
        ABORT_OOM
      }

      TRI_aql_node_t* node = TRI_CreateNodeCollectionAql(context, $1);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | T_QUOTED_STRING {
      TRI_aql_context_t* context = nullptr;

      if ($1 == nullptr) {
        ABORT_OOM
      }

      TRI_aql_node_t* node = TRI_CreateNodeCollectionAql(context, $1);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  | T_PARAMETER {
      TRI_aql_context_t* context = nullptr;

      if ($1 == nullptr) {
        ABORT_OOM
      }
      
      if (strlen($1) < 2 || $1[0] != '@') {
        parser->registerError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, $1);
        YYABORT;
      }

      TRI_aql_node_t* node = TRI_CreateNodeParameterAql(context, $1);

      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  ;

bind_parameter:
    T_PARAMETER {
      TRI_aql_context_t* context = nullptr;
      TRI_aql_node_t* node = TRI_CreateNodeParameterAql(context, $1);

      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  ;

array_element_name:
    T_STRING {
      if ($1 == nullptr) {
        ABORT_OOM
      }

      $$ = $1;
    }
  | T_QUOTED_STRING {
      if ($1 == nullptr) {
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
      TRI_aql_context_t* context = nullptr;

      if ($1 == nullptr) {
        ABORT_OOM
      }

      int64_t value = TRI_Int64String($1);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        parser->registerError(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE);
        YYABORT;
      }

      TRI_aql_node_t* node = TRI_CreateNodeValueIntAql(context, value);
      if (node == nullptr) {
        ABORT_OOM
      }

      $$ = node;
    }
  ;

