# Getting started with ArangoSearch views

## The DDL configuration

All DDL operations on Views can be done via JavaScript or REST calls. The DDL syntax follows the well established ArangoDB guidelines and thus is very similar between JavaScript and REST. This article uses the JavaScript syntax.

Assume the following collections were intially defined in a database using the following commands:

```javascript
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
```javascript
v0 = db._createView("ExampleView", "arangosearch", {});
```

## Linking created View with a collection and adding indexing parameters
```javascript
v0 = db._view("ExampleView");
v0.properties({
    links: {
      'ExampleCollection0': /* collection Link 0 with additional custom configuration */
      {
        includeAllFields: true, /* examine fields of all linked collections  using default configuration */
        fields:
        {
          name: /* a field to apply custom configuration that will index English text */
          {
            analyzers: ["text_en"]
          },
          text: /* another field to apply custom that will index Chineese text */
          {
            analyzers: ["text_zh"]
          }
        }
      },
      'ExampleCollection1': /* collection Link 1 with custom configuration */
      {
        includeAllFields: true, /* examine all fields using default configuration */
        fields:
        {
          a:
          {
            analyzers: ["text_en"] /* a field to apply custom configuration that will index English text */
          }
        }
      }
    }
  }
);
```

## Query data using created View with linked collections
```javascript
db._query("FOR doc IN VIEW ExampleView FILTER PHRASE(doc.text, '型数 据库', 'text_zh') OR STARTS_WITH(doc.b, 'ba') SORT (doc) DESC RETURN doc");
```

## Examine query result
Result of the latter query will include all documents from both linked collections that include `多模 型数` phrase in Chineese at any part of `text` property or `b` property in English that starts with `ba`. Additionally, descendant sorting using [TFIDF algorithm](https://en.wikipedia.org/wiki/TF-IDF) will be applied during a search:
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
