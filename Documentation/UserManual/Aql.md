ArangoDB's Query Language (AQL) {#Aql}
======================================

@NAVIGATE_Aql
@EMBEDTOC{AqlTOC}

Introduction {#AqlPurpose}
==========================

The ArangoDB query language (AQL) can be used to retrieve data that is stored in
ArangoDB. The general workflow when executing a query is as follows:

- a client application ships an AQL query to the ArangoDB server. The query text
  contains everything ArangoDB needs to compile the result set.
- ArangoDB will parse the query, execute it and compile the results. If the
  query is invalid or cannot be executed, the server will return an error that
  the client can process and react to. If the query can be executed
  successfully, the server will return the query results to the client

AQL is mainly a declarative language, meaning that in a query it is expressed
what result should be achieved and not how. AQL aims to be human- readable and
therefore uses keywords from the English language. Another design goal of AQL
was client independency, meaning that the language and syntax are the same for
all clients, no matter what programming language the clients might use.  Further
design goals of AQL were to support complex query patterns, and to support the
different data models ArangoDB offers.

In its purpose, AQL is similar to the Structured Query Language (SQL), but the
two languages have major syntactic differences. Furthermore, to avoid any
confusion between the two languages, the keywords in AQL have been chosen to be
different from the keywords used in SQL.

AQL currently supports reading data only. That means you can use the language to
issue read-requests on your database, but modifying data via AQL is currently
not supported.

For some example queries, please refer to the page @ref AqlExamples.

How to invoke AQL {#AqlHowToUse}
================================

You can run AQL queries from your application via the HTTP REST API. The full
API description is available at @ref HttpCursor.

You can also run AQL queries from arangosh. To do so, first create an
ArangoStatement object as follows:

    arangosh> stmt = db._createStatement( { "query": "FOR i IN [ 1, 2 ] RETURN i * 2" } );
    [object ArangoStatement]

To execute the query, use the `execute` method:

    arangosh> c = stmt.execute();
    [object ArangoQueryCursor]

This has executed the query. The query results are available in a cursor
now. The cursor can return all its results at once using the `toArray` method.
This is a short-cut that you can use if you want to access the full result
set without iterating over it yourself.

    arangosh> c.toArray();
    [2, 4]
    
Cursors can also be used to iterate over the result set document-by-document.
To do so, use the `hasNext` and `next` methods of the cursor:

    arangosh> while (c.hasNext()) { require("internal").print(c.next()); }
    2
    4

Please note that you can iterate over the results of a cursor only once, and that
the cursor will be empty when you have fully iterated over it. To iterate over
the results again, the query needs to be re-executed.
 
Additionally, the iteration can be done in a forward-only fashion. There is no 
backwards iteration or random access to elements in a cursor.    

To execute an AQL query using bind parameters, you need to create a statement first
and then bind the parameters to it before execution:

    arangosh> stmt = db._createStatement( { "query": "FOR i IN [ @one, @two ] RETURN i * 2" } );
    [object ArangoStatement]
    arangosh> stmt.bind("one", 1);
    arangosh> stmt.bind("two", 2);
    arangosh> c = stmt.execute();
    [object ArangoQueryCursor]

The cursor results can then be dumped or iterated over as usual, e.g.:
    
    arangosh> c.toArray();
    [2, 4]

or 

    arangosh> while (c.hasNext()) { require("internal").print(c.next()); }
    2
    4

Please note that bind variables can also be passed into the `_createStatement` method directly,
making it a bit more convenient:
    
    arangosh> stmt = db._createStatement( { 
      "query": "FOR i IN [ @one, @two ] RETURN i * 2", 
      "bindVars": { 
        "one": 1, 
        "two": 2 
      } 
    } );

Cursors also optionally provide the total number of results. By default, they do not. 
To make the server return the total number of results, you may set the `count` attribute to 
`true` when creating a statement:
    
    arangosh> stmt = db._createStatement( { "query": "FOR i IN [ 1, 2, 3, 4 ] RETURN i", "count": true } );

After executing this query, you can use the `count` method of the cursor to get the 
number of total results from the result set:
    
    arangosh> c = stmt.execute();
    [object ArangoQueryCursor]
    arangosh> c.count();
    4

Please note that the `count` method returns nothing if you did not specifiy the `count`
attribute when creating the query. 

This is intentional so that the server may apply optimisations when executing the query and 
construct the result set incrementally. Incremental creating of the result sets would not be possible
if the total number of results needs to be shipped to the client anyway. Therefore, the client
has the choice to specify `count` and retrieve the total number of results for a query (and
disable potential incremental result set creation on the server), or to not retrieve the total
number of results, and allow the server to apply optimisations.

Please note that at the moment the server will always create the full result set for each query so 
specifying or omitting the `count` attribute currently does not have any impact on query execution.
This might change in the future. Future versions of ArangoDB might create result sets incrementally 
on the server-side and might be able to apply optimisations if a result set is not fully fetched by 
a client.

Query results {#AqlQueryResults}
================================

Result sets {#AqlQueryResultsSet}
---------------------------------

The result of an AQL query is a list of values. The individual values in the
result list may or may not have a homogenuous structure, depending on what is
actually queried.

For example, when returning data from a collection with inhomogenuous documents
(the individual documents in the collection have different attribute names)
without modification, the result values will as well have an inhomogenuous
structure. Each result value itself is a document:

    FOR u IN users
      RETURN u
    
    [ { "id" : 1, "name" : "John", "active" : false }, 
      { "age" : 32, "id" : 2, "name" : "Vanessa" }, 
      { "friends" : [ "John", "Vanessa" ], "id" : 3, "name" : "Amy" } ]

However, if a fixed set of attributes from the collection is queried, then the 
query result values will have a homogenuous structure. Each result value is
still a document:

    FOR u IN users
      RETURN { "id" : u.id, "name" : u.name }
    
    [ { "id" : 1, "name" : "John" }, 
      { "id" : 2, "name" : "Vanessa" }, 
      { "id" : 3, "name" : "Amy" } ]

It is also possible to query just scalar values. In this case, the result set
is a list of scalars, and each result value is a scalar value:

    FOR u IN users
      RETURN u.id
    
    [ 1, 2, 3 ]

If a query does not produce any results because no matching data can be
found, it will produce an empty result list:

    [ ]

Errors {#AqlQueryResultsErrors}
-------------------------------

Issuing an invalid query to the server will result in a parse error if the query
is syntactically invalid. ArangoDB will detect such errors during query
inspection and abort further processing. Instead, the error number and an error
message are returned so that the errors can be fixed.

If a query passes the parsing stage, all collections referenced in the query
will be opened. If any of the referenced collections is not present, query
execution will again be aborted and an appropriate error message will be
returned.

Executing a query might also produce run-time errors under some circumstances
that cannot be predicted from inspecting the query text alone. This is because
queries might use data from collections that might also be inhomogenuous.  Some
examples that will cause run-time errors are:

- division by zero: will be triggered when an attempt is made to use the value
  `0` as the divisor in an arithmetic division or modulus operation
- invalid operands for arithmetic operations: will be triggered when an attempt
  is made to use any non-numeric values as operands in arithmetic operations.
  This includes unary (unary minus, unary plus) and binary operations (plus,
  minus, multiplication, division, and modulus)
- invalid operands for logical operations: will be triggered when an attempt is
  made to use any non-boolean values as operand(s) in logical operations. This
  includes unary (logical not/negation), binary (logical and, logical or), and
  the ternary operators.

Please refer to the @ref ArangoErrors page for a list of error codes and
meanings.

Language basics {#AqlBasics}
============================

Whitespace {#AqlWhitespace}
---------------------------

Whitespace can be used in the query text to increase its readability.  However,
for the parser any whitespace (spaces, carriage returns, line feeds, and tab
stops) does not have any special meaning except that it separates individual
tokens in the query. Whitespace within strings or names must be enclosed in
quotes in order to be preserved.

Comments {#AqlComments}
-----------------------

Comments can be embedded at any position in a query. The text contained in the
comment is ignored by the AQL parser. Comments cannot be nested, meaning
the comment text may not contain another comment.

AQL supports two types of comments:
- single line comments: these start with a double forward slash and end at
  the end of the line, or the end of the query string (whichever is first).
- multi line comments: these start with a forward slash and asterisk, and
  end with an asterik and a following forward slash. They can span as many
  lines as necessary.

@code
/* this is a comment */ RETURN 1
    
/* these */ RETURN /* are */ 1 /* multiple */ + /* comments */ 1

/* this is
   a multi line
   comment */

// a single line comment
@endcode

Keywords {#AqlKeywords}
-----------------------

On the top level, AQL offers the following operations:
- FOR: list iteration
- RETURN: results projection
- FILTER: results filtering
- SORT: result sorting
- LIMIT: result slicing
- LET: variable assignment
- COLLECT: result grouping

Each of the above operations can be initiated in a query by using a keyword of
the same name. An AQL query can (and typically does) consist of multiple of the
above operations.

An example AQL query might look like this:

    FOR u IN users
      FILTER u.type == "newbie" && u.active == true
      RETURN u.name

In this example query, the terms `FOR`, `FILTER`, and `RETURN` initiate the
higher-level operation according to their name. These terms are also keywords,
meaning that they have a special meaning in the language.

For example, the query parser will use the keywords to find out which high-level
operations to execute. That also means keywords can only be used at certains
locations in a query. This also makes all keywords reserved words that must not
be used for other purposes than they are intended for.

For example, it is not possible to use a keyword as a collection or attribute
name. If a collection or attribute need to have the same name as a keyword, the
collection or attribute name needs to be quoted.

Keywords are case-insensitive, meaning they can be specified in lower, upper, or
mixed case in queries. In this documentation, all keywords are written in upper
case to make them distinguishable from other query parts.

In addition to the higher-level operations keywords, there are other keywords.
The current list of keywords is:

- FOR
- RETURN
- FILTER
- SORT
- LIMIT
- LET
- COLLECT
- ASC
- DESC
- IN
- INTO
- NULL
- TRUE
- FALSE

Additional keywords might be added in future versions of ArangoDB. 

Names {#AqlNames}
-----------------

In general, names are used to identify objects (collections, attributes,
variables, and functions) in AQL queries.

The maximum supported length of any name is 64 bytes. Names in AQL are always
case-sensitive.

Keywords must not be used as names. If a reserved keyword should be used as a
name, the name must be enclosed in backticks. Enclosing a name in backticks
allows using otherwise-reserved keywords as names. An example for this is:

    FOR f IN `filter` 
      RETURN f.`sort`

Due to the backticks, `filter` and `sort` are interpreted as names and not as
keywords here.

@subsubsection AqlCollectionNames Collection names

Collection names can be used in queries as they are. If a collection happens to
have the same name as a keyword, the name must be enclosed in backticks.

Please refer to the @ref NamingConventions about collection name naming
conventions.

@subsubsection AqlAttributeNames Attribute names

When referring to attributes of documents from a collection, the fully qualified
attribute name must be used. This is because multiple collections with ambiguous
attribute names might be used in a query.  To avoid any ambiguity, it is not
allowed to refer to an unqualified attribute name.

Please refer to the @ref NamingConventions for more information about the
attribute naming conventions.

    FOR u IN users
      FOR f IN friends
	FILTER u.active == true && f.active == true && u.id == f.userId
	RETURN u.name

In the above example, the attribute names `active`, `name`, `id`, and `userId`
are qualified using the collection names they belong to (`u` and `f`
respectively).

@subsubsection AqlVariableNames Variable names

AQL offers the user to assign values to additional variables in a query.  All
variables that are assigned a value must have a name that is unique within the
context of the query. Variable names must be different from the names of any
collection name used in the same query.

    FOR u IN users
      LET friends = u.friends
      RETURN { "name" : u.name, "friends" : friends }

In the above query, `users` is a collection name, and both `u` and `friends` are
variable names. This is because the `FOR` and `LET` operations need target
variables to store their intermediate results.

Allowed characters in variable names are the letters `a` to `z` (both in lower
and upper case), the numbers `0` to `9` and the underscore (`_`) symbol. A
variable name must not start with a number.  If a variable name starts with the
underscore character, it must also contain at least one letter (a-z or A-Z).

Data types {#AqlTypes}
----------------------

AQL supports both primitive and compound data types. The following types are
available:

- primitive types: consisting of exactly one value
  - null: an empty value, also: the absence of a value
  - bool: boolean truth value with possible values `false` and `true`
  - number: signed (real) number
  - string: UTF-8 encoded text value
- compound types: consisting of multiple values
  - list: sequence of values, referred to by their positions
  - document: sequence of values, referred to by their names

@subsubsection AqlLiteralsNumber Numeric literals

Numeric literals can be integers or real values. They can optionally be signed
using the `+` or `-` symbols. The scientific notation is also supported.

    1
    42
    -1
    -42
    1.23
    -99.99
    0.1
    -4.87e103

All numeric values are treated as 64-bit double-precision values internally.
The internal format used is IEEE 754.

@subsubsection AqlLiteralsString String literals

String literals must be enclosed in single or double quotes. If the used quote
character is to be used itself within the string literal, it must be escaped
using the backslash symbol.  Backslash literals themselves also be escaped using
a backslash.

    "yikes!"
    "don't know"
    "this is a \"quoted\" word"
    "this is a longer string."
    "the path separator on Windows is \\"

    'yikes!'
    'don\'t know'
    'this is a longer string."
    'the path separator on Windows is \\'

All string literals must be UTF-8 encoded. It is currently not possible to use
arbitrary binary data if it is not UTF-8 encoded. A workaround to use binary
data is to encode the data using base64 or other algorithms on the application
side before storing, and decoding it on application side after retrieval.

@subsubsection AqlCompoundLists Lists

AQL supports two compound types:

- lists: a composition of unnamed values, each accessible by their positions
- documents: a composition of named values, each accessible by their names

The first supported compound type is the list type. Lists are effectively
sequences of (unnamed/anonymous) values. Individual list elements can be
accessed by their positions. The order of elements in a list is important.

An `list-declaration` starts with the `[` symbol and ends with the `]` symbol. A
`list-declaration` contains zero or many `expression`s, seperated from each
other with the `,` symbol.

In the easiest case, a list is empty and thus looks like:

    [ ]

List elements can be any legal `expression` values. Nesting of lists is
supported.

    [ 1, 2, 3 ]
    [ -99, "yikes!", [ true, [ "no"], [ ] ], 1 ]
    [ [ "fox", "marshal" ] ] 

Individual list values can later be accesses by their positions using the `[]`
accessor. The position of the accessed element must be a numeric
value. Positions start at 0.  It is also possible to use negative index values
to access list values starting from the end of the list. This is convenient if
the length of the list is unknown and access to elements at the end of the list
is required.

    // access 1st list element (element start at index 0)
    u.friends[0]

    // access 3rd list element
    u.friends[2]

    // access last list element 
    u.friends[-1]

    // access second last list element 
    u.friends[-2]

@subsubsection AqlCompoundDocuments Documents

The other supported compound type is the document type. Documents are a
composition of zero to many attributes. Each attribute is a name/value pair.
Document attributes can be accessed individually by their names.

Document declarations start with the `{` symbol and end with the `}` symbol. A
document contains zero to many attribute declarations, seperated from each other
with the `,` symbol.  In the simplest case, a document is empty. Its
declaration would then be:

    { }

Each attribute in a document is a name/value pair. Name and value of an
attribute are separated using the `:` symbol.

The attribute name is mandatory and must be specified as a quoted or unquoted
string. If a keyword is to be used as an attribute name, the name must be
quoted.

Any valid expression can be used as an attribute value. That also means nested
documents can be used as attribute values

    { name : "Peter" }
    { "name" : "Vanessa", "age" : 15 }
    { "name" : "John", likes : [ "Swimming", "Skiing" ], "address" : { "street" : "Cucumber lane", "zip" : "94242" } }

Individual document attributes can later be accesses by their names using the
`.` accessor. If a non-existing attribute is accessed, the result is `null`.

    u.address.city.name
    u.friends[0].name.first

Bind parameters {#AqlParameters}
--------------------------------

AQL supports the usage of bind parameters, thus allowing to separate the query
text from literal values used in the query. It is good practice to separate the
query text from the literal values because this will prevent (malicious)
injection of keywords and other collection names into an existing query. This
injection would be dangerous because it might change the meaning of an existing
query.

Using bind parameters, the meaning of an existing query cannot be changed.  Bind
parameters can be used everywhere in a query where literals can be used.

The syntax for bind parameters is `@nameparameter` where `nameparameter` is the
actual parameter name. The bind parameter values need to be passed along with
the query when it is executed, but not as part of the query text itself. Please
refer to the @ref HttpCursorHttp manual section for information about how to
pass the bind parameter values to the server.

    FOR u IN users
      FILTER u.id == @id && u.name == @nameparameter
      RETURN u

Bind parameter names must start with any of the letters `a` to `z` (both in
lower and upper case) or a digit (`0` to `9`), and can be followed by any
letter, digit, or the underscore symbol.

A special type of bind parameter exists for injecting collection names. This
type of bind parameter has a name prefixed with an additional `@` symbol (thus
when using the bind parameter in a query, two `@` symbols must be used.

    FOR u IN @@collection
      FILTER u.active == true
	RETURN u

Type and value order {#AqlTypeOrder}
------------------------------------

When checking for equality or inequality or when determining the sort order of
values, AQL uses a deterministic algorithm that takes both the data types and
the actual values into account.

The compared operands are first compared by their data types, and only by their
data values if the operands have the same data types.

The following type order is used when comparing data types:

    null < bool  < number < string < list < document

This means `null` is the smallest type in AQL, and `document` is the type with
the highest order. If the compared operands have a different type, then the
comparison result is determined and the comparison is finished.

For example, the boolean `true` value will always be less than any numeric or
string value, any list (even an empty list) or any document. Additionally, any
string value (even an empty string) will always be greater than any numeric
value, a boolean value, `true`, or `false`.

    null < false
    null < true
    null < 0
    null < ''
    null < ' '
    null < '0'
    null < 'abc'
    null < [ ]
    null < { }

    false < true
    false < 0
    false < ''
    false < ' '
    false < '0'
    false < 'abc'
    false < [ ]
    false < { }

    true < 0
    true < ''
    true < ' '
    true < '0'
    true < 'abc'
    true < [ ]
    true < { }

    0 < ''
    0 < ' '
    0 < '0'
    0 < 'abc'
    0 < [ ]
    0 < { }

    '' < ' '
    '' < '0'
    '' < 'abc'
    '' < [ ]
    '' < { }

    [ ] < { }

If the two compared operands have the same data types, then the operands values
are compared. For the primitive types (null, boolean, number, and string), the
result is defined as follows:

- null: `null` is equal to `null`
- boolean:`false` is less than `true`
- number: numeric values are ordered by their cardinal value
- string: string values are ordered using a localized comparison,
  see @ref CommandLineDefaultLanguage "--default-language"

Note: unlike in SQL, `null` can be compared to any value, including `null`
itself, without the result being converted into `null` automatically.

For compound, types the following special rules are applied:

Two list values are compared by comparing their individual elements position by
position, starting at the first element. For each position, the element types
are compared first. If the types are not equal, the comparison result is
determined, and the comparison is finished. If the types are equal, then the
values of the two elements are compared.  If one of the lists is finished and
the other list still has an element at a compared position, then `null` will be
used as the element value of the fully traversed list.

If a list element is itself a compound value (a list or a document), then the
comparison algorithm will check the element's sub values recursively.  element's
sub elements are compared recursively.

    [ ] < [ 0 ]
    [ 1 ] < [ 2 ]
    [ 1, 2 ] < [ 2 ]
    [ 99, 99 ] < [ 100 ]
    [ false ] < [ true ]
    [ false, 1 ] < [ false, '' ]

Two documents operands are compared by checking attribute names and value. The
attribute names are compared first. Before attribute names are compared, a
combined list of all attribute names from both operands is created and sorted
lexicographically.  This means that the order in which attributes are declared
in a document is not relevant when comparing two documents.

The combined and sorted list of attribute names is then traversed, and the
respective attributes from the two compared operands are then looked up. If one
of the documents does not have an attribute with the sought name, its attribute
value is considered to be `null`.  Finally, the attribute value of both
documents is compared using the beforementioned data type and value comparison.
The comparisons are performed for all document attributes until there is an
unambiguous comparison result. If an unambiguous comparison result is found, the
comparison is finished. If there is no unambiguous comparison result, the two
compared documents are considered equal.

    { } < { "a" : 1 }
    { } < { "a" : null }
    { "a" : 1 } < { "a" : 2 }
    { "b" : 1 } < { "a" : 0 }
    { "a" : { "c" : true } } < { "a" : { "c" : 0 } }
    { "a" : { "c" : true, "a" : 0 } } < { "a" : { "c" : false, "a" : 1 } }

    { "a" : 1, "b" : 2 } == { "b" : 2, "a" : 1 }

Accessing data from collections {#AqlData}
------------------------------------------

Collection data can be accessed by specifying a collection name in a query.  A
collection can be understood as a list of documents, and that is how they are
treated in AQL. Documents from collections are normally accessing using the
`FOR` keyword. Note that when iterating over documents from a collection, the
order of documents is undefined. To traverse documents in an explicit and
deterministic order, the `SORT` keyword should be used in addition.

Data in collections is stored in documents, with each document potentially
having different attributes than other documents. This is true even for
documents of the same collection.

It is therefore quite normal to encounter documents that do not have some or all
of the attributes that are queried in an AQL query. In this case, the
non-existing attributes in the document will be treated as if they would exist
with a value of `null`.  That means that comparing a document attribute to
`null` will return true if the document has the particular attribute and the
attribute has a value of `null`, or that the document does not have the
particular attribute at all.

For example, the following query will return all documents from the collection
`users` that have a value of `null` in the attribute `name`, plus all documents
from `users` that do not have the `name` attribute at all:

    FOR u IN users
      FILTER u.name == null
      RETURN u

Furthermore, `null` is less than any other value (excluding `null` itself). That
means documents with non-existing attributes might be included in the result
when comparing attribute values with the less than or less equal operators.

For example, the following query with return all documents from the collection
`users` that have an attribute `age` with a value less than `39`, but also all
documents from the collection that do not have the attribute `age` at all.

    FOR u IN users
      FILTER u.age < 39
      RETURN u

This behavior should always be taken into account when writing queries.

Operators {#AqlOperators}
-------------------------

AQL supports a number of operators that can be used in expressions.  There are
comparison, logical, arithmetic, and the ternary operator.

@subsubsection AqlOperatorsComparison Comparison operators

Comparison (or relational) operators compare two operands. They can be used with
any input data types, and will return a boolean result value.

The following comparison operators are supported:

- `==` equality
- `!=` inequality
- `<`  less than 
- `<=` less or equal
- `>`  greater than
- `>=` greater or equal
- `in` test if a value is contained in a list

The `in` operator expects the second operand to be of type list. All other
operators accept any data types for the first and second operands.

Each of the comparison operators returns a boolean value if the comparison can
be evaluated and returns `true` if the comparison evaluates to true, and `false`
otherwise.

Some examples for comparison operations in AQL:

    1 > 0
    true != null
    45 <= "yikes!"
    65 != "65"
    65 == 65
    1.23 < 1.32
    1.5 IN [ 2, 3, 1.5 ]

@subsubsection AqlOperatorsLogical Logical operators

Logical operators combine two boolean operands in a logical operation and return
a boolean result value.

The following logical operators are supported:

- `&&` logical and operator
- `||` logical or operator
- `!` logical not/negation operator

Some examples for logical operations in AQL:

    u.age > 15 && u.address.city != ""
    true || false
    !u.isInvalid

The `&&`, `||`, and `!` operators expect their input operands to be boolean
values each. If a non-boolean operand is used, the operation will fail with an
error. In case all operands are valid, the result of each logical operator is a
boolean value.

Both the `&&` and `||` operators use short-circuit evaluation and only evaluate
the second operand if the result of the operation cannot be determined by
checking the first operand alone.

@subsubsection AqlOperatorsArithmetic Arithmetic operators

Arithmetic operators perform an arithmetic operation on two numeric
operands. The result of an arithmetic operation is again a numeric value.
operators are supported:

AQL supports the following arithmetic operators:

- `+` addition
- `-` subtraction
- `*` multiplication
- `/` division
- `%` modulus

These operators work with numeric operands only. Invoking any of the operators
with non-numeric operands will result in an error. An error will also be raised
for some other edge cases as division by zero, numeric over- or underflow etc.
If both operands are numeric and the computation result is also valid, the
result will be returned as a numeric value.

The unary plus and unary minus are supported as well.

Some example arithmetic operations:

    1 + 1
    33 - 99
    12.4 * 4.5
    13.0 / 0.1
    23 % 7
    -15
    +9.99

@subsubsection AQLOperatorTernary Ternary operator

AQL also supports a ternary operator that can be used for conditional
evaluation. The ternary operator expects a boolean condition as its first
operand, and it returns the result of the second operand if the condition
evaluates to true, and the third operand otherwise.

Example:

    u.age > 15 || u.active == true ? u.userId : null

@subsubsection AQLOperatorRange Range operator

AQL supports expressing simple numeric ranges with the operator `..`.
This operator can be used to easily iterate over a sequence of numeric
values.    

The `..` operator will produce a list of values in the defined range, with 
both bounding values included.

Example:

    2010..2013

will produce the following result:

    [ 2010, 2011, 2012, 2013 ]

@subsubsection AQLOperatorsPrecedence Operator precedence

The operator precedence in AQL is as follows (lowest precedence first):

- `? :` ternary operator
- `||` logical or
- `&&` logical and
- `==`, `!=` equality and inequality
- `in` in operator
- `<`, `<=`, `>=`, `>` less than, less equal,
  greater equal, greater than
- `+`, `-` addition, subtraction
- `*`, `/`, `%` multiplication, division, modulus
- `!`, `+`, `-` logical negation, unary plus, unary minus
- `[*]` expansion
- `()` function call
- `.` member access
- `[]` indexed value access

The parentheses `(` and `)` can be used to enforce a different operator
evaluation order.

Functions {#AqlFunctions}
-------------------------

AQL supports functions to allow more complex computations. Functions can be
called at any query position where an expression is allowed. The general
function call syntax is:

    FUNCTIONNAME(arguments)

where `FUNCTIONNAME` is the name of the function to be called, and `arguments`
is a comma-separated list of function arguments. If a function does not need any
arguments, the argument list can be left empty. However, even if the argument
list is empty the parentheses around it are still mandatory to make function
calls distinguishable from variable names.

Some example function calls:

    HAS(user, "name")
    LENGTH(friends)
    COLLECTIONS()

In contrast to collection and variable names, function names are case-insensitive, 
i.e. `LENGTH(foo)` and `length(foo)` are equivalent.

@subsubsection AqlFunctionsExtending Extending AQL
 
Since ArangoDB 1.3, it is possible to extend AQL with user-defined functions. 
These functions need to be written in Javascript, and be registered before usage
in a query.

Please refer to @ref ExtendingAql for more details on this.

By default, any function used in an AQL query will be sought in the built-in 
function namespace `_aql`. This is the default namespace that contains all AQL
functions that are shipped with ArangoDB. 
To refer to a user-defined AQL function, the function name must be fully qualified
to also include the user-defined namespace. The `::` symbol is used as the namespace
separator:

    MYGROUP::MYFUNC()

    MYFUNCTIONS::MATH::RANDOM()
    
As all AQL function names, user function names are also case-insensitive.


@subsubsection AqlFunctionsCasting Type cast functions

As mentioned before, some of the operators expect their operands to have a
certain data type. For example, the logical operators expect their operands to
be boolean values, and the arithmetic operators expect their operands to be
numeric values.  If an operation is performed with operands of an unexpect type,
the operation will fail with an error. To avoid such failures, value types can
be converted explicitly in a query. This is called type casting.

In an AQL query, type casts are performed only upon request and not implicitly.
This helps avoiding unexpected results. All type casts have to be performed by
invoking a type cast function. AQL offers several type cast functions for this
task. Each of the these functions takes an operand of any data type and returns
a result value of type corresponding to the function name (e.g. `TO_NUMBER()`
will return a number value):

- @FN{TO_BOOL(@FA{value})}: takes an input @FA{value} of any type and converts it 
  into the appropriate boolean value as follows:
  - `null` is converted to `false`.
  - Numbers are converted to `true` if they are unequal to 0, and to `false` otherwise. 
  - Strings are converted to `true` if they are non-empty, and to `false` otherwise. 
  - Lists are converted to `true` if they are non-empty, and to `false` otherwise.
  - Documents are converted to `true` if they are non-empty, and to `false` otherwise.

- @FN{TO_NUMBER(@FA{value})}: takes an input @FA{value} of any type and converts it 
  into a numeric value as follows:
  - `null`, `false`, lists, and documents are converted to the value `0`.
  - `true` is converted to `1`.
  - Strings are converted to their numeric equivalent if the full string content is
    is a valid number, and to `0` otherwise.

- @FN{TO_STRING(@FA{value})}: takes an input @FA{value} of any type and converts it 
  into a string value as follows:
  - `null` is converted to the string `"null"`
  - `false` is converted to the string `"false"`, `true` to the string `"true"`
  - numbers, lists, and documents are converted to their string equivalents. 

- @FN{TO_LIST(@FA{value})}: takes an input @FA{value} of any type and converts it 
  into a list value as follows:
  - `null` is converted to an empty list
  - Boolean values, numbers, and strings are converted to a list containing the original
    value as its single element
  - Documents are converted to a list containing their attribute values as list elements

@subsubsection AqlFunctionsChecking Type check functions

AQL also offers functions to check the data type of a value at runtime. The
following type check functions are available. Each of these functions takes an
argument of any data type and returns true if the value has the type that is
checked for, and false otherwise.

The following type check functions are available:

- @FN{IS_NULL(@FA{value})}: checks whether @FA{value} is a `null` value

- @FN{IS_BOOL(@FA{value})}: checks whether @FA{value} is a boolean value

- @FN{IS_NUMBER(@FA{value})}: checks whether @FA{value} is a numeric value

- @FN{IS_STRING(@FA{value})}: checks whether @FA{value} is a string value

- @FN{IS_LIST(@FA{value})}: checks whether @FA{value} is a list value

- @FN{IS_DOCUMENT(@FA{value})}: checks whether @FA{value} is a document value

@subsubsection AqlFunctionsString String functions

For string processing, AQL offers the following functions:

- @FN{CONCAT(@FA{value1}, @FA{value2}, ... @FA{valuen})}: concatenate the strings 
  passed as in @FA{value1} to @FA{valuen}. `null` values are ignored.

- @FN{CONCAT_SEPARATOR(@FA{separator}, @FA{value1}, @FA{value2}, ... @FA{valuen})}: 
  concatenate the strings passed as arguments @FA{value1} to @FA{valuen} using the 
  @FA{separator} string. `null` values are ignored.

- @FN{CHAR_LENGTH(@FA{value})}: return the number of characters in @FA{value}. This is
  a synonym for @FN{LENGTH(@FA{value})}.

- @FN{LOWER(@FA{value})}: lower-case @FA{value}

- @FN{UPPER(@FA{value})}: upper-case @FA{value}

- @FN{SUBSTRING(@FA{value}, @FA{offset}, @FA{length})}: return a substring of @FA{value},
  starting at @FA{offset} and with a maximum length of @FA{length} characters. Offsets
  start at position 0.

- @FN{LEFT(@FA{value}, @FA{LENGTH})}: returns the @FA{LENGTH} leftmost characters of
  the string @FA{VALUE}.

- @FN{RIGHT(@FA{value}, @FA{LENGTH})}: returns the @FA{LENGTH} rightmost characters of
  the string @FA{VALUE}.

- @FN{TRIM(@FA{value}, @FA{type})}: returns the string @FA{VALUE} with whitespace stripped 
  from the start and/or end. The optional @FA{type} parameter specifies from which parts
  of the string the whitespace is stripped:
  - @FA{type} 0 will strip whitespace from the start and end of the string
  - @FA{type} 1 will strip whitespace from the start of the string only
  - @FA{type} 2 will strip whitespace from the end of the string only

- @FN{REVERSE(@FA{value})}: returns the reverse of the string @FA{value}.

- @FN{CONTAINS(@FA{text}, @FA{search}, @FA{return-index})}: checks whether the string
  @FA{search} is contained in the string @FA{text}. By default, this function returns 
  `true` if @FA{search} is contained in @FA{text}, and `false` otherwise. By
  passing `true` as the third function parameter @FA{return-index}, the function
  will return the position of the first occurence of @FA{search} within @FA{text}, 
  starting at offset 0, or `-1` if @FA{search} is not contained in @FA{text}.

  The string matching performed by @FN{CONTAINS} is case-sensitive.

- @FN{LIKE(@FA{text}, @FA{search}, @FA{case-insensitive})}: checks whether the pattern
  @FA{search} is contained in the string @FA{text}, using wildcard matching. 
  Returns `true` if the pattern is contained in @FA{text}, and `false` otherwise. 
  The @FA{pattern} string can contain the wildcard characters `%` (meaning any
  sequence of characters) and `_` (any single character).

  The string matching performed by @FN{LIKE} is case-sensitive by default, but by
  passing `true` as the third parameter, the matching will be case-insensitive.

  The value for @FA{search} cannot be a variable or a document attribute. The actual 
  value must be present at query parse time already.

@subsubsection AqlFunctionsNumeric Numeric functions

AQL offers some numeric functions for calculations. The following functions are
supported:

- @FN{FLOOR(@FA{value})}: returns the integer closest but not greater to @FA{value}

- @FN{CEIL(@FA{value})}: returns the integer closest but not less than @FA{value}

- @FN{ROUND(@FA{value})}: returns the integer closest to @FA{value}

- @FN{ABS(@FA{value})}: returns the absolute part of @FA{value}

- @FN{SQRT(@FA{value})}: returns the square root of @FA{value}

- @FN{RAND()}: returns a pseudo-random number between 0 and 1

@subsubsection AqlFunctionsList List functions

AQL supports the following functions to operate on list values:

- @FN{LENGTH(@FA{list})}: returns the length (number of list elements) of @FA{list}. If 
  @FA{list} is a document, returns the number of attribute keys of the document, 
  regardless of their values.

- @FN{MIN(@FA{list})}: returns the smallest element of @FA{list}. `null` values
  are ignored. If the list is empty or only `null` values are contained in the list, the
  function will return `null`.

- @FN{MAX(@FA{list})}: returns the greatest element of @FA{list}. `null` values
  are ignored. If the list is empty or only `null` values are contained in the list, the
  function will return `null`.

- @FN{AVERAGE(@FA{list})}: returns the average (arithmetic mean) of the values in @FA{list}. 
  This requires the elements in @FA{list} to be numbers. `null` values are ignored. 
  If the list is empty or only `null` values are contained in the list, the function 
  will return `null`.

- @FN{SUM(@FA{list})}: returns the sum of the values in @FA{list}. This
  requires the elements in @FA{list} to be numbers. `null` values are ignored. 

- @FN{MEDIAN(@FA{list})}: returns the median value of the values in @FA{list}. This 
  requires the elements in @FA{list} to be numbers. `null` values are ignored. If the 
  list is empty or only `null` values are contained in the list, the function will return 
  `null`.

- @FN{VARIANCE_POPULATION(@FA{list})}: returns the population variance of the values in 
  @FA{list}. This requires the elements in @FA{list} to be numbers. `null` values 
  are ignored. If the list is empty or only `null` values are contained in the list, 
  the function will return `null`.

- @FN{VARIANCE_SAMPLE(@FA{list})}: returns the sample variance of the values in 
  @FA{list}. This requires the elements in @FA{list} to be numbers. `null` values 
  are ignored. If the list is empty or only `null` values are contained in the list, 
  the function will return `null`.

- @FN{STDDEV_POPULATION(@FA{list})}: returns the population standard deviation of the 
  values in @FA{list}. This requires the elements in @FA{list} to be numbers. `null` 
  values are ignored. If the list is empty or only `null` values are contained in the list, 
  the function will return `null`.

- @FN{STDDEV_SAMPLE(@FA{list})}: returns the sample standard deviation of the values in 
  @FA{list}. This requires the elements in @FA{list} to be numbers. `null` values 
  are ignored. If the list is empty or only `null` values are contained in the list, 
  the function will return `null`.

- @FN{REVERSE(@FA{list})}: returns the elements in @FA{list} in reversed order.

- @FN{FIRST(@FA{list})}: returns the first element in @FA{list} or `null` if the
  list is empty.

- @FN{LAST(@FA{list})}: returns the last element in @FA{list} or `null` if the
  list is empty.

- @FN{UNIQUE(@FA{list})}: returns all unique elements in @FA{list}. To determine
  uniqueness, the function will use the comparison order defined in @ref AqlTypeOrder.
  Calling this function might return the unique elements in any order.

- @FN{UNION(@FA{list1, list2, ...})}: returns the union of all lists specified.
  The function expects at least two list values as its arguments. The result is a list
  of values in an undefined order.

  Note: no duplicates will be removed. In order to remove duplicates, please use the
  @LIT{UNIQUE} function.

  Example:
    RETURN UNION(
      [ 1, 2, 3 ],
      [ 1, 2 ]
    )

  will produce:
    [ [ 1, 2, 3, 1, 2 ] ]

  with duplicate removal:

    RETURN UNIQUE(
      UNION(
        [ 1, 2, 3 ],
        [ 1, 2 ]
      )
    )
  
  will produce:
    [ [ 1, 2, 3 ] ]

- @FN{INTERSECTION(@FA{list1, list2, ...})}: returns the intersection of all lists specified.
  The function expects at least two list values as its arguments.
  The result is a list of values that occur in all arguments. The order of the result list
  is undefined and should not be relied on.
  Note: duplicates will be removed.


Apart from these functions, AQL also offers several language constructs (e.g.
`FOR`, `SORT`, `LIMIT`, `COLLECT`) to operate on lists.

@subsubsection AqlFunctionsDocument Document functions

AQL supports the following functions to operate on document values:

- @FN{MATCHES(@FA{document}, @FA{examples}, @FA{return-index})}: compares the document
  @FA{document} against each example document provided in the list @FA{examples}. 
  If @FA{document} matches one of the examples, `true` is returned, and if there is
  no match `false` will be returned. The default return value type can be changed by
  passing `true` as the third function parameter @FA{return-index}. Setting this
  flag will return the index of the example that matched (starting at offset 0), or 
  `-1` if there was no match.

  The comparisons will be started with the first example. All attributes of the example
  will be compared against the attributes of @FA{document}. If all attributes match, the 
  comparison stops and the result is returned. If there is a mismatch, the function will
  continue the comparison with the next example until there are no more examples left.

  The @FA{examples} must be a list of 1..n example documents, with any number of attributes
  each. Note: specifying an empty list of examples is not allowed.
   
  Example usage:

    RETURN MATCHES({ "test" : 1 }, [ 
      { "test" : 1, "foo" : "bar" }, 
      { "foo" : 1 }, 
      { "test : 1 } 
    ], true)

  This will return `2`, because the third example matches, and because the 
  `return-index` flag is set to `true`.

- @FN{MERGE(@FA{document1}, @FA{document2}, ... @FA{documentn})}: merges the documents
  in @FA{document1} to @FA{documentn} into a single document. If document attribute
  keys are ambiguous, the merged result will contain the values of the documents 
  contained later in the argument list.

  For example, two documents with distinct attribute names can easily be merged into one: 

    RETURN MERGE(
      { "user1" : { "name" : "J" } }, 
      { "user2" : { "name" : "T" } }
    )
    [ { "user1" : { "name" : "J" }, 
	"user2" : { "name" : "T" } } ]

  When merging documents with identical attribute names, the attribute values of the
  latter documents will be used in the end result:

    return MERGE(
      { "users" : { "name" : "J" } }, 
      { "users" : { "name" : "T" } }
    )
    [ { "users" : { "name" : "T" } } ]

  Please note that merging will only be done for top-level attributes. If you wish to
  merge sub-attributes, you should consider using `MERGE_RECURSIVE` instead.

- @FN{MERGE_RECURSIVE(@FA{document1}, @FA{document2}, ... @FA{documentn})}: recursively
  merges the documents in @FA{document1} to @FA{documentn} into a single document. If 
  document attribute keys are ambiguous, the merged result will contain the values of the 
  documents contained later in the argument list.

  For example, two documents with distinct attribute names can easily be merged into one: 

    RETURN MERGE_RECURSIVE(
      { "user-1" : { "name" : "J", "livesIn" : { "city" : "LA" } } }, 
      { "user-1" : { "age" : 42, "livesIn" : { "state" : "CA" } } }
    )
    [ { "user-1" : { "name" : "J", "livesIn" : { "city" : "LA", "state" : "CA" }, "age" : 42 } } ]


- @FN{HAS(@FA{document}, @FA{attributename})}: returns `true` if @FA{document} has an
  attribute named @FA{attributename}, and `false` otherwise.

- @FN{ATTRIBUTES(@FA{document}, @FA{removeInternal}, @FA{sort})}: returns the attribute
  names of the document @FA{document} as a list. 
  If @FA{removeInternal} is set to `true`, then all internal attributes (such as `_id`, 
  `_key` etc.) are removed from the result. If @FA{sort} is set to `true`, then the
  attribute names in the result will be sorted. Otherwise they will be returned in any order.

- @FN{UNSET(@FA{document}, @FA{attributename}, ...)}: removes the attributes @FA{attributename}
  (can be one or many) from @FA{document}. All other attributes will be preserved.
  Multiple attribute names can be specified by either passing multiple individual string argument 
  names, or by passing a list of attribute names:

    RETURN UNSET(doc, '_id', '_key', [ 'foo', 'bar' ])

- @FN{KEEP(@FA{document}, @FA{attributename}, ...)}: keeps only the attributes @FA{attributename}
  (can be one or many) from @FA{document}. All other attributes will be removed from the result.
  Multiple attribute names can be specified by either passing multiple individual string argument 
  names, or by passing a list of attribute names:

    RETURN KEEP(doc, 'firstname', 'name', 'likes')

@subsubsection AqlFunctionsGeo Geo functions

AQL offers the following functions to filter data based on geo indexes:

- @FN{NEAR(@FA{collection}, @FA{latitude}, @FA{longitude}, @FA{limit}, @FA{distancename})}: 
  returns at most @FA{limit} documents from collection @FA{collection} that are near
  @FA{latitude} and @FA{longitude}. The result contains at most @FA{limit} documents, returned in
  any order. If more than @FA{limit} documents qualify, it is undefined which of the qualifying
  documents are returned. Optionally, the distances between the specified coordinate
  (@FA{latitude} and @FA{longitude}) and the document coordinates can be returned as well.
  To make use of that, an attribute name for the distance result has to be specified in
  the @FA{distancename} argument. The result documents will contain the distance value in
  an attribute of that name.
  @FA{limit} is an optional parameter since ArangoDB 1.3. If it is not specified or null, a limit
  value of 100 will be applied.

- @FN{WITHIN(@FA{collection}, @FA{latitude}, @FA{longitude}, @FA{radius}, @FA{distancename})}: 
  returns all documents from collection @FA{collection} that are within a radius of
  @FA{radius} around that specified coordinate (@FA{latitude} and @FA{longitude}). The order
  in which the result documents are returned is undefined. Optionally, the distance between the
  coordinate and the document coordinates can be returned as well.
  To make use of that, an attribute name for the distance result has to be specified in
  the @FA{distancename} argument. The result documents will contain the distance value in
  an attribute of that name.

Note: these functions require the collection @FA{collection} to have at least
one geo index.  If no geo index can be found, calling this function will fail
with an error.

@subsubsection AqlFunctionsFulltext Fulltext functions

AQL offers the following functions to filter data based on fulltext indexes:

- @FN{FULLTEXT(@FA{collection}, @FA{attribute}, @FA{query})}: 
  returns all documents from collection @FA{collection} for which the attribute @FA{attribute}
  matches the fulltext query @FA{query}.
  @FA{query} is a comma-separated list of sought words (or prefixes of sought words). To 
  distinguish between prefix searches and complete-match searches, each word can optionally be
  prefixed with either the `prefix:` or `complete:` qualifier. Different qualifiers can
  be mixed in the same query. Not specifying a qualifier for a search word will implicitly
  execute a complete-match search for the given word:

  - `FULLTEXT(emails, "body", "banana")` will look for the word `banana` in the 
    attribute `body` of the collection `collection`.

  - `FULLTEXT(emails, "body", "banana,orange")` will look for boths the words 
    `banana` and `orange` in the mentioned attribute. Only those documents will be
    returned that contain both words.

  - `FULLTEXT(emails, "body", "prefix:head")` will look for documents that contain any
    words starting with the prefix `head`.

  - `FULLTEXT(emails, "body", "prefix:head,complete:aspirin")` will look for all 
    documents that contain a word starting with the prefix `head` and that also contain 
    the (complete) word `aspirin`. Note: specifying `complete` is optional here.

  - `FULLTEXT(emails, "body", "prefix:cent,prefix:subst")` will look for all documents 
    that contain a word starting with the prefix `cent` and that also contain a word
    starting with the prefix `subst`.

  If multiple search words (or prefixes) are given, then by default the results will be 
  AND-combined, meaning only the logical intersection of all searches will be returned. 
  It is also possible to combine partial results with a logical OR, and with a logical NOT:

  - `FULLTEXT(emails, "body", "+this,+text,+document")` will return all documents that 
    contain all the mentioned words. Note: specifying the `+` symbols is optional here.

  - `FULLTEXT(emails, "body", "banana,|apple")` will return all documents that contain
    either (or both) words `banana` or `apple`.

  - `FULLTEXT(emails, "body", "banana,-apple")` will return all documents that contain
    the word `banana` but do not contain the word `apple`.

  - `FULLTEXT(emails, "body", "banana,pear,-cranberry")` will return all documents that 
    contain both the words `banana` and `pear` but do not contain the word 
    `cranberry`.

  No precedence of logical operators will be honored in a fulltext query. The query will simply
  be evaluated from left to right.
  
Note: the `FULLTEXT` function requires the collection @FA{collection} to have a
fulltext index on `attribute`. If no fulltext index is available, this function
will fail with an error.

@subsubsection AqlFunctionsGraph Graph functions

AQL has the following functions to traverse graphs:

- @FN{PATHS(@FA{vertexcollection}, @FA{edgecollection}, @FA{direction}, @FA{followcycles})}: 
  returns a list of paths through the graph defined by the nodes in the collection 
  @FA{vertexcollection} and edges in the collection @FA{edgecollection}. For each vertex
  in @FA{vertexcollection}, it will determine the paths through the graph depending on the
  value of @FA{direction}:
  - `"outbound"`: follow all paths that start at the current vertex and lead to another vertex
  - `"inbound"`: follow all paths that lead from another vertex to the current vertex
  - `"any"`: combination of `"outbound"` and `"inbound"`.
  The default value for @FA{direction} is `"outbound"`.
  If @FA{followcycles} is true, cyclic paths will be followed as well. This is turned off by
  default.

  The result of the function is a list of paths. Paths of length 0 will also be returned. Each 
  path is a document consisting of the following attributes:
  - `vertices`: list of vertices visited along the path
  - `edges`: list of edges visited along the path (might be empty)
  - `source`: start vertex of path
  - `destination`: destination vertex of path

Example calls:

    PATHS(friends, friendrelations, "outbound", false)

    FOR p IN PATHS(friends, friendrelations, "outbound") 
      FILTER p.source._id == "123456/123456" && LENGTH(p.edges) == 2
      RETURN p.vertices[*].name

- @FN{TRAVERSAL(@FA{vertexcollection}, @FA{edgecollection}, @FA{startVertex}, @FA{direction}, @FA{options})}: 
  traverses the graph described by @FA{vertexcollection} and @FA{edgecollection}, 
  starting at the vertex identified by id @FA{startVertex}. Vertex connectivity is
  specified by the @FA{direction} parameter:
  - `"outbound"`: vertices are connected in `_from` to `_to` order
  - `"inbound"`: vertices are connected in `_to` to `_from` order
  - `"any"`: vertices are connected in both `_to` to `_from` and in 
    `_from` to `_to` order

  Additional options for the traversal can be provided via the @FA{options} document:
  - `strategy`: defines the traversal strategy. Possible values are `depthfirst` 
    and `breadthfirst`. Defaults to `depthfirst`
  - `order`: defines the traversal order: Possible values are `preorder` and
    `postorder`. Defaults to `preorder`
  - `itemOrder`: Defines the level item order. Can be `forward` or 
    `backward`. Defaults to `forward`
  - `minDepth`: Minimum path depths for vertices to be included. This can be used to
    include only vertices in the result that are found after a certain minimum depth.
    Defaults to 0. 
  - `maxIterations`: Maximum number of iterations in each traversal. This number can be
    set to prevent endless loops in traversal of cyclic graphs. When a traversal performs
    as many iterations as the `maxIterations` value, the traversal will abort with an
    error. If `maxIterations` is not set, a server-defined value may be used.
  - `maxDepth`: Maximum path depth for sub-edges expansion. This can be used to 
    limit the depth of the traversal to a sensible amount. This should especially be used
    for big graphs to limit the traversal to some sensible amount, and for graphs 
    containing cycles to prevent infinite traversals. The maximum depth defaults to 256, 
    with the chance of this value being non-sensical. For several graphs, a much lower
    maximum depth is sensible, whereas for other, more list-oriented graphs a higher
    depth should be used.
  - `paths`: if `true`, the paths encountered during the traversal will
    also be returned along with each traversed vertex. If `false`, only the 
    encountered vertices will be returned.
  - `uniqueness`: an optional document with the following properties:
    - `vertices`: 
      - `none`: no vertex uniqueness is enforced
      - `global`: a vertex may be visited at most once. This is the default.
      - `path`: a vertex is visited only if not already contained in the current
        traversal path
    - `edges`: 
      - `none`: no edge uniqueness is enforced
      - `global`: an edge may be visited at most once. This is the default.
      - `path`: an edge is visited only if not already contained in the current
        traversal path
  - `followEdges`: an optional list of example edge documents that the traversal will
    expand into. If no examples are given, the traversal will follow all edges. If one
    or many edge examples are given. The traversal will only follow an edge if it matches
    at least one of the specified examples.

  The result of the TRAVERSAL function is a list of traversed points. Each point is a 
  document consisting of the following properties:
  - `vertex`: the vertex at the traversal point
  - `path`: The path history for the traversal point. The path is a document with the
    properties `vertices` and `edges`, which are both lists.

Example calls:

    TRAVERSAL(friends, friendrelations, "friends/john", "outbound", {
      strategy: "depthfirst",
      order: "postorder",
      itemOrder: "backward",
      maxDepth: 6,
      trackPaths: true
    })

    TRAVERSAL(friends, friendrelations, "friends/john", "outbound", {
      strategy: "breadthfirst",
      order: "preorder",
      itemOrder: "forward",
      followEdges: [ { type: "knows" }, { state: "FL" } ]
    })

- @FN{TRAVERSAL_TREE(@FA{vertexcollection}, @FA{edgecollection}, @FA{startVertex}, @FA{direction}, @FA{connectName}, @FA{options})}: 
  traverses the graph described by @FA{vertexcollection} and @FA{edgecollection}, 
  starting at the vertex identified by id @FA{startVertex} and creates a hierchical result.
  Vertex connectivity is establish by inserted an attribute which has the name specified via
  the @FA{connectName} parameter. Connected vertices will be placed in this attribute as a 
  list.

  The @FA{options} are the same as for the `TRAVERSAL` function, except that the result will
  be set up in a way that resembles a depth-first, pre-order visitation result. Thus, the
  `strategy` and `order` attributes of the @FA{options} attribute will be ignored.

Example calls:

    TREE(friends, friendrelations, "friends/john", "outbound", "likes", { 
      itemOrder: "forward"
    })

When using one of AQL's graph functions please make sure that the graph does not contain cycles,
or that you at least specify some maximum depth or uniqueness criteria for a traversal. 

If no bounds are set, a traversal might run into an endless loop in a cyclic graph or sub-graph,
and even in a non-cyclic graph, traversing far into the graph might consume a lot of processing
time and memory for the result set.

- @FN{EDGES(@FA{edgecollection}, @FA{startvertex}, @FA{direction}, @FA{edgeexamples})}:
  return all edges connected to the vertex @FA{startvertex} as a list. The possible values for
  @FA{direction} are:
  - `outbound`: return all outbound edges
  - `inbound`: return all inbound edges
  - `any`: return outbound and inbound edges
  
  The @FA{edgeexamples} parameter can optionally be used to restrict the results to specific
  edge connections only. The matching is then done via the @LIT{MATCHES} function.
  To not restrict the result to specific connections, @FA{edgeexamples} should be left
  unspecified. 

Example calls:

    EDGES(friendrelations, "friends/john", "outbound")
    EDGES(friendrelations, "friends/john", "any", [ { "$label": "knows" } ])

- @FN{NEIGHBORS(@FA{vertexcollection}, @FA{edgecollection}, @FA{startvertex}, @FA{direction}, @FA{edgeexamples})}:
  return all neighbors that are directly connected to the vertex @FA{startvertex} as a list. 
  The possible values for @FA{direction} are:
  - `outbound`: return all outbound edges
  - `inbound`: return all inbound edges
  - `any`: return outbound and inbound edges

  The @FA{edgeexamples} parameter can optionally be used to restrict the results to specific
  edge connections only. The matching is then done via the @LIT{MATCHES} function.
  To not restrict the result to specific connections, @FA{edgeexamples} should be left
  unspecified.

Example calls:

    NEIGHBORS(friends, friendrelations, "friends/john", "outbound")
    NEIGHBORS(users, usersrelations, "users/john", "any", [ { "$label": "recommends" } ] )

@subsubsection AqlFunctionsControl Control flow functions

AQL offers the following functions to let the user control the flow of operations:

- @FN{NOT_NULL(@FA{alternative}, ...)}: returns the first alternative that is not `null`, 
  and `null` if all alternatives are `null` themselves.

- @FN{FIRST_LIST(@FA{alternative}, ...)}: returns the first alternative that is a list, and
  `null` if none of the alternatives is a list.

- @FN{FIRST_DOCUMENT(@FA{alternative}, ...)}: returns the first alternative that is a document,
  and `null` if none of the alternatives is a document.

@subsubsection AqlFunctionsMisc Miscellaneous functions

Finally, AQL supports the following functions that do not belong to any of the other
function categories:

- @FN{COLLECTIONS()}: returns a list of collections. Each collection is returned as a document
  with attributes `name` and `_id`.

- @FN{DOCUMENT(@FA{collection}, @FA{id})}: returns the document that id uniquely identified by
  the @FA{id}. ArangoDB will try to find the document using the `_id` value of the document
  in the specified collection. If there is a mismatch between the @FA{collection} passed and
  the collection specified in @FA{id}, then `null` will be returned. Additionally, if the
  @FA{collection} matches the collection value specified in @FA{id} but the document cannot be
  found, `null` will be returned. This function also allows @FA{id} to be a list of ids.
  In this case, the function will return a list of all documents that could be found. 

High-level operations {#AqlOperations}
======================================

FOR {#AqlOperationFor}
----------------------

The `FOR` keyword can be to iterate over all elements of a list.
The general syntax is:

    FOR variable-name IN expression

Each list element returned by `expression` is visited exactly once. It is
required that `expression` returns a list in all cases. The empty list is
allowed, too. The current list element is made available for further processing 
in the variable specified by `variable-name`.

    FOR u IN users
      RETURN u

This will iterate over all elements from the list `users` (note: this list
consists of all documents from the collection named "users" in this case) and
make the current list element available in variable `u`. `u` is not modified in
this example but simply pushed into the result using the `RETURN` keyword.

Note: when iterating over collection-based lists as shown here, the order of
documents is undefined unless an explicit sort order is defined using a `SORT`
statement.

The variable introduced by `FOR` is available until the scope the `FOR` is
placed in is closed.

Another example that uses a statically declared list of values to iterate over:

    FOR year IN [ 2011, 2012, 2013 ]
      RETURN { "year" : year, "isLeapYear" : year % 4 == 0 && (year % 100 != 0 || year % 400 == 0) }

Nesting of multiple `FOR` statements is allowed, too. When `FOR` statements are
nested, a cross product of the list elements returned by the individual `FOR`
statements will be created.

    FOR u IN users
      FOR l IN locations
	RETURN { "user" : u, "location" : l }

In this example, there are two list iterations: an outer iteration over the list
`users` plus an inner iteration over the list `locations`.  The inner list is
traversed as many times as there are elements in the outer list.  For each
iteration, the current values of `users` and `locations` are made available for
further processing in the variable `u` and `l`.

RETURN {#AqlOperationReturn}
----------------------------

The `RETURN` statement can (and must) be used to produce the result of a query.
It is mandatory to specify a `RETURN` statement at the end of each block in a
query, otherwise the query result would be undefined.

The general syntax for `return` is:

    RETURN expression

The `expression` returned by `RETURN` is produced for each iteration the
`RETURN` statement is placed in. That means the result of a `RETURN` statement
is always a list (this includes the empty list).  To return all elements from
the currently iterated list without modification, the following simple form can
be used:

    FOR variable-name IN expression
      RETURN variable-name

As `RETURN` allows specifying an expression, arbitrary computations can be
performed to calculate the result elements. Any of the variables valid in the
scope the `RETURN` is placed in can be used for the computations.

Note: return will close the current scope and eliminate all local variables in
it.

FILTER {#AqlOperationFilter}
----------------------------

The `FILTER` statement can be used to restrict the results to elements that
match an arbitrary logical condition.  The general syntax is:

    FILTER condition

`condition` must be a condition that evaluates to either `false` or `true`. If
the condition result is false, the current element is skipped, so it will not be
processed further and not be part of the result. If the condition is true, the
current element is not skipped and can be further processed.

    FOR u IN users
      FILTER u.active == true && u.age < 39
      RETURN u

In the above example, all list elements from `users` will be included that have
an attribute `active` with value `true` and that have an attribute `age` with a
value less than `39`. All other elements from `users` will be skipped and not be
included the result produced by `RETURN`.

It is allowed to specifiy multiple `FILTER` statements in a query, and even in
the same block. If multiple `FILTER` statements are used, their results will be
combined with a logical and, meaning all filter conditions must be true to
include an element.

    FOR u IN users
      FILTER u.active == true
      FILTER u.age < 39
      RETURN u

SORT {#AqlOperationSort}
------------------------

The `SORT` statement will force a sort of the list of already produced
intermediate results in the current block. `SORT` allows specifying one or
multiple sort criteria and directions.  The general syntax is:

    SORT expression direction

Specifiyng the `direction` is optional. The default (implict) direction for a
sort is the ascending order. To explicitly specify the sort direction, the
keywords `ASC` (ascending) and `DESC` can be used. Multiple sort criteria can be
separated using commas.

Note: when iterating over collection-based lists, the order of documents is
always undefined unless an explicit sort order is defined using `SORT`.

    FOR u IN users
      SORT u.lastName, u.firstName, u.id DESC
      RETURN u

LIMIT {#AqlOperationLimit}
--------------------------

The `LIMIT` statement allows slicing the list of result documents using an
offset and a count. It reduces the number of elements in the result to at most
the specified number.  Two general forms of `LIMIT` are followed:

    LIMIT count
    LIMIT offset, count

The first form allows specifying only the `count` value whereas the second form
allows specifying both `offset` and `count`. The first form is identical using
the second form with an `offset` value of `0`.

The `offset` value specifies how many elements from the result shall be
discarded.  It must be 0 or greater. The `count` value specifies how many
elements should be at most included in the result.

    FOR u IN users
      SORT u.firstName, u.lastName, u.id DESC
      LIMIT 0, 5
      RETURN u

LET {#AqlOperationLet}
----------------------

The `LET` statement can be used to assign an arbitrary value to a variable.  The
variable is then introduced in the scope the `LET` statement is placed in.  The
general syntax is:

    LET variable-name = expression

`LET` statements are mostly used to declare complex computations and to avoid
repeated computations of the same value at multiple parts of a query.

    FOR u IN users
      LET numRecommendations = LENGTH(u.recommendations)
      RETURN { "user" : u, "numRecommendations" : numRecommendations, "isPowerUser" : numRecommendations >= 10 } 

In the above example, the computation of the number of recommendations is
factored out using a `LET` statement, thus avoiding computing the value twice in
the `RETURN` statement.

Another use case for `LET` is to declare a complex computation in a subquery,
making the whole query more readable.

    FOR u IN users
      LET friends = (
	FOR f IN friends 
	  FILTER u.id == f.userId
	  RETURN f
      )
      LET memberships = (
	FOR m IN memberships
	  FILTER u.id == m.userId
	  RETURN m
      )
      RETURN { "user" : u, "friends" : friends, "numFriends" : LENGTH(friends), "memberShips" : memberships }

COLLECT {#AqlOperationCollect}
------------------------------

The `COLLECT` keyword can be used to group a list by one or multiple group
criteria.  The two general syntaxes for `COLLECT` are:

    COLLECT variable-name = expression
    COLLECT variable-name = expression INTO groups

The first form only groups the result by the defined group criteria defined by
`expression`. In order to further process the results produced by `COLLECT`, a
new variable (specified by `variable-name` is introduced.  This variable
contains the group value.

The second form does the same as the first form, but additionally introduces a
variable (specified by `groups`) that contains all elements that fell into the
group. Specifying the `INTO` clause is optional-

    FOR u IN users
      COLLECT city = u.city INTO g
      RETURN { "city" : city, "users" : g }

In the above example, the list of `users` will be grouped by the attribute
`city`. The result is a new list of documents, with one element per distinct
`city` value. The elements from the original list (here: `users`) per city are
made available in the variable `g`. This is due to the `INTO` clause.

`COLLECT` also allows specifying multiple group criteria. Individual group
criteria can be separated by commas.

    FOR u IN users
      COLLECT first = u.firstName, age = u.age INTO g
      RETURN { "first" : first, "age" : age, "numUsers" : LENGTH(g) }

In the above example, the list of `users` is grouped by first names and ages
first, and for each distinct combination of first name and age, the number of
users found is returned.

Note: the `COLLECT` statement eliminates all local variables in the current
scope. After `COLLECT` only the variables introduced by `COLLECT` itself are
available.

Advanced features {#AqlAdvanced}
================================

Subqueries {#AqlSubqueries}
---------------------------

Whereever an expression is allowed in AQL, a subquery can be placed. A subquery
is a query part that can introduce its own local variables without affecting
variables and values in its outer scope(s).

It is required that subqueries be put inside parentheses `(` and `)` to
explicitly mark their start and end points:

    FOR u IN users
      LET recommendations = ( 
	FOR r IN recommendations
	  FILTER u.id == r.userId
	  SORT u.rank DESC
	  LIMIT 10
	  RETURN r
      )
      RETURN { "user" : u, "recommendations" : recommendations }


    FOR u IN users
      COLLECT city = u.city INTO g
      RETURN { "city" : city, "numUsers" : LENGTH(g), "maxRating": MAX(
	FOR r IN g 
	  RETURN r.user.rating
      ) }

Subqueries might also include other subqueries themselves.

Variable expansion {#AqlExpansion}
----------------------------------

In order to access a named attribute from all elements in a list easily, AQL
offers the shortcut operator `[*]` for variable expansion.

Using the `[*]` operator with a variable will iterate over all elements in the
variable thus allowing to access a particular attribute of each element.  It is
required that the expanded variable is a list.  The result of the `[*]`
operator is again a list.

    FOR u IN users
      RETURN { "user" : u, "friendNames" : u.friends[*].name }

In the above example, the attribute `name` is accessed for each element in the
list `u.friends`. The result is a flat list of friend names, made available as
the attribute `friendNames`.
