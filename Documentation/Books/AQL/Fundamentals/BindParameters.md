Bind parameters
===============

AQL supports the usage of bind parameters, thus allowing to separate the query
text from literal values used in the query. It is good practice to separate the
query text from the literal values because this will prevent (malicious)
injection of keywords and other collection names into an existing query. This
injection would be dangerous because it may change the meaning of an existing
query.

Using bind parameters, the meaning of an existing query cannot be changed. Bind
parameters can be used everywhere in a query where literals can be used.

The syntax for bind parameters is *@name* where *@* signifies that this is a
bind parameter and *name* is the actual parameter name. Parameter names must
start with any of the letters *a* to *z* (upper or lower case) or a digit
(*0* to *9*), and can be followed by any letter, digit or the underscore symbol.

```js
FOR u IN users
  FILTER u.id == @id && u.name == @name
  RETURN u
```

The bind parameter values need to be passed along with the query when it is
executed, but not as part of the query text itself. In the web interface,
there is a pane next to the query editor where the bind parameters can be
entered. When using `db._query()` (in arangosh for instance), then an
object of key-value pairs can be passed for the parameters. Such an object
can also be passed to the HTTP API endpoint `_api/cursor`, as attribute
value for the key *bindVars*:

```json
{
  "query": "FOR u IN users FILTER u.id == @id && u.name == @name RETURN u",
  "bindVars": {
    "id": 123,
    "name": "John Smith"
  }
}
```

Bind parameters that are declared in the query must also be passed a parameter
value, or the query will fail. Specifying parameters that are not declared in
the query will result in an error too.

Bind variables represent a value like a string, and must not be put in quotes
in the AQL code:

```js
FILTER u.name == "@name" // wrong
FILTER u.name == @name   // correct
```

If you need to do string processing (concatenation, etc.) in the query, you
need to use [string functions](../Functions/String.md) to do so:

```js
FOR u IN users
  FILTER u.id == CONCAT('prefix', @id, 'suffix') && u.name == @name
  RETURN u
```

Bind paramers can be used for both, the dot notation as well as the square
bracket notation for sub-attribute access. They can also be chained:

```js
LET doc = { foo: { bar: "baz" } }

RETURN doc.@attr.@subattr
// or
RETURN doc[@attr][@subattr]
```

```json
{
  "attr": "foo",
  "subattr": "bar"
}
```

Both variants in above example return `[ "baz" ]` as query result.

The whole attribute path, for highly nested data in particular, can also be
specified using the dot notation and a single bind parameter, by passing an
array of strings as parameter value. The elements of the array represent the
attribute keys of the path:

```js
LET doc = { a: { b: { c: 1 } } }
RETURN doc.@attr
```

```json
{ "attr": [ "a", "b", "c" ] }
```

The example query returns `[ 1 ]` as result. Note that `{ "attr": "a.b.c" }`
would return the value of an attribute called *a.b.c*, not the value of
attribute *c* with the parents *a* and *b* as `[ "a", "b", "c" ]` would.

A special type of bind parameter exists for injecting collection names. This
type of bind parameter has a name prefixed with an additional *@* symbol (thus
when using the bind parameter in a query, two *@* symbols must be used).

```js
FOR u IN @@collection
  FILTER u.active == true
  RETURN u
```

```json
{ "@collection": "myCollection" }
```

Specific information about parameters binding can also be found in:

- [AQL with Web Interface](../Invocation/WithWebInterface.md)
- [AQL with Arangosh](../Invocation/WithArangosh.md)
- [HTTP Interface for AQL Queries](../../HTTP/AqlQueryCursor/index.html)
