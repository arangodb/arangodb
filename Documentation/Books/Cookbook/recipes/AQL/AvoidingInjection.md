# Avoiding parameter injection in AQL

## Problem

I don't want my AQL queries to be affected by parameter injection.

### What is parameter injection?

Parameter injection means that potentially content is inserted into a query,
and that injection may change the meaning of the query. It is a security issue
that may allow an attacker to execute arbitrary queries on the database data.

It often occurs if applications trustfully insert user-provided inputs into a
query string, and do not fully or incorrectly filter them. It also occurs often
when applications build queries naively, without using security mechanisms often
provided by database software or querying mechanisms. 

## Parameter injection examples

Assembling query strings with simple string concatenation looks trivial,
but is potentially unsafe. Let's start with a simple query that's fed with some
dynamic input value, let's say from a web form. A client application or a Foxx
route happily picks up the input value, and puts it into a query:

```js
/* evil ! */
var what = req.params("searchValue");  /* user input value from web form */
...
var query = "FOR doc IN collection FILTER doc.value == " + what + " RETURN doc";
db._query(query, params).toArray();
```

The above will probably work fine for numeric input values. 

What could an attacker do to this query? Here are a few suggestions to use for the
`searchValue` parameter:

* for returning all documents in the collection: `1 || true` 
* for removing all documents: `1 || true REMOVE doc IN collection //` 
* for inserting new documents: `1 || true INSERT { foo: "bar" } IN collection //` 

It should have become obvious that this is extremely unsafe and should be avoided.

An pattern often seen to counteract this is trying to quote and escape potentially 
unsafe input values before putting them into query strings. This may work in some situations,
but it's easy to overlook something or get it subtly wrong:

```js 
/* we're sanitzing now, but it's still evil ! */
var value = req.params("searchValue").replace(/'/g, '');
...
var query = "FOR doc IN collection FILTER doc.value == '" + value + "' RETURN doc";
db._query(query, params).toArray();
```

The above example uses single quotes for enclosing the potentially unsafe user
input, and also replaces all single quotes in the input value beforehand. Not only may
that change the user input (leading to subtle errors such as *"why does my search for 
`O'Brien` don't return any results?"*), but it is also unsafe. If the user input contains 
a backslash at the end (e.g. `foo bar\`), that backslash will escape the closing single 
quote, allowing the user input to break out of the string fence again.

It gets worse if user input is inserted into the query at multiple places. Let's assume
we have a query with two dynamic values:

```js
query = "FOR doc IN collection FILTER doc.value == '" + value + "' && doc.type == '" + type + "' RETURN doc";
```

If an attacker inserted `\` for parameter `value` and ` || true REMOVE doc IN collection //` for 
parameter `type`, then the effective query would become

```    
FOR doc IN collection FILTER doc.value == '\' && doc.type == ' || true REMOVE doc IN collection //' RETURN doc
```

which is highly undesirable.

## Solution

Instead of mixing query string fragments with user inputs naively via string 
concatenation, use either **bind parameters** or a **query builder**. Both can
help to avoid the problem of injection, because they allow separating the actual
query operations (like `FOR`, `INSERT`, `REMOVE`) from (user input) values.

This recipe focuses on using bind parameters. This is not to say that query
builders shouldn't be used. They were simply omitted here for the sake of simplicity.
To get started with a using an AQL query builder in ArangoDB or other JavaScript
environments, have a look at [aqb](https://www.npmjs.com/package/aqb) (which comes 
bundled with ArangoDB). Inside ArangoDB, there are also [Foxx queries](https://docs.arangodb.com/2.8/Foxx/Develop/Queries.html)
which can be combined with aqb.

### What bind parameters are

Bind parameters in AQL queries are special tokens that act as placeholders for
actual values. Here's an example:

```
FOR doc IN collection
  FILTER doc.value == @what
  RETURN doc
```

In the above query, `@what` is a bind parameter. In order to execute this query,
a value for bind parameter `@what` must be specified. Otherwise query execution will
fail with error 1551 (*no value specified for declared bind parameter*). If a value
for `@what` gets specified, the query can be executed. However, the query string
and the bind parameter values (i.e. the contents of the `@what` bind parameter) will
be handled separately. What's in the bind parameter will always be treated as a value,
and it can't get out of its sandbox and change the semantic meaning of a query.

### How bind parameters are used

To execute a query with bind parameters, the query string (containing the bind 
parameters) and the bind parameter values are specified separately (note that when
the bind parameter value is assigned, the prefix `@` needs to be omitted): 

```js
/* query string with bind parameter */
var query = "FOR doc IN collection FILTER doc.value == @what RETURN doc";

/* actual value for bind parameter */
var params = { what: 42 };

/* run query, specifying query string and bind parameter separately */
db._query(query, params).toArray();
```

If a malicious user would set `@what` to a value of `1 || true`, this wouldn't do
any harm. AQL would treat the contents of `@what` as a single string token, and
the meaning of the query would remain unchanged. The actually executed query would be:
    
```
FOR doc IN collection
  FILTER doc.value == "1 || true"
  RETURN doc
```

Thanks to bind parameters it is also impossible to turn a selection (i.e. read-only)
query into a data deletion query.

### Using JavaScript variables as bind parameters

There is also a template string generator function `aql` that can be used to safely
(and conveniently) built AQL queries using JavaScript variables and expressions. It
can be invoked as follows:

```js
const aql = require('@arangodb')aql; // not needed in arangosh

var value = "some input value";
var query = aql`FOR doc IN collection
  FILTER doc.value == ${value}
  RETURN doc`;
var result = db._query(query).toArray();
```

Note that an ES6 template string is used for populating the `query` variable. The
string is assembled using the `aql` generator function which is bundled with 
ArangoDB. The template string can contain references to JavaScript variables or
expressions via `${...}`. In the above example, the query references a variable
named `value`. The `aql` function generates an object with two separate
attributes: the query string, containing references to bind parameters, and the actual
bind parameter values.

Bind parameter names are automatically generated by the `aql` function:

```js
var value = "some input value";
aql`FOR doc IN collection FILTER doc.value == ${value} RETURN doc`;

{ 
  "query" : "FOR doc IN collection FILTER doc.value == @value0 RETURN doc", 
  "bindVars" : { 
    "value0" : "some input value" 
  } 
}
```

### Using bind parameters in dynamic queries

Bind parameters are helpful, so it makes sense to use them for handling the dynamic values.
You can even use them for queries that itself are highly dynamic, for example with conditional 
`FILTER` and `LIMIT` parts. Here's how to do this:

```js
/* note: this example has a slight issue... hang on reading */
var query = "FOR doc IN collection";
var params = { };

if (useFilter) {
  query += " FILTER doc.value == @what";
  params.what = req.params("searchValue");
}
 
if (useLimit) {
  /* not quite right, see below */
  query += " LIMIT @offset, @count";
  params.offset = req.params("offset");
  params.count = req.params("count");
}

query += " RETURN doc";
db._query(query, params).toArray();
```

Note that in this example we're back to string concatenation, but without the problem of
the query being vulnerable to arbitrary modifications.

### Input value validation and sanitation

Still you should prefer to be paranoid, and try to detect invalid input values as early as
possible, at least before executing a query with them. This is because some input parameters
may affect the runtime behavior of queries negatively or, when modified, may lead to queries 
throwing runtime errors instead of returning valid results. This isn't something an attacker
should deserve.

`LIMIT` is a good example for this: if used with a single argument, the argument should
be numeric. When `LIMIT` is given a string value, executing the query will fail. You
may want to detect this early and don't return an HTTP 500 (as this would signal attackers 
that they were successful breaking your application).

Another problem with `LIMIT` is that high `LIMIT` values are likely more expensive than low
ones, and you may want to disallow using `LIMIT` values exceeding a certain threshold.

Here's what you could do in such cases:

```js
var query = "FOR doc IN collection LIMIT @count RETURN doc";

/* some default value for limit */
var params = { count: 100 }; 

if (useLimit) {
  var count = req.params("count");

  /* abort if value does not look like an integer */
  if (! preg_match(/^d+$/, count)) {
    throw "invalid count value!";
  }

  /* actually turn it into an integer */
  params.count = parseInt(count, 10); // turn into numeric value
}

if (params.count < 1 || params.count > 1000) {
  /* value is outside of accepted thresholds */
  throw "invalid count value!";
}

db._query(query, params).toArray();
```

This is a bit more complex, but that's a price you're likely willing to pay for a 
bit of extra safety. In reality you may want to use a framework for validation (such as
[joi](https://www.npmjs.com/package/joi) which comes bundled with ArangoDB) instead
of writing your own checks all over the place.

### Bind parameter types

There are two types of bind parameters in AQL:

* bind parameters for values: those are prefixed with a single `@` in AQL queries, and
  are specified without the prefix when they get their value assigned. These bind parameters 
  can contain any valid JSON value.

  Examples: `@what`, `@searchValue`

* bind parameters for collections: these are prefixed with `@@` in AQL queries, and are
  replaced with the name of a collection. When the bind parameter value is assigned, the
  parameter itself must be specified with a single `@` prefix. Only string values are allowed
  for this type of bind parameters.

  Examples: `@@collection`

The latter type of bind parameter is probably not used as often, and it should not be used
together with user input. Otherwise users may freely determine on which collection your
AQL queries will operate (note: this may be a valid use case, but normally it is extremely
undesired).

**Authors**: [Jan Steemann](https://github.com/jsteemann)

**Tags**: #injection #aql #security
