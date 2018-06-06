Writing queries
===============

The minimum requirement to execute an AQL query in a Foxx service is the `db` module imported with `require('@arangodb').db`. The `_query` method in `db` executes an AQL query in the database the Foxx service is installed.

```js
const db = require('@arangodb').db;

router.get('/users', function (req, res) {
  const users = db._query('FOR u IN users FILTER u.name == "John" RETURN u');
  res.send(users);
})
```

Calling `db._query` returns you an cursor. You can process the content step by step with `next()` or get all results with `toArray()`. If you only want to return the result in the response of your route you can just pass the cursor with `res.send(cursor)`, there is no need to call `toArray()` first.

Bind parameters
---------------

To separate the query text from literal values, AQL supports [bind parameters](../../../AQL/Fundamentals/BindParameters.html) which can be passed to `_query` as second parameter. Here you can also reference to a collection variable defined earlier. This is especially useful if you are using prefixed collections in your Foxx service with `module.context.collection('users')` instead of `db._collection('users')`. In this case you don't know the final collection name.

```js
const db = require('@arangodb').db;
const userColl = db._collection('users'); // module.context.collection('users');

router.get('/users', function (req, res) {
  const users = db._query(
    'FOR u IN @@coll FILTER u.name == @name RETURN u',
    {'@coll': userColl, 'name' : 'John' }
  );
  res.send(users);
})
```

The `aql` template
------------------

With the `aql` template string handler you can write multi-line AQL queries. It also handles parameters and collection names as bind parameters. You will see, that this is a much more convenient way to write AQL queries.

```js
const db = require('@arangodb').db;
const aql = require('@arangodb').aql;
const userColl = db._collection('users');

router.get('/users', function (req, res) {
  const name = 'John';
  const users = db._query(aql`
    FOR u IN ${userColl}
      FILTER u.name == ${name}
      RETURN u
  `);
  res.send(users);
})
```

The `query` helper
------------------

In most cases you will likely use the `aql` template handler to create a query you directly pass to `db._query`. To make this even easier ArangoDB provides the `query` template handler, which behaves exactly like `aql` but also directly executes the query and returns the result cursor instead of the query object:

```js
const db = require('@arangodb').db;
const query = require('@arangodb').query;
const userColl = db._collection('users');

router.get('/users', function (req, res) {
  const name = 'John';
  const users = query`
    FOR u IN ${userColl}
      FILTER u.name == ${name}
      RETURN u
  `);
  res.send(users);
})
```

The `aql.literal` helper
------------------------

You can use the `aql.literal` helper to mark strings containing AQL snippets that should be inlined directly into the query rather than be treated as bind variables.

```js
const db = require('@arangodb').db;
const query = require('@arangodb').query;
const userColl = db._collection('users');

router.get('/users', function (req, res) {
  const filterJohn = aql.literal('FILTER u.name == "John"')
  const users = query`
    FOR u IN ${userColl}
      ${filterJohn}
      RETURN u
  `);
  res.send(users);
})
```

<!--

# Queries

Use `query` helper to execute queries and automatically escape bindvars.

Collection objects can be passed directly so you don't have to worry about collection names.

Dynamic snippets can be escaped with `aql.literal` but create injection risk.

## Don't repeat yourself

Stick `query` calls in helper functions, e.g.:
```js
'use strict';
const users = module.context.collection('users');
exports.getUserEmails = (activeOnly = true) => query`
  FOR u IN ${users}
  ${aql.literal(activeOnly ? 'FILTER u.active' : '')}
  RETURN u.email
`.toArray();
```

Easier to create tests this way. Can also create folder for them:
```
api/
  index.js
  ...
queries/
  getUserEmails.js
```

## Low-level query access

Use `aql` template string to create query objects, use `db._query` to execute.
Almost never necessary. Avoid if possible.

-->