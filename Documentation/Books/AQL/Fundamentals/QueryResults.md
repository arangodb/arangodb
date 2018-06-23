Query results
=============

### Result sets

The result of an AQL query is an array of values. The individual values in the
result array may or may not have a homogeneous structure, depending on what is
actually queried.

For example, when returning data from a collection with inhomogeneous documents
(the individual documents in the collection have different attribute names)
without modification, the result values will as well have an inhomogeneous
structure. Each result value itself is a document:

```js
FOR u IN users
    RETURN u
```

```json
[ { "id": 1, "name": "John", "active": false }, 
  { "age": 32, "id": 2, "name": "Vanessa" }, 
  { "friends": [ "John", "Vanessa" ], "id": 3, "name": "Amy" } ]
```

However, if a fixed set of attributes from the collection is queried, then the 
query result values will have a homogeneous structure. Each result value is
still a document:

```js
FOR u IN users
    RETURN { "id": u.id, "name": u.name }
```

```json
[ { "id": 1, "name": "John" }, 
  { "id": 2, "name": "Vanessa" }, 
  { "id": 3, "name": "Amy" } ]
```

It is also possible to query just scalar values. In this case, the result set
is an array of scalars, and each result value is a scalar value:

```js
FOR u IN users
    RETURN u.id
```

```json
[ 1, 2, 3 ]
```

If a query does not produce any results because no matching data can be
found, it will produce an empty result array:

```json
[ ]
```
