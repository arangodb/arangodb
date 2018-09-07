Document functions
==================

AQL provides below listed functions to operate on objects / document values.
Also see [object access](../Fundamentals/DataTypes.md#objects--documents) for
additional language constructs.

ATTRIBUTES()
------------

`ATTRIBUTES(document, removeInternal, sort) → strArray`

Return the attribute keys of the *document* as an array. Optionally omit
system attributes.

- **document** (object): an arbitrary document / object
- **removeInternal** (bool, *optional*): whether all system attributes (*_key*, *_id* etc.,
  every attribute key that starts with an underscore) shall be omitted in the result.
  The default is *false*.
- **sort** (bool, *optional*): optionally sort the resulting array alphabetically.
  The default is *false* and will return the attribute names in any order.
- returns **strArray** (array): the attribute keys of the input *document* as an
  array of strings

```js
ATTRIBUTES( { "foo": "bar", "_key": "123", "_custom": "yes" } )
// [ "foo", "_key", "_custom" ]

ATTRIBUTES( { "foo": "bar", "_key": "123", "_custom": "yes" }, true )
// [ "foo" ]

ATTRIBUTES( { "foo": "bar", "_key": "123", "_custom": "yes" }, false, true )
// [ "_custom", "_key", "foo" ]
```

Complex example to count how often every attribute key occurs in the documents
of *collection* (expensive on large collections):

```js
LET attributesPerDocument = (
    FOR doc IN collection RETURN ATTRIBUTES(doc, true)
)
FOR attributeArray IN attributesPerDocument
    FOR attribute IN attributeArray
        COLLECT attr = attribute WITH COUNT INTO count
        SORT count DESC
        RETURN {attr, count}
```

COUNT()
-------

This is an alias for [LENGTH()](#length).

HAS()
-----

`HAS(document, attributeName) → isPresent`

Test whether an attribute is present in the provided document.

- **document** (object): an arbitrary document / object
- **attributeName** (string): the attribute key to test for
- returns **isPresent** (bool): *true* if *document* has an attribute named
  *attributeName*, and *false* otherwise. An attribute with a falsy value (*0*, *false*,
  empty string *""*) or *null* is also considered as present and returns *true*.

```js
HAS( { name: "Jane" }, "name" ) // true
HAS( { name: "Jane" }, "age" ) // false
HAS( { name: null }, "name" ) // true
```

Note that the function checks if the specified attribute exists. This is different
from similar ways to test for the existance of an attribute, in case the attribute
has a falsy value or is not present (implicitly *null* on object access):

```js
!!{ name: "" }.name        // false
HAS( { name: "" }, "name") // true

{ name: null }.name == null   // true
{ }.name == null              // true
HAS( { name: null }, "name" ) // true
HAS( { }, "name" )            // false
```

Note that `HAS()` can not utilize indexes. If it's not necessary to distinguish
between explicit and implicit *null* values in your query, you may use an equality
comparison to test for *null* and create a non-sparse index on the attribute you
want to test against:

```js
FILTER !HAS(doc, "name")    // can not use indexes
FILTER IS_NULL(doc, "name") // can not use indexes
FILTER doc.name == null     // can utilize non-sparse indexes
```

IS_SAME_COLLECTION()
--------------------

`IS_SAME_COLLECTION(collectionName, documentHandle) → bool`

  collection id as the collection specified in *collection*. *document* can either be
  a [document handle](../../Manual/Appendix/Glossary.html#document-handle) string, or a document with
  an *_id* attribute. The function does not validate whether the collection actually
  contains the specified document, but only compares the name of the specified collection
  with the collection name part of the specified document.
  If *document* is neither an object with an *id* attribute nor a *string* value,
  the function will return *null* and raise a warning.

- **collectionName** (string): the name of a collection as string
- **documentHandle** (string|object): a document identifier string (e.g. *_users/1234*)
  or a regular document from a collection. Passing either a non-string or a non-document
  or a document without an *_id* attribute will result in an error.
- returns **bool** (bool): return *true* if the collection of *documentHandle* is the same
  as *collectionName*, otherwise *false*

```js
// true
IS_SAME_COLLECTION( "_users", "_users/my-user" )
IS_SAME_COLLECTION( "_users", { _id: "_users/my-user" } )

// false
IS_SAME_COLLECTION( "_users", "foobar/baz")
IS_SAME_COLLECTION( "_users", { _id: "something/else" } )
```

KEEP()
------

`KEEP(document, attributeName1, attributeName2, ... attributeNameN) → doc`

Keep only the attributes *attributeName* to *attributeNameN* of *document*.
All other attributes will be removed from the result.

- **document** (object): a document / object
- **attributeNames** (string, *repeatable*): an arbitrary number of attribute
  names as multiple arguments
- returns **doc** (object): a document with only the specified attributes on
  the top-level

```js
KEEP(doc, "firstname", "name", "likes")
```

`KEEP(document, attributeNameArray) → doc`

- **document** (object): a document / object
- **attributeNameArray** (array): an array of attribute names as strings
- returns **doc** (object): a document with only the specified attributes on
  the top-level

```js
KEEP(doc, [ "firstname", "name", "likes" ])
```

LENGTH()
--------

`LENGTH(doc) → attrCount`

Determine the number of attribute keys of an object / document.

- **doc** (object): a document / object
- returns **attrCount** (number): the number of attribute keys in *doc*, regardless
  of their values

*LENGTH()* can also determine the [number of elements](Array.md#length) in an array,
the [amount of documents](Miscellaneous.md#length) in a collection and
the [character length](String.md#length) of a string.

MATCHES()
---------

`MATCHES(document, examples, returnIndex) → match`

Compare the given *document* against each example document provided. The comparisons
will be started with the first example. All attributes of the example will be compared
against the attributes of *document*. If all attributes match, the comparison stops
and the result is returned. If there is a mismatch, the function will continue the
comparison with the next example until there are no more examples left.

The *examples* can be an array of 1..n example documents or a single document,
with any number of attributes each.

Note that *MATCHES()* can not utilize indexes.

- **document** (object): document to determine whether it matches any example
- **examples** (object|array): a single document, or an array of documents to compare
  against. Specifying an empty array is not allowed.
- **returnIndex** (bool): by setting this flag to *true*, the index of the example that
  matched will be returned (starting at offset 0), or *-1* if there was no match.
  The default is *false* and makes the function return a boolean.
- returns **match** (bool|number): if *document* matches one of the examples, *true* is
  returned, otherwise *false*. A number is returned instead if *returnIndex* is used.

```js
LET doc = {
    name: "jane",
    age: 27,
    active: true
}
RETURN MATCHES(doc, { age: 27, active: true } )
```

This will return *true*, because all attributes of the example are present in the document.

```js
RETURN MATCHES(
    { "test": 1 },
    [
        { "test": 1, "foo": "bar" },
        { "foo": 1 },
        { "test": 1 }
    ], true)
```

This will return *2*, because the third example matches, and because the
*returnIndex* flag is set to *true*.

MERGE()
-------

`MERGE(document1, document2, ... documentN) → mergedDocument`

Merge the documents *document1* to *documentN* into a single document.
If document attribute keys are ambiguous, the merged result will contain the values
of the documents contained later in the argument list.


- **documents** (object, *repeatable*): an arbitrary number of documents as
  multiple arguments (at least 2)
- returns **mergedDocument** (object): a combined document

Note that merging will only be done for top-level attributes. If you wish to
merge sub-attributes, use [MERGE_RECURSIVE()](#mergerecursive) instead.

Two documents with distinct attribute names can easily be merged into one:

```js
MERGE(
    { "user1": { "name": "Jane" } },
    { "user2": { "name": "Tom" } }
)
// { "user1": { "name": "Jane" }, "user2": { "name": "Tom" } }
```

When merging documents with identical attribute names, the attribute values of the
latter documents will be used in the end result:

```js
MERGE(
    { "users": { "name": "Jane" } },
    { "users": { "name": "Tom" } }
)
// { "users": { "name": "Tom" } }
```

`MERGE(docArray) → mergedDocument`

*MERGE* works with a single array parameter, too. This variant allows combining the
attributes of multiple objects in an array into a single object.

- **docArray** (array): an array of documents, as sole argument
- returns **mergedDocument** (object): a combined document

```js
MERGE(
    [
        { foo: "bar" },
        { quux: "quetzalcoatl", ruled: true },
        { bar: "baz", foo: "done" }
    ]
)
```

This will now return:

```js
{
    "foo": "done",
    "quux": "quetzalcoatl",
    "ruled": true,
    "bar": "baz"
}
```

MERGE_RECURSIVE()
-----------------

`MERGE_RECURSIVE(document1, document2, ... documentN) → mergedDocument`

Recursively merge the documents *document1* to *documentN* into a single document.
If document attribute keys are ambiguous, the merged result will contain the values
of the documents contained later in the argument list.

- **documents** (object, *repeatable*): an arbitrary number of documents as
  multiple arguments (at least 2)
- returns **mergedDocument** (object): a combined document

For example, two documents with distinct attribute names can easily be merged into one:

```js
MERGE_RECURSIVE(
    { "user-1": { "name": "Jane", "livesIn": { "city": "LA" } } },
    { "user-1": { "age": 42, "livesIn": { "state": "CA" } } }
)
// { "user-1": { "name": "Jane", "livesIn": { "city": "LA", "state": "CA" }, "age": 42 } }
```

*MERGE_RECURSIVE()* does not support the single array parameter variant that *MERGE* offers.

PARSE_IDENTIFIER()
------------------

`PARSE_IDENTIFIER(documentHandle) → parts`

Parse a [document handle](../../Manual/Appendix/Glossary.html#document-handle) and return its
individual parts as separate attributes.

This function can be used to easily determine the
[collection name](../../Manual/Appendix/Glossary.html#collection-name) and key of a given document.

- **documentHandle** (string|object): a document identifier string (e.g. *_users/1234*)
  or a regular document from a collection. Passing either a non-string or a non-document
  or a document without an *_id* attribute will result in an error.
- returns **parts** (object): an object with the attributes *collection* and *key*

```js
PARSE_IDENTIFIER("_users/my-user")
// { "collection": "_users", "key": "my-user" }

PARSE_IDENTIFIER( { "_id": "mycollection/mykey", "value": "some value" } )
// { "collection": "mycollection", "key": "mykey" }
```

TRANSLATE()
-----------

`TRANSLATE(value, lookupDocument, defaultValue) → mappedValue`

Look up the specified *value* in the *lookupDocument*. If *value* is a key in
*lookupDocument*, then *value* will be replaced with the lookup value found.
If *value* is not present in *lookupDocument*, then *defaultValue* will be returned
if specified. If no *defaultValue* is specified, *value* will be returned unchanged.

- **value** (string): the value to encode according to the mapping
- **lookupDocument** (object): a key/value mapping as document
- **defaultValue** (any, *optional*): a fallback value in case *value* is not found
- returns **mappedValue** (any): the encoded value, or the unaltered *value* or *defaultValue*
  (if supplied) in case it couldn't be mapped

```js
TRANSLATE("FR", { US: "United States", UK: "United Kingdom", FR: "France" } )
// "France"

TRANSLATE(42, { foo: "bar", bar: "baz" } )
// 42

TRANSLATE(42, { foo: "bar", bar: "baz" }, "not found!")
// "not found!"
```

UNSET()
-------

`UNSET(document, attributeName1, attributeName2, ... attributeNameN) → doc`

Remove the attributes *attributeName1* to *attributeNameN* from *document*.
All other attributes will be preserved.

- **document** (object): a document / object
- **attributeNames** (string, *repeatable*): an arbitrary number of attribute
  names as multiple arguments (at least 1)
- returns **doc** (object): *document* without the specified attributes on the
  top-level

```js
UNSET( doc, "_id", "_key", "foo", "bar" )
```

`UNSET(document, attributeNameArray) → doc`

- **document** (object): a document / object
- **attributeNameArray** (array): an array of attribute names as strings
- returns **doc** (object): *document* without the specified attributes on the
  top-level

```js
UNSET( doc, [ "_id", "_key", "foo", "bar" ] )
```

UNSET_RECURSIVE()
-----------------

`UNSET_RECURSIVE(document, attributeName1, attributeName2, ... attributeNameN) → doc`

Recursively remove the attributes *attributeName1* to *attributeNameN* from
*document* and its sub-documents. All other attributes will be preserved.

- **document** (object): a document / object
- **attributeNames** (string, *repeatable*): an arbitrary number of attribute
  names as multiple arguments (at least 1)
- returns **doc** (object): *document* without the specified attributes on
  all levels (top-level as well as nested objects)

```js
UNSET_RECURSIVE( doc, "_id", "_key", "foo", "bar" )
```

`UNSET_RECURSIVE(document, attributeNameArray) → doc`

- **document** (object): a document / object
- **attributeNameArray** (array): an array of attribute names as strings
- returns **doc** (object): *document* without the specified attributes on
  all levels (top-level as well as nested objects)

```js
UNSET_RECURSIVE( doc, [ "_id", "_key", "foo", "bar" ] )
```

VALUES()
--------

`VALUES(document, removeInternal) → anyArray`

Return the attribute values of the *document* as an array. Optionally omit
system attributes.

- **document** (object): a document / object
- **removeInternal** (bool, *optional*): if set to *true*, then all internal attributes
  (such as *_id*, *_key* etc.) are removed from the result
- returns **anyArray** (array): the values of *document* returned in any order

```js
VALUES( { "_key": "users/jane", "name": "Jane", "age": 35 } )
// [ "Jane", 35, "users/jane" ]

VALUES( { "_key": "users/jane", "name": "Jane", "age": 35 }, true )
// [ "Jane", 35 ]
```

ZIP()
-----

`ZIP(keys, values) → doc`

Return a document object assembled from the separate parameters *keys* and *values*.

*keys* and *values* must be arrays and have the same length.

- **keys** (array): an array of strings, to be used as attribute names in the result
- **values** (array): an array with elements of arbitrary types, to be used as
  attribute values
- returns **doc** (object): a document with the keys and values assembled

```js
ZIP( [ "name", "active", "hobbies" ], [ "some user", true, [ "swimming", "riding" ] ] )
// { "name": "some user", "active": true, "hobbies": [ "swimming", "riding" ] }
```
