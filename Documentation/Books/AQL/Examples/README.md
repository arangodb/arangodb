!CHAPTER Usual Query Patterns Examples

Those pages contain some common query patterns with examples. For better
understandability the query results are also included directly below each query.

Normally you would want to run queries on data stored in collections. This section
will provide several examples for that.

Some of the following example queries are executed on a collection 'users' with the data provided here below.


!SUBSECTION Things to consider when running queries on collections

Note that all documents created in any collections will automatically get the
following server-generated attributes:

- *_id*: A unique id, consisting of [collection name](../../Manual/Appendix/Glossary.html#collection-name)
  and a server-side sequence value
- *_key*: The server sequence value
- *_rev*: The document's revision id

Whenever you run queries on the documents in collections, don't be surprised if
these additional attributes are returned as well.

Please also note that with real-world data, you might want to create additional
indexes on the data (left out here for brevity). Adding indexes on attributes that are
used in *FILTER* statements may considerably speed up queries. Furthermore, instead of
using attributes such as *id*, *from* and *to*, you might want to use the built-in
*_id*, *_from* and *_to* attributes. Finally, [edge collection](../../Manual/Appendix/Glossary.html#edge-collection)s provide a nice way of
establishing references / links between documents. These features have been left out here 
for brevity as well.


!SUBSECTION Example data

Some of the following example queries are executed on a collection *users*
with the following initial data:

```json
[ 
  { "id": 100, "name": "John", "age": 37, "active": true, "gender": "m" },
  { "id": 101, "name": "Fred", "age": 36, "active": true, "gender": "m" },
  { "id": 102, "name": "Jacob", "age": 35, "active": false, "gender": "m" },
  { "id": 103, "name": "Ethan", "age": 34, "active": false, "gender": "m" },
  { "id": 104, "name": "Michael", "age": 33, "active": true, "gender": "m" },
  { "id": 105, "name": "Alexander", "age": 32, "active": true, "gender": "m" },
  { "id": 106, "name": "Daniel", "age": 31, "active": true, "gender": "m" },
  { "id": 107, "name": "Anthony", "age": 30, "active": true, "gender": "m" },
  { "id": 108, "name": "Jim", "age": 29, "active": true, "gender": "m" },
  { "id": 109, "name": "Diego", "age": 28, "active": true, "gender": "m" },
  { "id": 200, "name": "Sophia", "age": 37, "active": true, "gender": "f" },
  { "id": 201, "name": "Emma", "age": 36,  "active": true, "gender": "f" },
  { "id": 202, "name": "Olivia", "age": 35, "active": false, "gender": "f" },
  { "id": 203, "name": "Madison", "age": 34, "active": true, "gender": "f" },
  { "id": 204, "name": "Chloe", "age": 33, "active": true, "gender": "f" },
  { "id": 205, "name": "Eva", "age": 32, "active": false, "gender": "f" },
  { "id": 206, "name": "Abigail", "age": 31, "active": true, "gender": "f" },
  { "id": 207, "name": "Isabella", "age": 30, "active": true, "gender": "f" },
  { "id": 208, "name": "Mary", "age": 29, "active": true, "gender": "f" },
  { "id": 209, "name": "Mariah", "age": 28, "active": true, "gender": "f" }
]
```

For some of the examples, we'll also use a collection *relations* to store
relationships between users. The example data for *relations* are as follows:

```json
[
  { "from": 209, "to": 205, "type": "friend" },
  { "from": 206, "to": 108, "type": "friend" },
  { "from": 202, "to": 204, "type": "friend" },
  { "from": 200, "to": 100, "type": "friend" },
  { "from": 205, "to": 101, "type": "friend" },
  { "from": 209, "to": 203, "type": "friend" },
  { "from": 200, "to": 203, "type": "friend" },
  { "from": 100, "to": 208, "type": "friend" },
  { "from": 101, "to": 209, "type": "friend" },
  { "from": 206, "to": 102, "type": "friend" },
  { "from": 104, "to": 100, "type": "friend" },
  { "from": 104, "to": 108, "type": "friend" },
  { "from": 108, "to": 209, "type": "friend" },
  { "from": 206, "to": 106, "type": "friend" },
  { "from": 204, "to": 105, "type": "friend" },
  { "from": 208, "to": 207, "type": "friend" },
  { "from": 102, "to": 108, "type": "friend" },
  { "from": 207, "to": 203, "type": "friend" },
  { "from": 203, "to": 106, "type": "friend" },
  { "from": 202, "to": 108, "type": "friend" },
  { "from": 201, "to": 203, "type": "friend" },
  { "from": 105, "to": 100, "type": "friend" },
  { "from": 100, "to": 109, "type": "friend" },
  { "from": 207, "to": 109, "type": "friend" },
  { "from": 103, "to": 203, "type": "friend" },
  { "from": 208, "to": 104, "type": "friend" },
  { "from": 105, "to": 104, "type": "friend" },
  { "from": 103, "to": 208, "type": "friend" },
  { "from": 203, "to": 107, "type": "boyfriend" },
  { "from": 107, "to": 203, "type": "girlfriend" },
  { "from": 208, "to": 109, "type": "boyfriend" },
  { "from": 109, "to": 208, "type": "girlfriend" },
  { "from": 106, "to": 205, "type": "girlfriend" },
  { "from": 205, "to": 106, "type": "boyfriend" },
  { "from": 103, "to": 209, "type": "girlfriend" },
  { "from": 209, "to": 103, "type": "boyfriend" },
  { "from": 201, "to": 102, "type": "boyfriend" },
  { "from": 102, "to": 201, "type": "girlfriend" },
  { "from": 206, "to": 100, "type": "boyfriend" },
  { "from": 100, "to": 206, "type": "girlfriend" }
]
```



