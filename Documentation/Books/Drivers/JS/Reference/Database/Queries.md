<!-- don't edit here, it's from https://@github.com/arangodb/arangojs.git / docs/Drivers/ -->
# Queries

This function implements the
[HTTP API for single round-trip AQL queries](../../../..//HTTP/AqlQueryCursor/QueryResults.html).

For collection-specific queries see [Simple Queries](../Collection/SimpleQueries.md).

## database.query

`async database.query(query, [bindVars,] [opts]): Cursor`

Performs a database query using the given _query_ and _bindVars_, then returns a
[new _Cursor_ instance](../Cursor.md) for the result list.

**Arguments**

- **query**: `string`

  An AQL query string or a [query builder](https://npmjs.org/package/aqb)
  instance.

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
db.query(
  "FOR u IN _users FILTER u.authData.active == @active RETURN u.user",
  { active: true }
).then(function(cursor) {
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
