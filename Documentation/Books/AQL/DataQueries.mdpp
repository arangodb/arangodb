!CHAPTER Data Queries

!SECTION Data Access Queries

Retrieving data from the database with AQL does always include a **RETURN**
operation. It can be used to return a static value, such as a string:

```js
RETURN "Hello ArangoDB!"
```

The query result is always an array of elements, even if a single element was
returned and contains a single element in that case: `["Hello ArangoDB!"]`

The function `DOCUMENT()` can be called to retrieve a single document via
its document handle, for instance:

```js
RETURN DOCUMENT("users/phil")
```

*RETURN* is usually accompanied by a **FOR** loop to iterate over the
documents of a collection. The following query executes the loop body for all
documents of a collection called *users*. Each document is returned unchanged
in this example:

```js
FOR doc IN users
    RETURN doc
```

Instead of returning the raw `doc`, one can easily create a projection:

```js
FOR doc IN users
    RETURN { user: doc, newAttribute: true }
```

For every user document, an object with two attributes is returned. The value
of the attribute *user* is set to the content of the user document, and
*newAttribute* is a static attribute with the boolean value *true*.

Operations like **FILTER**, **SORT** and **LIMIT** can be added to the loop body
to narrow and order the result. Instead of above shown call to `DOCUMENT()`,
one can also retrieve the document that describes user *phil* like so:

```js
FOR doc IN users
    FILTER doc._key == "phil"
    RETURN doc
```

The document key is used in this example, but any other attribute could equally
be used for filtering. Since the document key is guaranteed to be unique, no
more than a single document will match this filter. For other attributes this
may not be the case. To return a subset of active users (determined by an
attribute called *status*), sorted by name in ascending order, you can do: 

```js
FOR doc IN users
    FILTER doc.status == "active"
    SORT doc.name
    LIMIT 10
```

Note that operations do not have to occur in a fixed order and that their order
can influence the result significantly. Limiting the number of documents
before a filter is usually not what you want, because it easily misses a lot
of documents that would fulfill the filter criterion, but are ignored because
of a premature *LIMIT* clause. Because of the aforementioned reasons, *LIMIT*
is usually put at the very end, after *FILTER*, *SORT* and other operations.

See the [High Level Operations](Operations/README.md) chapter for more details.

!SECTION Data Modification Queries

AQL supports the following data-modification operations:

- **INSERT**: insert new documents into a collection
- **UPDATE**: partially update existing documents in a collection
- **REPLACE**: completely replace existing documents in a collection
- **REMOVE**: remove existing documents from a collection
- **UPSERT**: conditionally insert or update documents in a collection

Below you find some simple example queries that use these operations.
The operations are detailed in the chapter [High Level Operations](Operations/README.md).


!SUBSECTION Modifying a single document 

Let's start with the basics: `INSERT`, `UPDATE` and `REMOVE` operations on single documents.
Here is an example that insert a document in an existing collection *users*:

```js
INSERT {
    firstName: "Anna",
    name: "Pavlova",
    profession: "artist"
} IN users
```

You may provide a key for the new document; if not provided, ArangoDB will create one for you.  

```js
INSERT {
    _key: "GilbertoGil",
    firstName: "Gilberto",
    name: "Gil",
    city: "Fortalezza"
} IN users
```

As ArangoDB is schema-free, attributes of the documents may vary: 

```js
INSERT {
    _key: "PhilCarpenter",
    firstName: "Phil",
    name: "Carpenter",
    middleName: "G.",
    status: "inactive"
} IN users
```

```js
INSERT {
    _key: "NatachaDeclerck",
    firstName: "Natacha",
    name: "Declerck",
    location: "Antwerp"
} IN users 
```

Update is quite simple. The following AQL statement will add or change the attributes status and location

```js
UPDATE "PhilCarpenter" WITH {
    status: "active",
    location: "Beijing"
} IN users
```

Replace is an alternative to update where all attributes of the document are replaced.

```js
REPLACE {
    _key: "NatachaDeclerck",
    firstName: "Natacha",
    name: "Leclerc",
    status: "active",
    level: "premium"
} IN users
```

Removing a document if you know its key is simple as well : 

```js
REMOVE "GilbertoGil" IN users
```

or 

```js
REMOVE { _key: "GilbertoGil" } IN users
```

!SUBSECTION Modifying multiple documents

Data-modification operations are normally combined with *FOR* loops to
iterate over a given list of documents. They can optionally be combined with
*FILTER* statements and the like.

Let's start with an example that modifies existing documents in a collection
*users* that match some condition:

```js
FOR u IN users
    FILTER u.status == "not active"
    UPDATE u WITH { status: "inactive" } IN users
```


Now, let's copy the contents of the collection *users* into the collection
*backup*:

```js
FOR u IN users
    INSERT u IN backup
```

As a final example, let's find some documents in collection *users* and
remove them from collection *backup*. The link between the documents in both
collections is established via the documents' keys:

```js
FOR u IN users
    FILTER u.status == "deleted"
    REMOVE u IN backup
```

!SUBSECTION Returning documents

Data-modification queries can optionally return documents. In order to reference
the inserted, removed or modified documents in a `RETURN` statement, data-modification 
statements introduce the `OLD` and/or `NEW` pseudo-values:

```js
FOR i IN 1..100
    INSERT { value: i } IN test 
    RETURN NEW
```

```js
FOR u IN users
    FILTER u.status == "deleted"
    REMOVE u IN users 
    RETURN OLD
```

```js
FOR u IN users
    FILTER u.status == "not active"
    UPDATE u WITH { status: "inactive" } IN users 
    RETURN NEW
```

`NEW` refers to the inserted or modified document revision, and `OLD` refers
to the document revision before update or removal. `INSERT` statements can 
only refer to the `NEW` pseudo-value, and `REMOVE` operations only to `OLD`. 
`UPDATE`, `REPLACE` and `UPSERT` can refer to either.

In all cases the full documents will be returned with all their attributes,
including the potentially auto-generated attributes such as `_id`, `_key`, or `_rev`
and the attributes not specified in the update expression of a partial update.

!SUBSUBSECTION Projections

It is possible to return a projection of the documents in `OLD` or `NEW` instead of 
returning the entire documents. This can be used to reduce the amount of data returned 
by queries.

For example, the following query will return only the keys of the inserted documents:

```js
FOR i IN 1..100
    INSERT { value: i } IN test 
    RETURN NEW._key
```

!SUBSUBSECTION Using OLD and NEW in the same query

For `UPDATE`, `REPLACE` and `UPSERT` statements, both `OLD` and `NEW` can be used
to return the previous revision of a document together with the updated revision:

```js
FOR u IN users
    FILTER u.status == "not active"
    UPDATE u WITH { status: "inactive" } IN users 
    RETURN { old: OLD, new: NEW }
```

!SUBSUBSECTION Calculations with OLD or NEW

It is also possible to run additional calculations with `LET` statements between the
data-modification part and the final `RETURN` of an AQL query. For example, the following
query performs an upsert operation and returns whether an existing document was
updated, or a new document was inserted. It does so by checking the `OLD` variable
after the `UPSERT` and using a `LET` statement to store a temporary string for
the operation type:
  
```js
UPSERT { name: "test" }
    INSERT { name: "test" }
    UPDATE { } IN users
LET opType = IS_NULL(OLD) ? "insert" : "update"
RETURN { _key: NEW._key, type: opType }
```

!SUBSECTION Restrictions

The name of the modified collection (*users* and *backup* in the above cases) 
must be known to the AQL executor at query-compile time and cannot change at 
runtime. Using a bind parameter to specify the
[collection name](../Manual/Appendix/Glossary.html#collection-name) is allowed.

Data-modification queries are restricted to modifying data in a single 
collection per query. That means a data-modification query cannot modify 
data in multiple collections with a single query. It is still possible (and
was shown above) to read from one or many collections and modify data in another
within the same query.

Only a single data-modification operation can be used per AQL query. Data-modification
queries cannot be used inside subqueries. Data-modification operations can optionally
be followed by `LET` operations and a single `RETURN` operation to return data. If
expressions are used within these operations, they cannot contain subqueries or 
access data in collections using AQL functions.

Finally, data-modification operations can optionally be followed by `LET` and `RETURN`,
but not by other statements such as `SORT`, `COLLECT` etc.


!SUBSECTION Transactional Execution
  
On a single server, data-modification operations are executed transactionally.
If a data-modification operation fails, any changes made by it will be rolled 
back automatically as if they never happened.

In a cluster, AQL data-modification queries are currently not executed transactionally.
Additionally, *update*, *replace*, *upsert* and *remove* AQL queries currently 
require the *_key* attribute to be specified for all documents that should be 
modified or removed, even if a shared key attribute other than *_key* was chosen 
for the collection. This restriction may be overcome in a future release of ArangoDB.
