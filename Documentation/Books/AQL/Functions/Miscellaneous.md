Miscellaneous functions
=======================

Control flow functions
----------------------

### NOT_NULL()

`NOT_NULL(alternative, ...) → value`

Return the first element that is not *null*, and *null* if all alternatives
are *null* themselves. It is also known as `COALESCE()` in SQL.

- **alternative** (any, *repeatable*): input of arbitrary type
- returns **value** (any): first non-null parameter, or *null* if all arguments
  are *null*

### FIRST_LIST()

Return the first alternative that is an array, and *null* if none of the
alternatives is an array.

- **alternative** (any, *repeatable*): input of arbitrary type
- returns **list** (list|null): array / list or null

### FIRST_DOCUMENT()

`FIRST_DOCUMENT(value) → doc`

Return the first alternative that is a document, and *null* if none of the
alternatives is a document.

- **alternative** (any, *repeatable*): input of arbitrary type
- returns **doc** (object|null): document / object or null

### Ternary operator

For conditional evaluation, check out the
[ternary operator](../Operators.md#ternary-operator).

Database functions
------------------

### CHECK_DOCUMENT()

<small>Introduced in: v3.3.22, v3.4.2</small>

`CHECK_DOCUMENT(document) → checkResult`

Returns *true* if *document* is a valid document object, i.e. a document
without any duplicate attribute names. Will return *false* for any
non-objects/non-documents or documents with duplicate attribute names.

{% hint 'warning' %}
This is an internal function for validating database objects and
is not supposed to be useful for anything else.
{% endhint %}

The primary use case for this function is to apply it on all
documents in a given collection as follows:

```js
FOR doc IN collection
  FILTER !CHECK_DOCUMENT(doc)
  RETURN JSON_STRINGIFY(doc)
```

This query will return all documents in the given collection with redundant
attribute names and export them. This output can be used for subsequent
cleanup operations.

{% hint 'info' %}
When using object literals in AQL, there will be an automatic 
removal/cleanup of duplicate attribute names, so the function will be effective
only for **already stored** database documents. Therefore,
`RETURN CHECK_DOCUMENT( { a: 1, a: 2 } )` is expected to return `true`.
{% endhint %}

- **document** (object): an arbitrary document / object
- returns **checkResult** (bool): *true* for any valid objects/documents without
  duplicate attribute names, and *false* for any non-objects/non-documents or 
  objects/documents with duplicate attribute names

### COLLECTION_COUNT()

`COLLECTION_COUNT(coll) → count`

Determine the amount of documents in a collection. [LENGTH()](#length)
is preferred.

### COLLECTIONS()

`COLLECTIONS() → docArray`

Return an array of collections.

- returns **docArray** (array): each collection as a document with attributes
  *name* and *_id* in an array

### COUNT()

This is an alias for [LENGTH()](#length).

### CURRENT_USER()

`CURRENT_USER() → userName`

Return the name of the current user.

The current user is the user account name that was specified in the
*Authorization* HTTP header of the request. It will only be populated if
authentication on the server is turned on, and if the query was executed inside
a request context. Otherwise, the return value of this function will be *null*.

- returns **userName** (string|null): the current user name, or *null* if
  authentication is disabled

### DECODE_REV()

`DECODE_REV(revision) → details`

Decompose the specified `revision` string into its components.
The resulting object has a `date` and a `count` attribute.
This function is supposed to be called with the `_rev` attribute value
of a database document as argument.

- **revision** (string): revision ID string
- returns **details** (object|null): object with two attributes
  *date* (string in ISO 8601 format) and *count* (integer number),
  or *null*

If the input revision ID is not a string or cannot be processed, the function
issues a warning and returns *null*.

Please note that the result structure may change in future versions of
ArangoDB in case the internal format of revision strings is modified. Please 
also note that the *date* value in the current result provides the date and
time of when the document record was put together on the server, but not
necessarily the time of insertion into the underlying storage engine. Therefore
in case of concurrent document operations the exact document storage order 
cannot be derived unambiguously from the revision value. It should thus be
treated as a rough estimate of when a document was created or last updated.

```js
DECODE_REV( "_YU0HOEG---" )
// { "date" : "2019-03-11T16:15:05.314Z", "count" : 0 }
```

### DOCUMENT()

`DOCUMENT(collection, id) → doc`

Return the document which is uniquely identified by its *id*. ArangoDB will
try to find the document using the *_id* value of the document in the specified
collection. 

If there is a mismatch between the *collection* passed and the
collection specified in *id*, then *null* will be returned. Additionally,
if the *collection* matches the collection value specified in *id* but the
document cannot be found, *null* will be returned.

This function also allows *id* to be an array of ids. In this case, the
function will return an array of all documents that could be found.

It is also possible to specify a document key instead of an id, or an array
of keys to return all documents that can be found.

- **collection** (string): name of a collection
- **id** (string|array): a document handle string (consisting of collection
  name and document key), a document key, or an array of both document handle
  strings and document keys
- returns **doc** (document|array|null): the content of the found document,
  an array of all found documents or *null* if nothing was found

```js
DOCUMENT( users, "users/john" )
DOCUMENT( users, "john" )

DOCUMENT( users, [ "users/john", "users/amy" ] )
DOCUMENT( users, [ "john", "amy" ] )
```

`DOCUMENT(id) → doc`

The function can also be used with a single parameter *id* as follows:

- **id** (string|array): either a document handle string (consisting of
  collection name and document key) or an array of document handle strings
- returns **doc** (document|null): the content of the found document
  or *null* if nothing was found

```js
DOCUMENT("users/john")
DOCUMENT( [ "users/john", "users/amy" ] )
```

Please also consider to use
[`DOCUMENT` in conjunction with `WITH`](../Operations/With.md)

### LENGTH()

`LENGTH(coll) → documentCount`

Determine the amount of documents in a collection.

It calls [COLLECTION_COUNT()](#collectioncount) internally.

- **coll** (collection): a collection (not string)
- returns **documentCount** (number): the total amount of documents in *coll*

*LENGTH()* can also determine the [number of elements](Array.md#length) in an array,
the [number of attribute keys](Document.md#length) of an object / document and
the [character length](String.md#length) of a string.

Hash functions
--------------

### HASH()

`HASH(value) → hashNumber`

Calculate a hash value for *value*.

- **value** (any): an element of arbitrary type
- returns **hashNumber** (number): a hash value of *value*

*value* is not required to be a string, but can have any data type. The calculated
hash value will take the data type of *value* into account, so for example the
number *1* and the string *"1"* will have different hash values. For arrays the
hash values will be equal if the arrays contain exactly the same values
(including value types) in the same order. For objects the same hash values will
be created if the objects have exactly the same attribute names and values
(including value types). The order in which attributes appear inside objects
is not important for hashing.

The hash value returned by this function is a number. The hash algorithm is not
guaranteed to remain the same in future versions of ArangoDB. The hash values
should therefore be used only for temporary calculations, e.g. to compare if two
documents are the same, or for grouping values in queries.

### String-based hashing

See the following string functions:

- [CRC32()](String.md#crc32)
- [FNV64()](String.md#fnv64)
- [MD5()](String.md#md5)
- [SHA1()](String.md#sha1)
- [SHA512()](String.md#sha512)

Function calling
----------------

### APPLY()

`APPLY(functionName, arguments) → retVal`

Dynamically call the function *funcName* with the arguments specified.
Arguments are given as array and are passed as separate parameters to
the called function.

Both built-in and user-defined functions can be called. 

- **funcName** (string): a function name
- **arguments** (array, *optional*): an array with elements of arbitrary type
- returns **retVal** (any): the return value of the called function

```js
APPLY( "SUBSTRING", [ "this is a test", 0, 7 ] )
// "this is"
```

### ASSERT() / WARN()

`ASSERT(expr, message) → retVal`<br>
`WARN(expr, message) → retVal`

The two functions evaluate an expression. In case the expression evaluates to
*true* both functions will return *true*. If the expression evaluates to
*false* *ASSERT* will throw an error and *WARN* will issue a warning and return
*false*. This behavior allows the use of *ASSERT* and *WARN* in *FILTER*
conditions.

- **expr** (expression): AQL expression to be evaluated
- **message** (string): message that will be used in exception or warning if expression evaluates to false
- returns **retVal** (bool): returns true if expression evaluates to true

```js
FOR i IN 1..3 FILTER ASSERT(i > 0, "i is not greater 0") RETURN i
FOR i IN 1..3 FILTER WARN(i < 2, "i is not smaller 2") RETURN i
```
### CALL()

`CALL(funcName, arg1, arg2, ... argN) → retVal`

Dynamically call the function *funcName* with the arguments specified.
Arguments are given as multiple parameters and passed as separate
parameters to the called function.

Both built-in and user-defined functions can be called.

- **funcName** (string): a function name
- **args** (any, *repeatable*): an arbitrary number of elements as
  multiple arguments, can be omitted
- returns **retVal** (any): the return value of the called function

```js
CALL( "SUBSTRING", "this is a test", 0, 4 )
// "this"
```

Internal functions
------------------

The following functions are used during development of ArangoDB as a database
system, primarily for unit testing. They are not intended to be used by end
users, especially not in production environments.

### FAIL()

`FAIL(reason)`

Let a query fail on purpose. Can be used in a conditional branch, or to verify
if lazy evaluation / short circuiting is used for instance.

- **reason** (string): an error message
- returns nothing, because the query is aborted

```js
RETURN 1 == 1 ? "okay" : FAIL("error") // "okay"
RETURN 1 == 1 || FAIL("error") ? true : false // true
RETURN 1 == 2 && FAIL("error") ? true : false // false
RETURN 1 == 1 && FAIL("error") ? true : false // aborted with error
```

### NOOPT()

`NOOPT(value) → retVal`

No-operation that prevents certain query compile-time and run-time optimizations. 
Constant expressions can be forced to be evaluated at runtime with this.
This function is marked as non-deterministic so its argument withstands
query optimization. There is no need to call this function explicitly, it is 
mainly used for internal testing.

- **value** (any): a value of arbitrary type
- returns **retVal** (any): *value*

```js
// differences in execution plan (explain)
FOR i IN 1..3 RETURN (1 + 1)      // const assignment
FOR i IN 1..3 RETURN NOOPT(1 + 1) // simple expression

NOOPT( 123 ) // evaluates 123 at runtime
NOOPT( CONCAT("a", "b") ) // evaluates concatenation at runtime
```

### PASSTHRU()

`PASSTHRU(value) → retVal`

Simply returns its call argument unmodified. There is no need to call this function 
explicitly, it is mainly used for internal testing.

- **value** (any): a value of arbitrary type
- returns **retVal** (any): *value*

### SLEEP()

`SLEEP(seconds) → null`

Wait for a certain amount of time before continuing the query.

- **seconds** (number): amount of time to wait
- returns a *null* value

```js
SLEEP(1)    // wait 1 second
SLEEP(0.02) // wait 20 milliseconds
```

### V8()

`V8(expression) → retVal`

No-operation that enforces the usage of the V8 JavaScript engine. There is
no need to call this function explicitly, it is mainly used for internal
testing.

- **expression** (any): arbitrary expression
- returns **retVal** (any): the return value of the *expression*

```js
// differences in execution plan (explain)
FOR i IN 1..3 RETURN (1 + 1)          // const assignment
FOR i IN 1..3 RETURN V8(1 + 1)        // simple expression
```

### VERSION()

`VERSION() → serverVersion`

Returns the server version as a string. In a cluster, returns the version
of the coordinator.

- returns **serverVersion** (string): the server version string

```js
RETURN VERSION()        // e.g. "3.4.0" 
```
