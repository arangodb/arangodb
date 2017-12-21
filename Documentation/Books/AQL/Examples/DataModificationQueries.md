Data-modification queries
=========================

The following operations can be used to modify data of multiple documents
with one query. This is superior to fetching and updating the documents individually
with multiple queries. However, if only a single document needs to be modified,
ArangoDB's specialized data-modification operations for single documents
might execute faster.

Updating documents
------------------

To update existing documents, we can either use the *UPDATE* or the *REPLACE*
operation. *UPDATE* updates only the specified attributes in the found documents,
and *REPLACE* completely replaces the found documents with the specified values.

We'll start with an *UPDATE* query that rewrites the gender attribute in all
documents:

```js
FOR u IN users
  UPDATE u WITH { gender: TRANSLATE(u.gender, { m: 'male', f: 'female' }) } IN users
```

To add new attributes to existing documents, we can also use an *UPDATE* query.
The following query adds an attribute *numberOfLogins* for all users with status
active:

```js
FOR u IN users
  FILTER u.active == true
  UPDATE u WITH { numberOfLogins: 0 } IN users
```

Existing attributes can also be updated based on their previous value:

```js
FOR u IN users
  FILTER u.active == true
  UPDATE u WITH { numberOfLogins: u.numberOfLogins + 1 } IN users
```

The above query will only work if there was already a *numberOfLogins* attribute
present in the document. If it is unsure whether there is a *numberOfLogins*
attribute in the document, the increase must be made conditional:

```js
FOR u IN users
  FILTER u.active == true
  UPDATE u WITH {
    numberOfLogins: HAS(u, 'numberOfLogins') ? u.numberOfLogins + 1 : 1
  } IN users
```

Updates of multiple attributes can be combined in a single query:

```js
FOR u IN users
  FILTER u.active == true
  UPDATE u WITH {
    lastLogin: DATE_NOW(),
    numberOfLogins: HAS(u, 'numberOfLogins') ? u.numberOfLogins + 1 : 1
  } IN users
```

Note than an update query might fail during execution, for example because a
document to be updated does not exist. In this case, the query will abort at
the first error. In single-server mode, all modifications done by the query will
be rolled back as if they never happened.


Replacing documents
-------------------

To not just partially update, but completely replace existing documents, use
the *REPLACE* operation.
The following query replaces all documents in the collection backup with
the documents found in collection users. Documents common to both
collections will be replaced. All other documents will remain unchanged.
Documents are compared using their *_key* attributes:

```js
FOR u IN users
  REPLACE u IN backup
```

The above query will fail if there are documents in collection users that are
not in collection backup yet. In this case, the query would attempt to replace
documents that do not exist. If such case is detected while executing the query,
the query will abort. In single-server mode, all changes made by the query will
also be rolled back.

To make the query succeed for such case, use the *ignoreErrors* query option:

```js
FOR u IN users
  REPLACE u IN backup OPTIONS { ignoreErrors: true }
```


Removing documents
------------------

Deleting documents can be achieved with the *REMOVE* operation.
To remove all users within a certain age range, we can use the following query:

```js
FOR u IN users
  FILTER u.active == true && u.age >= 35 && u.age <= 37
  REMOVE u IN users
```


Creating documents
------------------

To create new documents, there is the *INSERT* operation.
It can also be used to generate copies of existing documents from other collections,
or to create synthetic documents (e.g. for testing purposes). The following
query creates 1000 test users in collection users with some attributes set:

```js
FOR i IN 1..1000
  INSERT {
    id: 100000 + i,
    age: 18 + FLOOR(RAND() * 25),
    name: CONCAT('test', TO_STRING(i)),
    active: false,
    gender: i % 2 == 0 ? 'male' : 'female'
  } IN users
```


Copying data from one collection into another
---------------------------------------------

To copy data from one collection into another, an *INSERT* operation can be
used:

```js
FOR u IN users
  INSERT u IN backup
```

This will copy over all documents from collection users into collection
backup. Note that both collections must already exist when the query is
executed. The query might fail if backup already contains documents, as
executing the insert might attempt to insert the same document (identified
by *_key* attribute) again. This will trigger a unique key constraint violation
and abort the query. In single-server mode, all changes made by the query
will also be rolled back.
To make such copy operation work in all cases, the target collection can
be emptied before, using a *REMOVE* query.


Handling errors
---------------

In some cases it might be desirable to continue execution of a query even in
the face of errors (e.g. "document not found"). To continue execution of a
query in case of errors, there is the *ignoreErrors* option.

To use it, place an *OPTIONS* keyword directly after the data modification
part of the query, e.g.

```js
FOR u IN users
  REPLACE u IN backup OPTIONS { ignoreErrors: true }
```

This will continue execution of the query even if errors occur during the
*REPLACE* operation. It works similar for *UPDATE*, *INSERT*, and *REMOVE*.


Altering substructures
----------------------

To modify lists in documents we have to work with temporary variables.
We will collect the sublist in there and alter it. We choose a simple
boolean filter condition to make the query better comprehensible.

First lets create a collection with a sample:

```js
database = db._create('complexCollection')
database.save({
  "topLevelAttribute" : "a",
  "subList" : [
    {
      "attributeToAlter" : "oldValue",
      "filterByMe" : true
    },
    {
      "attributeToAlter" : "moreOldValues",
      "filterByMe" : true
    },
    {
      "attributeToAlter" : "unchangedValue",
      "filterByMe" : false
    }
  ]
})
```

Heres the Query which keeps the *subList* on *alteredList* to update it later:

```js
FOR document in complexCollection
  LET alteredList = (
    FOR element IN document.subList
       LET newItem = (! element.filterByMe ?
                      element :
                      MERGE(element, { attributeToAlter: "shiny New Value" }))
       RETURN newItem)
  UPDATE document WITH { subList:  alteredList } IN complexCollection
```

While the query as it is is now functional:

```js
db.complexCollection.toArray()
[
  {
    "_id" : "complexCollection/392671569467",
    "_key" : "392671569467",
    "_rev" : "392799430203",
    "topLevelAttribute" : "a",
    "subList" : [
      {
        "filterByMe" : true,
        "attributeToAlter" : "shiny New Value"
      },
      {
        "filterByMe" : true,
        "attributeToAlter" : "shiny New Value"
      },
      {
        "filterByMe" : false,
        "attributeToAlter" : "unchangedValue"
      }
    ]
  }
]
```

It will probably be soonish a performance bottleneck, since it **modifies**
all documents in the collection **regardless whether the values change or not**.
Therefore we want to only *UPDATE* the documents if we really change their value.
Hence we employ a second *FOR* to test whether *subList* will be altered or not:

```js
FOR document in complexCollection
  LET willUpdateDocument = (
    FOR element IN docToAlter.subList
      FILTER element.filterByMe LIMIT 1 RETURN 1)

  FILTER LENGTH(willUpdateDocument) > 0

  LET alteredList = (
    FOR element IN document.subList
       LET newItem = (! element.filterByMe ?
                      element :
                      MERGE(element, { attributeToAlter: "shiny New Value" }))
       RETURN newItem)

  UPDATE document WITH { subList:  alteredList } IN complexCollection
```
