---
layout: default
description: View and Analyzer usage examples
title: ArangoSearch Examples
redirect_from:
  - /3.5/views-arango-search-getting-started.html # 3.4 -> 3.5
---
ArangoSearch Examples
=====================

View Configuration and Search
-----------------------------

Assume the following collections were initially defined in a database using
the following arangosh commands:

```js
c0 = db._create("ExampleCollection0");
c1 = db._create("ExampleCollection1");
 
c0.save({ i: 0, name: "full", text: "是一个 多模 型数 据库" });
c0.save({ i: 1, name: "half", text: "是一个 多模" });
c0.save({ i: 2, name: "other half", text: "型数 据库" });
c0.save({ i: 3, name: "quarter", text: "是一" });
 
c1.save({ a: "foo", b: "bar", i: 4 });
c1.save({ a: "foo", b: "baz", i: 5 });
c1.save({ a: "bar", b: "foo", i: 6 });
c1.save({ a: "baz", b: "foo", i: 7 });
```

A View with default parameters can be created by calling `db._createView()`
(see [JavaScript Interface for Views](data-modeling-views.html)):

```js
v0 = db._createView("ExampleView", "arangosearch", {});
```

Then link the collections to the View, define which properties shall be indexed
and with which Analyzers the values are supposed to be processed:

```js
v0 = db._view("ExampleView");
v0.properties({
    links: {
      /* collection Link 0 with additional custom configuration: */
      'ExampleCollection0':
      {
        /* examine fields of all linked collections,
           using default configuration: */
        includeAllFields: true,
        fields:
        {
          /* a field to apply custom configuration
             that will index English text: */
          name:
          {
            analyzers: ["text_en"]
          },
          /* another field to apply custom configuration
             that will index Chinese text: */
          text:
          {
            analyzers: ["text_zh"]
          }
        }
      },
      /* collection Link 1 with custom configuration: */
      'ExampleCollection1':
      {
        /* examine all fields using default configuration: */
        includeAllFields: true,
        fields:
        {
          a:
          {
            /* a field to apply custom configuration
                that will index English text: */
            analyzers: ["text_en"]
          }
        }
      }
    }
  }
);
```

You have to wait a few seconds for the View to update its index.
Then you can query it:

```js
db._query(`FOR doc IN ExampleView
  SEARCH PHRASE(doc.text, "型数 据库", "text_zh") OR
  ANALYZER(STARTS_WITH(doc.b, "ba"), "text_en")
  SORT TFIDF(doc) DESC
  RETURN doc`);
```

The result will include all documents from both linked collections that have
the Chinese phrase `多模 型数` at any position in the `text` attribute value
**or** the property `b` starting with `ba` in English. Additionally,
descendant sorting using the TF-IDF scoring algorithm will be applied.

```json
[
  {
    "_key" : "120",
    "_id" : "ExampleCollection0/120",
    "_rev" : "_XPoMzCi--_",
    "i" : 0,
    "name" : "full",
    "text" : "是一个 多模 型数 据库"
  },
  {
    "_key" : "124",
    "_id" : "ExampleCollection0/124",
    "_rev" : "_XPoMzCq--_",
    "i" : 2,
    "name" : "other half",
    "text" : "型数 据库"
  },
  {
    "_key" : "128",
    "_id" : "ExampleCollection1/128",
    "_rev" : "_XPoMzCu--_",
    "a" : "foo",
    "b" : "bar",
    "c" : 0
  },
  {
    "_key" : "130",
    "_id" : "ExampleCollection1/130",
    "_rev" : "_XPoMzCy--_",
    "a" : "foo",
    "b" : "baz",
    "c" : 1
  }
]
```

Use cases
---------

### Prefix search

The data contained in our View looks like that:

```json
{ "id": 1, "body": "ThisIsAVeryLongWord" }
{ "id": 2, "body": "ThisIsNotSoLong" }
{ "id": 3, "body": "ThisIsShorter" }
{ "id": 4, "body": "ThisIs" }
{ "id": 5, "body": "ButNotThis" }
```

We now want to search for documents where the attribute `body` starts with
`"ThisIs"`. A simple AQL query executing this prefix search:

```js
FOR doc IN viewName
  SEARCH STARTS_WITH(doc.body, 'ThisIs')
  RETURN doc
```

It will find the documents with the ids `1`, `2`, `3`, `4`, but not `5`.
