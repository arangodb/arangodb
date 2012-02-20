
%pure-parser
%name-prefix="QL"
%locations
%defines
%parse-param { QL_parser_context_t *context }
%lex-param { void *scanner }
%error-verbose

%{

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <BasicsC/common.h>
#include <BasicsC/conversions.h>
#include <BasicsC/strings.h>

#include "QL/ast-node.h"
#include "QL/parser-context.h"
#include "QL/formatter.h"
#include "QL/error.h"

#define ABORT_IF_OOM(ptr) \
  if (!ptr) { \
    QLParseRegisterParseError(context, ERR_OOM); \
    YYABORT; \
  }

%}

%union {
  QL_ast_node_t *node;
  int intval;
  double floatval;
  char *strval;
}


%{
int QLlex (YYSTYPE *,YYLTYPE *, void *);

void QLerror (YYLTYPE *locp,QL_parser_context_t *context, const char *err) {
  context->_lexState._errorState._code      = ERR_PARSE; 
  context->_lexState._errorState._message   = QLParseAllocString(context, (char *) err);
  context->_lexState._errorState._line      = locp->first_line;
  context->_lexState._errorState._column    = locp->first_column;
}

#define scanner context->_scanner
%}

%type <strval> STRING
%type <strval> REAL
%type <strval> IDENTIFIER; 
%type <strval> QUOTED_IDENTIFIER; 
%type <strval> PARAMETER; 
%type <strval> PARAMETER_NAMED; 

%type <node> select_clause;
%type <node> attribute_list;
%type <node> attribute;
%type <node> named_attribute;
%type <node> array_declaration;
%type <node> array_list;
%type <node> from_clause;
%type <node> from_list;
%type <node> join_type;
%type <node> list_join;
%type <node> inner_join;
%type <node> outer_join;
%type <node> collection_reference;
%type <node> collection_name;
%type <node> collection_alias;
%type <node> where_clause;
%type <node> order_clause;
%type <node> order_list;
%type <node> order_element;
%type <node> order_direction;
%type <node> expression;
%type <node> unary_operator;
%type <node> binary_operator;
%type <node> conditional_operator;
%type <node> function_call;
%type <node> function_invocation;
%type <node> function_args_list;
%type <node> document;
%type <node> atom;
%type <node> object_access;

        
%token SELECT 
%token FROM 
%token WHERE 
%token JOIN LIST INNER OUTER LEFT RIGHT ON
%token ORDER BY ASC DESC
%token LIMIT
%token AND OR NOT IN 
%token ASSIGNMENT GREATER LESS GREATER_EQUAL LESS_EQUAL EQUAL UNEQUAL IDENTICAL UNIDENTICAL
%token NULLX TRUE FALSE UNDEFINED
%token IDENTIFIER QUOTED_IDENTIFIER PARAMETER PARAMETER_NAMED STRING REAL  

%left ASSIGNMENT
%right TERNARY COLON
%left OR 
%left AND
%left IDENTICAL UNIDENTICAL EQUAL UNEQUAL
%left IN
%left LESS GREATER LESS_EQUAL GREATER_EQUAL
%left '+' '-'
%left '*' '/' '%'
%right NOT
%left FCALL
%nonassoc UMINUS UPLUS
%left MEMBER

%start query


%%


query:
    select_query {
    }
  | select_query ';' {
    }
  | empty_query {
    }
  | empty_query ';' {
    }
  ;

empty_query:
    { 
      context->_query->_type        = QLQueryTypeEmpty;
    }
  ;

select_query:
    SELECT select_clause from_clause where_clause order_clause limit_clause {
      // full blown SELECT query
      context->_query->_type         = QLQueryTypeSelect;
      context->_query->_select._base = $2;
      context->_query->_from._base   = $3;
      context->_query->_where._base  = $4;
      context->_query->_order._base  = $5;
    }
  ;



select_clause:
    document {
      // select part of a SELECT
      $$ = $1;
      ABORT_IF_OOM($$);
    }
  ;	

from_clause:
    FROM {
      // from part of a SELECT
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    } from_list {
      $$ = QLParseContextPop(context);
      ABORT_IF_OOM($$);
    }
  ;	      						

from_list:
    collection_reference {
      // single table query
      ABORT_IF_OOM($1);
      QLParseContextAddElement(context, $1);
    } 
  | from_list join_type collection_reference ON expression {
      // multi-table query
      ABORT_IF_OOM($2);
      ABORT_IF_OOM($3);
      ABORT_IF_OOM($5);
      $$ = $2;
      $$->_lhs  = $3;
      $$->_rhs  = $5;
      
      QLParseContextAddElement(context, $2);
    }
  ;

where_clause:
    /* empty */ {
      $$ = 0;
    }
  | WHERE expression {
      // where condition set
      ABORT_IF_OOM($2);
      $$ = $2;
    }
  ;

order_clause:
    /* empty */ {
      $$ = 0;
    }
  | ORDER BY {
      // order by part of a query
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    } order_list {
      $$ = QLParseContextPop(context);
      ABORT_IF_OOM($$);
    }
  ;

order_list:
    order_element {
      ABORT_IF_OOM($1);
      QLParseContextAddElement(context, $1);
    }
  | order_list ',' order_element {
      ABORT_IF_OOM($3);
      QLParseContextAddElement(context, $3);
    }
  ;

order_element:
    expression order_direction {
      $$ = QLAstNodeCreate(context, QLNodeContainerOrderElement);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($2);
      $$->_lhs = $1;
      $$->_rhs = $2;
    }
  ;

order_direction:
    /* empty */ {
      $$ = QLAstNodeCreate(context, QLNodeValueOrderDirection);
      ABORT_IF_OOM($$);
      $$->_value._boolValue = true;
    }
  | ASC {
      $$ = QLAstNodeCreate(context, QLNodeValueOrderDirection);
      ABORT_IF_OOM($$);
      $$->_value._boolValue = true;
    }
  | DESC {
      $$ = QLAstNodeCreate(context, QLNodeValueOrderDirection);
      ABORT_IF_OOM($$);
      $$->_value._boolValue = false;
    }
  ;

limit_clause:
    /* empty */ {
    }
  | LIMIT REAL {
      // limit value
      int64_t d = TRI_Int64String($2);

      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterParseError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, $2);
        YYABORT;
      }
      
      context->_query->_limit._isUsed = true;
      context->_query->_limit._offset = 0; 
      context->_query->_limit._count  = d; 
    }
  | LIMIT '-' REAL {
      // limit - value
      int64_t d = TRI_Int64String($3);

      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterParseError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, $3);
        YYABORT;
      }
      
      context->_query->_limit._isUsed = true;
      context->_query->_limit._offset = 0; 
      context->_query->_limit._count  = -d; 
    }
  | LIMIT REAL ',' REAL { 
      // limit value, value
      int64_t d1, d2;
      
      d1 = TRI_Int64String($2);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterParseError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, $2);
        YYABORT;
      }

      d2 = TRI_Int64String($4);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterParseError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, $4);
        YYABORT;
      }

      context->_query->_limit._isUsed = true;
      context->_query->_limit._offset = d1; 
      context->_query->_limit._count  = d2;
    }
  | LIMIT REAL ',' '-' REAL { 
      // limit value, -value
      int64_t d1, d2;
      
      d1 = TRI_Int64String($2);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterParseError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, $2);
        YYABORT;
      }

      d2 = TRI_Int64String($5);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterParseError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, $5);
        YYABORT;
      }

      context->_query->_limit._isUsed = true;
      context->_query->_limit._offset = d1; 
      context->_query->_limit._count  = -d2;
    }
  ;

document:
    collection_alias {
      // document is a reference to a collection (by using its alias)
      ABORT_IF_OOM($1);
      $$ = $1; 
    }
  | '{' '}' {
      // empty document
      $$ = QLAstNodeCreate(context, QLNodeValueDocument);
      ABORT_IF_OOM($$);
    }
  | '{' {
      // listing of document attributes
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    } attribute_list '}' {
      $$ = QLAstNodeCreate(context, QLNodeValueDocument);
      ABORT_IF_OOM($$);
      QLPopIntoRhs($$, context);
    }
  ;

attribute_list:
    attribute {
      ABORT_IF_OOM($1);
      QLParseContextAddElement(context, $1);
    }
  | attribute_list ',' attribute {
      ABORT_IF_OOM($3);
      QLParseContextAddElement(context, $3);
    }
  ;

attribute:
    named_attribute {
      ABORT_IF_OOM($1);
      $$ = $1;
    }
  ;

named_attribute:
    IDENTIFIER COLON expression {
      QL_ast_node_t* identifier = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM(identifier);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      identifier->_value._stringValue = $1;

      $$ = QLAstNodeCreate(context, QLNodeValueNamedValue);
      ABORT_IF_OOM($$);
      $$->_lhs = identifier;
      $$->_rhs = $3;
    }
  | STRING COLON expression {
      size_t outLength;
      QL_ast_node_t* str = QLAstNodeCreate(context, QLNodeValueString);
      ABORT_IF_OOM(str);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      str->_value._stringValue = QLParseRegisterString(context, TRI_UnescapeUtf8String($1 + 1, strlen($1) - 2, &outLength)); 

      $$ = QLAstNodeCreate(context, QLNodeValueNamedValue);
      ABORT_IF_OOM($$);
      $$->_lhs = str;
      $$->_rhs = $3;
    }
  ;

collection_reference:
    collection_name collection_alias {
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($1->_value._stringValue);
      ABORT_IF_OOM($2);
      ABORT_IF_OOM($2->_value._stringValue);

      if (!QLParseValidateCollectionName($1->_value._stringValue)) {
        // validate collection name
        QLParseRegisterParseError(context, ERR_COLLECTION_NAME_INVALID, $1->_value._stringValue);
        YYABORT;
      }

      if (!QLParseValidateCollectionAlias($2->_value._stringValue)) {
        // validate alias
        QLParseRegisterParseError(context, ERR_COLLECTION_ALIAS_INVALID, $2->_value._stringValue);
        YYABORT;
      }

      if (!QLAstQueryAddCollection(context->_query, $1->_value._stringValue, $2->_value._stringValue)) {
        QLParseRegisterParseError(context, ERR_COLLECTION_ALIAS_REDECLARED, $2->_value._stringValue);
        YYABORT;
      }

      $$ = QLAstNodeCreate(context, QLNodeReferenceCollection);
      ABORT_IF_OOM($$);
      $$->_lhs = $1;
      $$->_rhs = $2;
    }
  ;


collection_name:
    IDENTIFIER {
      $$ = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_value._stringValue = $1;
    }
  | QUOTED_IDENTIFIER {
      size_t outLength;
      $$ = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_value._stringValue = QLParseRegisterString(context, TRI_UnescapeUtf8String($1 + 1, strlen($1) - 2, &outLength)); 
    }
  ;

collection_alias:
    IDENTIFIER {
      $$ = QLAstNodeCreate(context, QLNodeReferenceCollectionAlias);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_value._stringValue = $1;
    }
  | QUOTED_IDENTIFIER {
      size_t outLength;
      $$ = QLAstNodeCreate(context, QLNodeReferenceCollectionAlias);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_value._stringValue = QLParseRegisterString(context, TRI_UnescapeUtf8String($1 + 1, strlen($1) - 2, &outLength)); 
    }
  ;	

join_type:
    list_join {
      ABORT_IF_OOM($1);
      $$ = $1;
    }
  | inner_join {
      ABORT_IF_OOM($1);
      $$ = $1;
    }
  | outer_join {
      ABORT_IF_OOM($1);
      $$ = $1;
    }
  ;

list_join: 
    LIST JOIN {
      $$ = QLAstNodeCreate(context, QLNodeJoinList);
      ABORT_IF_OOM($$);
    }
  ;

inner_join:
    JOIN {
      $$ = QLAstNodeCreate(context, QLNodeJoinInner);
      ABORT_IF_OOM($$);
    }
  | INNER JOIN {
      $$ = QLAstNodeCreate(context, QLNodeJoinInner);
      ABORT_IF_OOM($$);
    }
  ;

outer_join:
    LEFT OUTER JOIN {
      $$ = QLAstNodeCreate(context, QLNodeJoinLeft);
      ABORT_IF_OOM($$);
    }
  | LEFT JOIN {
      $$ = QLAstNodeCreate(context, QLNodeJoinLeft);
      ABORT_IF_OOM($$);
    } 
  | RIGHT OUTER JOIN {
      $$ = QLAstNodeCreate(context, QLNodeJoinRight);
      ABORT_IF_OOM($$);
    }
  | RIGHT JOIN {
      $$ = QLAstNodeCreate(context, QLNodeJoinRight);
      ABORT_IF_OOM($$);
    }
  ;

expression:
    '(' expression ')' {
      ABORT_IF_OOM($2);
      $$ = $2;
    }
  | unary_operator {
      ABORT_IF_OOM($1);
      $$ = $1;
    }
  | binary_operator {
      ABORT_IF_OOM($1);
      $$ = $1;
    }
  | conditional_operator {
      ABORT_IF_OOM($1);
      $$ = $1;
    }
  | function_call {
      ABORT_IF_OOM($1);
      $$ = $1;
    }
  | document { 
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    } object_access %prec MEMBER {
      $$ = QLAstNodeCreate(context, QLNodeContainerMemberAccess);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_lhs = $1;
      QLPopIntoRhs($$, context);
    }
  | document {
      ABORT_IF_OOM($1);
      $$ = $1;
    }
  | array_declaration {
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    } object_access %prec MEMBER {
      $$ = QLAstNodeCreate(context, QLNodeContainerMemberAccess);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_lhs = $1;
      QLPopIntoRhs($$, context);
    }
  | array_declaration {
      ABORT_IF_OOM($1);
      $$ = $1;  
    }
  | atom {
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    } object_access %prec MEMBER {
      $$ = QLAstNodeCreate(context, QLNodeContainerMemberAccess);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_lhs = $1;
      QLPopIntoRhs($$, context);
    }
  | atom {
      ABORT_IF_OOM($1);
      $$ = $1;
    }
  ;
      

object_access:
    '.' IDENTIFIER {
      QL_ast_node_t* name = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM($2);
      name->_value._stringValue = $2;
      QLParseContextAddElement(context, name);
    }
  | '.' function_call {
      ABORT_IF_OOM($2);
      QLParseContextAddElement(context, $2);
    }
  | object_access '.' IDENTIFIER {
      QL_ast_node_t* name = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM($3);
      name->_value._stringValue = $3;
      QLParseContextAddElement(context, name);
    }
  | object_access '.' function_call {
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      QLParseContextAddElement(context, $3);
    }
  ; 

unary_operator:
    '+' expression %prec UPLUS {
      $$ = QLAstNodeCreate(context, QLNodeUnaryOperatorPlus);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($2);
      $$->_lhs = $2;
    }
  | '-' expression %prec UMINUS {
      $$ = QLAstNodeCreate(context, QLNodeUnaryOperatorMinus);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($2);
      $$->_lhs = $2;
    }
  | NOT expression { 
      $$ = QLAstNodeCreate(context, QLNodeUnaryOperatorNot);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($2);
      $$->_lhs = $2;
    }
  ;

binary_operator:
    expression OR expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorOr);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression AND expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorAnd);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3; 
    }
  | expression '+' expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorAdd);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression '-' expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorSubtract);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression '*' expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorMultiply);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression '/' expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorDivide);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression '%' expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorModulus);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression IDENTICAL expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorIdentical);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression UNIDENTICAL expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorUnidentical);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression EQUAL expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorEqual);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression UNEQUAL expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorUnequal);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression LESS expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorLess);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression GREATER expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorGreater);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    } 
  | expression LESS_EQUAL expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorLessEqual);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression GREATER_EQUAL expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorGreaterEqual);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression IN expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorIn);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  ;

conditional_operator:
  expression TERNARY expression COLON expression {
      QL_ast_node_t* node = QLAstNodeCreate(context, QLNodeContainerTernarySwitch);
      ABORT_IF_OOM(node);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      ABORT_IF_OOM($5);
      node->_lhs = $3;
      node->_rhs = $5;

      $$ = QLAstNodeCreate(context, QLNodeControlTernary);
      ABORT_IF_OOM($$);
      $$->_lhs = $1;
      $$->_rhs = node;
    }
  ;

function_call: 
    function_invocation {
      ABORT_IF_OOM($1);
      $$ = $1;
    }
  ;

function_invocation:
    IDENTIFIER '(' ')' %prec FCALL {
      QL_ast_node_t* name = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM($1);
      name->_value._stringValue = $1;
      
      $$ = QLAstNodeCreate(context, QLNodeControlFunctionCall);
      ABORT_IF_OOM($$);
      $$->_lhs = name;
      $$->_rhs = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM($$->_rhs);
    }
  | IDENTIFIER '(' {
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    } function_args_list ')' {
      QL_ast_node_t* name = QLAstNodeCreate(context, QLNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM($1);
      name->_value._stringValue = $1;

      $$ = QLAstNodeCreate(context, QLNodeControlFunctionCall);
      ABORT_IF_OOM($$);
      $$->_lhs = name;
      QLPopIntoRhs($$, context);
    }
  ;

function_args_list:
    expression {
      QLParseContextAddElement(context, $1);
    }
  | function_args_list ',' expression {
      ABORT_IF_OOM($3);
      QLParseContextAddElement(context, $3);
    }
  ;

array_declaration:
    '[' ']' {
      $$ = QLAstNodeCreate(context, QLNodeValueArray);
      ABORT_IF_OOM($$);
    }
  | '[' {
      QL_ast_node_t* list = QLAstNodeCreate(context, QLNodeContainerList);
      ABORT_IF_OOM(list);
      QLParseContextPush(context, list); 
    } array_list ']' { 
      $$ = QLAstNodeCreate(context, QLNodeValueArray);
      ABORT_IF_OOM($$);
      QLPopIntoRhs($$, context);
    } 
  ;

array_list:
    expression {
      QLParseContextAddElement(context, $1);
    }
  | array_list ',' expression {
      ABORT_IF_OOM($3);
      QLParseContextAddElement(context, $3);
    }
  ;

atom:
    STRING {
      size_t outLength;
      $$ = QLAstNodeCreate(context, QLNodeValueString);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_value._stringValue = QLParseRegisterString(context, TRI_UnescapeUtf8String($1 + 1, strlen($1) - 2, &outLength)); 
    }
  | REAL {
      double d = TRI_DoubleString($1);
      if (TRI_errno() != TRI_ERROR_NO_ERROR && d != 0.0) {
        QLParseRegisterParseError(context, ERR_NUMBER_OUT_OF_RANGE, $1);
        YYABORT;
      }
      $$ = QLAstNodeCreate(context, QLNodeValueNumberDoubleString);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_value._stringValue = $1; 
    }
  | NULLX {
      $$ = QLAstNodeCreate(context, QLNodeValueNull);
      ABORT_IF_OOM($$);
    }
  | UNDEFINED {
      $$ = QLAstNodeCreate(context, QLNodeValueUndefined); 
      ABORT_IF_OOM($$);
    }
  | TRUE { 
      $$ = QLAstNodeCreate(context, QLNodeValueBool);
      ABORT_IF_OOM($$);
      $$->_value._boolValue = true;
    }
  | FALSE {
      $$ = QLAstNodeCreate(context, QLNodeValueBool);
      ABORT_IF_OOM($$);
      $$->_value._boolValue = false;
    }
  | PARAMETER {
      // numbered parameter
      int64_t d = TRI_Int64String($1);

      if (TRI_errno() != TRI_ERROR_NO_ERROR || d < 0 || d >= 256) {
        QLParseRegisterParseError(context, ERR_PARAMETER_NUMBER_OUT_OF_RANGE, $1);
        YYABORT;
      }

      $$ = QLAstNodeCreate(context, QLNodeValueParameterNumeric);
      ABORT_IF_OOM($$);
      $$->_value._intValue = d;
    }
  | PARAMETER_NAMED {
      // named parameter
      $$ = QLAstNodeCreate(context, QLNodeValueParameterNamed);
      ABORT_IF_OOM($$);
      $$->_value._stringValue = $1;
    }
 ;

%%

