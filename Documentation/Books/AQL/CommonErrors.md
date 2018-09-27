Common Errors
=============

Trailing semicolons in query strings
------------------------------------

Many SQL databases allow sending multiple queries at once. In this case, multiple
queries are seperated using the semicolon character. Often it is also supported to
execute a single query that has a semicolon at its end.

AQL does not support this, and it is a parse error to use a semicolon at the end
of an AQL query string.


String concatenation
--------------------

In AQL, strings must be concatenated using the [CONCAT()](Functions/String.md#concat)
function. Joining them together with the `+` operator is not supported. Especially
as JavaScript programmer it is easy to walk into this trap:

```js
RETURN "foo" + "bar" // [ 0 ]
RETURN "foo" + 123   // [ 123 ]
RETURN "123" + 200   // [ 323 ]
```

The arithmetic plus operator expects numbers as operands, and will try to implicitly
cast them to numbers if they are of different type. `"foo"` and `"bar"` are casted
to `0` and then added to together (still zero). If an actual number is added, that
number will be returned (adding zero doesn't change the result). If the string is a
valid string representation of a number, then it is casted to a number. Thus, adding
`"123"` and `200` results in two numbers being added up to `323`.

To concatenate elements (with implicit casting to string for non-string values), do:

```js
RETURN CONCAT("foo", "bar") // [ "foobar" ]
RETURN CONCAT("foo", 123)   // [ "foo123" ]
RETURN CONCAT("123", 200)   // [ "123200" ]
```

Unexpected long running queries
-------------------------------

Slow queries can have various reasons and be legitimate for queries with a high
computational complexity or if they touch a lot of data. Use the *Explain*
feature to inspect execution plans and verify that appropriate indexes are
utilized. Also check for mistakes such as references to the wrong variables.

A literal collection name, which is not part of constructs like `FOR`,
`UPDATE ... IN` etc., stands for an array of all documents of that collection
and can cause an entire collection to be materialized before further
processing. It should thus be avoided.

Check the execution plan for `/* all collection documents */` and verify that
it is intended. You should also see a warning if you execute such a query:

> collection 'coll' used as expression operand

For example, instead of:

```js
RETURN coll[* LIMIT 1]
```

... with the execution plan ...

```
Execution plan:
 Id   NodeType          Est.   Comment
  1   SingletonNode        1   * ROOT
  2   CalculationNode      1     - LET #2 = coll   /* all collection documents */[* LIMIT  0, 1]   /* v8 expression */
  3   ReturnNode           1     - RETURN #2
```

... you can use the following equivalent query:

```js
FOR doc IN coll
    LIMIT 1
    RETURN doc
```

... with the (better) execution plan:

```
Execution plan:
 Id   NodeType                  Est.   Comment
  1   SingletonNode                1   * ROOT
  2   EnumerateCollectionNode     44     - FOR doc IN Characters   /* full collection scan */
  3   LimitNode                    1       - LIMIT 0, 1
  4   ReturnNode                   1       - RETURN doc
```

Similarly, make sure you have not confused any variable names with collection
names by accident:

```js
LET names = ["John", "Mary", ...]
// supposed to refer to variable "names", not collection "Names"
FOR name IN Names
    ...
```

<!--

Rename to Error Sources?
Include article about parameter injection from cookbook?

Quote marks around bind parameter placeholders
https://github.com/arangodb/arangodb/issues/1634#issuecomment-167808660

FILTER HAS(doc, "attr") instead of FITLER doc.attr / FILTER doc.attr != null

collection ... not found error, e.g. access of variable after COLLECT (no longer existing)

-->
