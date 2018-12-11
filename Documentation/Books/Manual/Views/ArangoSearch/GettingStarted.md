# Getting started with ArangoSearch Views

## The DDL configuration

[DDL](https://en.wikipedia.org/wiki/Data_definition_language) is a data
definition language or data description language for defining data structures,
especially database schemas.

All DDL operations on Views can be done via JavaScript or REST calls. The DDL
syntax follows the well established ArangoDB guidelines and thus is very
similar between the [JavaScript interface for views](../../DataModeling/Views/README.md)
and the [HTTP interface for views](../../../HTTP/Views/index.html).This article
uses the JavaScript syntax.

Assume the following collections were initially defined in a database using
the following commands:

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

## Creating a View (with default parameters)

```js
v0 = db._createView("ExampleView", "arangosearch", {});
```

## Linking created View with a collection and adding indexing parameters

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

## Query data using created View with linked collections

```js
db._query(`FOR doc IN ExampleView
  SEARCH PHRASE(doc.text, '型数 据库', 'text_zh') OR STARTS_WITH(doc.b, 'ba')
  SORT TFIDF(doc) DESC
  RETURN doc`);
```

## Examine query result

Result of the latter query will include all documents from both linked
collections that include `多模 型数` phrase in Chinese at any part of `text`
property or `b` property in English that starts with `ba`. Additionally,
descendant sorting using [TFIDF algorithm](https://en.wikipedia.org/wiki/TF-IDF)
will be applied during a search:

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
