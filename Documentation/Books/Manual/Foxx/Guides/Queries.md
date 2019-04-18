Writing queries
===============

ArangoDB provides the `query` template string handler
(or [template tag](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Template_literals))
to make it easy to write and execute [AQL queries](../../../AQL/index.html)
in your Foxx services:

```js
const { query } = require("@arangodb");
const max = 13;
const oddNumbers = query`
  FOR i IN 1..${max}
  FILTER i % 2 == 1
  RETURN i
`.toArray();
console.log(oddNumbers); // 1,3,5,7,9,11,13
```

Any values passed via interpolation (i.e. using the `${expression}` syntax)
are passed to ArangoDB as
[AQL bind parameters](../../../AQL/Fundamentals/BindParameters.html),
so you don't have to worry about escaping them in order to protect against
injection attacks in user-supplied data.

The result of the executed query is
[an ArangoDB array cursor](../../../AQL/Invocation/WithArangosh.html#cursors).
You can extract all query results using the `toArray()` method or
step through the result set using the `next()` method.

You can also consume a cursor with a for-loop:

```js
const cursor = query`
  FOR i IN 1..5
  RETURN i
`;
for (const item of cursor) {
  console.log(item);
}
```

It is also possible to pass options to the query helper:

```js
const cursor = query({ fullCount: true })`
  FOR i IN 1..1000
  LIMIT 0, 100
  RETURN i
`;
const { fullCount } = cursor.getExtra().stats;
console.log(`${fullCount} total`);
console.log(cursor.toArray());
```

Using collections
-----------------

When [working with collections in your service](Collections.md) you generally
want to avoid hardcoding exact collection names. But if you pass a
collection name directly to a query it will be treated as a string:

```js
// THIS DOES NOT WORK
const users = module.context.collectionName("users");
// e.g. "myfoxx_users"
const admins = query`
  FOR user IN ${users}
  FILTER user.isAdmin
  RETURN user
`.toArray(); // ERROR
```

Instead you need to pass an ArangoDB collection object:

```js
const users = module.context.collection("users");
// users is now a collection, not a string
const admins = query`
  FOR user IN ${users}
  FILTER user.isAdmin
  RETURN user
`.toArray();
```

Note that you don't need to use any different syntax to use
a collection in a query, but you do need to make sure the collection is
an actual ArangoDB collection object rather than a plain string.

Low-level access
----------------

In addition to the `query` template tag, ArangoDB also provides
the `aql` template tag, which only generates a query object
but doesn't execute it:

```js
const { db, aql } = require("@arangodb");
const max = 7;
const query = aql`
  FOR i IN 1..${max}
  RETURN i
`;
const numbers = db._query(query).toArray();
```

You can also use the `db._query` method to execute queries using
plain strings and passing the bind parameters as an object:

```js
// Note the lack of a tag, this is a normal string
const query = `
  FOR user IN @@users
  FILTER user.isAdmin
  RETURN user
`;
const admins = db._query(query, {
  // We're passing a string instead of a collection
  // because this is an explicit collection bind parameter
  // using the AQL double-at notation
  "@users": module.context.collectionName("users")
}).toArray();
```

Note that when using plain strings as queries ArangoDB provides
no safeguards to prevent accidental AQL injections:

```js
// Malicious user input where you might expect a number
const evil = "1 FOR u IN myfoxx_users REMOVE u IN myfoxx_users";
// DO NOT DO THIS
const numbers = db._query(`
  FOR i IN 1..${evil}
  RETURN i
`).toArray();
// Actual query executed by the code:
// FOR i IN i..1
// FOR u IN myfoxx_users
// REMOVE u IN myfoxx_users
// RETURN i
```

If possible, you should always use the `query` or `aql` template tags
rather than passing raw query strings to `db._query` directly.

AQL fragments
-------------

If you need to insert AQL snippets dynamically, you can still use
the `query` template tag by using the `aql.literal` helper function to
mark the snippet as a raw AQL fragment:

```js
const filter = aql.literal(
  adminsOnly ? 'FILTER user.isAdmin' : ''
);
const result = query`
  FOR user IN ${users}
  ${filter}
  RETURN user
`.toArray();
```

Both the `query` and `aql` template tags understand fragments marked
with the `aql.literal` helper and inline them directly into the query
instead of converting them to bind parameters.

Note that because the `aql.literal` helper takes a raw string as argument
the same security implications apply to it as when writing raw AQL queries
using plain strings:

```js
// Malicious user input where you might expect a condition
const evil = "true REMOVE u IN myfoxx_users";
// DO NOT DO THIS
const filter = aql.literal(`FILTER ${evil}`);
const result = query`
  FOR user IN ${users}
  ${filter}
  RETURN user
`.toArray();
// Actual query executed by the code:
// FOR user IN myfoxx_users
// FILTER true
// REMOVE user IN myfoxx_users
// RETURN user
```

A typical scenario that might result in an exploit like this is taking
arbitrary strings from a search UI to filter or sort results by a field name.
Make sure to restrict what values you accept.

Managing queries in your service
--------------------------------

In many cases it may be initially more convenient to perform queries
right where you use their results:

```js
router.get("/emails", (req, res) => {
  res.json(query`
    FOR u IN ${users}
    FILTER u.active
    RETURN u.email
  `.toArray())
});
```

However to [help testability](Testing.md) and make the queries more reusable,
it's often a good idea to move them out of your request handlers
into separate functions, e.g.:

```js
// in queries/get-user-emails.js
"use strict";
const { query, aql } = require("@arangodb");
const users = module.context.collection("users");
module.exports = (activeOnly = true) => query`
  FOR user IN ${users}
  ${aql.literal(activeOnly ? "FILTER user.active" : "")}
  RETURN user.email
`.toArray();

// in your router
const getUserEmails = require("../queries/get-user-emails");

router.get("/active-emails", (req, res) => {
  res.json(getUserEmails(true));
});
router.get("/all-emails", (req, res) => {
  res.json(getUserEmails(false));
});
```
