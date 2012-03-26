////////////////////////////////////////////////////////////////////////////////
/// @brief query execution
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_VOC_BASE_QUERY_EXECUTE_H
#define TRIAGENS_DURHAM_VOC_BASE_QUERY_EXECUTE_H 1

#include "VocBase/vocbase.h"

#include "VocBase/query-cursor.h"
#include "VocBase/query-base.h"
#include "VocBase/query-join-execute.h"
#include "VocBase/query-order.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @page AQLBasics Query language basics
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
/// (both in lower and upper case) and the numbers @LIT{0} to @LIT{9} and the
/// the underscore (@LIT{_}) symbol. A collection name must start with either
/// a letter or a number, but not with an underscore.
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
/// qualified (i.e. aliased) name must always be used. It is not valid to use 
/// an attribute name without the collection alias.
/// The following examples are all valid fully qualified attribute names:
/// 
/// @verbinclude attributenames
///
/// whereas the following examples are all invalid because the attribute names
/// are not fully qualified:
///
/// @verbinclude attributenames2
///
/// @subsection AqlTypes Data types
///
/// The following data types exist in AQL:
/// - @LIT{undefined}: an unknown value
/// - @LIT{null}: an empty value
/// - bool: boolean value with possible values @LIT{false} and @LIT{true}
/// - number: signed (real) numbers
/// - strings: UTF-8 encoded text values
/// - lists: sequences of values, referred to by their positions
/// - documents: sequences of values, referred to by their names
///
/// @subsection AqlLiterals Literals
///
/// Literals are fixed values in queries. There are
/// - @LIT{undefined}
/// - @LIT{null}
/// - the boolean literals @LIT{true} and @LIT{false}
/// - numeric 
/// - string 
/// literals.
///
/// @subsubsection AqlLiteralsSpecial Special values
///
/// There is a special predefined literal for expressing an unknown value
/// (@LIT{undefined}). Using the @LIT{undefined} value in any context of a query
/// will also produce a result of type @LIT{undefined}. Many of AQL's operators
/// will also produce a value of @LIT{undefined} when the operation cannot be
/// processed or would have an undefined result (e.g. division by zero).
/// @LIT{undefined} is also returned when accessing a non-existing attribute in
/// a document. Comparing @LIT{undefined} to itself will also return @LIT{undefined}.
///
/// The other predefined literals are @LIT{null} for an empty value, and the boolean
/// literals @LIT{false} and @LIT{true}. 
///
/// @verbinclude specials
///
/// Note: all the predefined literals are case-insensitive.
///
/// @subsubsection AqlLiteralsNumber Numeric literals
///
/// Numeric literals can be integers or real values. They can optionally be
/// signed using the @LIT{+} or @LIT{-} symbols. The scientific notation is also
/// supported.
///
/// @verbinclude numbers
///
/// All numeric values are treated as 64-bit double-precision values internally. 
/// The format used is IEEE 754.
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
/// @subsection AqlCompound Compound types
/// 
/// AQL supports two compound types:
/// - documents: a composition of named values, each accessible by name
/// - lists: a composition of unnamed values, each accessible by position
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
/// names, (nested) documents, lists, and operators.
///
/// @verbinclude documentvalues
///
/// @subsubsection AqlCompoundList Lists
///
/// The other supported compound type is the list type. Lists are effectively
/// lists of (unnamed) values. Individual list elements can be accessed by
/// their position.
/// 
/// An @LIT{list-declaration} starts with the @LIT{[} symbol and ends 
/// with the @LIT{]} symbol. A @LIT{list-declaration} contains zero or
/// many @LIT{expression}s, seperated from each other with the 
/// @LIT{\,} symbol.
///
/// In the easiest case, a list is empty and thus looks like:
/// 
/// @verbinclude listempty
///
/// List elements can be any legal @LIT{expression} values. Nesting of lists
/// is supported.
/// 
/// @verbinclude listvalues
///
/// @subsection AqlOperators Operators
///
/// AQL supports a number of operators that can be used in expressions. 
/// There are comparison, logical, arithmetic, and string concatenation 
/// operators.
///
/// @subsubsection AqlOperatorsComp Comparison operators
///
/// The following comparison operators are supported:
///
/// - @LIT{==} equality
/// - @LIT{!=} inequality
/// - @LIT{<}  less 
/// - @LIT{<=} less or equal
/// - @LIT{>}  greater
/// - @LIT{>=} greater or equal
/// - @LIT{in} test if a value is contained in a list
///
/// @verbinclude comparison
///
/// Each of the comparison operators returns a boolean value if the comparison 
/// can be evaluated and returns an exact result, or @LIT{undefined} 
/// if the comparison cannot be executed or would return an unknown result.
///
/// When comparing two values of different data types, there will not be any 
/// implicit type conversions. That means it is crucial to either compare values 
/// of the same data type or to use AQL's type casting functions before performing 
/// the comparison. 
///
/// The result of the equality comparison @LIT{==} is true if the two values compared have
/// the same data type and exactly the same value (or values). If the data types differ, 
/// false will be returned. If any of the operands is @LIT{undefined}, 
/// @LIT{undefined} will be returned.
/// 
/// The result of the inequality comparison @LIT{!=} is true if the two values compared have
/// a different data type or have a different value. If one of the operands is 
/// @LIT{undefined}, @LIT{undefined} will be returned.
///
/// For the @LIT{<}, @LIT{<=}, @LIT{>}, and @LIT{>=} comparisons, the following relative
/// order of data types is used:
/// - @LIT{undefined}
/// - @LIT{null}
/// - @LIT{false}
/// - @LIT{true}
/// - numeric values,
/// - string values
/// - lists
/// - documents
///
/// The compared operands are first compared by data type, and only by value if the two
/// operands have the same data type.
///
/// For example, the boolean @LIT{true} value will always be less than any numeric or
/// string value, any list (even an empty list) or any document. Additionally, any string
/// value (even an empty string) will always be greater than any numeric value, a 
/// boolean value, @LIT{true}, or @LIT{false}. Comparing with @LIT{undefined} will result
/// in a value of @LIT{undefined}
///
/// @verbinclude compare1
///
/// Two list values are compared by comparing their individual elements position by
/// position. If a list element itself is a list or a document, then the element's 
/// sub elements are compared recursively.
///
/// @verbinclude compare2
///
/// Documents are compared by comparing their key names and their values. The key names
/// are checked first. If one of the document operands does not have a key that the
/// other operand has, it is considered to be less than the other document. That means
/// the order in which keys are declared in a document is not relevant for ordering.
/// Instead, the key names will always be sorted for the comparison.
///
/// @verbinclude compare3
///
/// The @LIT{in} operator checks whether the left-hand operand is contained in the 
/// right-hand operand. Therefore, the right-hand operand is supposed to be a 
/// list. If the right-hand operand is not of type list, the result will be @LIT{undefined}.
/// The result will also be @LIT{undefined} if the left-hand operand is of type
/// @LIT{undefined}. If the right-hand operand is a list, the list values will be
/// checked one by one and be compared to the left-hand operand. If the left-hand
/// operand is equal to one of the list elements, the result will be @LIT{true}.
/// Otherwise, the result will be @LIT{false}.
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
/// The @LIT{&&} and @LIT{||} operators expect their input operands to be boolean
/// values. They will return @LIT{undefined} if any of the input operands is not a 
/// boolean value. If both input operands are booleans, the result of the operation
/// will also be a boolean.
///
/// Both @LIT{&&} and @LIT{||} use short-circuit evaluation and only
/// evaluate the second operand if the result of the operation cannot be
/// determined by the first operand alone.
///
/// The @LIT{!} operator returns boolean value if the operation input operand is a 
/// boolean value, and @LIT{undefined} if the input operand is not of type boolean.
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
/// These operators work with numeric values only. Invoking any of the operators with
/// non-numeric operands will result in a value of @LIT{undefined}.
/// If both operands are numeric, the result will also be a numeric value, but 
/// @LIT{undefined} if the result value is @LIT{undefined} (e.g. for division by zero)
/// or out of range. 
///
/// The unary plus and unary minus are supported as well.
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
/// result is the @LIT{false-expression}. If the condition result is @LIT{undefined}, 
/// the result of the ternary operator is also @LIT{undefined}
/// 
/// @subsubsection AQLOperatorsOther String concatenation
///
/// String concatenation can be achieved by using the @LIT{concat} function.
/// 
/// @verbinclude concat
///
/// @subsubsection AQLOperatorsPrec Operator precedence
///
/// The operator precedence is as follows (lowest precedence first):
///
/// - @LIT{? :} ternary operator
/// - @LIT{||} logical or
/// - @LIT{&&} logical and
/// - @LIT{==}, @LIT{!=} equality and inequality
/// - @LIT{IN} in operator
/// - @LIT{<}, @LIT{<=}, @LIT{>=}, @LIT{>} less than, less than or equal,
///   greater than or equal, greater than
/// - @LIT{+}, @LIT{-} addition, subtraction
/// - @LIT{*}, @LIT{/}, @LIT{%} multiplication, division, modulus
/// - @LIT{!} logical not
/// - @LIT{-}, @LIT{+} unary minus, unary plus
/// - @LIT{.} member access
///
/// Parentheses (@LIT{(} and @LIT{)}) can be used to enforce a different
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
/// right hand side. The list is produced as a list of complete documents of
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
/// a list of two attribute names, or two separate attribute names, separated by
/// a comma. The attribute names in @LIT{document-coordinate} must refer to the
/// @LIT{collection-alias} specified directly before the geo query. The first
/// attribute is interpreted as the latitude attribute name, the second as the 
/// longitude attribute name.
///
/// The reference coordinate can either be a list of two numbers, or or two 
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
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query
////////////////////////////////////////////////////////////////////////////////

TRI_query_cursor_t* TRI_ExecuteQueryInstance (TRI_query_instance_t* const,
                                              const bool,
                                              const uint32_t);

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
