////////////////////////////////////////////////////////////////////////////////
/// @brief query
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_VOC_BASE_QUERY_H
#define TRIAGENS_DURHAM_VOC_BASE_QUERY_H 1

#include "VocBase/vocbase.h"

#include "ShapedJson/shaped-json.h"
//#include "V8/v8-c-utils.h"
#include "VocBase/document-collection.h"
#include "VocBase/index.h"
#include "VocBase/join.h"
//#include "VocBase/join-execute.h"
#include "VocBase/where.h"
#include "VocBase/order.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @page BuildExecuteQuery Building and Executing a Query
///
/// @section LifeCycleQuery Life-Cycle of a Query
///
/// The query life-cycle starts with a string describing the query in the
/// Avocado query language. For example:
///
/// @verbinclude query1
/// 
/// In order to get the result the following steps are taken.
///
/// - the query is parsed
/// - an @CODE{TRI_query_t} representation is constructed
///
/// Later the query is executed:
///
/// - an @CODE{TRI_rc_context_t} is created
/// - the query is executed in this context
/// - a @CODE{TRI_rc_cursor_t} is returned
///
/// Later the result are accessed:
///
/// - the @CODE{next} method returns a @CODE{TRI_rc_result_t}
/// - which can be convert into a JSON or JavaScript object using
///   the @CODE{TRI_qry_select_t}
///
/// @section QueryRepresentation Query Representation
///
/// @copydetails TRI_query_t
///
/// @subsection QueryRepresentationSelect Query Select Representation
///
/// @copydetails TRI_qry_select_t
///
/// @section QueryExecution Query Execution
///
/// @subsection QueryContext Query Execution Context
///
/// @copydetails TRI_rc_context_t
///
/// @subsection ResultCursor Result Cursor
///
/// @copydetails TRI_rc_cursor_t
///
/// @subsection QueryResult Building the Query Result
///
/// @copydetails TRI_rc_result_t
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page AQL Avocado Query Language (AQL)
/// 
/// Queries can be used to extract arbitrary data from one or multiple
/// collections. A query needs to be composed in the Avocado Query Language
/// (AQL).
///
/// The purpose of AQL is somewhat similar to SQL, but the language has
/// notable differences to SQL. This means that AQL queries cannot be run in 
/// an SQL database and vice versa.
///
/// @section AqlBasics Language basics
///
/// AQL consists mainly of keywords, names, literals, compound types, and 
/// operators.
///
/// An example AQL query looks like this:
///
/// @verbinclude query0
///
/// @subsection AqlKeywords Keywords
///
/// For example, the terms @LIT{SELECT}, @LIT{FROM}, @LIT{JOIN}, and 
/// @LIT{WHERE} are keywords. Keywords have a special meaning in the language
/// and are used by the language parser to uniquely identify the meaning of a
/// query part.
///
/// Keywords can only be used at certains locations of a query and must not be 
/// used at others. For example, it is not possible to use a keyword as a 
/// collection or attribute name.
///
/// Keywords are case-insensitive, meaning they can be specified in lower or 
/// upper case in queries. 
/// 
/// In this documentation, all keywords are written in upper case to make them
/// distinguishable from the rest of the definitions.
///
/// @subsection AqlNames Names
///
/// In general, names are used to identify objects (collections, attributes,
/// functions in queries. 
/// 
/// The maximum length of any name is 64 bytes. All names are case-sensitive.
///
/// Keywords must not be used as names. Instead, if a reserved keyword should
/// be as used a query, the name must be enclosed in backticks. Enclosing a 
/// name in backticks allows to use otherwise-reserved words as names, e.g. 
/// @LIT{`select`}.
///
/// @subsubsection AqlCollectionNames Collection names and aliases
///
/// All collections to be used in a query need to be specified. When specified,
/// the original collection name and a mandatory alias must be specified for
/// each collection used. The alias can be any (unquoted) string, as long as
/// it is unique within the context of the query. This means the same alias
/// cannot be declared multiple times in the same query.
///
/// Allowed characters in collection names are the letters @LIT{a} to @LIT{z} 
/// (both in lower and upper case) and the numbers @LIT{0} to @LIT{9}. A 
/// collection name must always start with a letter.
///
/// Allowed characters in aliases are the letters @LIT{a} to @LIT{z} (both in
/// lower and upper case), the numbers @LIT{0} to @LIT{9} and the underscore 
/// (@LIT{_}) symbol. An alias must not start with a number. If an alias
/// starts with the underscore character, it must also contain at least one 
/// letter (a-z or A-Z).
///
/// @verbinclude collectionnames
/// 
/// @subsubsection AqlAttributeNames Attribute names
/// 
/// When referring to attributes of documents from a collection, the fully
/// qualified (i.e. aliased) name must be used. It is not valid to use an 
/// attribute name without the collection alias.
/// 
/// @verbinclude attributenames
///
/// @subsection AqlLiterals Literals
///
/// Literals are fixed values in queries. There are
/// - string literals
/// - numeric 
/// - the boolean literals @LIT{true} and @LIT{false}
/// - @LIT{null}
/// - @LIT{undefined}
///
/// @subsubsection AqlLiteralsString String literals
///
/// String literals are enclosed in single or double quotes. The quote character
/// within the string literal must be escaped using the @LIT{\\} symbol. 
/// Backslash literals must also be escaped using the @LIT{\\} symbol.
/// 
/// @verbinclude strings
///
/// The string encoding is always UTF-8. It is currently not possible to use 
/// arbitrary binary data if it is not UTF-8 encoded. A workaround to use
/// binary data is to encode the data using base64 or other algorithms on the
/// application side before storing, and decoding it on application side after
/// retrieval.
/// 
/// @subsubsection AqlLiteralsNumber Numeric literals
///
/// Numeric literals can be integers or real values. They can optionally be
/// signed using the @LIT{+} or @LIT{-} symbols. The scientific notation is
/// supported.
///
/// @verbinclude numbers
///
/// All numeric values are treated as 64-bit double-precision values internally. 
/// The format used is IEEE 754.
///
/// @subsubsection AqlLiteralsSpecial Special values
///
/// There are also special predefined literals for the boolean values 
/// @LIT{true} and @LIT{false}. There are also predefined @LIT{null} and
/// @LIT{undefined} values.
///
/// @verbinclude specials
///
/// Note: the special predefined literals are case-insensitive.
///
/// @subsection AqlCompound Compound types
/// 
/// AQL supports two compound types:
/// - documents: a composition of named values, accessible by name
/// - array: a composition of unnamed values, accessible by position
///
/// @subsubsection AqlCompoundDoc Documents
///
/// Documents are a composition of zero to many name/value pairs. Document
/// attributes can individually accessed by their names.
///
/// In its easiest form, a document can be expressed as a reference to the
/// current document of a collection that is used in the query. In this case
/// the collection alias can be used. The document then contains all 
/// attributes and attribute values that the referenced document from the
/// collection has.
///
/// The alternative is to construct a document on-the-fly using a 
/// @LIT{document-declaration}. A @LIT{document-declaration} starts with the 
/// @LIT{\{} symbol and ends with the @LIT{\}} symbol. 
///
/// A @LIT{document-declaration} contains zero or
/// many @LIT{attribute-declaration}s, seperated from each other with the 
/// @LIT{\,} symbol.
/// In the simplest case, a document is empty. Its declaration would then be:
///
/// @verbinclude documentempty
///
/// Each @LIT{attribute-declaration} is a name/value pair. Name and value are
/// separated using the @LIT{:} symbol.
///
/// The syntax for a single @LIT{attribute-declaration} is thus:
///
/// @verbinclude namedattribute
///
/// The @LIT{attribute-name} is an unquoted string. It is not possible to
/// use a keyword as an @LIT{attribute-name}.
///
/// The @LIT{attribute-value} can either be a @LIT{collection-alias} or an 
/// @LIT{expression}. @LIT{expression}s can be constructed using literals, 
/// names, (nested) documents, arrays, and operators.
///
/// @verbinclude documentvalues
///
/// @subsubsection AqlCompoundArray Arrays
///
/// The other supported compound type is the array type. Arrays are effectively
/// lists of (unnamed) values. Individual array elements can be accessed by
/// their position.
/// 
/// An @LIT{array-declaration} starts with the @LIT{[} symbol and ends 
/// with the @LIT{]} symbol. A @LIT{array-declaration} contains zero or
/// many @LIT{expression}s, seperated from each other with the 
/// @LIT{\,} symbol.
///
/// In the easiest case, an array is empty and thus looks like:
/// 
/// @verbinclude arrayempty
///
/// Array elements can be any legal @LIT{expression} values. Nesting of arrays
/// is supported.
/// 
/// @verbinclude arrayvalues
///
/// @subsection AqlOperators Operators
///
/// AQL supports a number of operators that can be used in expressions. 
/// There are comparison, logical, and arithmetic operators.
///
/// @subsubsection AqlOperatorsComp Comparison operators
///
/// The following comparison operators are supported:
///
/// - @LIT{==} equality (value equality, values might have different types)
/// - @LIT{===} strict equality (value and type equality)
/// - @LIT{!=} inequality (value inequality)
/// - @LIT{!==} strict inequality (value or type inequality)
/// - @LIT{<}, @LIT{<=}, @LIT{>=}, @LIT{>} less than, less than or equal,
///   greater than or equal, greater than
/// - @LIT{in} tests if a key exists
///
/// @verbinclude comparison
///
/// The result of each comparison is a boolean value.
///
/// @subsubsection AqlOperatorsLog Logical operators
///
/// The following logical operators are supported:
///
/// - @LIT{&&} logical @LIT{and} operator
/// - @LIT{||} logical @LIT{or} operator
/// - @LIT{!} logical @LIT{not} operator
///
/// @verbinclude logical
///
/// The @LIT{!} operator returns boolean value.
///
/// The @LIT{&&} operator returns the first operand if it evaluates to 
/// @LIT{false}. It returns the result of the evaluation of the second
/// operand otherwise. 
///
/// The @LIT{||} operators returns the first operand if it evaluates to
/// @LIT{true}. Otherwise it returns the result of the evaluation of 
/// the second operand.
///
/// Both @LIT{&&} and @LIT{||} use short-circuit evaluation and only
/// evaluate the second operand if the result of the operation cannot be
/// determined by the first operand alone.
///
/// @subsubsection AqlOperatorsArit Arithmetic operators
///
/// The following arithmetic operators are supported:
///
/// - @LIT{+} addition
/// - @LIT{-} subtraction
/// - @LIT{*} multiplication
/// - @LIT{/} division
/// - @LIT{%} modulus
///
/// Unary plus and unary minus are supported as well.
///
/// @verbinclude arithmetic
/// 
/// @subsubsection AQLOperatorsTernary Ternary operator
///
/// AQL also supports the ternary operator @LIT{? :} that can be used 
/// for conditional evaluation. The syntax is:
///
/// @verbinclude ternary
///
/// If @LIT{condition-expression} evaluates to true, the result of the 
/// ternary operator is the @LIT{true-expression}, otherwise, the 
/// result is the @LIT{false-expression}.
/// 
/// @subsubsection AQLOperatorsOther Other operators
///
/// The @LIT{+} sign can also be used for string concatenation.
///
/// @subsubsection AQLOperatorsPrec Operator precedence
///
/// The operator precedence is as follows (lowest precedence first):
///
/// - @LIT{? :} ternary operator
/// - @LIT{||} logical or
/// - @LIT{&&} logical and
/// - @LIT{===}, @LIT{!==}, @LIT{==}, @LIT{!=} equality and inequality
/// - @LIT{IN} in operator
/// - @LIT{<}, @LIT{<=}, @LIT{>=}, @LIT{>} less than, less than or equal,
///   greater than or equal, greater than
/// - @LIT{+}, @LIT{-} addition, subtraction
/// - @LIT{*}, @LIT{/}, @LIT{%} multiplication, division, modulus
/// - @LIT{!} logical not
/// - @LIT{-}, @LIT{+} unary minus, unary plus
/// - @LIT{.} member access
///
/// Parenthesis (@LIT{(} and @LIT{)}) can be used to enforce a different
/// evaluation order.
///
/// @section AqlSelect Select queries
///
/// Select queries can be used to extract data from one or many collections.
/// The result can be restricted to certain documents, and the result can 
/// also be a modified version of the original documents.
///
/// An AQL select query has the following structure:
///
/// @verbinclude select
///
/// Note: the terms @LIT{SELECT}, @LIT{FROM}, @LIT{WHERE}, @LIT{ORDER}, 
/// @LIT{BY}, and @LIT{LIMIT} are keywords. 
///
/// The select part of a query, consisting of the @LIT{SELECT} keyword plus
/// the @LIT{select-expression} is mandatory, so is the from part, consisting
/// of the @LIT{FROM} keyword plus the @LIT{join-expression}.
///
/// The where, order by and limit parts are optional.
///
/// @subsection AqlSelectClause Select clause
///
/// The @LIT{select-expression} determines the structure of the documents to
/// be returned by the select.
/// There are two ways to define the structure of the documents to return.
/// Therefore, the @LIT{select-expression} can either be @LIT{collection-alias} 
/// or a newly constructed document.
///
/// To select all documents from a collection without alteration use the
/// following syntax:
///
/// @verbinclude select1
///
/// Example (assumes there is a collection named @LIT{users}):
///
/// @verbinclude select2
///
/// You can also construct new documents using expressions that may or may not 
/// contain attributes of the documents found:
/// 
/// @verbinclude select3
///
/// Note: document attributes always need to be accessed in combination
/// with their @LIT{collection-alias}. Furthermore if a document attribute 
/// is used and it is not defined in the current document, the attribute 
/// will be assigned a value of @LIT{null}.
///
/// @section AqlFrom From clause
///
/// An AQL select query must have a from clause. The from clause defines the
/// collections that should be accessed by the query. The from clause always
/// starts with the @LIT{FROM} keyword, following by a list of collections.
///
/// Each collection must be declared using a @LIT{collection-definition}. It
/// looks like:
///
/// @verbinclude collectiondef1
///
/// Where @LIT{collection-name} is the name of the collection and 
/// @LIT{collection-alias} is its alias.
///
/// Specifying an alias is mandatory. Aliases must be unique within the context
/// of one query, i.e. the same alias cannot be used multiple times in the
/// same query.
///
/// The @LIT{from-expression} of a query can either consist of a single 
/// @LIT{collection-definition} or multiple joined 
/// @LIT{collection-definition}s.
///
/// @subsection AqlFromSingle Single collection access
///
/// As we've seen before, the @LIT{collection-definition} for a single 
/// collection can look as follows:
///
/// @verbinclude collectiondef
///
/// In this case, the collection accessed is named @LIT{users} and @LIT{u} is
/// used as its alias.
///
/// @subsection AqlJoin Multi collection access (joins)
///
/// Data from multiple collections can be accessed together in one query, provided
/// the documents from the collections satisfy some criteria. This is called joining.
/// There are 3 different types of joins, but all have the same structure:
///
/// @verbinclude join
///
/// @LIT{left-collection} and @LIT{right-collection} are simply 
/// @LIT{collection-definition}s. @LIT{join-type} is one of 
/// - @LIT{INNER JOIN}
/// - @LIT{LEFT JOIN} 
/// - @LIT{RIGHT JOIN} 
/// - @LIT{LIST JOIN}
///
/// The @LIT{on-expression} consists of the @LIT{ON} keyword, followed by
/// a @LIT{(} and an expression and is terminated by a @LIT{)} symbol.
/// 
/// The @LIT{left-collection} and @LIT{right-collection} values define which
/// collections should be joined to together, the @LIT{join-types} determines
/// how to join, and the @LIT{on-expression} determines what match criteria should
/// be applied.
///
/// Multiple joins are evaluated from left to right. That means @LIT{on-expression}s 
/// can only contain references to collections that have been specified before
/// (left) of them. References to collections specified on the right of them 
/// will cause a parse error.
///
/// @subsubsection AllInnerJoin Inner join
/// 
/// An inner join outputs the documents from both the left and right hand sides
/// of the join if, and only if the @LIT{on-condition} is true.
///
/// @verbinclude joininner
///
/// There will be one result document for each match found.
/// As each document on the left side might match multiple documents on the
/// right side (and vice versa), this means that each document from either
/// side might appear multiple times in the result.
///
/// @subsubsection AllOuterJoin Left and right (outer) joins
///
/// A left (outer) join outputs the documents from both the left and right hand 
/// sides of the join if, and only if the @LIT{on-condition} is true.
///
/// Additionally, it will output the document on the left hand side of the join
/// if no matching document on the right side can be found. If this is the case, 
/// the right document will be substituted with @LIT{null} when the result is
/// produced.
///
/// @verbinclude joinleft
///
/// There will be one result document for each match found.
///
/// As each document on the left side might match multiple documents on the
/// right side (and vice versa), this means that each document from either
/// side might appear multiple times in the result. If a document on the left
/// side has no matching document on the right side, it will be output exactly
/// once. Altogether this means all documents from the left hand side will be 
/// contained in the result set at least once.
///
/// A right (outer) join acts like a left (outer join) but the roles of the
/// left and right operands are swapped. Thus, right joins are implemented as 
/// reversed left joins (i.e. left joins with their operands swapped).
///
/// @subsubsection AllInnerJoin List join
///
/// A list join outputs all documents on the left hand side exactly once,
/// and for each document, it produces a list of matching documents of the 
/// right hand side. The list is produced as an array of complete documents of
/// the right hand side.
///
/// @verbinclude joinleft
///
/// @subsection GeoQueries Geo restrictions
///
/// AQL supports special geo restrictions in case a collection has a geo index 
/// defined for it. These queries can be used to find document that have coordinates
/// attached to them that are located around some two-dimensional geo point 
/// (reference point).
///
/// There are two types of geo queries:
/// - WITHIN: finds all documents that have coordinates that are within a certain
///   radius around the reference point
/// - NEAR: finds the first n documents whose coordinates have the least distance
///   to the reference point
///
/// For all geo queries, the distance between the coordinates as specified in the
/// document and the reference point. This distance is returned as an additional
/// (virtual) collection. This collection can be used in other parts of the
/// query (e.g. WHERE condition) by using its alias.
///
/// Geo queries extend the FROM/JOIN syntax as follows:
///
/// @verbinclude geoquery
///
/// For the @LIT{geo-alias}, the same naming rules apply as for normal collections. 
/// It must be unique within the query. The @LIT{document-coordinate} can either be
/// an array of two attribute names, or two separate attribute names, separated by
/// a comma. The attribute names in @LIT{document-coordinate} must refer to the
/// @LIT{collection-alias} specified directly before the geo query. The first
/// attribute is interpreted as the latitude attribute name, the second as the 
/// longitude attribute name.
///
/// The reference coordinate can either be an array of two numbers, or or two 
/// separate numbers, separated by a comma. The first number is interpreted as the
/// latitude value, the second number as the longitude value of the reference point.
///
/// @verbinclude geo1
/// 
/// For WITHIN, the @LIT{radius} is specified in meters.
/// For NEAR, the @LIT{number-of-documents} value will restrict the result to at
/// most this number of documents.
///
/// Geo restrictions can be used for joined collections as well. In this case, the
/// restriction is applied first to find the relevant documents from the collection,
/// and afterwards, the ON condition specified for the join is evaluated. Evaluating
/// the ON condition might further reduce the result. 
///
/// @section AqlWhere Where clause
///
/// The where clause can be specified to restrict the result to documents that
/// match certain criteria.
///
/// The where clause is initiated by the @LIT{WHERE} keyword, followed by an
/// expression that defines the search condition. A document is included
/// in the result if the search condition evaluates to @LIT{true}.
///
/// Geo restrictions cannot be used in the where clause.
///
/// @section AqlParameter Parameters
///
/// AQL supports usage of query parameters, thus allowing to separate the
/// query from the actual parameter values used. It is good practice to separate
/// the query from the values because this will prevent the (malicious) injection 
/// of keywords and other collection names into an existing query. This injection
/// would be dangerous because it might change the meaning of an existing query.
///
/// Using parameters, the meaning of an existing query cannot be changed. 
/// Parameters can be used everywhere in a query where literals or attributes can
/// be used.
///
/// Two types of parameters are supported:
/// - @LIT{\@0} - positional parameter, starting with 0
/// - @LIT{\@name\@} - named parameter
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief select clause type - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_QRY_SELECT_DOCUMENT,
  TRI_QRY_SELECT_GENERAL
}
TRI_qry_select_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract select clause - DEPRECATED
///
/// A description of the @CODE{select} statement of the Avocado query language.
/// This class is respsonible for taking an element of type
/// @CODE{TRI_rc_result_t} as returned by the cursor and transforming it into
/// either a shaped json object of type @CODE{TRI_shaped_json_t} or a JavaScript
/// object of type @CODE{v8::Value}. The returned objects are either freed when
/// the @CODE{TRI_rc_result_t} is deleted or are garbage collected in case of
/// JavaScript.
///
/// The following implementations exist.
///
/// @copydetails TRI_qry_select_direct_t
///
/// @copydetails TRI_qry_select_general_t
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_select_s {
  TRI_qry_select_type_e _type;

  struct TRI_qry_select_s* (*clone) (struct TRI_qry_select_s const*);
  void (*free) (struct TRI_qry_select_s*);

  TRI_shaped_json_t* (*shapedJson) (struct TRI_qry_select_s*, TRI_rc_result_t*);
  bool (*toJavaScript) (struct TRI_qry_select_s*, TRI_rc_result_t*, void*);
}
TRI_qry_select_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief built-in select clause for a single, unaltered document - DEPRECATED
///
/// If a query returns a document unaltered, then @CODE{TRI_qry_select_direct_t}
/// can be used. It extracts a given document from the result set and returns
/// this document without any modification. The member @CODE{_variable} contains
/// the variable identifier of the result document of the next result to use.
///
/// For example
///
/// @verbinclude query2
///
/// will generate a query whose select part consists of an instance of
/// @CODE{TRI_qry_select_direct_t}. 
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_select_direct_s {
  TRI_qry_select_t base;
}
TRI_qry_select_direct_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript select clause - DEPRECATED
///
/// If a query returns a newly created document, then
/// @CODE{TRI_qry_select_general_t} can be used. It uses the V8 engine to
/// compute the document.  The member @CODE{_code} contains the JavaScript code
/// used to generate the document.  When this code is executed, then the result
/// documents of the next result are passed into the JavaScript code bound to
/// variables.
///
/// For example
///
/// @verbinclude query3
///
/// will generate new documents containing only @LIT{name} and @LIT{email}.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_qry_select_general_s {
  TRI_qry_select_t base;

  char* _code;
}
TRI_qry_select_general_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief general query - DEPRECATED
///
/// A query is built using the fluent-interface or the Avocado Query
/// Language. It is of type @CODE{TRI_query_t}. After the query is built, it can
/// be executed. An execution returns a result cursor of type
/// @CODE{TRI_rc_cursor_t}. An execution requires a context of type
/// @CODE{TRI_rc_context_t}. All result are only vaild for the life-time of this
/// context.
///
/// It is possible to execute the same query multiple times - even in
/// parallel. However, each execution run requires it's own context.
///
/// A query contains the following parts:
///
/// - a description of the result in @CODE{_select}
/// - the primary collection for the query in @CODE{_primary}
/// - a list of joins in @CODE{_joins}
/// - a description of the where clause in @CODE{_where}
/// - an optional order by clause in @CODE{_order}
/// - a number limiting the number of returned documents in @CODE{_limit}
/// - a number of documents to skip in @CODE{_skip}
///
/// The @CODE{_select} is of type @CODE{TRI_qry_select_t}. This is in general a
/// representation of the part following the @CODE{select} statement.  There are
/// at least two implementions. The first one uses a built-in evaluator to
/// generate the result document from the next set of selected documents. The
/// second one uses the V8 engine to generate the result.
///
/// The @CODE{_primary} is of type @CODE{TRI_doc_collection_t}. This is the
/// collection following the @CODE{from} statement. The Avocado query language
/// only supports one collection after the @CODE{from}, this is the primary
/// collection of the query. All other collections must be joined with this
/// primary collection.
///
/// The @CODE{_joins} attributes contains a vector of joined collections
/// together with the join condition. The vector elements are of type
/// @CODE{TRI_qry_join_t}.
///
/// The @CODE{_where} attributes contains a description of the where
/// clause. There are at least two implementations. The first one uses a
/// built-in evaluator to evaluate the predicate. The second one uses the V8
/// engine.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_query_s {
  TRI_vocbase_t* _vocbase;
  TRI_qry_select_t* _select;
  TRI_doc_collection_t* _primary;
  TRI_qry_where_t* _where;
  TRI_qry_order_t* _order;
  TRI_select_join_t* _joins;

  TRI_voc_size_t _skip;
  TRI_voc_ssize_t _limit;
}
TRI_query_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief the result cursor - DEPRECATED
///
/// The result of a query execution. The metod @FN{next} returns the next
/// result documents. There is one document for the primary collection and a
/// list of documents for a list join. The next result documents are delivered
/// as instance of @CODE{TRI_rc_result_t}. You must call the @FN{free} method
/// for these instances.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_rc_cursor_s {
    TRI_rc_context_t* _context;
    TRI_qry_select_t* _select;
    TRI_select_result_t* _selectResult;
    TRI_js_exec_context_t* _selectContext;

    TRI_vector_pointer_t _containers;

    TRI_voc_size_t _scannedIndexEntries;
    TRI_voc_size_t _scannedDocuments;
    TRI_voc_size_t _matchedDocuments;

    void (*free) (struct TRI_rc_cursor_s*);
    TRI_rc_result_t* (*next)(struct TRI_rc_cursor_s*);
    bool (*hasNext)(struct TRI_rc_cursor_s*);
}
TRI_rc_cursor_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief the result cursor - DEPRECATED
///
/// The result of a query execution. The metod @FN{next} returns the next
/// result documents. There is one document for the primary collection and a
/// list of documents for a list join. The next result documents are delivered
/// as instance of @CODE{TRI_rc_result_t}. You must call the @FN{free} method
/// for these instances.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_query_cursor_s {
//  TRI_select_result_t* _selectResult;
  TRI_js_exec_context_t* _selectContext;

  TRI_vector_pointer_t _containers;

  TRI_rc_result_t _result;
  TRI_select_size_t _length;
  TRI_select_size_t _currentRow;

  void (*free) (struct TRI_query_cursor_s*);
  TRI_rc_result_t* (*next)(struct TRI_query_cursor_s* const);
  bool (*hasNext)(const struct TRI_query_cursor_s* const);

//  TRI_doc_mptr_t* _documents;
//  TRI_doc_mptr_t* _current;
//  TRI_doc_mptr_t* _end;

}
TRI_query_cursor_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief no limit
////////////////////////////////////////////////////////////////////////////////

#define TRI_QRY_NO_LIMIT ((TRI_voc_ssize_t) INT32_MAX)

////////////////////////////////////////////////////////////////////////////////
/// @brief no skip
////////////////////////////////////////////////////////////////////////////////

#define TRI_QRY_NO_SKIP ((TRI_voc_size_t) 0)

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_query_t* TRI_CreateHashQuery (const TRI_qry_where_t*, TRI_doc_collection_t*);

TRI_query_t* TRI_CreateSkiplistQuery (const TRI_qry_where_t*, TRI_doc_collection_t*);

TRI_query_t* TRI_CreateQuery (TRI_vocbase_t*,
                              TRI_qry_select_t*,
                              TRI_qry_where_t*,
                              TRI_qry_order_t*,
                              TRI_select_join_t*,
                              TRI_voc_size_t,
                              TRI_voc_ssize_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a query - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQuery (TRI_query_t* query);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a context - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_rc_context_t* TRI_CreateContextQuery (TRI_query_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees a context - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeContextQuery (TRI_rc_context_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query selection for unaltered documents - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_select_t* TRI_CreateQuerySelectDocument (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query selection for general, generated documents - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_select_t* TRI_CreateQuerySelectGeneral (char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition for constant conditions - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereBoolean (bool where);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition for general, JavaScript conditions - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereGeneral (char const*);


////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition for a hash with constant parameters - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereHashConstant (TRI_idx_iid_t, TRI_json_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition for a hash with constant parameters - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereSkiplistConstant (TRI_idx_iid_t, TRI_json_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition using the primary index and a constant - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWherePrimaryConstant (TRI_voc_did_t did);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a query condition using an geo index and a constants - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_qry_where_t* TRI_CreateQueryWhereWithinConstant (TRI_idx_iid_t iid,
                                                     char const* nameDistance,
                                                     double latitiude,
                                                     double longitude,
                                                     double radius);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks all collections of a query - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadLockCollectionsQuery (TRI_query_t* query);

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks all collections of a query - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadUnlockCollectionsQuery (TRI_query_t* query);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a gc marker for all collections
////////////////////////////////////////////////////////////////////////////////

void TRI_AddCollectionsCursor (TRI_rc_cursor_t* cursor, TRI_query_t* query);

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks all collections of a query
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveCollectionsCursor (TRI_rc_cursor_t* cursor);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

TRI_rc_cursor_t* TRI_ExecuteQueryAql (TRI_query_t*, TRI_rc_context_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query
////////////////////////////////////////////////////////////////////////////////

TRI_query_cursor_t* TRI_ExecuteQueryInstance (void* const);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
