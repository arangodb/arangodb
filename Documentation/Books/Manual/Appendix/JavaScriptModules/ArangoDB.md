ArangoDB Module
===============

`const arangodb = require('@arangodb')`

**Note**: This module should not be confused with the [`arangojs` JavaScript driver](https://github.com/arangodb/arangojs) which can be used to access ArangoDB from outside the database. Although the APIs share similarities and the functionality overlaps, the two are not compatible with each other and can not be used interchangeably.

The `db` object
---------------

`arangodb.db`

The `db` object represents the current database and lets you access collections and run queries. For more information see the [db object reference](../References/DBObject.html).

**Examples**

```js
const {db} = require('@arangodb');

const thirteen = db._query('RETURN 5 + 8').next();
```

The `aql` template tag
----------------------

`arangodb.aql`

The `aql` function is a JavaScript template string handler (or template tag).
It can be used to write complex AQL queries as multi-line strings without
having to worry about bindVars and the distinction between collections
and regular parameters.

To use it just prefix a JavaScript template string (the ones with backticks
instead of quotes) with its import name (e.g. `aql`) and pass in variables
like you would with a regular template string. The string will automatically
be converted into an object with `query` and `bindVars` attributes which you
can pass directly to `db._query` to execute. If you pass in a collection it
will be automatically recognized as a collection reference
and handled accordingly.

Starting with ArangoDB 3.4.1 queries generated using the `aql` template tag can
be used inside other `aql` template strings, allowing arbitrary nesting. Bind
parameters of nested queries will be merged automatically.

To find out more about AQL see the [AQL documentation](../../../AQL/index.html).

**Examples**

```js

const filterValue = 23;
const mydata = db._collection('mydata');
const result = db._query(aql`
  FOR d IN ${mydata}
  FILTER d.num > ${filterValue}
  RETURN d
`).toArray();

// nested queries

const color = "green";
const filterByColor = aql`FILTER d.color == ${color}'`;
const result2 = db._query(aql`
  FOR d IN ${mydata}
  ${filterByColor}
  RETURN d
`).toArray();
```

The `aql.literal` helper
------------------------

`arangodb.aql.literal`

The `aql.literal` helper can be used to mark strings to be inlined into an AQL
query when using the `aql` template tag, rather than being treated as a bind
parameter.

{% hint 'danger' %}
Any value passed to `aql.literal` will be treated as part of the AQL query.
To avoid becoming vulnerable to AQL injection attacks you should always prefer
nested `aql` queries if possible.
{% endhint %}

**Examples**

```js
const {aql} = require('@arangodb');

const filterGreen = aql.literal('FILTER d.color == "green"');
const result = db._query(aql`
  FOR d IN ${mydata}
  ${filterGreen}
  RETURN d
`).toArray();
```

The `aql.join` helper
---------------------

`arangodb.aql.join`

The `aql.join` helper takes an array of queries generated using the `aql` tag
and combines them into a single query. The optional second argument will be
used as literal string to combine the queries.

```js
const {aql} = require('@arangodb');

// Basic usage
const parts = [aql`FILTER`, aql`x`, aql`%`, aql`2`];
const joined = aql.join(parts); // aql`FILTER x % 2`

// Merge without the extra space
const parts = [aql`FIL`, aql`TER`];
const joined = aql.join(parts, ''); // aql`FILTER`;

// Real world example: translate keys into document lookups
const users = db._collection("users");
const keys = ["abc123", "def456"];
const docs = keys.map(key => aql`DOCUMENT(${users}, ${key})`);
const aqlArray = aql`[${aql.join(docs, ", ")}]`;
const result = db._query(aql`
  FOR d IN ${aqlArray}
  RETURN d
`).toArray();
// Query:
//   FOR d IN [DOCUMENT(@@value0, @value1), DOCUMENT(@@value0, @value2)]
//   RETURN d
// Bind parameters:
//   @value0: "users"
//   value1: "abc123"
//   value2: "def456"

// Alternative without `aql.join`
const users = db._collection("users");
const keys = ["abc123", "def456"];
const result = db._query(aql`
  FOR key IN ${keys}
  LET d = DOCUMENT(${users}, key)
  RETURN d
`).toArray();
// Query:
//   FOR key IN @value0
//   LET d = DOCUMENT(@@value1, key)
//   RETURN d
// Bind parameters:
//   value0: ["abc123", "def456"]
//   @value1: "users"
```

The `query` helper
------------------

`arangodb.query`

In most cases you will likely use the `aql` template handler to create a query you directly pass to
`db._query`. To make this even easier ArangoDB provides the `query` template handler, which behaves
exactly like `aql` but also directly executes the query and returns the result cursor instead of
the query object:

```js
const {query} = require('@arangodb');

const filterValue = 23;
const mydata = db._collection('mydata');
const result = query`
  FOR d IN ${mydata}
  FILTER d.num > ${filterValue}
  RETURN d
`.toArray();

// Nesting with `aql` works as expected
const {aql} = require('@arangodb');

const filter = aql`FILTER d.num > ${filterValue}`;
const result2 = query`
  FOR d IN ${mydata}
  ${filter}
  RETURN d
`.toArray();
```

The `errors` object
-------------------

`arangodb.errors`

This object provides useful objects for each error code ArangoDB might use in `ArangoError` errors. This is helpful when trying to catch specific errors raised by ArangoDB, e.g. when trying to access a document that does not exist. Each object has a `code` property corresponding to the `errorNum` found on `ArangoError` errors.

For a complete list of the error names and codes you may encounter see the [appendix on error codes](../ErrorCodes.md).

**Examples**

```js
const errors = require('@arangodb').errors;

try {
  someCollection.document('does-not-exist');
} catch (e) {
  if (e.isArangoError && e.errorNum === errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
    throw new Error('Document does not exist');
  }
  throw new Error('Something went wrong');
}
```

The `time` function
-------------------

`arangodb.time`

This function provides the current time in seconds as a floating point value with microsecond precisison.

This function can be used instead of `Date.now()` when additional precision is needed.

**Examples**

```js
const time = require('@arangodb').time;

const start = time();
db._query(someVerySlowQuery);
console.log(`Elapsed time: ${time() - start} secs`);
```

