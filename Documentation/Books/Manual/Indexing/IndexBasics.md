Index basics
------------

Indexes allow fast access to documents, provided the indexed attribute(s)
are used in a query. While ArangoDB automatically indexes some system
attributes, users are free to create extra indexes on non-system attributes
of documents.

User-defined indexes can be created on collection level. Most user-defined indexes 
can be created by specifying the names of the index attributes.
Some index types allow indexing just one attribute (e.g. fulltext index) whereas 
other index types allow indexing multiple attributes at the same time.

The system attributes `_id`, `_key`, `_from` and `_to` are automatically indexed
by ArangoDB, without the user being required to create extra indexes for them.
`_id` and `_key` are covered by a collection's primary key, and `_from` and `_to`
are covered by an edge collection's edge index automatically.

Using the system attribute `_id` in user-defined indexes is not possible, but 
indexing `_key`, `_rev`, `_from`, and `_to` is.

ArangoDB provides the following index types:

### Primary Index

For each collection there will always be a *primary index* which is a hash index 
for the [document keys](../Appendix/Glossary.md#document-key) (`_key` attribute)
of all documents in the collection. The primary index allows quick selection
of documents in the collection using either the `_key` or `_id` attributes. It will
be used from within AQL queries automatically when performing equality lookups on
`_key` or `_id`. 

There are also dedicated functions to find a document given its `_key` or `_id`
that will always make use of the primary index:

```js
db.collection.document("<document-key>");
db._document("<document-id>");
```

As the primary index is an unsorted hash index, it cannot be used for non-equality 
range queries or for sorting.

The primary index of a collection cannot be dropped or changed, and there is no
mechanism to create user-defined primary indexes.


### Edge Index

Every [edge collection](../Appendix/Glossary.md#edge-collection) also has an 
automatically created *edge index*. The edge index provides quick access to
documents by either their `_from` or `_to` attributes. It can therefore be
used to quickly find connections between vertex documents and is invoked when 
the connecting edges of a vertex are queried.

Edge indexes are used from within AQL when performing equality lookups on `_from`
or `_to` values in an edge collections. There are also dedicated functions to 
find edges given their `_from` or `_to` values that will always make use of the 
edge index:

```js
db.collection.edges("<from-value>");
db.collection.edges("<to-value>");
db.collection.outEdges("<from-value>");
db.collection.outEdges("<to-value>");
db.collection.inEdges("<from-value>");
db.collection.inEdges("<to-value>");
```

Internally, the edge index is implemented as a hash index, which stores the union
of all `_from` and `_to` attributes. It can be used for equality 
lookups, but not for range queries or for sorting. Edge indexes are automatically 
created for edge collections. It is not possible to create user-defined edge indexes.
However, it is possible to freely use the `_from` and `_to` attributes in user-defined
indexes.

An edge index cannot be dropped or changed.


### Hash Index

A hash index can be used to quickly find documents with specific attribute values.
The hash index is unsorted, so it supports equality lookups but no range queries or sorting.

A hash index can be created on one or multiple document attributes. A hash index will 
only be used by a query if all index attributes are present in the search condition,
and if all attributes are compared using the equality (`==`) operator. Hash indexes are 
used from within AQL and several query functions, e.g. `byExample`, `firstExample` etc.

Hash indexes can optionally be declared unique, then disallowing saving the same
value(s) in the indexed attribute(s). Hash indexes can optionally be sparse.

The different types of hash indexes have the following characteristics:

- **unique hash index**: all documents in the collection must have different values for 
  the attributes covered by the unique index. Trying to insert a document with the same 
  key value as an already existing document will lead to a unique constraint 
  violation. 

  This type of index is not sparse. Documents that do not contain the index attributes or 
  that have a value of `null` in the index attribute(s) will still be indexed. 
  A key value of `null` may only occur once in the index, so this type of index cannot 
  be used for optional attributes.

  The unique option can also be used to ensure that
  [no duplicate edges](Hash.md#ensure-uniqueness-of-relations-in-edge-collections) are
  created, by adding a combined index for the fields `_from` and `_to` to an edge collection.

- **unique, sparse hash index**: all documents in the collection must have different 
  values for the attributes covered by the unique index. Documents in which at least one
  of the index attributes is not set or has a value of `null` are not included in the 
  index. This type of index can be used to ensure that there are no duplicate keys in
  the collection for documents which have the indexed attributes set. As the index will
  exclude documents for which the indexed attributes are `null` or not set, it can be
  used for optional attributes.

- **non-unique hash index**: all documents in the collection will be indexed. This type
  of index is not sparse. Documents that do not contain the index attributes or that have 
  a value of `null` in the index attribute(s) will still be indexed. Duplicate key values 
  can occur and do not lead to unique constraint violations.
 
- **non-unique, sparse hash index**: only those documents will be indexed that have all
  the indexed attributes set to a value other than `null`. It can be used for optional
  attributes.

The amortized complexity of lookup, insert, update, and removal operations in unique hash 
indexes is O(1). 

Non-unique hash indexes have an amortized complexity of O(1) for insert, update, and
removal operations. That means non-unique hash indexes can be used on attributes with 
low cardinality. 

If a hash index is created on an attribute that is missing in all or many of the documents,
the behavior is as follows:

- if the index is sparse, the documents missing the attribute will not be indexed and not
  use index memory. These documents will not influence the update or removal performance
  for the index.

- if the index is non-sparse, the documents missing the attribute will be contained in the
  index with a key value of `null`. 

Hash indexes support [indexing array values](#indexing-array-values) if the index
attribute name is extended with a <i>[\*]</i>.


### Skiplist Index

A skiplist is a sorted index structure. It can be used to quickly find documents 
with specific attribute values, for range queries and for returning documents from
the index in sorted order. Skiplists will be used from within AQL and several query 
functions, e.g. `byExample`, `firstExample` etc.

Skiplist indexes will be used for lookups, range queries and sorting only if either all
index attributes are provided in a query, or if a leftmost prefix of the index attributes
is specified.

For example, if a skiplist index is created on attributes `value1` and `value2`, the 
following filter conditions can use the index (note: the `<=` and `>=` operators are 
intentionally omitted here for the sake of brevity):

```js
FILTER doc.value1 == ...
FILTER doc.value1 < ...
FILTER doc.value1 > ...
FILTER doc.value1 > ... && doc.value1 < ...

FILTER doc.value1 == ... && doc.value2 == ...
FILTER doc.value1 == ... && doc.value2 > ...
FILTER doc.value1 == ... && doc.value2 > ... && doc.value2 < ...
```

In order to use a skiplist index for sorting, the index attributes must be specified in
the `SORT` clause of the query in the same order as they appear in the index definition.
Skiplist indexes are always created in ascending order, but they can be used to access
the indexed elements in both ascending or descending order. However, for a combined index
(an index on multiple attributes) this requires that the sort orders in a single query
as specified in the `SORT` clause must be either all ascending (optionally ommitted 
as ascending is the default) or all descending. 

For example, if the skiplist index is created on attributes `value1` and `value2` 
(in this order), then the following sorts clauses can use the index for sorting:

- `SORT value1 ASC, value2 ASC` (and its equivalent `SORT value1, value2`)
- `SORT value1 DESC, value2 DESC`
- `SORT value1 ASC` (and its equivalent `SORT value1`)
- `SORT value1 DESC`

The following sort clauses cannot make use of the index order, and require an extra
sort step:

- `SORT value1 ASC, value2 DESC`
- `SORT value1 DESC, value2 ASC`
- `SORT value2` (and its equivalent `SORT value2 ASC`)
- `SORT value2 DESC` (because first indexed attribute `value1` is not used in sort clause)

Note: the latter two sort clauses cannot use the index because the sort clause does not
refer to a leftmost prefix of the index attributes.

Skiplists can optionally be declared unique, disallowing saving the same value in the indexed 
attribute. They can be sparse or non-sparse.

The different types of skiplist indexes have the following characteristics:

- **unique skiplist index**: all documents in the collection must have different values for 
  the attributes covered by the unique index. Trying to insert a document with the same 
  key value as an already existing document will lead to a unique constraint 
  violation. 

  This type of index is not sparse. Documents that do not contain the index attributes or 
  that have a value of `null` in the index attribute(s) will still be indexed. 
  A key value of `null` may only occur once in the index, so this type of index cannot 
  be used for optional attributes.

- **unique, sparse skiplist index**: all documents in the collection must have different 
  values for the attributes covered by the unique index. Documents in which at least one
  of the index attributes is not set or has a value of `null` are not included in the 
  index. This type of index can be used to ensure that there are no duplicate keys in
  the collection for documents which have the indexed attributes set. As the index will
  exclude documents for which the indexed attributes are `null` or not set, it can be
  used for optional attributes.

- **non-unique skiplist index**: all documents in the collection will be indexed. This type
  of index is not sparse. Documents that do not contain the index attributes or that have 
  a value of `null` in the index attribute(s) will still be indexed. Duplicate key values 
  can occur and do not lead to unique constraint violations.
 
- **non-unique, sparse skiplist index**: only those documents will be indexed that have all
  the indexed attributes set to a value other than `null`. It can be used for optional
  attributes.

The operational amortized complexity for skiplist indexes is logarithmically correlated
with the number of documents in the index.

Skiplist indexes support [indexing array values](#indexing-array-values) if the index
attribute name is extended with a <i>[\*]</i>`.


### Persistent Index

The persistent index is a sorted index with persistence. The index entries are written to
disk when documents are stored or updated. That means the index entries do not need to be
rebuilt from the collection data when the server is restarted or the indexed collection
is initially loaded. Thus using persistent indexes may reduce collection loading times.

The persistent index type can be used for secondary indexes at the moment. That means the
persistent index currently cannot be made the only index for a collection, because there
will always be the in-memory primary index for the collection in addition, and potentially
more indexes (such as the edges index for an edge collection).

The index implementation is using the RocksDB engine, and it provides logarithmic complexity
for insert, update, and remove operations. As the persistent index is not an in-memory
index, it does not store pointers into the primary index as all the in-memory indexes do,
but instead it stores a document's primary key. To retrieve a document via a persistent
index via an index value lookup, there will therefore be an additional O(1) lookup into 
the primary index to fetch the actual document.

As the persistent index is sorted, it can be used for point lookups, range queries and sorting
operations, but only if either all index attributes are provided in a query, or if a leftmost 
prefix of the index attributes is specified.


### Geo Index

Users can create additional geo indexes on one or multiple attributes in collections. 
A geo index is used to find places on the surface of the earth fast. 

The geo index stores two-dimensional coordinates. It can be created on either two 
separate document attributes (latitude and longitude) or a single array attribute that
contains both latitude and longitude. Latitude and longitude must be numeric values.

The geo index provides operations to find documents with coordinates nearest to a given 
comparison coordinate, and to find documents with coordinates that are within a specifiable
radius around a comparison coordinate.

The geo index is used via dedicated functions in AQL, the simple queries
functions and it is implicitly applied when in AQL a SORT or FILTER is used with
the distance function. Otherwise it will not be used for other types of queries
or conditions.


### Fulltext Index

A fulltext index can be used to find words, or prefixes of words inside documents. 
A fulltext index can be created on a single attribute only, and will index all words 
contained in documents that have a textual value in that attribute. Only words with a (specifiable) 
minimum length are indexed. Word tokenization is done using the word boundary analysis 
provided by libicu, which is taking into account the selected language provided at 
server start. Words are indexed in their lower-cased form. The index supports complete 
match queries (full words) and prefix queries, plus basic logical operations such as 
`and`, `or` and `not` for combining partial results.

The fulltext index is sparse, meaning it will only index documents for which the index
attribute is set and contains a string value. Additionally, only words with a configurable
minimum length will be included in the index.

The fulltext index is used via dedicated functions in AQL or the simple queries, but will
not be enabled for other types of queries or conditions.

### Indexing attributes and sub-attributes

Top-level as well as nested attributes can be indexed. For attributes at the top level,
the attribute names alone are required. To index a single field, pass an array with a
single element (string of the attribute key) to the *fields* parameter of the
[ensureIndex() method](WorkingWithIndexes.md#creating-an-index). To create a
combined index over multiple fields, simply add more members to the *fields* array:

```js
// { name: "Smith", age: 35 }
db.posts.ensureIndex({ type: "hash", fields: [ "name" ] })
db.posts.ensureIndex({ type: "hash", fields: [ "name", "age" ] })
```

To index sub-attributes, specify the attribute path using the dot notation:

```js
// { name: {last: "Smith", first: "John" } }
db.posts.ensureIndex({ type: "hash", fields: [ "name.last" ] })
db.posts.ensureIndex({ type: "hash", fields: [ "name.last", "name.first" ] })
```

### Indexing array values

If an index attribute contains an array, ArangoDB will store the entire array as the index value
by default. Accessing individual members of the array via the index is not possible this
way. 

To make an index insert the individual array members into the index instead of the entire array
value, a special array index needs to be created for the attribute. Array indexes can be set up 
like regular hash or skiplist indexes using the `collection.ensureIndex()` function. To make a 
hash or skiplist index an array index, the index attribute name needs to be extended with <i>[\*]</i>
when creating the index and when filtering in an AQL query using the `IN` operator.

The following example creates an array hash index on the `tags` attribute in a collection named
`posts`:

```js
db.posts.ensureIndex({ type: "hash", fields: [ "tags[*]" ] });
db.posts.insert({ tags: [ "foobar", "baz", "quux" ] });
```

This array index can then be used for looking up individual `tags` values from AQL queries via 
the `IN` operator:

```js
FOR doc IN posts
  FILTER 'foobar' IN doc.tags
  RETURN doc
```

It is possible to add the [array expansion operator](../../AQL/Advanced/ArrayOperators.html#array-expansion)
<i>[\*]</i>, but it is not mandatory. You may use it to indicate that an array index is used,
it is purely cosmetic however:

```js
FOR doc IN posts
  FILTER 'foobar' IN doc.tags[*]
  RETURN doc
```

The following FILTER conditions will **not use** the array index:

```js
FILTER doc.tags ANY == 'foobar'
FILTER doc.tags ANY IN 'foobar'
FILTER doc.tags IN 'foobar'
FILTER doc.tags == 'foobar'
FILTER 'foobar' == doc.tags
```

It is also possible to create an index on subattributes of array values. This makes sense
if the index attribute is an array of objects, e.g.

```js
db.posts.ensureIndex({ type: "hash", fields: [ "tags[*].name" ] });
db.posts.insert({ tags: [ { name: "foobar" }, { name: "baz" }, { name: "quux" } ] });
```

The following query will then use the array index (this does require the
[array expansion operator](../../AQL/Advanced/ArrayOperators.html#array-expansion)):

```js
FOR doc IN posts
  FILTER 'foobar' IN doc.tags[*].name
  RETURN doc
```

If you store a document having the array which does contain elements not having
the subattributes this document will also be indexed with the value `null`, which
in ArangoDB is equal to attribute not existing.

ArangoDB supports creating array indexes with a single <i>[\*]</i> operator per index 
attribute. For example, creating an index as follows is **not supported**:

```js
db.posts.ensureIndex({ type: "hash", fields: [ "tags[*].name[*].value" ] });
```

Array values will automatically be de-duplicated before being inserted into an array index.
For example, if the following document is inserted into the collection, the duplicate array
value `bar` will be inserted only once:

```js
db.posts.insert({ tags: [ "foobar", "bar", "bar" ] });
```

This is done to avoid redudant storage of the same index value for the same document, which
would not provide any benefit.

If an array index is declared **unique**, the de-duplication of array values will happen before 
inserting the values into the index, so the above insert operation with two identical values
`bar` will not necessarily fail

It will always fail if the index already contains an instance of the `bar` value. However, if
the value `bar` is not already present in the index, then the de-duplication of the array values will
effectively lead to `bar` being inserted only once.

To turn off the deduplication of array values, it is possible to set the **deduplicate** attribute
on the array index to `false`. The default value for **deduplicate** is `true` however, so 
de-duplication will take place if not explicitly turned off.

```js
db.posts.ensureIndex({ type: "hash", fields: [ "tags[*]" ], deduplicate: false });

// will fail now
db.posts.insert({ tags: [ "foobar", "bar", "bar" ] }); 
```

If an array index is declared and you store documents that do not have an array at the specified attribute
this document will not be inserted in the index. Hence the following objects will not be indexed:

```js
db.posts.ensureIndex({ type: "hash", fields: [ "tags[*]" ] });
db.posts.insert({ something: "else" });
db.posts.insert({ tags: null });
db.posts.insert({ tags: "this is no array" });
db.posts.insert({ tags: { content: [1, 2, 3] } });
```

An array index is able to index explicit `null` values. When queried for `null`values, it 
will only return those documents having explicitly `null` stored in the array, it will not 
return any documents that do not have the array at all.

```js
db.posts.ensureIndex({ type: "hash", fields: [ "tags[*]" ] });
db.posts.insert({tags: null}) // Will not be indexed
db.posts.insert({tags: []})  // Will not be indexed
db.posts.insert({tags: [null]}); // Will be indexed for null
db.posts.insert({tags: [null, 1, 2]}); // Will be indexed for null, 1 and 2
```

Declaring an array index as **sparse** does not have an effect on the array part of the index,
this in particular means that explicit `null` values are also indexed in the **sparse** version.
If an index is combined from an array and a normal attribute the sparsity will apply for the attribute e.g.:

```js
db.posts.ensureIndex({ type: "hash", fields: [ "tags[*]", "name" ], sparse: true });
db.posts.insert({tags: null, name: "alice"}) // Will not be indexed
db.posts.insert({tags: [], name: "alice"}) // Will not be indexed
db.posts.insert({tags: [1, 2, 3]}) // Will not be indexed
db.posts.insert({tags: [1, 2, 3], name: null}) // Will not be indexed
db.posts.insert({tags: [1, 2, 3], name: "alice"})
// Will be indexed for [1, "alice"], [2, "alice"], [3, "alice"]
db.posts.insert({tags: [null], name: "bob"})
// Will be indexed for [null, "bob"] 
```

Please note that filtering using array indexes only works from within AQL queries and
only if the query filters on the indexed attribute using the `IN` operator. The other
comparison operators (`==`, `!=`, `>`, `>=`, `<`, `<=`, `ANY`, `ALL`, `NONE`) currently
cannot use array indexes.

### Vertex centric indexes

As mentioned above, the most important indexes for graphs are the edge
indexes, indexing the `_from` and `_to` attributes of edge collections.
They provide very quick access to all edges originating in or arriving
at a given vertex, which allows to quickly find all neighbours of a vertex
in a graph.

In many cases one would like to run more specific queries, for example
finding amongst the edges originating in a given vertex only those
with the 20 latest time stamps. Exactly this is achieved with "vertex 
centric indexes". In a sense these are localized indexes for an edge
collection, which sit at every single vertex.

Technically, they are implemented in ArangoDB as indexes, which sort the 
complete edge collection first by `_from` and then by other attributes.
If we for example have a skiplist index on the attributes `_from` and 
`timestamp` of an edge collection, we can answer the above question
very quickly with a single range lookup in the index.

Since ArangoDB 3.0 one can create sorted indexes (type "skiplist" and 
"persistent") that index the special edge attributes `_from` or `_to`
and additionally other attributes. Since ArangoDB 3.1, these are used
in graph traversals, when appropriate `FILTER` statements are found
by the optimizer.

For example, to create a vertex centric index of the above type, you 
would simply do

```js
db.edges.ensureIndex({"type":"skiplist", "fields": ["_from", "timestamp"]});
```

Then, queries like

```js
FOR v, e, p IN 1..1 OUTBOUND "V/1" edges
  FILTER e.timestamp ALL >= "2016-11-09"
  RETURN p
```

will be considerably faster in case there are many edges originating
in vertex `"V/1"` but only few with a recent time stamp.


