!CHAPTER Data types

AQL supports both primitive and compound data types. The following types are
available:

- Primitive types: Consisting of exactly one value
  - null: An empty value, also: The absence of a value
  - bool: Boolean truth value with possible values *false* and *true*
  - number: Signed (real) number
  - string: UTF-8 encoded text value
- Compound types: Consisting of multiple values
  - array: Sequence of values, referred to by their positions
  - object / document: Sequence of values, referred to by their names

!SECTION Primitive types

!SUBSECTION Numeric literals

Numeric literals can be integers or real values. They can optionally be signed
using the *+* or *-* symbols. The scientific notation is also supported.

```
1
42
-1
-42
1.23
-99.99
0.1
-4.87e103
```

All numeric values are treated as 64-bit double-precision values internally.
The internal format used is IEEE 754.

!SUBSECTION String literals

String literals must be enclosed in single or double quotes. If the used quote
character is to be used itself within the string literal, it must be escaped
using the backslash symbol.  Backslash literals themselves also be escaped using
a backslash.

```
"yikes!"
"don't know"
"this is a \"quoted\" word"
"this is a longer string."
"the path separator on Windows is \\"

'yikes!'
'don\'t know'
'this is a longer string."
'the path separator on Windows is \\'
```

All string literals must be UTF-8 encoded. It is currently not possible to use
arbitrary binary data if it is not UTF-8 encoded. A workaround to use binary
data is to encode the data using base64 or other algorithms on the application
side before storing, and decoding it on application side after retrieval.

!SECTION Compound types

AQL supports two compound types:

- arrays: A composition of unnamed values, each accessible by their positions
- objects / documents: A composition of named values, each accessible by their names

!SUBSECTION Arrays / Lists

The first supported compound type is the array type. Arrays are effectively
sequences of (unnamed / anonymous) values. Individual array elements can be
accessed by their positions. The order of elements in an array is important.

An *array-declaration* starts with the *[* symbol and ends with the *]* symbol. An
*array-declaration* contains zero or many *expression*s, separated from each
other with the *,* symbol.

In the easiest case, an array is empty and thus looks like:

```json
[ ]
```

Array elements can be any legal *expression* values. Nesting of arrays is
supported.

```json
[ 1, 2, 3 ]
[ -99, "yikes!", [ true, [ "no"], [ ] ], 1 ]
[ [ "fox", "marshal" ] ]
```

Individual array values can later be accessed by their positions using the *[]*
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

!SUBSECTION Objects / Documents

The other supported compound type is the object (or document) type. Objects are a
composition of zero to many attributes. Each attribute is a name/value pair.
Object attributes can be accessed individually by their names.

Object declarations start with the *{* symbol and end with the *}* symbol. An
object contains zero to many attribute declarations, separated from each other
with the *,* symbol.  In the simplest case, an object is empty. Its
declaration would then be:

```json
{ }
```


Each attribute in an object is a name / value pair. Name and value of an
attribute are separated using the *:* symbol.

The attribute name is mandatory and must be specified as a quoted or unquoted
string. If a keyword is used as an attribute name, the attribute name must be quoted:

```js
{ return : 1 }     /* won't work */
{ "return" : 1 }   /* works ! */
{ `return` : 1 }   /* works, too! */
```

Since ArangoDB 2.6, object attribute names can be computed using dynamic expressions, too.
To disambiguate regular attribute names from attribute name expressions, computed
attribute names must be enclosed in *[* and *]*:

```js
{ [ CONCAT("test/", "bar") ] : "someValue" }
```

Since ArangoDB 2.7, there is also shorthand notation for attributes which is handy for
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
RETURN { name : name, age : age }
```

Any valid expression can be used as an attribute value. That also means nested
objects can be used as attribute values:

```json
{ name : "Peter" }
{ "name" : "Vanessa", "age" : 15 }
{ "name" : "John", likes : [ "Swimming", "Skiing" ], "address" : { "street" : "Cucumber lane", "zip" : "94242" } }
```

Individual object attributes can later be accessed by their names using the
*.* accessor:

```js
u.address.city.name
u.friends[0].name.first
```

Attributes can also be accessed using the *[]* accessor:

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

Note that if a non-existing attribute is accessed in one or the other way,
the result will be *null*, without error or warning.
