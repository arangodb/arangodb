Data types
==========

AQL supports both *primitive* data types consisting of exactly one value and
*compound* data types comprised of multiple values. The following types are
available:

| Data type   | Description |
|------------:|-------------|
| **null**    | An empty value, also: the absence of a value
| **boolean** | Boolean truth value with possible values *false* and *true*
| **number**  | Signed (real) number
| **string**  | UTF-8 encoded text value
| **array** / list | Sequence of values, referred to by their positions
| **object** / document | Sequence of values, referred to by their names

Primitive types
---------------

### Null value

A `null` value can be used to represent an empty or absent value.
It is different from a numerical value of zero (`null != 0`) and other
*falsy* values (`false`, zero-length string, empty array or object).
It is also known as *nil* or *None* in other languages.

The system may return `null` in the absence of value, for example
if you call a [function](../Functions/README.md) with unsupported values
as arguments or if you try to [access an attribute](DocumentData.md)
which does not exist.

### Boolean data type

The Boolean data type has two possible values, `true` and `false`.
They represent the two truth values in logic and mathematics.

### Numeric literals

Numeric literals can be integers or real values (floating-point numbers).
They can optionally be signed with the `+` or `-` symbols.
A decimal point `.` is used as separator for the optional fractional part.
The scientific notation (*E-notation*) is also supported.

```
  1
 +1
 42
 -1
-42
  1.23
-99.99
  0.5
   .5
 -4.87e103
 -4.87E103
```

The following notations are invalid and will throw a syntax error:

```
 1.
01.23
00.23
00
```

All numeric values are treated as 64-bit double-precision values internally.
The internal format used is IEEE 754.

### String literals

String literals must be enclosed in single or double quotes. If the used quote
character is to be used itself within the string literal, it must be escaped
using the backslash symbol. A literal backslash also needs to be escaped with
a backslash.

```
"yikes!"
"don't know"
"this is a \"quoted\" word"
"this is a longer string."
"the path separator on Windows is \\"

'yikes!'
'don\'t know'
'this is a "quoted" word'
'this is a longer string.'
'the path separator on Windows is \\'
```

All string literals must be UTF-8 encoded. It is currently not possible to use
arbitrary binary data if it is not UTF-8 encoded. A workaround to use binary
data is to encode the data using [Base64](https://en.wikipedia.org/wiki/Base64)
or other algorithms on the application
side before storing, and decoding it on application side after retrieval.

Compound types
--------------

AQL supports two compound types:

- **array**: A composition of unnamed values, each accessible
  by their positions. Sometimes called *list*.
- **object**: A composition of named values, each accessible
  by their names. A *document* is an object at the top level.

### Arrays / Lists

The first supported compound type is the array type. Arrays are effectively
sequences of (unnamed / anonymous) values. Individual array elements can be
accessed by their positions. The order of elements in an array is important.

An *array declaration* starts with a left square bracket `[` and ends with
a right square bracket `]`. The declaration contains zero, one or more
*expression*s, separated from each other with the comma `,` symbol.
Whitespace around elements is ignored in the declaration, thus line breaks,
tab stops and blanks can be used for formatting.

In the easiest case, an array is empty and thus looks like:

```json
[ ]
```

Array elements can be any legal *expression* values. Nesting of arrays is
supported.

```json
[ true ]
[ 1, 2, 3 ]
[ -99, "yikes!", [ false, ["no"], [] ], 1 ]
[ [ "fox", "marshal" ] ]
```

Individual array values can later be accessed by their positions using the `[]`
accessor. The position of the accessed element must be a numeric
value. Positions start at 0. It is also possible to use negative index values
to access array values starting from the end of the array. This is convenient if
the length of the array is unknown and access to elements at the end of the array
is required.

```js
// access 1st array element (elements start at index 0)
u.friends[0]

// access 3rd array element
u.friends[2]

// access last array element
u.friends[-1]

// access second to last array element
u.friends[-2]
```

### Objects / Documents

The other supported compound type is the object (or document) type. Objects are a
composition of zero to many attributes. Each attribute is a name/value pair.
Object attributes can be accessed individually by their names. This data type is
also known as dictionary, map, associative array and other names.

Object declarations start with a left curly bracket `{` and end with a
right curly bracket `}`. An object contains zero to many attribute declarations,
separated from each other with the `,` symbol. Whitespace around elements is ignored
in the declaration, thus line breaks, tab stops and blanks can be used for formatting.

In the simplest case, an object is empty. Its declaration would then be:

```json
{ }
```

Each attribute in an object is a name/value pair. Name and value of an
attribute are separated using the colon `:` symbol. The name is always a string,
whereas the value can be of any type including sub-objects.

The attribute name is mandatory - there can't be anonymous values in an object.
It can be specified as a quoted or unquoted string:

```js
{ name: … }    // unquoted
{ 'name': … }  // quoted (apostrophe / "single quote mark")
{ "name": … }  // quoted (quotation mark / "double quote mark")
```

It must be quoted if it contains whitespace, escape sequences or characters
other than ASCII letters (`a`-`z`, `A`-`Z`), digits (`0`-`9`),
underscores (`_`) and dollar signs (`$`). The first character has to be a
letter, underscore or dollar sign.

If a [keyword](../Fundamentals/Syntax.md#keywords) is used as an attribute name
then the attribute name must be quoted or escaped by ticks or backticks:

```js
{ return: … }    // error, return is a keyword!
{ 'return': … }  // quoted
{ "return": … }  // quoted
{ `return`: … }  // escaped (backticks)
{ ´return´: … }  // escaped (ticks)
```

Attribute names can be computed using dynamic expressions, too.
To disambiguate regular attribute names from attribute name expressions,
computed attribute names must be enclosed in square brackets `[ … ]`:

```js
{ [ CONCAT("test/", "bar") ] : "someValue" }
```

There is also shorthand notation for attributes which is handy for
returning existing variables easily:

```js
LET name = "Peter"
LET age = 42
RETURN { name, age }
```

The above is the shorthand equivalent for the generic form:

```js
LET name = "Peter"
LET age = 42
RETURN { name: name, age: age }
```

Any valid expression can be used as an attribute value. That also means nested
objects can be used as attribute values:

```js
{ name : "Peter" }
{ "name" : "Vanessa", "age" : 15 }
{ "name" : "John", likes : [ "Swimming", "Skiing" ], "address" : { "street" : "Cucumber lane", "zip" : "94242" } }
```

Individual object attributes can later be accessed by their names using the
dot `.` accessor:

```js
u.address.city.name
u.friends[0].name.first
```

Attributes can also be accessed using the square bracket `[]` accessor:

```js
u["address"]["city"]["name"]
u["friends"][0]["name"]["first"]
```

In contrast to the dot accessor, the square brackets allow for expressions:

```js
LET attr1 = "friends"
LET attr2 = "name"
u[attr1][0][attr2][ CONCAT("fir", "st") ]
```

{% hint 'info' %}
If a non-existing attribute is accessed in one or the other way,
the result will be `null`, without error or warning.
{% endhint %}
