
%pure-parser
%name-prefix="QL"
%locations
%defines
%parse-param { TRI_query_template_t* const template_ }
%lex-param { void* scanner }
%error-verbose

%{

#include <stdio.h>
#include <stdlib.h>

#include <BasicsC/common.h>
#include <BasicsC/conversions.h>
#include <BasicsC/strings.h>

#include "VocBase/query-node.h"
#include "VocBase/query-base.h"
#include "VocBase/query-parse.h"
#include "VocBase/query-error.h"

#define ABORT_IF_OOM(ptr) \
  if (!ptr) { \
    TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_OOM, NULL); \
    YYABORT; \
  }

%}

%union {
  TRI_query_node_t* node;
  int intval;
  double floatval;
  char *strval;
}


%{
int QLlex (YYSTYPE*, YYLTYPE*, void*);

void QLerror (YYLTYPE* locp, TRI_query_template_t* const template_, const char* err) {
  TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_PARSE, err); 
}

#define scanner template_->_parser->_scanner
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
%type <node> geo_restriction;
%type <node> geo_reference;
%type <node> geo_1dreference;
%type <node> geo_2dreference;
%type <node> geo_value;
%type <node> geo_1dvalue;
%type <node> geo_2dvalue;
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
%token WITHIN NEAR
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
      template_->_query->_type        = QLQueryTypeEmpty;
    }
  ;

select_query:
    SELECT select_clause from_clause where_clause order_clause limit_clause {
      // full blown SELECT query
      template_->_query->_type         = QLQueryTypeSelect;
      template_->_query->_select._base = $2;
      template_->_query->_from._base   = $3;
      template_->_query->_where._base  = $4;
      template_->_query->_order._base  = $5;
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
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    } from_list {
      $$ = TRI_ParseQueryContextPop(template_);
      ABORT_IF_OOM($$);
    }
  ;	      						

from_list:
    collection_reference geo_restriction {
      // single table query
      ABORT_IF_OOM($1);
      TRI_ParseQueryContextAddElement(template_, $1);

      if ($2) {
        if (!QLAstQueryAddGeoRestriction(template_->_query, $1, $2)) {
          TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_GEO_RESTRICTION_INVALID, ((TRI_query_node_t*) $2->_lhs)->_value._stringValue);
          YYABORT;
        }
      }
    } 
  | from_list join_type collection_reference geo_restriction ON expression {
      // multi-table query
      ABORT_IF_OOM($2);
      ABORT_IF_OOM($3);
      ABORT_IF_OOM($6);
      $$ = $2;
      $$->_lhs  = $3;
      $$->_rhs  = $6;
      
      if ($4) {
        if (!QLAstQueryAddGeoRestriction(template_->_query, $3, $4)) {
          TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_GEO_RESTRICTION_INVALID, ((TRI_query_node_t*) $4->_lhs)->_value._stringValue);
          YYABORT;
        }
      }
      
      TRI_ParseQueryContextAddElement(template_, $2);
    }
  ;

geo_2dvalue: 
    '[' geo_1dvalue ',' geo_1dvalue ']' {
      ABORT_IF_OOM($2);
      ABORT_IF_OOM($4);

      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueCoordinate);
      ABORT_IF_OOM($$);

      $$->_lhs = $2;
      $$->_rhs = $4;
    }
  ;

geo_1dvalue:
    REAL {
      double d;

      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueNumberDouble);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      
      d = TRI_DoubleString($1);
      if (TRI_errno() != TRI_ERROR_NO_ERROR && d != 0.0) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, $1);
        YYABORT;
      }
      $$->_value._doubleValue = d; 
    }
  | '-' REAL %prec UMINUS { 
      double d;

      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueNumberDouble);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($2);
      
      d = TRI_DoubleString($2);
      if (TRI_errno() != TRI_ERROR_NO_ERROR && d != 0.0) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, $2);
        YYABORT;
      }
      $$->_value._doubleValue = -1.0 * d; 
    }
  ;

geo_value:
    geo_2dvalue {
      ABORT_IF_OOM($1);
      $$ = $1;
    }
  | geo_1dvalue ',' geo_1dvalue {
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);

      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueCoordinate);
      ABORT_IF_OOM($$);

      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  ;

geo_2dreference:
    '[' geo_1dreference ',' geo_1dreference ']' {
      ABORT_IF_OOM($2);
      ABORT_IF_OOM($4);

      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueCoordinate);
      ABORT_IF_OOM($$);
      $$->_lhs = $2;
      $$->_rhs = $4;
    }
  ;

geo_1dreference:
    collection_alias {
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    } object_access %prec MEMBER {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerMemberAccess);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_lhs = $1;
      TRI_ParseQueryPopIntoRhs($$, template_);
    }
  ;

geo_reference: 
    geo_2dreference {
      ABORT_IF_OOM($1);
      $$ = $1;
    }
  | geo_1dreference ',' geo_1dreference {
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);

      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueCoordinate);
      ABORT_IF_OOM($$);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  ;

geo_restriction:
    /* empty */ {
      $$ = 0;
    }
  | WITHIN collection_alias ASSIGNMENT '(' geo_reference ',' geo_value ',' REAL ')' {
      TRI_query_node_t* comp;
      double distance;
      
      ABORT_IF_OOM($2);
      ABORT_IF_OOM($5);
      ABORT_IF_OOM($7);
      ABORT_IF_OOM($9);
      
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeRestrictWithin);
      ABORT_IF_OOM($$);

      distance = TRI_DoubleString($9);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, $9);
        YYABORT;
      }
      $$->_value._doubleValue = distance;

      comp = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerCoordinatePair);
      ABORT_IF_OOM(comp);
      comp->_lhs = $5;
      comp->_rhs = $7;

      $$->_lhs = $2;
      $$->_rhs = comp;
    }
  | NEAR collection_alias ASSIGNMENT '(' geo_reference ',' geo_value ',' REAL ')' {
      TRI_query_node_t* comp;
      int64_t num;
      
      ABORT_IF_OOM($2);
      ABORT_IF_OOM($5);
      ABORT_IF_OOM($7);
      ABORT_IF_OOM($9);
      
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeRestrictNear);
      ABORT_IF_OOM($$);

      num = TRI_Int64String($9);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE, $9);
        YYABORT;
      }
      $$->_value._intValue = num;

      comp = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerCoordinatePair);
      ABORT_IF_OOM(comp);
      comp->_lhs = $5;
      comp->_rhs = $7;

      $$->_lhs = $2;
      $$->_rhs = comp;
    }
    /*
  | NEAR collection_alias ASSIGNMENT '(' geo_reference ',' geo_value ',' geo_value ',' REAL ')' {
    }
    */
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
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    } order_list {
      $$ = TRI_ParseQueryContextPop(template_);
      ABORT_IF_OOM($$);
    }
  ;

order_list:
    order_element {
      ABORT_IF_OOM($1);
      TRI_ParseQueryContextAddElement(template_, $1);
    }
  | order_list ',' order_element {
      ABORT_IF_OOM($3);
      TRI_ParseQueryContextAddElement(template_, $3);
    }
  ;

order_element:
    expression order_direction {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerOrderElement);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($2);
      $$->_lhs = $1;
      $$->_rhs = $2;
    }
  ;

order_direction:
    /* empty */ {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueOrderDirection);
      ABORT_IF_OOM($$);
      $$->_value._boolValue = true;
    }
  | ASC {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueOrderDirection);
      ABORT_IF_OOM($$);
      $$->_value._boolValue = true;
    }
  | DESC {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueOrderDirection);
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
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE, $2);
        YYABORT;
      }
      
      template_->_query->_limit._isUsed = true;
      template_->_query->_limit._offset = 0; 
      template_->_query->_limit._count  = d; 
    }
  | LIMIT '-' REAL {
      // limit - value
      int64_t d = TRI_Int64String($3);

      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE, $3);
        YYABORT;
      }
      
      template_->_query->_limit._isUsed = true;
      template_->_query->_limit._offset = 0; 
      template_->_query->_limit._count  = -d; 
    }
  | LIMIT REAL ',' REAL { 
      // limit value, value
      int64_t d1, d2;
      
      d1 = TRI_Int64String($2);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE, $2);
        YYABORT;
      }

      d2 = TRI_Int64String($4);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE, $4);
        YYABORT;
      }

      template_->_query->_limit._isUsed = true;
      template_->_query->_limit._offset = d1; 
      template_->_query->_limit._count  = d2;
    }
  | LIMIT REAL ',' '-' REAL { 
      // limit value, -value
      int64_t d1, d2;
      
      d1 = TRI_Int64String($2);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE, $2);
        YYABORT;
      }

      d2 = TRI_Int64String($5);
      if (TRI_errno() != TRI_ERROR_NO_ERROR) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE, $5);
        YYABORT;
      }

      template_->_query->_limit._isUsed = true;
      template_->_query->_limit._offset = d1; 
      template_->_query->_limit._count  = -d2;
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
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueDocument);
      ABORT_IF_OOM($$);
    }
  | '{' {
      // listing of document attributes
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    } attribute_list '}' {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueDocument);
      ABORT_IF_OOM($$);
      TRI_ParseQueryPopIntoRhs($$, template_);
    }
  ;

attribute_list:
    attribute {
      ABORT_IF_OOM($1);
      TRI_ParseQueryContextAddElement(template_, $1);
    }
  | attribute_list ',' attribute {
      ABORT_IF_OOM($3);
      TRI_ParseQueryContextAddElement(template_, $3);
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
      size_t outLength;
      TRI_query_node_t* str = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueString);
      ABORT_IF_OOM(str);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      str->_value._stringValue = TRI_ParseQueryRegisterString(template_, TRI_UnescapeUtf8String($1, strlen($1), &outLength)); 

      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueNamedValue);
      ABORT_IF_OOM($$);
      $$->_lhs = str;
      $$->_rhs = $3;
    }
  | STRING COLON expression {
      size_t outLength;
      TRI_query_node_t* str = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueString);
      ABORT_IF_OOM(str);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      str->_value._stringValue = TRI_ParseQueryRegisterString(template_, TRI_UnescapeUtf8String($1 + 1, strlen($1) - 2, &outLength)); 

      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueNamedValue);
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

      if (!TRI_ParseQueryValidateCollectionName($1->_value._stringValue)) {
        // validate collection name
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_COLLECTION_NAME_INVALID, $1->_value._stringValue);
        YYABORT;
      }

      if (!TRI_ParseQueryValidateCollectionAlias($2->_value._stringValue)) {
        // validate alias
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_COLLECTION_ALIAS_INVALID, $2->_value._stringValue);
        YYABORT;
      }

      if (!QLAstQueryAddCollection(template_->_query, $1->_value._stringValue, $2->_value._stringValue)) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_COLLECTION_ALIAS_REDECLARED, $2->_value._stringValue);
        YYABORT;
      }

      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeReferenceCollection);
      ABORT_IF_OOM($$);
      $$->_lhs = $1;
      $$->_rhs = $2;
    }
  ;


collection_name:
    IDENTIFIER {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueIdentifier);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_value._stringValue = $1;
    }
  | QUOTED_IDENTIFIER {
      size_t outLength;
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueIdentifier);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_value._stringValue = TRI_ParseQueryRegisterString(template_, TRI_UnescapeUtf8String($1 + 1, strlen($1) - 2, &outLength)); 
    }
  ;

collection_alias:
    IDENTIFIER {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeReferenceCollectionAlias);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_value._stringValue = $1;
    }
  | QUOTED_IDENTIFIER {
      size_t outLength;
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeReferenceCollectionAlias);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_value._stringValue = TRI_ParseQueryRegisterString(template_, TRI_UnescapeUtf8String($1 + 1, strlen($1) - 2, &outLength)); 
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
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeJoinList);
      ABORT_IF_OOM($$);
    }
  ;

inner_join:
    JOIN {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeJoinInner);
      ABORT_IF_OOM($$);
    }
  | INNER JOIN {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeJoinInner);
      ABORT_IF_OOM($$);
    }
  ;

outer_join:
    LEFT OUTER JOIN {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeJoinLeft);
      ABORT_IF_OOM($$);
    }
  | LEFT JOIN {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeJoinLeft);
      ABORT_IF_OOM($$);
    } 
  | RIGHT OUTER JOIN {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeJoinRight);
      ABORT_IF_OOM($$);
    }
  | RIGHT JOIN {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeJoinRight);
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
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    } object_access %prec MEMBER {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerMemberAccess);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_lhs = $1;
      TRI_ParseQueryPopIntoRhs($$, template_);
    }
  | document {
      ABORT_IF_OOM($1);
      $$ = $1;
    }
  | array_declaration {
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    } object_access %prec MEMBER {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerMemberAccess);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_lhs = $1;
      TRI_ParseQueryPopIntoRhs($$, template_);
    }
  | array_declaration {
      ABORT_IF_OOM($1);
      $$ = $1;  
    }
  | atom {
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    } object_access %prec MEMBER {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerMemberAccess);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_lhs = $1;
      TRI_ParseQueryPopIntoRhs($$, template_);
    }
  | atom {
      ABORT_IF_OOM($1);
      $$ = $1;
    }
  ;
      

object_access:
    '.' IDENTIFIER {
      TRI_query_node_t* name = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM($2);
      name->_value._stringValue = $2;
      TRI_ParseQueryContextAddElement(template_, name);
    }
  | '.' function_call {
      ABORT_IF_OOM($2);
      TRI_ParseQueryContextAddElement(template_, $2);
    }
  | object_access '.' IDENTIFIER {
      TRI_query_node_t* name = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM($3);
      name->_value._stringValue = $3;
      TRI_ParseQueryContextAddElement(template_, name);
    }
  | object_access '.' function_call {
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      TRI_ParseQueryContextAddElement(template_, $3);
    }
  ; 

unary_operator:
    '+' expression %prec UPLUS {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeUnaryOperatorPlus);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($2);
      $$->_lhs = $2;
    }
  | '-' expression %prec UMINUS {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeUnaryOperatorMinus);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($2);
      $$->_lhs = $2;
    }
  | NOT expression { 
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeUnaryOperatorNot);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($2);
      $$->_lhs = $2;
    }
  ;

binary_operator:
    expression OR expression {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorOr);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression AND expression {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorAnd);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3; 
    }
  | expression '+' expression {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorAdd);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression '-' expression {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorSubtract);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression '*' expression {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorMultiply);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression '/' expression {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorDivide);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression '%' expression {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorModulus);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression IDENTICAL expression {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorIdentical);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression UNIDENTICAL expression {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorUnidentical);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression EQUAL expression {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorEqual);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression UNEQUAL expression {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorUnequal);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression LESS expression {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorLess);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression GREATER expression {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorGreater);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    } 
  | expression LESS_EQUAL expression {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorLessEqual);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression GREATER_EQUAL expression {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorGreaterEqual);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  | expression IN expression {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeBinaryOperatorIn);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      $$->_lhs = $1;
      $$->_rhs = $3;
    }
  ;

conditional_operator:
  expression TERNARY expression COLON expression {
      TRI_query_node_t* node = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerTernarySwitch);
      ABORT_IF_OOM(node);
      ABORT_IF_OOM($1);
      ABORT_IF_OOM($3);
      ABORT_IF_OOM($5);
      node->_lhs = $3;
      node->_rhs = $5;

      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeControlTernary);
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
      TRI_query_node_t* name = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM($1);
      name->_value._stringValue = $1;
      
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeControlFunctionCall);
      ABORT_IF_OOM($$);
      $$->_lhs = name;
      $$->_rhs = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM($$->_rhs);
    }
  | IDENTIFIER '(' {
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    } function_args_list ')' {
      TRI_query_node_t* name = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueIdentifier);
      ABORT_IF_OOM(name);
      ABORT_IF_OOM($1);
      name->_value._stringValue = $1;

      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeControlFunctionCall);
      ABORT_IF_OOM($$);
      $$->_lhs = name;
      TRI_ParseQueryPopIntoRhs($$, template_);
    }
  ;

function_args_list:
    expression {
      TRI_ParseQueryContextAddElement(template_, $1);
    }
  | function_args_list ',' expression {
      ABORT_IF_OOM($3);
      TRI_ParseQueryContextAddElement(template_, $3);
    }
  ;

array_declaration:
    '[' ']' {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueArray);
      ABORT_IF_OOM($$);
    }
  | '[' {
      TRI_query_node_t* list = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeContainerList);
      ABORT_IF_OOM(list);
      TRI_ParseQueryContextPush(template_, list); 
    } array_list ']' { 
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueArray);
      ABORT_IF_OOM($$);
      TRI_ParseQueryPopIntoRhs($$, template_);
    } 
  ;

array_list:
    expression {
      TRI_ParseQueryContextAddElement(template_, $1);
    }
  | array_list ',' expression {
      ABORT_IF_OOM($3);
      TRI_ParseQueryContextAddElement(template_, $3);
    }
  ;

atom:
    STRING {
      size_t outLength;
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueString);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_value._stringValue = TRI_ParseQueryRegisterString(template_, TRI_UnescapeUtf8String($1 + 1, strlen($1) - 2, &outLength)); 
    }
  | REAL {
      double d = TRI_DoubleString($1);
      if (TRI_errno() != TRI_ERROR_NO_ERROR && d != 0.0) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, $1);
        YYABORT;
      }
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueNumberDoubleString);
      ABORT_IF_OOM($$);
      ABORT_IF_OOM($1);
      $$->_value._stringValue = $1; 
    }
  | NULLX {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueNull);
      ABORT_IF_OOM($$);
    }
  | UNDEFINED {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueUndefined); 
      ABORT_IF_OOM($$);
    }
  | TRUE { 
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueBool);
      ABORT_IF_OOM($$);
      $$->_value._boolValue = true;
    }
  | FALSE {
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueBool);
      ABORT_IF_OOM($$);
      $$->_value._boolValue = false;
    }
  | PARAMETER {
      // numbered parameter
      int64_t d = TRI_Int64String($1);

      if (TRI_errno() != TRI_ERROR_NO_ERROR || d < 0 || d >= 256) {
        TRI_SetQueryError(&template_->_error, TRI_ERROR_QUERY_BIND_PARAMETER_NUMBER_OUT_OF_RANGE, $1);
        YYABORT;
      }

      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueParameterNumeric);
      ABORT_IF_OOM($$);
      $$->_value._intValue = d;
      TRI_AddBindParameterQueryTemplate(template_, TRI_CreateBindParameter($1, NULL));   
    }
  | PARAMETER_NAMED {
      // named parameter
      $$ = TRI_ParseQueryCreateNode(template_, TRI_QueryNodeValueParameterNamed);
      ABORT_IF_OOM($$);
      $$->_value._stringValue = $1;
      TRI_AddBindParameterQueryTemplate(template_, TRI_CreateBindParameter($1, NULL));   
    }
 ;

%%

