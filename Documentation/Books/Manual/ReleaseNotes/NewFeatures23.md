Features and Improvements in ArangoDB 2.3
=========================================

The following list shows in detail which features have been added or improved in
ArangoDB 2.3. ArangoDB 2.3 also contains several bugfixes that are not listed
here.

AQL improvements
----------------

### Framework improvements

AQL queries are now sent through a query optimizer framework before execution. 
The query optimizer framework will first convert the internal representation of
the query, the abstract syntax tree, into an initial execution plan.

The execution plan is then send through optimizer rules that may directly modify 
the plan in place or create a new variant of the plan. New plans might again be
optimized, allowing the optimizer to carry out several optimizations.

After creating plans, the optimizer will estimate the costs for each plan and 
pick the plan with the lowest cost (termed the *optimal plan*) for the actual
query execution.

With the `explain()` method of `ArangoStatement` users can check which execution
plan the optimizer pick or retrieve a list of other plans that optimizer did not
choose. The plan will reveal many details about which indexes are used etc.
`explain()` will also return the of optimizer rules applied so users can validate
whether or not a query allows using a specific optimization.

Execution of AQL queries has been rewritten in C++, allowing many queries 
to avoid the conversion of documents between ArangoDB's internal low-level data 
structure and the V8 object representation format. 

The framework for optimizer rules is now also generally cluster-aware, allowing
specific optimizations for queries that run in a cluster. Additionally, the 
optimizer was designed to be extensible in order to add more optimizations
in the future.


### Language improvements

#### Alternative operator syntax

ArangoDB 2.3 allows to use the following alternative forms for the 
logical operators:
- `AND`: logical and
- `OR`: logical or
- `NOT`: negation
  
This new syntax is just an alternative to the old syntax, allowing easier 
migration from SQL. The old syntax is still fully supported and will be:
- `&&`: logical and
- `||`: logical or
- `!`: negation


#### `NOT IN` operator
AQL now has a dedicated `NOT IN` operator.

Previously, a `NOT IN` was only achievable by writing a negated `IN` condition:
   
    FOR i IN ... FILTER ! (i IN [ 23, 42 ]) ...

In ArangoDB 2.3, the same result can now alternatively be achieved by writing
the more intuitive variant:

    FOR i IN ... FILTER i NOT IN [ 23, 42 ] ...


#### Improvements of built-in functions

The following AQL string functions have been added:

- `LTRIM(value, characters)`: left-trims a string value
- `RTRIM(value, characters)`: right-trims a string value
- `FIND_FIRST(value, search, start, end)`: finds the first occurrence 
   of a search string
- `FIND_LAST(value, search, start, end)`: finds the last occurrence of a 
   search string
- `SPLIT(value, separator, limit) `: splits a string into an array, 
   using a separator
- `SUBSTITUTE(value, search, replace, limit)`: replaces characters 
   or strings inside another

The following other AQL functions have been added:

- `VALUES(document)`: returns the values of an object as an array (this is 
   the counterpart to the already existing `ATTRIBUTES` function)
- `ZIP(attributes, values)`: returns an object constructed from attributes
   and values passed in separate parameters
- `PERCENTILE(values, n, method)`: returns the nths percentile of the
   values provided, using rank or interpolation method

The already existing functions `CONCAT` and `CONCAT_SEPARATOR` now support
array arguments, e.g.:

    /* "foobarbaz" */
    CONCAT([ 'foo', 'bar', 'baz'])

    /* "foo,bar,baz" */
    CONCAT_SEPARATOR(", ", [ 'foo', 'bar', 'baz'])


#### AQL queries throw less exceptions

In previous versions of ArangoDB, AQL queries aborted with an exception in many
situations and threw a runtime exception. For example, exceptions were thrown when 
trying to find a value using the `IN` operator in a non-array element, when trying 
to use non-boolean values with the logical operands `&&` or `||` or `!`, when using 
non-numeric values in arithmetic operations, when passing wrong parameters into 
functions etc. 

The fact that many AQL operators could throw exceptions led to a lot of questions
from users, and a lot of more-verbose-than-necessary queries. For example, the
following query failed when there were documents that did not have a `topics` 
attribute at all:

    FOR doc IN mycollection
      FILTER doc.topics IN [ "something", "whatever" ]
      RETURN doc

This forced users to rewrite the query as follows:
    
    FOR doc IN mycollection
      FILTER IS_LIST(doc.topics) && doc.topics IN [ "something", "whatever" ]
      RETURN doc

In ArangoDB 2.3 this has been changed to make AQL easier to use. The change
provides an extra benefit, and that is that non-throwing operators allow the
query optimizer to perform much more transformations in the query without 
changing its overall result.

Here is a summary of changes:
- when a non-array value is used on the right-hand side of the `IN` operator, the 
  result will be `false` in ArangoDB 2.3, and no exception will be thrown.
- the boolean operators `&&` and `||` do not throw in ArangoDB 2.3 if any of the
  operands is not a boolean value. Instead, they will perform an implicit cast of
  the values to booleans. Their result will be as follows:
  - `lhs && rhs` will return `lhs` if it is `false` or would be `false` when converted
    into a boolean. If `lhs` is `true` or would be `true` when converted to a boolean,
    `rhs` will be returned.
  - `lhs || rhs` will return `lhs` if it is `true` or would be `true` when converted
    into a boolean. If `lhs` is `false` or would be `false` when converted to a boolean,
    `rhs` will be returned.
  - `! value` will return the negated value of `value` converted into a boolean
- the arithmetic operators (`+`, `-`, `*`, `/`, `%`) can be applied to any value and 
  will not throw exceptions when applied to non-numeric values. Instead, any value used
  in these operators will be casted to a numeric value implicitly. If no numeric result
  can be produced by an arithmetic operator, it will return `null` in ArangoDB 2.3. This
  is also true for division by zero. 
- passing arguments of invalid types into AQL functions does not throw a runtime
  exception in most cases, but may produce runtime warnings. Built-in AQL functions that 
  receive invalid arguments will then return `null`.


Performance improvements
------------------------

### Non-unique hash indexes

The performance of insertion into *non-unique* hash indexes has been improved
significantly. This fixes performance problems in case attributes were indexes
that contained only very few distinct values, or when most of the documents
did not even contain the indexed attribute. This also fixes problems when
loading collections with such indexes.

The insertion time now scales linearly with the number of documents regardless
of the cardinality of the indexed attribute.


### Reverse iteration over skiplist indexes

AQL queries can now use a sorted skiplist index for reverse iteration. This
allows several queries to run faster than in previous versions of ArangoDB.

For example, the following AQL query can now use the index on `doc.value`:

    FOR doc IN mycollection
      FILTER doc.value > 23
      SORT doc.values DESC
      RETURN doc

Previous versions of ArangoDB did not use the index because of the descending
(`DESC`) sort. 

Additionally, the new AQL optimizer can use an index for sorting now even
if the AQL query does not contain a `FILTER` statement. This optimization was
not available in previous versions of ArangoDB.


### Added basic support for handling binary data in Foxx

Buffer objects can now be used when setting the response body of any Foxx action.
This allows Foxx actions to return binary data.

Requests with binary payload can be processed in Foxx applications by
using the new method `res.rawBodyBuffer()`. This will return the unparsed request
body as a Buffer object.
  
There is now also the method `req.requestParts()` available in Foxx to retrieve
the individual components of a multipart HTTP request. That can be used for example
to process file uploads.


Additionally, the `res.send()` method has been added as a convenience method for 
returning strings, JSON objects or Buffers from a Foxx action. It provides some
auto-detection based on its parameter value:

```js
res.send("<p>some HTML</p>");  // returns an HTML string
res.send({ success: true });   // returns a JSON object
res.send(new Buffer("some binary data"));  // returns binary data
```

The convenience method `res.sendFile()` can now be used to return the contents of
a file from a Foxx action. They file may contain binary data:

```js
res.sendFile(applicationContext.foxxFilename("image.png"));
```

The filesystem methods `fs.write()` and `fs.readBuffer()` can be used to work 
with binary data, too:

`fs.write()` will perform an auto-detection of its second parameter's value so it
works with Buffer objects:

```js
fs.write(filename, "some data");  // saves a string value in file
fs.write(filename, new Buffer("some binary data"));  // saves (binary) contents of a buffer
```

`fs.readBuffer()` has been added as a method to read the contents of an
arbitrary file into a Buffer object.


### Web interface

Batch document removal and move functionality has been added to the web interface,
making it easier to work with multiple documents at once. Additionally, basic 
JSON import and export tools have been added.


### Command-line options added

The command-line option `--javascript.v8-contexts` was added to arangod to 
provide better control over the number of V8 contexts created in arangod.
  
Previously, the number of V8 contexts arangod created at startup was equal 
to the number of server threads (as specified by option `--server.threads`). 

In some situations it may be more sensible to create different amounts of threads 
and V8 contexts. This is because each V8 contexts created will consume memory
and requires CPU resources for periodic garbage collection. Contrary, server
threads do not have such high memory or CPU footprint. 

If the option `--javascript.v8-contexts` is not specified, the number of V8 
contexts created at startup will remain equal to the number of server threads. 
Thus no change in configuration is required to keep the same behavior as in 
previous ArangoDB versions.


The command-line option `--log.use-local-time` was added to print dates and 
times in ArangoDB's log in the server-local timezone instead of UTC. If it
is not set, the timezone will default to UTC.

    
The option `--backslash-escape` has been added to arangoimp. Specifying this
option will use the backslash as the escape character for literal quotes when 
parsing CSV files. The escape character for literal quotes is still the
double quote character.


Miscellaneous improvements
--------------------------

ArangoDB's built-in HTTP server now supports HTTP pipelining.

The ArangoShell tutorial from the arangodb.com website is now integrated into
the ArangoDB shell.

Powerful Foxx Enhancements
--------------------------

With the new **job queue** feature you can run async jobs to communicate with external services, **Foxx queries** make writing complex AQL queries much easier and **Foxx sessions** will handle the authentication and session hassle for you.

### Foxx Queries

Writing long AQL queries in JavaScript can quickly become unwieldy. As of 2.3 ArangoDB bundles the [ArangoDB Query Builder](https://npmjs.org/package/aqb) module that provides a JavaScript API for writing complex AQL queries without string concatenation. All built-in functions that accept AQL strings now support query builder instances directly. Additionally Foxx provides a method `Foxx.createQuery` for creating parametrized queries that can return Foxx models or apply arbitrary transformations to the query results.

### Foxx Sessions

The session functionality in Foxx has been completely rewritten. The old `activateAuthentication` API is still supported but may be deprecated in the future. The new `activateSessions` API supports cookies or configurable headers, provides optional JSON Web Token and cryptographic signing support and uses the new sessions Foxx app.

ArangoDB 2.3 provides Foxx apps for user management and salted hash-based authentication which can be replaced with or supplemented by alternative implementations. For an example app using both the built-in authentication and OAuth2 see the [Foxx Sessions Example app](https://github.com/arangodb/foxx-sessions-example).

### Foxx Queues

Foxx now provides async workers via the Foxx Queues API. Jobs enqueued in a job queue will be executed asynchronously outside of the request/response cycle of Foxx controllers and can be used to communicate with external services or perform tasks that take a long time to complete or may require multiple attempts.

Jobs can be scheduled in advance or set to be executed immediately, the number of retry attempts, the retry delay as well as success and failure handlers can be defined for each job individually. Job types that integrate various external services for transactional e-mails, logging and user tracking can be found in the Foxx app registry.

### Misc

The request and response objects in Foxx controllers now provide methods for reading and writing raw cookies and signed cookies.

Mounted Foxx apps will now be loaded when arangod starts rather than at the first database request. This may result in slightly slower start up times (but a faster response for the first request).
