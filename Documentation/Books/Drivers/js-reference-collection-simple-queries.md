---
layout: default
description: These functions implement theHTTP API for simple queries
---

# Simple queries

These functions implement the
[HTTP API for simple queries](../http/simple-query.html).

## collection.all

`async collection.all([opts]): Cursor`

Performs a query to fetch all documents in the collection. Returns a
[new _Cursor_ instance](js-reference-cursor.html) for the query results.

**Arguments**

- **opts**: `Object` (optional)

  For information on the possible options see the
  [HTTP API for returning all documents](../http/simple-query.html#return-all-documents).

## collection.any

`async collection.any(): Object`

Fetches a document from the collection at random.

## collection.first

`async collection.first([opts]): Array<Object>`

Performs a query to fetch the first documents in the collection. Returns an
array of the matching documents.

{% hint 'warning' %}
This method is not available when targeting ArangoDB 3.0 or later,
see [Compatibility](js-getting-started.html#compatibility).
{% endhint %}

**Arguments**

- **opts**: `Object` (optional)

  For information on the possible options see the
  [HTTP API for returning the first document of a collection](../../2.8/http-simple-query.html#first-document-of-a-collection).

  If _opts_ is a number it is treated as _opts.count_.

## collection.last

`async collection.last([opts]): Array<Object>`

Performs a query to fetch the last documents in the collection. Returns an array
of the matching documents.

{% hint 'warning' %}
This method is not available when targeting ArangoDB 3.0 or later,
see [Compatibility](js-getting-started.html#compatibility).
{% endhint %}

**Arguments**

- **opts**: `Object` (optional)

  For information on the possible options see the
  [HTTP API for returning the last document of a collection](../../2.8/http-simple-query.html#last-document-of-a-collection).

  If _opts_ is a number it is treated as _opts.count_.

## collection.byExample

`async collection.byExample(example, [opts]): Cursor`

Performs a query to fetch all documents in the collection matching the given
_example_. Returns a [new _Cursor_ instance](js-reference-cursor.html) for the query results.

**Arguments**

- **example**: _Object_

  An object representing an example for documents to be matched against.

- **opts**: _Object_ (optional)

  For information on the possible options see the
  [HTTP API for fetching documents by example](../http/simple-query.html#find-documents-matching-an-example).

## collection.firstExample

`async collection.firstExample(example): Object`

Fetches the first document in the collection matching the given _example_.

**Arguments**

- **example**: _Object_

  An object representing an example for documents to be matched against.

## collection.removeByExample

`async collection.removeByExample(example, [opts]): Object`

Removes all documents in the collection matching the given _example_.

**Arguments**

- **example**: _Object_

  An object representing an example for documents to be matched against.

- **opts**: _Object_ (optional)

  For information on the possible options see the
  [HTTP API for removing documents by example](../http/simple-query.html#remove-documents-by-example).

## collection.replaceByExample

`async collection.replaceByExample(example, newValue, [opts]): Object`

Replaces all documents in the collection matching the given _example_ with the
given _newValue_.

**Arguments**

- **example**: _Object_

  An object representing an example for documents to be matched against.

- **newValue**: _Object_

  The new value to replace matching documents with.

- **opts**: _Object_ (optional)

  For information on the possible options see the
  [HTTP API for replacing documents by example](../http/simple-query.html#replace-documents-by-example).

## collection.updateByExample

`async collection.updateByExample(example, newValue, [opts]): Object`

Updates (patches) all documents in the collection matching the given _example_
with the given _newValue_.

**Arguments**

- **example**: _Object_

  An object representing an example for documents to be matched against.

- **newValue**: _Object_

  The new value to update matching documents with.

- **opts**: _Object_ (optional)

  For information on the possible options see the
  [HTTP API for updating documents by example](../http/simple-query.html#update-documents-by-example).

## collection.lookupByKeys

`async collection.lookupByKeys(keys): Array<Object>`

Fetches the documents with the given _keys_ from the collection. Returns an
array of the matching documents.

**Arguments**

- **keys**: _Array_

  An array of document keys to look up.

## collection.removeByKeys

`async collection.removeByKeys(keys, [opts]): Object`

Deletes the documents with the given _keys_ from the collection.

**Arguments**

- **keys**: _Array_

  An array of document keys to delete.

- **opts**: _Object_ (optional)

  For information on the possible options see the
  [HTTP API for removing documents by keys](../http/simple-query.html#remove-documents-by-their-keys).

## collection.fulltext

`async collection.fulltext(fieldName, query, [opts]): Cursor`

Performs a fulltext query in the given _fieldName_ on the collection.

**Arguments**

- **fieldName**: _String_

  Name of the field to search on documents in the collection.

- **query**: _String_

  Fulltext query string to search for.

- **opts**: _Object_ (optional)

  For information on the possible options see the
  [HTTP API for fulltext queries](../http/indexes-fulltext.html).
