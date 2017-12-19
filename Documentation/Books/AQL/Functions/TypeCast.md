!CHAPTER Type cast functions

Some operators expect their operands to have a certain data type. For example,
logical operators expect their operands to be boolean values, and the arithmetic
operators expect their operands to be numeric values. If an operation is performed
with operands of other types, an automatic conversion to the expected types is
tried. This is called implicit type casting. It helps to avoid query
aborts.

Type casts can also be performed upon request by invoking a type cast function.
This is called explicit type casting. AQL offers several functions for this.
Each of the these functions takes an operand of any data type and returns a result
value with the type corresponding to the function name. For example, *TO_NUMBER()*
will return a numeric value.

!SUBSECTION TO_BOOL()

`TO_BOOL(value) → bool`

Take an input *value* of any type and convert it into the appropriate
boolean value.

- **value** (any): input of arbitrary type
- returns **bool** (boolean):
  - *null* is converted to *false*
  - Numbers are converted to *true*, except for 0, which is converted to *false*
  - Strings are converted to *true* if they are non-empty, and to *false* otherwise
  - Arrays are always converted to *true* (even if empty)
  - Objects / documents are always converted to *true*

It's also possible to use double negation to cast to boolean:

```js
!!1 // true
!!0 // false
!!-0.0 // false
not not 1 // true
!!"non-empty string" // true
!!"" // false
```

`TO_BOOL()` is preferred however, because it states the intention clearer.

!SUBSECTION TO_NUMBER()

`TO_NUMBER(value) → number`

Take an input *value* of any type and convert it into a numeric value.

- **value** (any): input of arbitrary type
- returns **number** (number):
  - *null* and *false* are converted to the value *0*
  - *true* is converted to *1*
  - Numbers keep their original value
  - Strings are converted to their numeric equivalent if the string contains a
    valid representation of a number. Whitespace at the start and end of the string
    is allowed. String values that do not contain any valid representation of a number
    will be converted to the number *0*.
  - An empty array is converted to *0*, an array with one member is converted into the
    result of `TO_NUMBER()` for its sole member. An array with two or more members is
    converted to the number *0*.
  - An object / document is converted to the number *0*.
  
    A unary plus will also cast to a number, but `TO_NUMBER()` is the preferred way:
    ```js
+'5' // 5
+[8] // 8
+[8,9] // 0
+{} // 0
    ```
    A unary minus works likewise, except that a numeric value is also negated:
    ```js
-'5' // -5
-[8] // -8
-[8,9] // 0
-{} // 0
    ```

!SUBSECTION TO_STRING()

`TO_STRING(value) → str`

Take an input *value* of any type and convert it into a string value.

- **value** (any): input of arbitrary type
- returns **str** (string):
  - *null* is converted to an empty string *""*
  - *false* is converted to the string *"false"*, *true* to the string *"true"*
  - Numbers are converted to their string representations. This can also be a
    scientific notation (e.g. "2e-7")
  - Arrays and objects / documents are converted to string representations,
    which means JSON-encoded strings with no additional whitespace

```js
TO_STRING(null) // ""
TO_STRING(true) // "true"
TO_STRING(false) // "false"
TO_STRING(123) // "123"
TO_STRING(+1.23) // "1.23"
TO_STRING(-1.23) // "-1.23"
TO_STRING(0.0000002) // "2e-7"
TO_STRING( [1, 2, 3] ) // "[1,2,3]"
TO_STRING( { foo: "bar", baz: null } ) // "{\"foo\":\"bar\",\"baz\":null}"
```

!SUBSECTION TO_ARRAY()

`TO_ARRAY(value) → array`

Take an input *value* of any type and convert it into an array value.

- **value** (any): input of arbitrary type
- returns **array** (array):
  - *null* is converted to an empty array
  - Boolean values, numbers and strings are converted to an array containing
    the original value as its single element
  - Arrays keep their original value
  - Objects / documents are converted to an array containing their attribute
    **values** as array elements, just like [VALUES()](Document.md#values)

```js
TO_ARRAY(null) // []
TO_ARRAY(false) // [false]
TO_ARRAY(true) // [true]
TO_ARRAY(5) // [5]
TO_ARRAY("foo") // ["foo"]
TO_ARRAY([1, 2, "foo"]) // [1, 2, "foo"]
TO_ARRAY({foo: 1, bar: 2, baz: [3, 4, 5]}) // [1, 2, [3, 4, 5]]
```

!SUBSECTION TO_LIST()

`TO_LIST(value) → array`

This is an alias for [TO_ARRAY()](#toarray).

!CHAPTER Type check functions

AQL also offers functions to check the data type of a value at runtime. The
following type check functions are available. Each of these functions takes an
argument of any data type and returns true if the value has the type that is
checked for, and false otherwise.

The following type check functions are available:

- `IS_NULL(value) → bool`: Check whether *value* is a *null* value, also see
  [HAS()](Document.md#has)

- `IS_BOOL(value) → bool`: Check whether *value* is a *boolean* value

- `IS_NUMBER(value) → bool`: Check whether *value* is a *numeric* value

- `IS_STRING(value) → bool`: Check whether *value* is a *string* value

- `IS_ARRAY(value) → bool`: Check whether *value* is an *array* value

- `IS_LIST(value) → bool`: This is an alias for *IS_ARRAY()*

- `IS_OBJECT(value) → bool`: Check whether *value* is an *object* /
  *document* value

- `IS_DOCUMENT(value) → bool`: This is an alias for *IS_OBJECT()*

- `IS_DATESTRING(value) → bool`: Check whether *value* is a string that can be used
  in a date function. This includes partial dates such as *"2015"* or *"2015-10"* and
  strings containing invalid dates such as *"2015-02-31"*. The function will return 
  false for all non-string values, even if some of them may be usable in date functions.

- `TYPENAME(value) → typeName`: Return the data type name of *value*. The data type
  name can be either *"null"*, *"bool"*, *"number"*, *"string"*, *"array"* or *"object"*.
