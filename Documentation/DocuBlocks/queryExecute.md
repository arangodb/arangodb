

@brief executes a query
`query.execute(batchSize)`

Executes a simple query. If the optional batchSize value is specified,
the server will return at most batchSize values in one roundtrip.
The batchSize cannot be adjusted after the query is first executed.

**Note**: There is no need to explicitly call the execute method if another
means of fetching the query results is chosen. The following two approaches
lead to the same result:

@EXAMPLE_ARANGOSH_OUTPUT{executeQuery}
~ db._create("users");
~ db.users.save({ name: "Gerhard" });
~ db.users.save({ name: "Helmut" });
~ db.users.save({ name: "Angela" });
  result = db.users.all().toArray();
  q = db.users.all(); q.execute(); result = [ ]; while (q.hasNext()) { result.push(q.next()); }
~ db._drop("users")
@END_EXAMPLE_ARANGOSH_OUTPUT

The following two alternatives both use a batchSize and return the same
result:

@EXAMPLE_ARANGOSH_OUTPUT{executeQueryBatchSize}
~ db._create("users");
~ db.users.save({ name: "Gerhard" });
~ db.users.save({ name: "Helmut" });
~ db.users.save({ name: "Angela" });
  q = db.users.all(); q.setBatchSize(20); q.execute(); while (q.hasNext()) { print(q.next()); }
  q = db.users.all(); q.execute(20); while (q.hasNext()) { print(q.next()); }
~ db._drop("users")
@END_EXAMPLE_ARANGOSH_OUTPUT


