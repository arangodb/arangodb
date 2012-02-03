
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
%type <node> collection_alias;
%type <node> collection_reference;
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
%token IDENTIFIER PARAMETER PARAMETER_NAMED STRING REAL  

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
%left MEMBER
%nonassoc UMINUS UPLUS

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
          QLParseRegisterParseError(context, ERR_COLLECTION_NAME_INVALID, lhs->_value._stringValue);
          YYABORT;
        }
        if (!QLAstQueryAddCollection(context->_query, lhs->_value._stringValue, rhs->_value._stringValue, true)) {
          QLParseRegisterParseError(context, ERR_COLLECTION_NAME_REDECLARED, rhs->_value._stringValue);
          YYABORT;
        }
      }

      $$ = $1;
    } 
  | from_list join_type collection_reference ON expression {
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
          QLParseRegisterParseError(context, ERR_COLLECTION_NAME_INVALID, lhs->_value._stringValue);
          YYABORT;
        }
        if (!QLAstQueryAddCollection(context->_query, lhs->_value._stringValue, rhs->_value._stringValue, false)) {
          QLParseRegisterParseError(context, ERR_COLLECTION_NAME_REDECLARED, rhs->_value._stringValue);
          YYABORT;
        }
      }
    }
  ;

where_clause:
    /* empty */ {
      $$ = 0;
    }
  | WHERE expression {
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
    expression order_direction {
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
      QLPopIntoRhs($$, context);
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
    IDENTIFIER COLON expression {
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
  | STRING COLON expression {
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

collection_reference:
    IDENTIFIER collection_alias {
      QL_ast_node_t *name;

      $$ = QLAstNodeCreate(context,QLNodeReferenceCollection);
      if ($$ != 0) {
        name = QLAstNodeCreate(context,QLNodeValueIdentifier);
        if (name != 0) {
          name->_value._stringValue = $1;
        }
        $$->_lhs = name;
        $$->_rhs = $2;
      }
    }
  ;

collection_alias:
    IDENTIFIER {
      $$ = QLAstNodeCreate(context,QLNodeReferenceCollectionAlias);
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

expression:
    '(' expression ')' {
      $$ = $2;
    }
  | unary_operator {
      $$ = $1;
    }
  | binary_operator {
      $$ = $1;
    }
  | conditional_operator {
      $$ = $1;
    }
  | function_call {
      $$ = $1;
    }
  | document { 
      QL_ast_node_t *list = QLAstNodeCreate(context,QLNodeContainerList);
      QLParseContextPush(context, list); 
    } object_access %prec MEMBER {
      $$ = QLAstNodeCreate(context, QLNodeContainerMemberAccess);
      if ($$ != 0) {
        $$->_lhs = $1;
        QLPopIntoRhs($$, context);
      }
    }
  | document {
      $$ = $1;
    }
  | array_declaration {
      QL_ast_node_t *list = QLAstNodeCreate(context,QLNodeContainerList);
      QLParseContextPush(context, list); 
    } object_access %prec MEMBER {
      $$ = QLAstNodeCreate(context, QLNodeContainerMemberAccess);
      if ($$ != 0) {
        $$->_lhs = $1;
        QLPopIntoRhs($$, context);
      }
    }
  | array_declaration {
      $$ = $1;  
    }
  | atom {
      QL_ast_node_t *list = QLAstNodeCreate(context,QLNodeContainerList);
      QLParseContextPush(context, list); 
    } object_access %prec MEMBER {
      $$ = QLAstNodeCreate(context, QLNodeContainerMemberAccess);
      if ($$ != 0) {
        $$->_lhs = $1;
        QLPopIntoRhs($$, context);
      }
    }
  | atom {
      $$ = $1;
    }
  ;
      

object_access:
    '.' IDENTIFIER {
      QL_ast_node_t *name = QLAstNodeCreate(context,QLNodeValueIdentifier);
      if (name != 0) {
        name->_value._stringValue = $2;
        QLParseContextAddElement(context, name);
      }
    }
  | '.' function_call {
      QLParseContextAddElement(context, $2);
    }
  | object_access '.' IDENTIFIER {
      QL_ast_node_t *name = QLAstNodeCreate(context,QLNodeValueIdentifier);
      if (name != 0) {
        name->_value._stringValue = $3;
        QLParseContextAddElement(context, name);
      }
    }
  | object_access '.' function_call {
      QLParseContextAddElement(context, $3);
    }
  ; 

unary_operator:
    '+' expression %prec UPLUS {
      $$ = QLAstNodeCreate(context,QLNodeUnaryOperatorPlus);
      if ($$ != 0) {
        $$->_lhs = $2;
      }
    }
  | '-' expression %prec UMINUS {
      $$ = QLAstNodeCreate(context,QLNodeUnaryOperatorMinus);
      if ($$ != 0) {
        $$->_lhs = $2;
      }
    }
  | NOT expression { 
      $$ = QLAstNodeCreate(context,QLNodeUnaryOperatorNot);
      if ($$ != 0) {
        $$->_lhs = $2;
      }
    }
  ;

binary_operator:
    expression OR expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorOr);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | expression AND expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorAnd);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3; 
      }
    }
  | expression '+' expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorAdd);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | expression '-' expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorSubtract);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | expression '*' expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorMultiply);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | expression '/' expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorDivide);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | expression '%' expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorModulus);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | expression IDENTICAL expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorIdentical);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | expression UNIDENTICAL expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorUnidentical);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | expression EQUAL expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorEqual);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | expression UNEQUAL expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorUnequal);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | expression LESS expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorLess);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | expression GREATER expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorGreater);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    } 
  | expression LESS_EQUAL expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorLessEqual);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | expression GREATER_EQUAL expression {
      $$ = QLAstNodeCreate(context,QLNodeBinaryOperatorGreaterEqual);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  | expression IN expression {
      $$ = QLAstNodeCreate(context, QLNodeBinaryOperatorIn);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = $3;
      }
    }
  ;

conditional_operator:
  expression TERNARY expression COLON expression {
      QL_ast_node_t *node = QLAstNodeCreate(context, QLNodeContainerTernarySwitch);
      if (node) {
        node->_lhs = $3;
        node->_rhs = $5;
      }
      $$ = QLAstNodeCreate(context, QLNodeControlTernary);
      if ($$ != 0) {
        $$->_lhs = $1;
        $$->_rhs = node;
      }
    }
  ;

function_call: 
    function_invocation {
      $$ = $1;
    }
  ;

function_invocation:
    IDENTIFIER '(' ')' %prec FCALL {
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
    } function_args_list ')' {
      QL_ast_node_t *name = QLAstNodeCreate(context,QLNodeValueIdentifier);
      if (name != 0) {
        name->_value._stringValue = $1;
      }

      $$ = QLAstNodeCreate(context,QLNodeControlFunctionCall);
      if ($$ != 0) {
        $$->_lhs = name;
        QLPopIntoRhs($$, context);
      }
    }
  ;

function_args_list:
    expression {
      QLParseContextAddElement(context, $1);
    }
  | function_args_list ',' expression {
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
        QLPopIntoRhs($$, context);
      }
    } 
  ;

array_list:
    expression {
      QLParseContextAddElement(context, $1);
    }
  | array_list ',' expression {
      QLParseContextAddElement(context, $3);
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
        QLParseRegisterParseError(context, ERR_NUMBER_OUT_OF_RANGE, $1);
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
  | PARAMETER {
      // numbered parameter
      int64_t d = TRI_Int64String($1);

      if (TRI_errno() != TRI_ERROR_NO_ERROR || d < 0 || d >= 256) {
        QLParseRegisterParseError(context, ERR_PARAMETER_NUMBER_OUT_OF_RANGE, $1);
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

