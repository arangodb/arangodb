<!-- don't edit here, its from https://@github.com/arangodb/arangodb-java-driver.git / docs/Drivers/ -->
# Cursor API

_ArangoCursor_ instances provide an abstraction over the HTTP API's limitations. Unless a method explicitly exhausts the cursor, the driver will only fetch as many batches from the server as necessary. Like the server-side cursors, _ArangoCursor_ instances are incrementally depleted as they are read from.

```Java
ArangoDB arango = new ArangoDB.Builder().build();
ArangoDatabase db = arango.db("myDB");

ArangoCursor<Integer> cursor = db.query("FOR x IN 1..5 RETURN x", null, null, Integer.class);
// query result list: [1, 2, 3, 4, 5]
Integer value = cursor.next();
assertThat(value, is(1));
// remaining result list: [2, 3, 4, 5]
```

## ArangoCursor.hasNext

```
ArangoCursor.hasNext() : boolean
```

Returns _true_ if the cursor has more elements in its current batch of results or the cursor on the server has more batches.

## ArangoCursor.next

```
ArangoCursor.next() : T
```

Returns the next element of the query result. If the current element is the last element of the batch and the cursor on the server provides more batches, the next batch is fetched from the server.

## ArangoCursor.iterator

```
ArangoCursor.iterator() : Iterator<T>
```

Returns an iterator over elements of the query result.

## ArangoCursor.asListRemaining

```
ArangoCursor.asListRemaining() : List<T>
```

Returns the remaining results as a _List_.

## ArangoCursor.getCount

```
ArangoCursor.getCount() : Integer
```

Returns the total number of result documents available (only available if the query was executed with the _count_ attribute set)

## ArangoCursor.getStats

```
ArangoCursor.getStats() : Stats
```

Returns extra information about the query result. For data-modification queries, the stats will contain the number of modified documents and the number of documents that could not be modified due to an error (if ignoreErrors query option is specified);

## ArangoCursor.getWarnings

```
ArangoCursor.getWarnings() : Collection<Warning>
```

Returns warnings which the query could have been produced.

## ArangoCursor.isCached

```
ArangoCursor.isCached() : boolean
```

Iindicating whether the query result was served from the query cache or not.
