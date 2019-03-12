Spreading
=========

<small>Introduced in: v3.5.0</small>

Array Spreading
---------------

The spread operator `...` can be used to expand an input array and substitute it with
all the array members as individual values instead.

For example, in the query:

```js
LET values = [2, 3, 4] 
RETURN [0, 1, ...values, 5]
```

… `...values` is expand to the values `2`, `3` and `4`, which are individually inserted
into the return array. This will produce a single flat return array `[0, 1, 2, 3, 4, 5]`.
Note that without using the array spread operator, the query would have produced a nested
array result `[0, 1, [2, 3, 4], 5]`.

This is especially useful when used in function call contexts. For example,

```js
LET values = ['the', 'quick', 'foxx', 'jumps', 'over', 'the', 'dog']
RETURN CONCAT(...values)
```

… is equivalent to:

```js
LET values = ['the', 'quick', 'foxx', 'jumps', 'over', 'the', 'dog'] 
RETURN CONCAT(values[0], values[1], values[2], values[3], values[4], values[5], values[6])
```

… and to:

```js
LET values = ['the', 'quick', 'foxx', 'jumps', 'over', 'the', 'dog']
RETURN APPLY("CONCAT", values)
```

Object Spreading
----------------

The spread operator `...` can also be used to expand an input object to merge
the attributes with the attribute of a target object.

Consider the following example query:

```js
LET name = { first: "John", last: "Doe" }
RETURN { last: "Smith", age: 39, ...name }
```

This is equivalent to the query:

```js
RETURN MERGE( { last: "Smith", age: 39}, { first: "John", last: "Doe" } )
```

Both queries will return the merged object, which will be:

```json
{ "first": "John", "last": "Doe", "age": 39 }
```
