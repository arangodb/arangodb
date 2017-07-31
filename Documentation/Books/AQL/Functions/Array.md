Array functions
===============

AQL provides functions for higher-level array manipulation. Also see the
[numeric functions](Numeric.md) for functions that work on number arrays.
If you want to concatenate the elements of an array equivalent to `join()`
in JavaScript, see [CONCAT()](String.md#concat) and
[CONCAT_SEPARATOR()](String.md#concatseparator) in the string functions chapter.

Apart from that, AQL also offers several language constructs:

- simple [array access](../Fundamentals/DataTypes.md#arrays--lists) of individual elements,
- [array operators](../Advanced/ArrayOperators.md) for array expansion and contraction,
  optionally with inline filter, limit and projection,
- [array comparison operators](../Operators.md#array-comparison-operators) to compare
  each element in an array to a value or the elements of another array,
- loop-based operations using [FOR](../Operations/For.md),
  [SORT](../Operations/Sort.md), [LIMIT](../Operations/Limit.md),
  as well as [COLLECT](../Operations/Collect.md) for grouping,
  which also offers efficient aggregation.

### APPEND()

`APPEND(anyArray, values, unique) → newArray`

Add all elements of an array to another array. All values are added at the end of the
array (right side).

- **anyArray** (array): array with elements of arbitrary type
- **values** (array): array, whose elements shall be added to *anyArray*
- **unique** (bool, *optional*): if set to *true*, only those *values* will be added
  that are not already contained in *anyArray*. The default is *false*.
- returns **newArray** (array): the modified array

```js
APPEND([ 1, 2, 3 ], [ 5, 6, 9 ])
// [ 1, 2, 3, 5, 6, 9 ]

APPEND([ 1, 2, 3 ], [ 3, 4, 5, 2, 9 ], true)
// [ 1, 2, 3, 4, 5, 9 ]
```

### COUNT()

This is an alias for [LENGTH()](#length).

### FIRST()

`FIRST(anyArray) → firstElement`

Get the first element of an array. It is the same as `anyArray[0]`.

- **anyArray** (array): array with elements of arbitrary type
- returns **firstElement** (any|null): the first element of *anyArray*, or *null* if
  the array is empty.

### FLATTEN()

`FLATTEN(anyArray, depth) → flatArray`

Turn an array of arrays into a flat array. All array elements in *array* will be
expanded in the result array. Non-array elements are added as they are. The function
will recurse into sub-arrays up to the specified depth. Duplicates will not be removed.

- **array** (array): array with elements of arbitrary type, including nested arrays
- **depth** (number, *optional*):  flatten up to this many levels, the default is 1
- returns **flatArray** (array): a flattened array

```js
FLATTEN( [ 1, 2, [ 3, 4 ], 5, [ 6, 7 ], [ 8, [ 9, 10 ] ] ] )
// [ 1, 2, 3, 4, 5, 6, 7, 8, [ 9, 10 ] ]
```

To fully flatten the example array, use a *depth* of 2:

```js
FLATTEN( [ 1, 2, [ 3, 4 ], 5, [ 6, 7 ], [ 8, [ 9, 10 ] ] ], 2 )
// [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ]
```

### INTERSECTION()

`INTERSECTION(array1, array2, ... arrayN) → newArray`

Return the intersection of all arrays specified. The result is an array of values that
occur in all arguments.

- **arrays** (array, *repeatable*): an arbitrary number of arrays as multiple arguments
  (at least 2)
- returns **newArray** (array): a single array with only the elements, which exist in all
  provided arrays. The element order is random. Duplicates are removed.

### LAST()

`LAST(anyArray) → lastElement`

Get the last element of an array. It is the same as `anyArray[-1]`.

- **anyArray** (array): array with elements of arbitrary type
- returns **lastElement** (any|null): the last element of *anyArray* or *null* if the
  array is empty.

### LENGTH()

`LENGTH(anyArray) → length`

Determine the number of elements in an array.

- **anyArray** (array): array with elements of arbitrary type
- returns **length** (number): the number of array elements in *anyArray*.

*LENGTH()* can also determine the [number of attribute keys](Document.md#length)
of an object / document, the [amount of documents](Miscellaneous.md#length) in a
collection and the [character length](String.md#length) of a string.

|input|length|
|---|---|
|String|number of unicode characters|
|Number|number of unicode characters that represent the number|
|Array|number of elements|
|Object|number of first level elements|
|true|1|
|false|0|
|null|0|

### MINUS()

`MINUS(array1, array2, ... arrayN) → newArray`

Return the difference of all arrays specified.

- **arrays** (array, *repeatable*): an arbitrary number of arrays as multiple
  arguments (at least 2)
- returns **newArray** (array): an array of values that occur in the first array,
  but not in any of the subsequent arrays. The order of the result array is undefined
  and should not be relied on. Duplicates will be removed.

### NTH()

`NTH(anyArray, position) → nthElement`

Get the element of an array at a given position. It is the same as `anyArray[position]`
for positive positions, but does not support negative positions.

- **anyArray** (array): array with elements of arbitrary type
- **position** (number): position of desired element in array, positions start at 0
- returns **nthElement** (any|null): the array element at the given *position*.
  If *position* is negative or beyond the upper bound of the array,
  then *null* will be returned.

```js
NTH( [ "foo", "bar", "baz" ], 2 )  // "baz"
NTH( [ "foo", "bar", "baz" ], 3 )  // null
NTH( [ "foo", "bar", "baz" ], -1 ) // null
```

### OUTERSECTION()

`OUTERSECTION(array1, array2, ... arrayN) → newArray`

Return the values that occur only once across all arrays specified.

- **arrays** (array, *repeatable*): an arbitrary number of arrays as multiple arguments
  (at least 2)
- returns **newArray** (array): a single array with only the elements that exist only once
  across all provided arrays. The element order is random.

```js
OUTERSECTION( [ 1, 2, 3 ], [ 2, 3, 4 ], [ 3, 4, 5 ] )
// [ 1, 5 ]
```

### POP()

`POP(anyArray) → newArray`

Remove the element at the end (right side) of *array*.

- **anyArray** (array): an array with elements of arbitrary type
- returns **newArray** (array): *anyArray* without the last element. If it's already
  empty or has only a single element left, an empty array is returned.

```js
POP( [ 1, 2, 3, 4 ] ) // [ 1, 2, 3 ]
POP( [ 1 ] ) // []
```

### POSITION()

`POSITION(anyArray, search, returnIndex) → position`

Return whether *search* is contained in *array*. Optionally return the position.

- **anyArray** (array): the haystack, an array with elements of arbitrary type
- **search** (any): the needle, an element of arbitrary type
- **returnIndex** (bool, *optional*): if set to *true*, the position of the match
  is returned instead of a boolean. The default is *false*.
- returns **position** (bool|number): *true* if *search* is contained in *anyArray*,
  *false* otherwise. If *returnIndex* is enabled, the position of the match is
  returned (positions start at 0), or *-1* if it's not found.

### PUSH()

`PUSH(anyArray, value, unique) → newArray`

Append *value* to the array specified by *anyArray*.

- **anyArray** (array): array with elements of arbitrary type
- **value** (any): an element of arbitrary type
- **unique** (bool): if set to *true*, then *value* is not added if already
  present in the array. The default is *false*.
- returns **newArray** (array): *anyArray* with *value* added at the end
  (right side)

Note: The *unique* flag only controls if *value* is added if it's already present
in *anyArray*. Duplicate elements that already exist in *anyArray* will not be
removed. To make an array unique, use the [UNIQUE()](#unique) function.

```js
PUSH([ 1, 2, 3 ], 4)
// [ 1, 2, 3, 4 ]

PUSH([ 1, 2, 2, 3 ], 2, true)
// [ 1, 2, 2, 3 ]
```

### REMOVE_NTH()

`REMOVE_NTH(anyArray, position) → newArray`

Remove the element at *position* from the *anyArray*.

- **anyArray** (array): array with elements of arbitrary type
- **position** (number): the position of the element to remove. Positions start
  at 0. Negative positions are supported, with -1 being the last array element.
  If *position* is out of bounds, the array is returned unmodified.
- returns **newArray** (array): *anyArray* without the element at *position*

```js
REMOVE_NTH( [ "a", "b", "c", "d", "e" ], 1 )
// [ "a", "c", "d", "e" ]

REMOVE_NTH( [ "a", "b", "c", "d", "e" ], -2 )
// [ "a", "b", "c", "e" ]
```

### REMOVE_VALUE()

`REMOVE_VALUE(anyArray, value, limit) → newArray`

Remove all occurrences of *value* in *anyArray*. Optionally with a *limit*
to the number of removals.

- **anyArray** (array): array with elements of arbitrary type
- **value** (any): an element of arbitrary type
- **limit** (number, *optional*): cap the number of removals to this value
- returns **newArray** (array): *anyArray* with *value* removed

```js
REMOVE_VALUE( [ "a", "b", "b", "a", "c" ], "a" )
// [ "b", "b", "c" ]

REMOVE_VALUE( [ "a", "b", "b", "a", "c" ], "a", 1 )
// [ "b", "b", "a", "c" ]
```

### REMOVE_VALUES()

`REMOVE_VALUES(anyArray, values) → newArray`

Remove all occurrences of any of the *values* from *anyArray*.

- **anyArray** (array): array with elements of arbitrary type
- **values** (array): an array with elements of arbitrary type, that shall
  be removed from *anyArray*
- returns **newArray** (array): *anyArray* with all individual *values* removed

```js
REMOVE_VALUES( [ "a", "a", "b", "c", "d", "e", "f" ], [ "a", "f", "d" ] )
// [ "b", "c", "e" ]
```

### REVERSE()

`REVERSE(anyArray) → reversedArray`

Return an array with its elements reversed.

- **anyArray** (array): array with elements of arbitrary type
- returns **reversedArray** (array): a new array with all elements of *anyArray* in
  reversed order

### SHIFT()

`SHIFT(anyArray) → newArray`

Remove the element at the start (left side) of *anyArray*.

- **anyArray** (array): array with elements with arbitrary type
- returns **newArray** (array): *anyArray* without the left-most element. If *anyArray*
  is already empty or has only one element left, an empty array is returned.

```js
SHIFT( [ 1, 2, 3, 4 ] ) // [ 2, 3, 4 ]
SHIFT( [ 1 ] ) // []
```

### SLICE()

`SLICE(anyArray, start, length) → newArray`

Extract a slice of *anyArray*.

- **anyArray** (array): array with elements of arbitrary type
- **start** (number): start extraction at this element. Positions start at 0.
  Negative values indicate positions from the end of the array.
- **length** (number, *optional*): extract up to *length* elements, or all
  elements from *start* up to *length* if negative (exclusive)
- returns **newArray** (array): the specified slice of *anyArray*. If *length*
  is not specified, all array elements starting at *start* will be returned.

```js
SLICE( [ 1, 2, 3, 4, 5 ], 0, 1 ) // [ 1 ]
SLICE( [ 1, 2, 3, 4, 5 ], 1, 2 ) // [ 2, 3 ]
SLICE( [ 1, 2, 3, 4, 5 ], 3 ) // [ 4, 5 ]
SLICE( [ 1, 2, 3, 4, 5 ], 1, -1 ) // [ 2, 3, 4 ]
SLICE( [ 1, 2, 3, 4, 5 ], 0, -2 ) // [ 1, 2, 3 ]
SLICE( [ 1, 2, 3, 4, 5 ], -3, 2 ) // [ 3, 4 ]
```

### UNION()

`UNION(array1, array2, ... arrayN) → newArray`

Return the union of all arrays specified.

- **arrays** (array, *repeatable*): an arbitrary number of arrays as multiple
  arguments (at least 2)
- returns **newArray** (array): all array elements combined in a single array,
  in any order

```js
UNION(
    [ 1, 2, 3 ],
    [ 1, 2 ]
)
// [ 1, 2, 3, 1, 2 ]
```

Note: No duplicates will be removed. In order to remove duplicates, please use
either [UNION_DISTINCT()](#uniondistinct) or apply [UNIQUE()](#unique) on the
result of *UNION()*:

```js
UNIQUE(
    UNION(
        [ 1, 2, 3 ],
        [ 1, 2 ]
    )
)
// [ 1, 2, 3 ]
```

### UNION_DISTINCT()

`UNION_DISTINCT(array1, array2, ... arrayN) → newArray`

Return the union of distinct values of all arrays specified.

- **arrays** (array, *repeatable*): an arbitrary number of arrays as multiple
  arguments (at least 2)
- returns **newArray** (array): the elements of all given arrays in a single
  array, without duplicates, in any order

```js
UNION_DISTINCT(
    [ 1, 2, 3 ],
    [ 1, 2 ]
)
// [ 1, 2, 3 ]
```

### UNIQUE()

`UNIQUE(anyArray) → newArray`

Return all unique elements in *anyArray*. To determine uniqueness, the
function will use the comparison order.

- **anyArray** (array): array with elements of arbitrary type
- returns **newArray** (array): *anyArray* without duplicates, in any order

### UNSHIFT()

`UNSHIFT(anyArray, value, unique) → newArray`

Prepend *value* to *anyArray*.

- **anyArray** (array): array with elements of arbitrary type
- **value** (any): an element of arbitrary type
- **unique** (bool): if set to *true*, then *value* is not added if already
  present in the array. The default is *false*.
- returns **newArray** (array): *anyArray* with *value* added at the start
  (left side)

Note: The *unique* flag only controls if *value* is added if it's already present
in *anyArray*. Duplicate elements that already exist in *anyArray* will not be
removed. To make an array unique, use the [UNIQUE()](#unique) function.

```js
UNSHIFT( [ 1, 2, 3 ], 4 ) // [ 4, 1, 2, 3 ]
UNSHIFT( [ 1, 2, 3 ], 2, true ) // [ 1, 2, 3 ]
```
