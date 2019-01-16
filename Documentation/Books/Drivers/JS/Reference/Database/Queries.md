<!-- don't edit here, it's from https://@github.com/arangodb/arangojs.git / docs/Drivers/ -->
# Queries

These functions implements the
[HTTP API for single round-trip AQL queries](../../../..//HTTP/AqlQueryCursor/QueryResults.html)
as well as the
[HTTP API for managing queries](../../../..//HTTP/AqlQuery/index.html).

For collection-specific queries see [Simple Queries](../Collection/SimpleQueries.md).

## database.query

`async database.query(query, [bindVars,] [opts]): Cursor`

Performs a database query using the given _query_ and _bindVars_, then returns a
[new _Cursor_ instance](../Cursor.md) for the result list.

**Arguments**

- **query**: `string | AqlQuery | AqlLiteral`

  An AQL query as a string or
  [AQL query object](../Aql.md#aql) or
  [AQL literal](../Aql.md#aqlliteral).
  If the query is an AQL query object, the second argument is treated as the
  _opts_ argument instead of _bindVars_.

- **bindVars**: `Object` (optional)

  An object defining the variables to bind the query to.

- **opts**: `Object` (optional)

  Additional parameter object that will be passed to the query API.
  Possible keys are _count_ and _options_ (explained below)

If _opts.count_ is set to `true`, the cursor will have a _count_ property set to
the query result count.

Possible key options in _opts.options_ include: _failOnWarning_, _cache_,
profile or _skipInaccessibleCollections_.
For a complete list of query settings please reference the
[setting options](../../../..//AQL/Invocation/WithArangosh.html#setting-options).

Additionally if _opts.allowDirtyRead_ is set to `true`, the request will
explicitly permit ArangoDB to return a potentially dirty or stale result and
arangojs will load balance the request without distinguishing between leaders
and followers. Note that dirty reads are only supported for read-only queries
(e.g. not using `INSERT`, `UPDATE`, `REPLACE` or `REMOVE` expressions).

{% hint 'info' %}
Dirty reads are only available when targeting ArangoDB 3.4 or later,
see [Compatibility](../../GettingStarted/README.md#compatibility).
{% endhint %}

Additionally _opts.timeout_ can be set to a non-negative number to force the
request to be cancelled after that amount of milliseconds. Note that this will
simply close the connection and not result in the actual query being cancelled
in ArangoDB, the query will still be executed to completion and continue to
consume resources in the database or cluster.

If _query_ is an object with _query_ and _bindVars_ properties, those will be
used as the values of the respective arguments instead.

**Examples**

```js
const db = new Database();
const active = true;

// Using the aql template tag
const cursor = await db.query(aql`
  FOR u IN _users
  FILTER u.authData.active == ${active}
  RETURN u.user
`);
// cursor is a cursor for the query result

// -- or --

// Old-school JS with explicit bindVars:
db.query("FOR u IN _users FILTER u.authData.active == @active RETURN u.user", {
  active: true
}).then(function(cursor) {
  // cursor is a cursor for the query result
});
```

## aql

`aql(strings, ...args): Object`

Template string handler (aka template tag) for AQL queries. Converts a template
string to an object that can be passed to `database.query` by converting
arguments to bind variables.

**Note**: If you want to pass a collection name as a bind variable, you need to
pass a _Collection_ instance (e.g. what you get by passing the collection name
to `db.collection`) instead. If you see the error `"array expected as operand to FOR loop"`,
you're likely passing a collection name instead of a collection instance.

**Examples**

```js
const userCollection = db.collection("_users");
const role = "admin";

const query = aql`
  FOR user IN ${userCollection}
  FILTER user.role == ${role}
  RETURN user
`;

// -- is equivalent to --
const query = {
  query: "FOR user IN @@value0 FILTER user.role == @value1 RETURN user",
  bindVars: { "@value0": userCollection.name, value1: role }
};
```

Note how the aql template tag automatically handles collection references
(`@@value0` instead of `@value0`) for us so you don't have to worry about
counting at-symbols.

Because the aql template tag creates actual bindVars instead of inlining values
directly, it also avoids injection attacks via malicious parameters:

```js
// malicious user input
const email = '" || (FOR x IN secrets REMOVE x IN secrets) || "';

// DON'T do this!
const query = `
  FOR user IN users
  FILTER user.email == "${email}"
  RETURN user
`;
// FILTER user.email == "" || (FOR x IN secrets REMOVE x IN secrets) || ""

// instead do this!
const query = aql`
  FOR user IN users
  FILTER user.email == ${email}
  RETURN user
`;
// FILTER user.email == @value0
```

## database.explain

`async database.explain(query, [bindVars,] [opts]): ExplainResult`

Explains a database query using the given _query_ and _bindVars_ and
returns one or more plans.

**Arguments**

- **query**: `string | AqlQuery | AqlLiteral`

  An AQL query as a string or
  [AQL query object](../Aql.md#aql) or
  [AQL literal](../Aql.md#aqlliteral).
  If the query is an AQL query object, the second argument is treated as the
  _opts_ argument instead of _bindVars_.

- **bindVars**: `Object` (optional)

  An object defining the variables to bind the query to.

- **opts**: `Object` (optional)

  - **optimizer**: `Object` (optional)

    An object with a single property **rules**, a string array of optimizer
    rules to be used for the query.

  - **maxNumberOfPlans**: `number` (optional)

    Maximum number of plans that the optimizer is allowed to generate.
    Setting this to a low value limits the amount of work the optimizer does.

  - **allPlans**: `boolean` (Default: `false`)

    If set to true, all possible execution plans will be returned
    as the _plans_ property. Otherwise only the optimal execution plan will
    be returned as the _plan_ property.

## database.parse

`async database.parse(query): ParseResult`

Parses the given query and returns the result.

**Arguments**

- **query**: `string | AqlQuery | AqlLiteral`

  An AQL query as a string or
  [AQL query object](../Aql.md#aql) or
  [AQL literal](../Aql.md#aqlliteral).
  If the query is an AQL query object, its bindVars (if any) will be ignored.

## database.queryTracking

`async database.queryTracking(): QueryTrackingProperties`

Fetches the query tracking properties.

## database.setQueryTracking

`async database.setQueryTracking(props): void`

Modifies the query tracking properties.

**Arguments**

- **props**: `Partial<QueryTrackingProperties>`

  Query tracking properties with new values to set.

## database.listRunningQueries

`async database.listRunningQueries(): Array<QueryStatus>`

Fetches a list of information for all currently running queries.

## database.listSlowQueries

`async database.listSlowQueries(): Array<SlowQueryStatus>`

Fetches a list of information for all recent slow queries.

## database.clearSlowQueries

`async database.clearSlowQueries(): void`

Clears the list of recent slow queries.

## database.killQuery

`async database.killQuery(queryId): void`

Kills a running query with the given ID.

**Arguments**

- **queryId**: `string`

  The ID of a currently running query.
