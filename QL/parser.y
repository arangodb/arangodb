
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

#include "ast-node.h"
#include "parser-context.h"
#include "formatter.h"
#include "error.h"

%}

%union {
  QL_ast_node_t *node;
  int intval;
  double floatval;
  char *strval;
}


%{
int QLlex (YYSTYPE *,YYLTYPE *, void *);
int QLget_lineno (void *);
int QLget_column (void *);

void QLerror (YYLTYPE *locp,QL_parser_context_t *context, const char *err) {
  context->_lexState._errorState._code      = ERR_PARSE; 
  context->_lexState._errorState._message   = QLParseAllocString(context, (char *) err);
  context->_lexState._errorState._line      = locp->first_line;
  context->_lexState._errorState._column    = locp->first_column;
}

void QLParseRegisterError (QL_parser_context_t *context, const QL_error_type_e errorCode, ...) {
  va_list args;

  context->_lexState._errorState._line      = QLget_lineno(context->_scanner);
  context->_lexState._errorState._column    = QLget_column(context->_scanner);
  context->_lexState._errorState._code      = errorCode; 
  va_start(args, errorCode);
  context->_lexState._errorState._message   = QLParseAllocString(context, QLErrorFormat(errorCode, args)); 
  va_end(args);
}

#define scanner context->_scanner
%}

%type <strval> STRING
%type <strval> REAL
%type <strval> INTEGER
%type <strval> IDENTIFIER; 
%type <strval> PARAMETER; 
%type <strval> PARAMETER_NAMED; 

%type <node> select_clause;
%type <node> attribute_list;
%type <node> attribute;
%type <node> named_attribute;
%type <node> array_declaration;
%type <node> array_list;
%type <node> qualified_identifier;
%type <node> qualified_identifier_list;
%type <node> args_list;
%type <node> from_clause;
%type <node> from_list;
%type <node> join_type;
%type <node> list_join;
%type <node> inner_join;
%type <node> outer_join;
%type <node> collection_name;
%type <node> collection_alias;
%type <node> collection_reference;
%type <node> where_clause;
%type <node> order_clause;
%type <node> order_list;
%type <node> order_element;
%type <node> order_direction;
%type <node> condition;
%type <node> predicate;
%type <node> comparison_predicate;
%type <node> in_predicate;
%type <node> scalar_expression;
%type <node> value_expression;
%type <node> function_call;
%type <node> document;
%type <node> atom;

        
%token SELECT 
%token FROM 
%token WHERE 
%token JOIN LIST INNER OUTER LEFT RIGHT ON
%token ORDER BY ASC DESC
%token LIMIT
%token AND OR NOT IN 
%token ASSIGNMENT GREATER LESS GREATER_EQUAL LESS_EQUAL EQUAL UNEQUAL IDENTICAL UNIDENTICAL
%token NULLX TRUE FALSE UNDEFINED
%token IDENTIFIER PARAMETER PARAMETER_NAMED STRING REAL INTEGER 

%left ASSIGNMENT
%left OR IN
%left AND
%right NOT
%nonassoc IDENTICAL UNIDENTICAL EQUAL UNEQUAL LESS GREATER LESS_EQUAL GREATER_EQUAL
%left '+' '-'
%left '*' '/' '%'
%nonassoc UMINUS
%nonassoc UPLUS

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
    }
  ;	

from_clause:
    FROM from_list {
      // from part of a SELECT
      $$ = $2;
    }
  ;	      						

from_list:
    collection_reference {
      // single table query
      QL_ast_node_t *lhs = $1->_lhs;
      QL_ast_node_t *rhs = $1->_rhs;

      if (lhs != 0 && rhs != 0) {
        if (strlen(lhs->_value._stringValue) > QL_QUERY_NAME_LEN || strlen(rhs->_value._stringValue) > QL_QUERY_NAME_LEN) {
          QLParseRegisterError(context, ERR_COLLECTION_NAME_INVALID, lhs->_value._stringValue);
          YYABORT;
        }
        if (!QLAstQueryAddCollection(context->_query, lhs->_value._stringValue, rhs->_value._stringValue, true)) {
          QLParseRegisterError(context, ERR_COLLECTION_NAME_REDECLARED, rhs->_value._stringValue);
          YYABORT;
        }
      }

      $$ = $1;
    } 
  | from_list join_type collection_reference ON condition {
      // multi-table query
      QL_ast_node_t *lhs = $3->_lhs;
      QL_ast_node_t *rhs = $3->_rhs;

      $$ = $2;
      if ($$ != 0) {
        $$->_lhs  = $1;
        $$->_rhs  = $3;
        $$->_next = $5;
      }
      
      if (lhs != 0 && rhs != 0) {
        if (strlen(lhs->_value._stringValue) > QL_QUERY_NAME_LEN || strlen(rhs->_value._stringValue) > QL_QUERY_NAME_LEN) {
          QLParseRegisterError(context, ERR_COLLECTION_NAME_INVALID, lhs->_value._stringValue);
          YYABORT;
        }
        if (!QLAstQueryAddCollection(context->_query, lhs->_value._stringValue, rhs->_value._stringValue, false)) {
          QLParseRegisterError(context, ERR_COLLECTION_NAME_REDECLARED, rhs->_value._stringValue);
          YYABORT;
        }
      }
    }
  ;

where_clause:
    /* empty */ {
      $$ = 0;
    }
  | WHERE condition {
      // where condition set
      $$ = $2;
    }
  ;

order_clause:
    /* empty */ {
      $$ = 0;
    }
  | ORDER BY {
      // order by part of a query
      QL_ast_node_t *list = QLAstNodeCreate(context,QLNodeContainerList);
      QLParseContextPush(context, list); 
    } order_list {
      $$ = QLParseContextPop(context);
    }
  ;

order_list:
    order_element {
      QLParseContextAddElement(context, $1);
    }
  | order_list ',' order_element {
      QLParseContextAddElement(context, $3);
    }
  ;

order_element:
    scalar_expression order_direction {
      $$ = QLAstNodeCreate(context,QLNodeContainerOrderElement);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $2;
      }
    }
  ;

order_direction:
    /* empty */ {
      $$ = QLAstNodeCreate(context,QLNodeValueOrderDirection);
      if ($$ != 0) {
        $$->_value._boolValue = true;
      }
    }
  | ASC {
      $$ = QLAstNodeCreate(context,QLNodeValueOrderDirection);
      if ($$ != 0) {
        $$->_value._boolValue = true;
      }
    }
  | DESC {
      $$ = QLAstNodeCreate(context,QLNodeValueOrderDirection);
      if ($$ != 0) {
        $$->_value._boolValue = false;
      }
    }
  ;

limit_clause:
    /* empty */ {
    }
  | LIMIT INTEGER {
      // limit value
      int64_t d = TRI_Int64String($2);

      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, $2);
        YYABORT;
      }
      
      context->_query->_limit._isUsed = true;
      context->_query->_limit._offset = 0; 
      context->_query->_limit._count  = d; 
    }
  | LIMIT '-' INTEGER %prec INTEGER {
      // limit - value
      int64_t d = TRI_Int64String($3);

      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, $3);
        YYABORT;
      }
      
      context->_query->_limit._isUsed = true;
      context->_query->_limit._offset = 0; 
      context->_query->_limit._count  = -d; 
    }
  | LIMIT INTEGER ',' INTEGER { 
      // limit value, value
      int64_t d1, d2;
      
      d1 = TRI_Int64String($2);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, $2);
        YYABORT;
      }

      d2 = TRI_Int64String($4);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, $4);
        YYABORT;
      }

      context->_query->_limit._isUsed = true;
      context->_query->_limit._offset = d1; 
      context->_query->_limit._count  = d2;
    }
  | LIMIT INTEGER ',' '-' INTEGER %prec UMINUS { 
      // limit value, -value
      int64_t d1, d2;
      
      d1 = TRI_Int64String($2);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, $2);
        YYABORT;
      }

      d2 = TRI_Int64String($5);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        QLParseRegisterError(context, ERR_LIMIT_VALUE_OUT_OF_RANGE, $5);
        YYABORT;
      }

      context->_query->_limit._isUsed = true;
      context->_query->_limit._offset = d1; 
      context->_query->_limit._count  = -d2;
    }
  ;

document:
    collection_name {
      // document is a reference to a collection (by using its alias)
      $$ = $1; 
    }
  | '{' '}' {
      // empty document
      $$ = QLAstNodeCreate(context,QLNodeValueDocument);
    }
  | '{' {
      // listing of document attributes
      QL_ast_node_t *list = QLAstNodeCreate(context,QLNodeContainerList);
      QLParseContextPush(context, list); 
    } attribute_list '}' {
      $$ = QLAstNodeCreate(context,QLNodeValueDocument);
      if ($$ != 0) {
        $$->_rhs = QLParseContextPop(context);
      }
    }
  ;

attribute_list:
    attribute {
      QLParseContextAddElement(context, $1);
    }
  | attribute_list ',' attribute {
      QLParseContextAddElement(context, $3);
    }
  ;

attribute:
    named_attribute {
      $$ = $1;
    }
  ;

named_attribute:
    IDENTIFIER ':' value_expression {
      QL_ast_node_t *identifier = QLAstNodeCreate(context,QLNodeValueIdentifier);
      if (identifier != 0) {
        identifier->_value._stringValue = $1;
      }

      $$ = QLAstNodeCreate(context,QLNodeValueNamedValue);
      if ($$ != 0) {
        $$->_lhs = identifier;
        $$->_rhs = $3;
      }
    }
  | STRING ':' value_expression {
      size_t outLength;
      QL_ast_node_t *str = QLAstNodeCreate(context,QLNodeValueString);
      if (str) {
        str->_value._stringValue = QLParseRegisterString(context, TRI_UnescapeUtf8String($1 + 1, strlen($1) - 2, &outLength)); 
      }

      $$ = QLAstNodeCreate(context,QLNodeValueNamedValue);
      if ($$ != 0) {
        $$->_lhs = str;
        $$->_rhs = $3;
      }
    }
  ;

value_expression:
    scalar_expression {
      $$ = $1;
    }
  | array_declaration {
      $$ = $1;
    }
  | document {
      $$ = $1;
    }
  ;

collection_reference:
    collection_name collection_alias {
      $$ = QLAstNodeCreate(context,QLNodeReferenceCollection);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $2;
      }
    }
  ;

collection_name:
    IDENTIFIER {
      $$ = QLAstNodeCreate(context,QLNodeValueIdentifier);
      if ($$ != 0) {
        $$->_value._stringValue = $1;
      }
    }
  ;

collection_alias:
    IDENTIFIER {
      $$ = QLAstNodeCreate(context,QLNodeValueIdentifier);
      if ($$ != 0) {
        $$->_value._stringValue = $1;
      }
    }
  ;	

join_type:
    list_join {
      $$ = $1;
    }
  | inner_join {
      $$ = $1;
    }
  | outer_join {
      $$ = $1;
    }
  ;

list_join: 
    LIST JOIN {
      $$ = QLAstNodeCreate(context,QLNodeJoinList);
    }
  ;

inner_join:
    JOIN {
      $$ = QLAstNodeCreate(context,QLNodeJoinInner);
    }
  | INNER JOIN {
      $$ = QLAstNodeCreate(context,QLNodeJoinInner);
    }
  ;

outer_join:
    LEFT OUTER JOIN {
      $$ = QLAstNodeCreate(context,QLNodeJoinLeft);
    }
  | LEFT JOIN {
      $$ = QLAstNodeCreate(context,QLNodeJoinLeft);
    }
  | RIGHT JOIN {
      $$ = QLAstNodeCreate(context,QLNodeJoinRight);
    }
  | RIGHT OUTER JOIN {
      $$ = QLAstNodeCreate(context,QLNodeJoinRight);
    }
  ;

condition:
    condition OR condition {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorOr);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | condition AND condition {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorAnd);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3; 
      }
    }
  | NOT condition { 
      $$ = QLAstNodeCreate(context,QLNodeUnaryOperatorNot);
      if ($$ != 0) {
        $$->_lhs = $2;
      }
    }
  | '(' condition ')' {
      $$ = $2;
    }
  | predicate {
      $$ = $1;
    }
  ;

predicate:
    comparison_predicate {
      $$ = $1;
    }
  | in_predicate {
      $$ = $1;
    }
  | scalar_expression {
      $$ = $1;
    }
  ;

comparison_predicate:
    scalar_expression IDENTICAL scalar_expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorIdentical);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | scalar_expression UNIDENTICAL scalar_expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorUnidentical);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | scalar_expression EQUAL scalar_expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorEqual);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | scalar_expression UNEQUAL scalar_expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorUnequal);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | scalar_expression LESS scalar_expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorLess);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | scalar_expression GREATER scalar_expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorGreater);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    } 
  | scalar_expression LESS_EQUAL scalar_expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorLessEqual);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | scalar_expression GREATER_EQUAL scalar_expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorGreaterEqual);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  ;

in_predicate: 
    scalar_expression IN value_expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorIn);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  ;

scalar_expression:
    scalar_expression '+' scalar_expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorAdd);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | scalar_expression '-' scalar_expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorSubtract);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | scalar_expression '*' scalar_expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorMultiply);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | scalar_expression '/' scalar_expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorDivide);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | scalar_expression '%' scalar_expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorModulus);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | '+' scalar_expression %prec UPLUS {
      $$ = QLAstNodeCreate(context,QLNodeUnaryOperatorPlus);
      if ($$ != 0) {
        $$->_lhs = $2;
      }
    }
  | '-' scalar_expression %prec UMINUS {
      $$ = QLAstNodeCreate(context,QLNodeUnaryOperatorMinus);
      if ($$ != 0) {
        $$->_lhs = $2;
      }
    }
  | '(' scalar_expression ')' {
      $$ = $2;
    }
  | function_call {
      $$ = $1;
    } 
  | atom {
      $$ = $1;
    }
  ;

function_call:
    IDENTIFIER '(' ')' {
      QL_ast_node_t *name = QLAstNodeCreate(context,QLNodeValueIdentifier);
      if (name != 0) {
        name->_value._stringValue = $1;
      }
      
      $$ = QLAstNodeCreate(context,QLNodeControlFunctionCall);
      if ($$ != 0) {
        $$->_lhs = name;
        $$->_rhs = QLAstNodeCreate(context,QLNodeContainerList);
      }
    }
  | IDENTIFIER '(' {
      QL_ast_node_t *list = QLAstNodeCreate(context,QLNodeContainerList);
      QLParseContextPush(context, list); 
    } args_list ')' {
      QL_ast_node_t *name = QLAstNodeCreate(context,QLNodeValueIdentifier);
      if (name != 0) {
        name->_value._stringValue = $1;
      }

      $$ = QLAstNodeCreate(context,QLNodeControlFunctionCall);
      if ($$ != 0) {
        $$->_lhs = name;
        $$->_rhs = QLParseContextPop(context);
      }
    }
  ;

args_list:
    value_expression {
      QLParseContextAddElement(context, $1);
    }
  | args_list ',' value_expression {
      QLParseContextAddElement(context, $3);
    }
  ;

array_declaration:
    '[' ']' {
      $$ = QLAstNodeCreate(context,QLNodeValueArray);
    }
  | '[' {
      QL_ast_node_t *list = QLAstNodeCreate(context,QLNodeContainerList);
      QLParseContextPush(context, list); 
    } array_list ']' { 
      $$ = QLAstNodeCreate(context,QLNodeValueArray);
      if ($$ != 0) {
        $$->_rhs = QLParseContextPop(context);
      }
    } 
  ;

array_list:
    value_expression {
      QLParseContextAddElement(context, $1);
    }
  | array_list ',' value_expression {
      QLParseContextAddElement(context, $3);
    }
  ;

qualified_identifier:
    IDENTIFIER '.' {
      QL_ast_node_t *list = QLAstNodeCreate(context,QLNodeContainerList);
      QLParseContextPush(context, list); 
    } qualified_identifier_list {
      QL_ast_node_t *lhs = QLAstNodeCreate(context,QLNodeValueIdentifier);
      if (lhs != 0) {
        lhs->_value._stringValue = $1;
      }

      $$ = QLAstNodeCreate(context,QLNodeReferenceAttribute);
      if ($$ != 0) {
        $$->_lhs = lhs;
        $$->_rhs = QLParseContextPop(context);
      }
    }
  ;

qualified_identifier_list:
    IDENTIFIER {
      QL_ast_node_t *node = QLAstNodeCreate(context,QLNodeValueIdentifier);
      if (node != 0) {
        node->_value._stringValue = $1;
        QLParseContextAddElement(context, node);
      }
    }
  | qualified_identifier_list '.' IDENTIFIER {
      QL_ast_node_t *node = QLAstNodeCreate(context,QLNodeValueIdentifier);
      if (node != 0) {
        node->_value._stringValue = $3;
        QLParseContextAddElement(context, node);
      }
    }
  ;

atom:
    STRING {
      size_t outLength;
      $$ = QLAstNodeCreate(context,QLNodeValueString);
      if ($$ != 0) {
        $$->_value._stringValue = QLParseRegisterString(context, TRI_UnescapeUtf8String($1 + 1, strlen($1) - 2, &outLength)); 
      }
    }
  | REAL {
      double d = TRI_DoubleString($1);
      if (TRI_errno() != TRI_ERROR_NO_ERROR && d != 0.0) {
        QLParseRegisterError(context, ERR_NUMBER_OUT_OF_RANGE, $1);
        YYABORT;
      }
      $$ = QLAstNodeCreate(context,QLNodeValueNumberDoubleString);
      if ($$ != 0) {
        $$->_value._stringValue = $1; 
      }
    }
  | INTEGER {
      // we'll be converting integers into doubles anyway
      double d = TRI_DoubleString($1);
      if (TRI_errno() != TRI_ERROR_NO_ERROR && d != 0.0) {
        QLParseRegisterError(context, ERR_NUMBER_OUT_OF_RANGE, $1);
        YYABORT;
      }
      $$ = QLAstNodeCreate(context,QLNodeValueNumberDoubleString);
      if ($$ != 0) {
        $$->_value._stringValue = $1; 
      }
    }
  | NULLX {
      $$ = QLAstNodeCreate(context,QLNodeValueNull);
    }
  | UNDEFINED {
      $$ = QLAstNodeCreate(context,QLNodeValueUndefined); 
    }
  | TRUE { 
      $$ = QLAstNodeCreate(context,QLNodeValueBool);
      if ($$ != 0) {
        $$->_value._boolValue = true;
      }
    }
  | FALSE {
      $$ = QLAstNodeCreate(context,QLNodeValueBool);
      if ($$ != 0) {
        $$->_value._boolValue = false;
      }
    }
  | qualified_identifier {
      $$ = $1;
    }
  | PARAMETER {
      // numbered parameter
      int64_t d = TRI_Int64String($1);

      if (TRI_errno() != TRI_ERROR_NO_ERROR || d < 0 || d >= 256) {
        QLParseRegisterError(context, ERR_PARAMETER_NUMBER_OUT_OF_RANGE, $1);
        YYABORT;
      }

      $$ = QLAstNodeCreate(context,QLNodeValueParameterNumeric);
      if ($$ != 0) {
        $$->_value._intValue = d;
      }
    }
  | PARAMETER_NAMED {
      // named parameter
      $$ = QLAstNodeCreate(context,QLNodeValueParameterNamed);
      if ($$ != 0) {
        $$->_value._stringValue = $1;
      }
    }
 ;

%%

