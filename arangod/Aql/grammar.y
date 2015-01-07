%define lr.type ielr
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
#include <Basics/conversions.h>
#include <Basics/tri-strings.h>

#include "Aql/AstNode.h"
#include "Aql/Parser.h"
%}

%union {
  triagens::aql::AstNode*  node;
  char*                    strval;
  bool                     boolval;
  int64_t                  intval;
}

%{

using namespace triagens::aql;

////////////////////////////////////////////////////////////////////////////////
/// @brief forward for lexer function defined in Aql/tokens.ll
////////////////////////////////////////////////////////////////////////////////

int Aqllex (YYSTYPE*, 
            YYLTYPE*, 
            void*);
 
////////////////////////////////////////////////////////////////////////////////
/// @brief register parse error
////////////////////////////////////////////////////////////////////////////////

void Aqlerror (YYLTYPE* locp, 
               triagens::aql::Parser* parser,
               char const* message) {
  parser->registerParseError(TRI_ERROR_QUERY_PARSE, message, locp->first_line, locp->first_column);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut macro for signalling out of memory
////////////////////////////////////////////////////////////////////////////////

#define ABORT_OOM                                   \
  parser->registerError(TRI_ERROR_OUT_OF_MEMORY);   \
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
%left T_COMMA 
%right T_QUESTION T_COLON
%right T_ASSIGN
%left T_OR 
%left T_AND
%left T_EQ T_NE 
%left T_IN 
%left T_LT T_GT T_LE T_GE
%left T_RANGE
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
%type <node> sort_direction;
%type <node> collect_list;
%type <node> collect_element;
%type <node> collect_variable_list;
%type <node> optional_keep;
%type <strval> optional_into;
%type <strval> count_into;
%type <node> expression;
%type <node> expression_or_query;
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
    }
  | optional_statement_block_statements remove_statement {
    }
  | optional_statement_block_statements insert_statement {
    }
  | optional_statement_block_statements update_statement {
    }
  | optional_statement_block_statements replace_statement {
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
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_FOR);
     
      auto node = parser->ast()->createNodeFor($2, $4);
      parser->ast()->addOperation(node);
    }
  ;

filter_statement:
    T_FILTER expression {
      // operand is a reference. can use it directly
      auto node = parser->ast()->createNodeFilter($2);
      parser->ast()->addOperation(node);
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
      auto node = parser->ast()->createNodeLet($1, $3, true);
      parser->ast()->addOperation(node);
    }
  ;

count_into: 
    T_WITH T_STRING T_INTO variable_name {
      if (! TRI_CaseEqualString($2, "COUNT")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'COUNT'", $2, yylloc.first_line, yylloc.first_column);
      }

      $$ = $4;
    }
  ;

collect_variable_list:
    T_COLLECT {
      auto node = parser->ast()->createNodeList();
      parser->pushStack(node);
    } collect_list { 
      auto list = static_cast<AstNode*>(parser->popStack());

      if (list == nullptr) {
        ABORT_OOM
      }
      $$ = list;
    }
  ;

collect_statement: 
    T_COLLECT count_into {
      if ($2 == nullptr) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "use of 'COUNT' without 'INTO'", yylloc.first_line, yylloc.first_column);
      }

      auto scopes = parser->ast()->scopes();

      // check if we are in the main scope
      bool reRegisterVariables = (scopes->type() != triagens::aql::AQL_SCOPE_MAIN); 

      if (reRegisterVariables) {
        // end the active scopes
        scopes->endNested();
        // start a new scope
        scopes->start(triagens::aql::AQL_SCOPE_COLLECT);
      }

      auto node = parser->ast()->createNodeCollectCount(parser->ast()->createNodeList(), $2);
      parser->ast()->addOperation(node);
    }
  | collect_variable_list count_into {
      auto scopes = parser->ast()->scopes();

      // check if we are in the main scope
      bool reRegisterVariables = (scopes->type() != triagens::aql::AQL_SCOPE_MAIN); 

      if (reRegisterVariables) {
        // end the active scopes
        scopes->endNested();
        // start a new scope
        scopes->start(triagens::aql::AQL_SCOPE_COLLECT);

        size_t const n = $1->numMembers();
        for (size_t i = 0; i < n; ++i) {
          auto member = $1->getMember(i);

          if (member != nullptr) {
            TRI_ASSERT(member->type == NODE_TYPE_ASSIGN);
            auto v = static_cast<Variable*>(member->getMember(0)->getData());
            scopes->addVariable(v);
          }
        }
      }

      auto node = parser->ast()->createNodeCollectCount($1, $2);
      parser->ast()->addOperation(node);
    }
  | collect_variable_list optional_into optional_keep {
      auto scopes = parser->ast()->scopes();

      // check if we are in the main scope
      bool reRegisterVariables = (scopes->type() != triagens::aql::AQL_SCOPE_MAIN); 

      if (reRegisterVariables) {
        // end the active scopes
        scopes->endNested();
        // start a new scope
        scopes->start(triagens::aql::AQL_SCOPE_COLLECT);

        size_t const n = $1->numMembers();
        for (size_t i = 0; i < n; ++i) {
          auto member = $1->getMember(i);

          if (member != nullptr) {
            TRI_ASSERT(member->type == NODE_TYPE_ASSIGN);
            auto v = static_cast<Variable*>(member->getMember(0)->getData());
            scopes->addVariable(v);
          }
        }
      }

      if ($2 == nullptr && $3 != nullptr) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "use of 'KEEP' without 'INTO'", yylloc.first_line, yylloc.first_column);
      } 

      auto node = parser->ast()->createNodeCollect($1, $2, $3);
      parser->ast()->addOperation(node);
    }
  | collect_variable_list T_INTO variable_name T_ASSIGN expression {
      auto scopes = parser->ast()->scopes();

      // check if we are in the main scope
      bool reRegisterVariables = (scopes->type() != triagens::aql::AQL_SCOPE_MAIN); 

      if (reRegisterVariables) {
        // end the active scopes
        scopes->endNested();
        // start a new scope
        scopes->start(triagens::aql::AQL_SCOPE_COLLECT);

        size_t const n = $1->numMembers();
        for (size_t i = 0; i < n; ++i) {
          auto member = $1->getMember(i);

          if (member != nullptr) {
            TRI_ASSERT(member->type == NODE_TYPE_ASSIGN);
            auto v = static_cast<Variable*>(member->getMember(0)->getData());
            scopes->addVariable(v);
          }
        }
      }

      auto node = parser->ast()->createNodeCollectExpression($1, $3, $5);
      parser->ast()->addOperation(node);
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
      auto node = parser->ast()->createNodeAssign($1, $3);
      parser->pushList(node);
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

variable_list: 
    variable_name {
      if (! parser->ast()->scopes()->existsVariable($1)) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "use of unknown variable '%s' for KEEP", $1, yylloc.first_line, yylloc.first_column);
      }
        
      auto node = parser->ast()->createNodeReference($1);
      if (node == nullptr) {
        ABORT_OOM
      }

      // indicate the this node is a reference to the variable name, not the variable value
      node->setFlag(FLAG_KEEP_VARIABLENAME);
      parser->pushList(node);
    }
  | variable_list T_COMMA variable_name {
      if (! parser->ast()->scopes()->existsVariable($3)) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "use of unknown variable '%s' for KEEP", $3, yylloc.first_line, yylloc.first_column);
      }
        
      auto node = parser->ast()->createNodeReference($3);
      if (node == nullptr) {
        ABORT_OOM
      }

      // indicate the this node is a reference to the variable name, not the variable value
      node->setFlag(FLAG_KEEP_VARIABLENAME);
      parser->pushList(node);
    }
  ;

optional_keep: 
    /* empty */ {
      $$ = nullptr;
    }
  | T_STRING {
      if (! TRI_CaseEqualString($1, "KEEP")) {
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'KEEP'", $1, yylloc.first_line, yylloc.first_column);
      }

      auto node = parser->ast()->createNodeList();
      parser->pushStack(node);
    } variable_list {
      auto list = static_cast<AstNode*>(parser->popStack());
      $$ = list;
    }
  ;

sort_statement:
    T_SORT {
      auto node = parser->ast()->createNodeList();
      parser->pushStack(node);
    } sort_list {
      auto list = static_cast<AstNode const*>(parser->popStack());
      auto node = parser->ast()->createNodeSort(list);
      parser->ast()->addOperation(node);
    }
  ;

sort_list: 
    sort_element {
      parser->pushList($1);
    }
  | sort_list T_COMMA sort_element {
      parser->pushList($3);
    }
  ;

sort_element:
    expression sort_direction {
      $$ = parser->ast()->createNodeSortElement($1, $2);
    }
  ;

sort_direction:
    /* empty */ {
      $$ = parser->ast()->createNodeValueBool(true);
    }
  | T_ASC {
      $$ = parser->ast()->createNodeValueBool(true);
    } 
  | T_DESC {
      $$ = parser->ast()->createNodeValueBool(false);
    }
  | atomic_value {
      $$ = $1;
    }
  ;

limit_statement: 
    T_LIMIT atomic_value {
      auto offset = parser->ast()->createNodeValueInt(0);
      auto node = parser->ast()->createNodeLimit(offset, $2);
      parser->ast()->addOperation(node);
    }
  | T_LIMIT atomic_value T_COMMA atomic_value {
      auto node = parser->ast()->createNodeLimit($2, $4);
      parser->ast()->addOperation(node);
    }
  ;

return_statement:
    T_RETURN expression {
      auto node = parser->ast()->createNodeReturn($2);
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
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
      if (! parser->configureWriteQuery(AQL_QUERY_REMOVE, $3, $4)) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeRemove($2, $3, $4);
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
  | T_REMOVE expression in_or_into_collection query_options T_WITH T_STRING T_INTO variable_name T_RETURN variable_name {
      if (! parser->configureWriteQuery(AQL_QUERY_REMOVE, $3, $4, $6, $8, $10)) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeRemove($2, $3, $4, $6, $8, $10);
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
  ;

insert_statement:
    T_INSERT expression in_or_into_collection query_options {
      if (! parser->configureWriteQuery(AQL_QUERY_INSERT, $3, $4)) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeInsert($2, $3, $4);
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
  | T_INSERT expression in_or_into_collection query_options T_WITH T_STRING T_INTO variable_name T_RETURN variable_name {
      if (! parser->configureWriteQuery(AQL_QUERY_INSERT, $3, $4, $6, $8, $10)) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeInsert($2, $3, $4, $6, $8, $10);
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }

  ;

update_statement:
    T_UPDATE expression in_or_into_collection query_options {
      if (! parser->configureWriteQuery(AQL_QUERY_UPDATE, $3, $4)) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeUpdate(nullptr, $2, $3, $4);
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
  | T_UPDATE expression T_WITH expression in_or_into_collection query_options {
      if (! parser->configureWriteQuery(AQL_QUERY_UPDATE, $5, $6)) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeUpdate($2, $4, $5, $6);
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
  | T_UPDATE expression in_or_into_collection query_options T_WITH T_STRING T_INTO variable_name T_RETURN variable_name {
      if (! parser->configureWriteQuery(AQL_QUERY_UPDATE, $3, $4, $6, $8, $10)) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeUpdate(nullptr, $2, $3, $4, $6, $8, $10);
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
  | T_UPDATE expression T_WITH expression in_or_into_collection query_options T_WITH T_STRING T_INTO variable_name T_RETURN variable_name {
      if (! parser->configureWriteQuery(AQL_QUERY_UPDATE, $5, $6, $8, $10, $12)) {
        YYABORT;
      }
      auto node = parser->ast()->createNodeUpdate($2, $4, $5, $6, $8, $10, $12);
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
  ;

replace_statement:
    T_REPLACE expression in_or_into_collection query_options {
      if (! parser->configureWriteQuery(AQL_QUERY_REPLACE, $3, $4)) {
        YYABORT;
      }
      fprintf(stderr, "aoeu1\n");
      auto node = parser->ast()->createNodeReplace(nullptr, $2, $3, $4);
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
  | T_REPLACE expression T_WITH expression in_or_into_collection query_options {
      if (! parser->configureWriteQuery(AQL_QUERY_REPLACE, $5, $6)) {
        YYABORT;
      }
      fprintf(stderr, "aoeu2\n");
      auto node = parser->ast()->createNodeReplace($2, $4, $5, $6);
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
  | T_REPLACE expression in_or_into_collection query_options T_WITH T_STRING T_INTO variable_name T_RETURN variable_name {
      if (! parser->configureWriteQuery(AQL_QUERY_REPLACE, $3, $4)) {
        YYABORT;
      }
      fprintf(stderr, "aoeu3\n");
      auto node = parser->ast()->createNodeReplace(nullptr, $2, $3, $4);
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
  | T_REPLACE expression T_WITH expression in_or_into_collection query_options T_WITH T_STRING T_INTO variable_name T_RETURN variable_name {
      if (! parser->configureWriteQuery(AQL_QUERY_REPLACE, $5, $6, $8, $10, $12)) {
        YYABORT;
      }
      fprintf(stderr, "aoeu4\n");
      auto node = parser->ast()->createNodeReplace($2, $4, $5, $6, $8, $10, $12);
      parser->ast()->addOperation(node);
      parser->ast()->scopes()->endNested();
    }
  ;

expression:
    T_OPEN expression T_CLOSE {
      $$ = $2;
    }
  | T_OPEN {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    } query T_CLOSE {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = parser->ast()->variables()->nextName();
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), node, false);
      parser->ast()->addOperation(subQuery);

      $$ = parser->ast()->createNodeReference(variableName.c_str());
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
      $$ = parser->ast()->createNodeRange($1, $3);
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
      $$ = parser->query()->registerString(temp.c_str(), temp.size(), false);

      if ($$ == nullptr) {
        ABORT_OOM
      }
    }
  ;

function_call:
    function_name {
      parser->pushStack($1);

      auto node = parser->ast()->createNodeList();
      parser->pushStack(node);
    } T_OPEN optional_function_call_arguments T_CLOSE %prec FUNCCALL {
      auto list = static_cast<AstNode const*>(parser->popStack());
      $$ = parser->ast()->createNodeFunctionCall(static_cast<char const*>(parser->popStack()), list);
    }
  ;

operator_unary:
    T_PLUS expression %prec UPLUS {
      $$ = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_PLUS, $2);
    }
  | T_MINUS expression %prec UMINUS {
      $$ = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_MINUS, $2);
    }
  | T_NOT expression %prec T_NOT { 
      $$ = parser->ast()->createNodeUnaryOperator(NODE_TYPE_OPERATOR_UNARY_NOT, $2);
    }
  ;

operator_binary:
    expression T_OR expression {
      $$ = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_OR, $1, $3);
    }
  | expression T_AND expression {
      $$ = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, $1, $3);
    }
  | expression T_PLUS expression {
      $$ = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_PLUS, $1, $3);
    }
  | expression T_MINUS expression {
      $$ = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MINUS, $1, $3);
    }
  | expression T_TIMES expression {
      $$ = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_TIMES, $1, $3);
    }
  | expression T_DIV expression {
      $$ = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_DIV, $1, $3);
    }
  | expression T_MOD expression {
      $$ = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_MOD, $1, $3);
    }
  | expression T_EQ expression {
      $$ = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, $1, $3);
    }
  | expression T_NE expression {
      $$ = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NE, $1, $3);
    }
  | expression T_LT expression {
      $$ = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LT, $1, $3);
    }
  | expression T_GT expression {
      $$ = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GT, $1, $3);
    } 
  | expression T_LE expression {
      $$ = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_LE, $1, $3);
    }
  | expression T_GE expression {
      $$ = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_GE, $1, $3);
    }
  | expression T_IN expression {
      $$ = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_IN, $1, $3);
    }
  | expression T_NOT T_IN expression {
      $$ = parser->ast()->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_NIN, $1, $4);
    }
  ;

operator_ternary:
    expression T_QUESTION expression T_COLON expression {
      $$ = parser->ast()->createNodeTernaryOperator($1, $3, $5);
    }
  ;

optional_function_call_arguments: 
    /* empty */ {
    }
  | function_arguments_list {
    }
  ;

expression_or_query:
    expression {
      $$ = $1;
    }
  | {
      parser->ast()->scopes()->start(triagens::aql::AQL_SCOPE_SUBQUERY);
      parser->ast()->startSubQuery();
    } query {
      AstNode* node = parser->ast()->endSubQuery();
      parser->ast()->scopes()->endCurrent();

      std::string const variableName = parser->ast()->variables()->nextName();
      auto subQuery = parser->ast()->createNodeLet(variableName.c_str(), node, false);
      parser->ast()->addOperation(subQuery);

      $$ = parser->ast()->createNodeReference(variableName.c_str());
    }
  ;

function_arguments_list:
    expression_or_query {
      parser->pushList($1);
    }
  | function_arguments_list T_COMMA expression_or_query {
      parser->pushList($3);
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
      auto node = parser->ast()->createNodeList();
      parser->pushStack(node);
    } optional_list_elements T_LIST_CLOSE {
      $$ = static_cast<AstNode*>(parser->popStack());
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
      parser->pushList($1);
    }
  | list_elements_list T_COMMA expression {
      parser->pushList($3);
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
        parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected qualifier '%s', expecting 'OPTIONS'", $1, yylloc.first_line, yylloc.first_column);
      }

      $$ = $2;
    }
  ;

array:
    T_DOC_OPEN {
      auto node = parser->ast()->createNodeArray();
      parser->pushStack(node);
    } optional_array_elements T_DOC_CLOSE {
      $$ = static_cast<AstNode*>(parser->popStack());
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
      parser->pushArray($1, $3);
    }
  ;

reference:
    single_reference {
      // start of reference (collection or variable name)
      $$ = $1;
    }
  | reference {
      // expanded variable access, e.g. variable[*]

      // create a temporary iterator variable
      std::string const nextName = parser->ast()->variables()->nextName() + "_";
      char const* iteratorName = nextName.c_str();
      auto iterator = parser->ast()->createNodeIterator(iteratorName, $1);

      parser->pushStack(iterator);
      parser->pushStack(parser->ast()->createNodeReference(iteratorName));
    } T_EXPAND expansion {
      // return from the "expansion" subrule

      // push the expand node into the statement list
      auto iterator = static_cast<AstNode*>(parser->popStack());
      $$ = parser->ast()->createNodeExpand(iterator, $4);

      if ($$ == nullptr) {
        ABORT_OOM
      }
    }
  ;

single_reference:
    T_STRING {
      // variable or collection
      AstNode* node;
      
      if (parser->ast()->scopes()->existsVariable($1)) {
        node = parser->ast()->createNodeReference($1);
      }
      else {
        node = parser->ast()->createNodeCollection($1);
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
      $$ = parser->ast()->createNodeAttributeAccess($1, $3);
    }
  | single_reference '.' bind_parameter %prec REFERENCE {
      // named variable access, e.g. variable.@reference
      $$ = parser->ast()->createNodeBoundAttributeAccess($1, $3);
    }
  | single_reference T_LIST_OPEN expression T_LIST_CLOSE %prec INDEXED {
      // indexed variable access, e.g. variable[index]
      $$ = parser->ast()->createNodeIndexedAccess($1, $3);
    }
  ;

expansion:
    '.' T_STRING %prec REFERENCE {
      // named variable access, continuation from * expansion, e.g. [*].variable.reference
      auto node = static_cast<AstNode*>(parser->popStack());
      $$ = parser->ast()->createNodeAttributeAccess(node, $2);
    }
  | '.' bind_parameter %prec REFERENCE {
      // named variable access w/ bind parameter, continuation from * expansion, e.g. [*].variable.@reference
      auto node = static_cast<AstNode*>(parser->popStack());
      $$ = parser->ast()->createNodeBoundAttributeAccess(node, $2);
    }
  | T_LIST_OPEN expression T_LIST_CLOSE %prec INDEXED {
      // indexed variable access, continuation from * expansion, e.g. [*].variable[index]
      auto node = static_cast<AstNode*>(parser->popStack());
      $$ = parser->ast()->createNodeIndexedAccess(node, $2);
    }
  | expansion '.' T_STRING %prec REFERENCE {
      // named variable access, continuation from * expansion, e.g. [*].variable.xx.reference
      $$ = parser->ast()->createNodeAttributeAccess($1, $3);
    }
  | expansion '.' bind_parameter %prec REFERENCE {
      // named variable access w/ bind parameter, continuation from * expansion, e.g. [*].variable.xx.@reference
      $$ = parser->ast()->createNodeBoundAttributeAccess($1, $3);
    }
  | expansion T_LIST_OPEN expression T_LIST_CLOSE %prec INDEXED {
      // indexed variable access, continuation from * expansion, e.g. [*].variable.xx.[index]
      $$ = parser->ast()->createNodeIndexedAccess($1, $3);
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
      if ($1 == nullptr) {
        ABORT_OOM
      }
      
      double value = TRI_DoubleString($1);

      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        parser->registerWarning(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, TRI_errno_string(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE), yylloc.first_line, yylloc.first_column);
        $$ = parser->ast()->createNodeValueNull();
      }
      else {
        $$ = parser->ast()->createNodeValueDouble(value); 
      }
    };
  
value_literal: 
    T_QUOTED_STRING {
      $$ = parser->ast()->createNodeValueString($1); 
    }
  | numeric_value {
      $$ = $1;
    }
  | T_NULL {
      $$ = parser->ast()->createNodeValueNull();
    }
  | T_TRUE {
      $$ = parser->ast()->createNodeValueBool(true);
    }
  | T_FALSE {
      $$ = parser->ast()->createNodeValueBool(false);
    }
  ;

collection_name:
    T_STRING {
      if ($1 == nullptr) {
        ABORT_OOM
      }

      $$ = parser->ast()->createNodeCollection($1);
    }
  | T_QUOTED_STRING {
      if ($1 == nullptr) {
        ABORT_OOM
      }

      $$ = parser->ast()->createNodeCollection($1);
    }
  | T_PARAMETER {
      if ($1 == nullptr) {
        ABORT_OOM
      }
      
      if (strlen($1) < 2 || $1[0] != '@') {
        parser->registerParseError(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE, TRI_errno_string(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE), $1, yylloc.first_line, yylloc.first_column);
      }

      $$ = parser->ast()->createNodeParameter($1);
    }
  ;

bind_parameter:
    T_PARAMETER {
      $$ = parser->ast()->createNodeParameter($1);
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
      if ($1 == nullptr) {
        ABORT_OOM
      }

      int64_t value = TRI_Int64String($1);
      if (TRI_errno() == TRI_ERROR_NO_ERROR) {
        $$ = parser->ast()->createNodeValueInt(value);
      }
      else {
        double value2 = TRI_DoubleString($1);
        if (TRI_errno() == TRI_ERROR_NO_ERROR) {
          $$ = parser->ast()->createNodeValueDouble(value2);
        }
        else {
          parser->registerWarning(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, TRI_errno_string(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE), yylloc.first_line, yylloc.first_column);
          $$ = parser->ast()->createNodeValueNull();
        }
      }
    }
  ;

