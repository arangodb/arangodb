<!-- don't edit here, it's from https://@github.com/arangodb/arangojs.git / docs/Drivers/ -->
# AQL Helpers

These helpers are available via the `aql` export from the arangojs module:

```js
import arangojs, { aql } from "arangojs";

// or CommonJS:

const arangojs = require("arangojs");
const aql = arangojs.aql;
```

## aql

`aql: AqlQuery`

The `aql` function is a JavaScript template string handler (or template tag).
It can be used to write complex AQL queries as multi-line strings without
having to worry about bindVars and the distinction between collections
and regular parameters.

To use it just prefix a JavaScript template string (the ones with backticks
instead of quotes) with its import name (e.g. `aql`) and pass in variables
like you would with a regular template string. The string will automatically
be converted into an object with `query` and `bindVars` attributes which you
can pass directly to `db.query` to execute. If you pass in a collection it
will be automatically recognized as a collection reference
and handled accordingly.

The `aql` template tag can also be used inside other `aql` template strings,
allowing arbitrary nesting. Bind parameters of nested queries will be merged
automatically.

**Examples**

```js
const filterValue = 23;
const mydata = db.collection("mydata");
const result = await db.query(aql`
  FOR d IN ${mydata}
  FILTER d.num > ${filterValue}
  RETURN d
`);

// nested queries

const color = "green";
const filterByColor = aql`FILTER d.color == ${color}'`;
const result2 = await db.query(aql`
  FOR d IN ${mydata}
  ${filterByColor}
  RETURN d
`);
```

## aql.literal

`aql.literal(value): AqlLiteral`

The `aql.literal` helper can be used to mark strings to be inlined into an AQL
query when using the `aql` template tag, rather than being treated as a bind
parameter.

{% hint 'danger' %}
Any value passed to `aql.literal` will be treated as part of the AQL query.
To avoid becoming vulnerable to AQL injection attacks you should always prefer
nested `aql` queries if possible.
{% endhint %}

**Arguments**

- **value**: `string`

  An arbitrary string that will be treated as a literal AQL fragment when used
  in an `aql` template.

**Examples**

```js
const filterGreen = aql.literal('FILTER d.color == "green"');
const result = await db.query(aql`
  FOR d IN ${mydata}
  ${filterGreen}
  RETURN d
`);
```

## aql.join

`aql.join(values)`

The `aql.join` helper takes an array of queries generated using the `aql` tag
and combines them into a single query. The optional second argument will be
used as literal string to combine the queries.

**Arguments**

- **values**: `Array`

  An array of arbitrary values, typically AQL query objects or AQL literals.

- **sep**: `string` (Default: `" "`)

  String that will be used in between the values.

**Examples**

```js
// Basic usage
const parts = [aql`FILTER`, aql`x`, aql`%`, aql`2`];
const joined = aql.join(parts); // aql`FILTER x % 2`

// Merge without the extra space
const parts = [aql`FIL`, aql`TER`];
const joined = aql.join(parts, ""); // aql`FILTER`;

// Real world example: translate keys into document lookups
const users = db.collection("users");
const keys = ["abc123", "def456"];
const docs = keys.map(key => aql`DOCUMENT(${users}, ${key})`);
const aqlArray = aql`[${aql.join(docs, ", ")}]`;
const result = await db.query(aql`
  FOR d IN ${aqlArray}
  RETURN d
`);
// Query:
//   FOR d IN [DOCUMENT(@@value0, @value1), DOCUMENT(@@value0, @value2)]
//   RETURN d
// Bind parameters:
//   @value0: "users"
//   value1: "abc123"
//   value2: "def456"

// Alternative without `aql.join`
const users = db.collection("users");
const keys = ["abc123", "def456"];
const result = await db.query(aql`
  FOR key IN ${keys}
  LET d = DOCUMENT(${users}, key)
  RETURN d
`);
// Query:
//   FOR key IN @value0
//   LET d = DOCUMENT(@@value1, key)
//   RETURN d
// Bind parameters:
//   value0: ["abc123", "def456"]
//   @value1: "users"
```
