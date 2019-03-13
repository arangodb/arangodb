# Array functions

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
- loop-based operations on arrays using [FOR](../Operations/For.md),
  [SORT](../Operations/Sort.md),
  [LIMIT](../Operations/Limit.md),
  as well as [COLLECT](../Operations/Collect.md) for grouping,
  which also offers efficient aggregation.

## APPEND()

`APPEND(anyArray, values, unique) → newArray`

Add all elements of an array to another array. All values are added at the end of the
array (right side).

It can also be used to append a single element to an array. It is not necessary to wrap
it in an array (unless it is an array itself). You may also use [PUSH()](#push) instead.

- **anyArray** (array): array with elements of arbitrary type
- **values** (array|any): array, whose elements shall be added to *anyArray*
- **unique** (bool, *optional*): if set to *true*, only those *values* will be added
  that are not already contained in *anyArray*. The default is *false*.
- returns **newArray** (array): the modified array

**Examples**

@startDocuBlockInline aqlArrayAppend_1
@EXAMPLE_AQL{aqlArrayAppend_1}
RETURN APPEND([ 1, 2, 3 ], [ 5, 6, 9 ])
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayAppend_1

@startDocuBlockInline aqlArrayAppend_2
@EXAMPLE_AQL{aqlArrayAppend_2}
RETURN APPEND([ 1, 2, 3 ], [ 3, 4, 5, 2, 9 ], true)
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayAppend_2

## CONTAINS_ARRAY()

This is an alias for [POSITION()](#position).


## COUNT()

This is an alias for [LENGTH()](#length).

## COUNT_DISTINCT()

`COUNT_DISTINCT(anyArray) → number`

Get the number of distinct elements in an array.

- **anyArray** (array): array with elements of arbitrary type
- returns **number**: the number of distinct elements in *anyArray*.

**Examples**

@startDocuBlockInline aqlArrayCountDistinct_1
@EXAMPLE_AQL{aqlArrayCountDistinct_1}
RETURN COUNT_DISTINCT([ 1, 2, 3 ])
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayCountDistinct_1

@startDocuBlockInline aqlArrayCountDistinct_2
@EXAMPLE_AQL{aqlArrayCountDistinct_2}
RETURN COUNT_DISTINCT([ "yes", "no", "yes", "sauron", "no", "yes" ])
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayCountDistinct_2

## COUNT_UNIQUE()

This is an alias for [COUNT_DISTINCT()](#countdistinct).

## FIRST()

`FIRST(anyArray) → firstElement`

Get the first element of an array. It is the same as `anyArray[0]`.

- **anyArray** (array): array with elements of arbitrary type
- returns **firstElement** (any|null): the first element of *anyArray*, or *null* if
  the array is empty.

**Examples**

@startDocuBlockInline aqlArrayFirst_1
@EXAMPLE_AQL{aqlArrayFirst_1}
RETURN FIRST([ 1, 2, 3 ])
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayFirst_1

@startDocuBlockInline aqlArrayFirst_2
@EXAMPLE_AQL{aqlArrayFirst_2}
RETURN FIRST([])
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayFirst_2

## FLATTEN()

`FLATTEN(anyArray, depth) → flatArray`

Turn an array of arrays into a flat array. All array elements in *array* will be
expanded in the result array. Non-array elements are added as they are. The function
will recurse into sub-arrays up to the specified depth. Duplicates will not be removed.

Also see [array contraction](../Advanced/ArrayOperators.md#array-contraction).

- **array** (array): array with elements of arbitrary type, including nested arrays
- **depth** (number, *optional*):  flatten up to this many levels, the default is 1
- returns **flatArray** (array): a flattened array

**Examples**

@startDocuBlockInline aqlArrayFlatten_1
@EXAMPLE_AQL{aqlArrayFlatten_1}
RETURN FLATTEN( [ 1, 2, [ 3, 4 ], 5, [ 6, 7 ], [ 8, [ 9, 10 ] ] ] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayFlatten_1

To fully flatten the example array, use a *depth* of 2:

@startDocuBlockInline aqlArrayFlatten_2
@EXAMPLE_AQL{aqlArrayFlatten_2}
RETURN FLATTEN( [ 1, 2, [ 3, 4 ], 5, [ 6, 7 ], [ 8, [ 9, 10 ] ] ], 2 )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayFlatten_2

## INTERSECTION()

`INTERSECTION(array1, array2, ... arrayN) → newArray`

Return the intersection of all arrays specified. The result is an array of values that
occur in all arguments.

Other set operations are [UNION()](#union),
[MINUS()](#minus) and
[OUTERSECTION()](#outersection).

- **arrays** (array, *repeatable*): an arbitrary number of arrays as multiple arguments
  (at least 2)
- returns **newArray** (array): a single array with only the elements, which exist in all
  provided arrays. The element order is random. Duplicates are removed.

**Examples**

@startDocuBlockInline aqlArrayIntersection_1
@EXAMPLE_AQL{aqlArrayIntersection_1}
RETURN INTERSECTION( [1,2,3,4,5], [2,3,4,5,6], [3,4,5,6,7] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayIntersection_1

@startDocuBlockInline aqlArrayIntersection_2
@EXAMPLE_AQL{aqlArrayIntersection_2}
RETURN INTERSECTION( [2,4,6], [8,10,12], [14,16,18] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayIntersection_2

## LAST()

`LAST(anyArray) → lastElement`

Get the last element of an array. It is the same as `anyArray[-1]`.

- **anyArray** (array): array with elements of arbitrary type
- returns **lastElement** (any|null): the last element of *anyArray* or *null* if the
  array is empty.

**Example**

@startDocuBlockInline aqlArrayLast_1
@EXAMPLE_AQL{aqlArrayLast_1}
RETURN LAST( [1,2,3,4,5] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayLast_1

## LENGTH()

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

**Examples**

@startDocuBlockInline aqlArrayLength_1
@EXAMPLE_AQL{aqlArrayLength_1}
RETURN LENGTH( "🥑" )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayLength_1

@startDocuBlockInline aqlArrayLength_2
@EXAMPLE_AQL{aqlArrayLength_2}
RETURN LENGTH( 1234 )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayLength_2

@startDocuBlockInline aqlArrayLength_3
@EXAMPLE_AQL{aqlArrayLength_3}
RETURN LENGTH( [1,2,3,4,5,6,7] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayLength_3

@startDocuBlockInline aqlArrayLength_4
@EXAMPLE_AQL{aqlArrayLength_4}
RETURN LENGTH( [1,2,3,4,5,6,7] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayLength_4

@startDocuBlockInline aqlArrayLength_5
@EXAMPLE_AQL{aqlArrayLength_5}
RETURN LENGTH( {a:1, b:2, c:3, d:4, e:{f:5,g:6}} )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayLength_5

## MINUS()

`MINUS(array1, array2, ... arrayN) → newArray`

Return the difference of all arrays specified.

Other set operations are [UNION()](#union),
[INTERSECTION()](#intersection) and
[OUTERSECTION()](#outersection).

- **arrays** (array, *repeatable*): an arbitrary number of arrays as multiple
  arguments (at least 2)
- returns **newArray** (array): an array of values that occur in the first array,
  but not in any of the subsequent arrays. The order of the result array is undefined
  and should not be relied on. Duplicates will be removed.

**Example**

@startDocuBlockInline aqlArrayMinus_1
@EXAMPLE_AQL{aqlArrayMinus_1}
RETURN MINUS( [1,2,3,4], [3,4,5,6], [5,6,7,8] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayMinus_1

## NTH()

`NTH(anyArray, position) → nthElement`

Get the element of an array at a given position. It is the same as `anyArray[position]`
for positive positions, but does not support negative positions.

- **anyArray** (array): array with elements of arbitrary type
- **position** (number): position of desired element in array, positions start at 0
- returns **nthElement** (any|null): the array element at the given *position*.
  If *position* is negative or beyond the upper bound of the array,
  then *null* will be returned.

**Examples**

@startDocuBlockInline aqlArrayNth_1
@EXAMPLE_AQL{aqlArrayNth_1}
RETURN NTH( [ "foo", "bar", "baz" ], 2 )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayNth_1

@startDocuBlockInline aqlArrayNth_2
@EXAMPLE_AQL{aqlArrayNth_2}
RETURN NTH( [ "foo", "bar", "baz" ], 3 )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayNth_2

@startDocuBlockInline aqlArrayNth_3
@EXAMPLE_AQL{aqlArrayNth_3}
RETURN NTH( [ "foo", "bar", "baz" ], -1 )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayNth_3

## OUTERSECTION()

`OUTERSECTION(array1, array2, ... arrayN) → newArray`

Return the values that occur only once across all arrays specified.

Other set operations are [UNION()](#union),
[MINUS()](#minus) and
[INTERSECTION()](#intersection).

- **arrays** (array, *repeatable*): an arbitrary number of arrays as multiple arguments
  (at least 2)
- returns **newArray** (array): a single array with only the elements that exist only once
  across all provided arrays. The element order is random.

**Example**

@startDocuBlockInline aqlArrayOutersection_1
@EXAMPLE_AQL{aqlArrayOutersection_1}
RETURN OUTERSECTION( [ 1, 2, 3 ], [ 2, 3, 4 ], [ 3, 4, 5 ] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayOutersection_1

## POP()

`POP(anyArray) → newArray`

Remove the last element of *array*.

To append an element (right side), see [PUSH()](#push).<br>
To remove the first element, see [SHIFT()](#shift).<br>
To remove an element at an arbitrary position, see [REMOVE_NTH()](#removenth).

- **anyArray** (array): an array with elements of arbitrary type
- returns **newArray** (array): *anyArray* without the last element. If it's already
  empty or has only a single element left, an empty array is returned.

**Examples**

@startDocuBlockInline aqlArrayPop_1
@EXAMPLE_AQL{aqlArrayPop_1}
RETURN POP( [ 1, 2, 3, 4 ] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayPop_1

@startDocuBlockInline aqlArrayPop_2
@EXAMPLE_AQL{aqlArrayPop_2}
RETURN POP( [ 1 ] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayPop_2

## POSITION()

`POSITION(anyArray, search, returnIndex) → position`

Return whether *search* is contained in *array*. Optionally return the position.

- **anyArray** (array): the haystack, an array with elements of arbitrary type
- **search** (any): the needle, an element of arbitrary type
- **returnIndex** (bool, *optional*): if set to *true*, the position of the match
  is returned instead of a boolean. The default is *false*.
- returns **position** (bool|number): *true* if *search* is contained in *anyArray*,
  *false* otherwise. If *returnIndex* is enabled, the position of the match is
  returned (positions start at 0), or *-1* if it's not found.

To determine if or at which position a string occurs in another string, see the
[CONTAINS() string function](String.md#contains).

**Examples**

@startDocuBlockInline aqlArrayPosition_1
@EXAMPLE_AQL{aqlArrayPosition_1}
RETURN POSITION( [2,4,6,8], 4 )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayPosition_1

@startDocuBlockInline aqlArrayPosition_2
@EXAMPLE_AQL{aqlArrayPosition_2}
RETURN POSITION( [2,4,6,8], 4, true )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayPosition_2

## PUSH()

`PUSH(anyArray, value, unique) → newArray`

Append *value* to *anyArray* (right side).

To remove the last element, see [POP()](#pop).<br>
To prepend a value (left side), see [UNSHIFT()](#unshift).<br>
To append multiple elements, see [APPEND()](#append).

- **anyArray** (array): array with elements of arbitrary type
- **value** (any): an element of arbitrary type
- **unique** (bool): if set to *true*, then *value* is not added if already
  present in the array. The default is *false*.
- returns **newArray** (array): *anyArray* with *value* added at the end
  (right side)

Note: The *unique* flag only controls if *value* is added if it's already present
in *anyArray*. Duplicate elements that already exist in *anyArray* will not be
removed. To make an array unique, use the [UNIQUE()](#unique) function.

**Examples**

@startDocuBlockInline aqlArrayPush_1
@EXAMPLE_AQL{aqlArrayPush_1}
RETURN PUSH([ 1, 2, 3 ], 4)
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayPush_1

@startDocuBlockInline aqlArrayPush_2
@EXAMPLE_AQL{aqlArrayPush_2}
RETURN PUSH([ 1, 2, 2, 3 ], 2, true)
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayPush_2

## REMOVE_NTH()

`REMOVE_NTH(anyArray, position) → newArray`

Remove the element at *position* from the *anyArray*.

To remove the first element, see [SHIFT()](#shift).<br>
To remove the last element, see [POP()](#pop).

- **anyArray** (array): array with elements of arbitrary type
- **position** (number): the position of the element to remove. Positions start
  at 0. Negative positions are supported, with -1 being the last array element.
  If *position* is out of bounds, the array is returned unmodified.
- returns **newArray** (array): *anyArray* without the element at *position*

**Examples**

@startDocuBlockInline aqlArrayRemoveNth_1
@EXAMPLE_AQL{aqlArrayRemoveNth_1}
RETURN REMOVE_NTH( [ "a", "b", "c", "d", "e" ], 1 )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayRemoveNth_1

@startDocuBlockInline aqlArrayRemoveNth_2
@EXAMPLE_AQL{aqlArrayRemoveNth_2}
RETURN REMOVE_NTH( [ "a", "b", "c", "d", "e" ], -2 )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayRemoveNth_2

## REMOVE_VALUE()

`REMOVE_VALUE(anyArray, value, limit) → newArray`

Remove all occurrences of *value* in *anyArray*. Optionally with a *limit*
to the number of removals.

- **anyArray** (array): array with elements of arbitrary type
- **value** (any): an element of arbitrary type
- **limit** (number, *optional*): cap the number of removals to this value
- returns **newArray** (array): *anyArray* with *value* removed

**Examples**

@startDocuBlockInline aqlArrayRemoveValue_1
@EXAMPLE_AQL{aqlArrayRemoveValue_1}
RETURN REMOVE_VALUE( [ "a", "b", "b", "a", "c" ], "a" )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayRemoveValue_1

@startDocuBlockInline aqlArrayRemoveValue_2
@EXAMPLE_AQL{aqlArrayRemoveValue_2}
RETURN REMOVE_VALUE( [ "a", "b", "b", "a", "c" ], "a", 1 )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayRemoveValue_2

## REMOVE_VALUES()

`REMOVE_VALUES(anyArray, values) → newArray`

Remove all occurrences of any of the *values* from *anyArray*.

- **anyArray** (array): array with elements of arbitrary type
- **values** (array): an array with elements of arbitrary type, that shall
  be removed from *anyArray*
- returns **newArray** (array): *anyArray* with all individual *values* removed

**Example**

@startDocuBlockInline aqlArrayRemoveValues_1
@EXAMPLE_AQL{aqlArrayRemoveValues_1}
RETURN REMOVE_VALUES( [ "a", "a", "b", "c", "d", "e", "f" ], [ "a", "f", "d" ] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayRemoveValues_1

## REVERSE()

`REVERSE(anyArray) → reversedArray`

Return an array with its elements reversed.

- **anyArray** (array): array with elements of arbitrary type
- returns **reversedArray** (array): a new array with all elements of *anyArray* in
  reversed order

**Example**

@startDocuBlockInline aqlArrayReverse_1
@EXAMPLE_AQL{aqlArrayReverse_1}
RETURN REVERSE ( [2,4,6,8,10] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayReverse_1

## SHIFT()

`SHIFT(anyArray) → newArray`

Remove the first element of *anyArray*.

To prepend an element (left side), see [UNSHIFT()](#unshift).<br>
To remove the last element, see [POP()](#pop).<br>
To remove an element at an arbitrary position, see [REMOVE_NTH()](#removenth).

- **anyArray** (array): array with elements with arbitrary type
- returns **newArray** (array): *anyArray* without the left-most element. If *anyArray*
  is already empty or has only one element left, an empty array is returned.

**Examples**

@startDocuBlockInline aqlArrayShift_1
@EXAMPLE_AQL{aqlArrayShift_1}
RETURN SHIFT( [ 1, 2, 3, 4 ] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayShift_1

@startDocuBlockInline aqlArrayShift_2
@EXAMPLE_AQL{aqlArrayShift_2}
RETURN SHIFT( [ 1 ] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayShift_2

## SLICE()

`SLICE(anyArray, start, length) → newArray`

Extract a slice of *anyArray*.

- **anyArray** (array): array with elements of arbitrary type
- **start** (number): start extraction at this element. Positions start at 0.
  Negative values indicate positions from the end of the array.
- **length** (number, *optional*): extract up to *length* elements, or all
  elements from *start* up to *length* if negative (exclusive)
- returns **newArray** (array): the specified slice of *anyArray*. If *length*
  is not specified, all array elements starting at *start* will be returned.

**Examples**

@startDocuBlockInline aqlArraySlice_1
@EXAMPLE_AQL{aqlArraySlice_1}
RETURN SLICE( [ 1, 2, 3, 4, 5 ], 0, 1 )
@END_EXAMPLE_AQL
@endDocuBlock aqlArraySlice_1

@startDocuBlockInline aqlArraySlice_2
@EXAMPLE_AQL{aqlArraySlice_2}
RETURN SLICE( [ 1, 2, 3, 4, 5 ], 1, 2 )
@END_EXAMPLE_AQL
@endDocuBlock aqlArraySlice_2

@startDocuBlockInline aqlArraySlice_3
@EXAMPLE_AQL{aqlArraySlice_3}
RETURN SLICE( [ 1, 2, 3, 4, 5 ], 3 )
@END_EXAMPLE_AQL
@endDocuBlock aqlArraySlice_3

@startDocuBlockInline aqlArraySlice_4
@EXAMPLE_AQL{aqlArraySlice_4}
RETURN SLICE( [ 1, 2, 3, 4, 5 ], 1, -1 )
@END_EXAMPLE_AQL
@endDocuBlock aqlArraySlice_4

@startDocuBlockInline aqlArraySlice_5
@EXAMPLE_AQL{aqlArraySlice_5}
RETURN SLICE( [ 1, 2, 3, 4, 5 ], 0, -2 )
@END_EXAMPLE_AQL
@endDocuBlock aqlArraySlice_5

@startDocuBlockInline aqlArraySlice_6
@EXAMPLE_AQL{aqlArraySlice_6}
RETURN SLICE( [ 1, 2, 3, 4, 5 ], -3, 2 )
@END_EXAMPLE_AQL
@endDocuBlock aqlArraySlice_6

## SORTED()

`SORTED(anyArray) → newArray`

Sort all elements in *anyArray*. The function will use the default comparison 
order for AQL value types.

- **anyArray** (array): array with elements of arbitrary type
- returns **newArray** (array): *anyArray*, with elements sorted

**Example**

@startDocuBlockInline aqlArraySorted_1
@EXAMPLE_AQL{aqlArraySorted_1}
RETURN SORTED( [ 8,4,2,10,6 ] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArraySorted_1

## SORTED_UNIQUE()

`SORTED_UNIQUE(anyArray) → newArray`

Sort all elements in *anyArray*. The function will use the default comparison 
order for AQL value types. Additionally, the values in the result array will
be made unique.

- **anyArray** (array): array with elements of arbitrary type
- returns **newArray** (array): *anyArray*, with elements sorted and duplicates
  removed

**Example**

@startDocuBlockInline aqlArraySortedUnique_1
@EXAMPLE_AQL{aqlArraySortedUnique_1}
RETURN SORTED_UNIQUE( [ 8,4,2,10,6,2,8,6,4 ] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArraySortedUnique_1

## UNION()

`UNION(array1, array2, ... arrayN) → newArray`

Return the union of all arrays specified.

Other set operations are [MINUS()](#minus),
[INTERSECTION()](#intersection) and
[OUTERSECTION()](#outersection).

- **arrays** (array, *repeatable*): an arbitrary number of arrays as multiple
  arguments (at least 2)
- returns **newArray** (array): all array elements combined in a single array,
  in any order

**Examples**

@startDocuBlockInline aqlArrayUnion_1
@EXAMPLE_AQL{aqlArrayUnion_1}
RETURN UNION(
    [ 1, 2, 3 ],
    [ 1, 2 ]
)
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayUnion_1

Note: No duplicates will be removed. In order to remove duplicates, please use
either [UNION_DISTINCT()](#uniondistinct)
or apply [UNIQUE()](#unique) on the
result of *UNION()*:

@startDocuBlockInline aqlArrayUnion_2
@EXAMPLE_AQL{aqlArrayUnion_2}
RETURN UNIQUE(
    UNION(
        [ 1, 2, 3 ],
        [ 1, 2 ]
    )
)
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayUnion_2

## UNION_DISTINCT()

`UNION_DISTINCT(array1, array2, ... arrayN) → newArray`

Return the union of distinct values of all arrays specified.

- **arrays** (array, *repeatable*): an arbitrary number of arrays as multiple
  arguments (at least 2)
- returns **newArray** (array): the elements of all given arrays in a single
  array, without duplicates, in any order

**Example**

@startDocuBlockInline aqlArrayUnionDistinct_1
@EXAMPLE_AQL{aqlArrayUnionDistinct_1}
RETURN UNION_DISTINCT(
    [ 1, 2, 3 ],
    [ 1, 2 ]
)
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayUnionDistinct_1

## UNIQUE()

`UNIQUE(anyArray) → newArray`

Return all unique elements in *anyArray*. To determine uniqueness, the
function will use the comparison order.

- **anyArray** (array): array with elements of arbitrary type
- returns **newArray** (array): *anyArray* without duplicates, in any order

**Example**

@startDocuBlockInline aqlArrayUnique_1
@EXAMPLE_AQL{aqlArrayUnique_1}
RETURN UNIQUE( [ 1,2,2,3,3,3,4,4,4,4,5,5,5,5,5 ] )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayUnique_1

## UNSHIFT()

`UNSHIFT(anyArray, value, unique) → newArray`

Prepend *value* to *anyArray* (left side).

To remove the first element, see [SHIFT()](#shift).<br>
To append a value (right side), see [PUSH()](#push).

- **anyArray** (array): array with elements of arbitrary type
- **value** (any): an element of arbitrary type
- **unique** (bool): if set to *true*, then *value* is not added if already
  present in the array. The default is *false*.
- returns **newArray** (array): *anyArray* with *value* added at the start
  (left side)

Note: The *unique* flag only controls if *value* is added if it's already present
in *anyArray*. Duplicate elements that already exist in *anyArray* will not be
removed. To make an array unique, use the [UNIQUE()](#unique) function.

**Examples**

@startDocuBlockInline aqlArrayUnshift_1
@EXAMPLE_AQL{aqlArrayUnshift_1}
RETURN UNSHIFT( [ 1, 2, 3 ], 4 )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayUnshift_1

@startDocuBlockInline aqlArrayUnshift_2
@EXAMPLE_AQL{aqlArrayUnshift_2}
RETURN UNSHIFT( [ 1, 2, 3 ], 2, true )
@END_EXAMPLE_AQL
@endDocuBlock aqlArrayUnshift_2
