!CHAPTER Common Errors

!SECTION String concatenation

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

<!--

Rename to Error Sources?
Include article about parameter injection from cookbook?

Quote marks around bind parameter placeholders
https://github.com/arangodb/arangodb/issues/1634#issuecomment-167808660

FILTER HAS(doc, "attr") instead of FITLER doc.attr / FILTER doc.attr != null

collection ... not found error, e.g. access of variable after COLLECT (no longer existing)

-->