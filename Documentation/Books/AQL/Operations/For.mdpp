!CHAPTER FOR


The *FOR* keyword can be to iterate over all elements of an array.
The general syntax is:

```js
FOR variableName IN expression
```

There is also a special variant for graph traversals:

```js
FOR vertexVariableName, edgeVariableName, pathVariableName IN traversalExpression
```

For this special case see [the graph traversals chapter](../Graphs/Traversals.md).
For all other cases read on:

Each array element returned by *expression* is visited exactly once. It is
required that *expression* returns an array in all cases. The empty array is
allowed, too. The current array element is made available for further processing 
in the variable specified by *variableName*.

```
FOR u IN users
  RETURN u
```

This will iterate over all elements from the array *users* (note: this array
consists of all documents from the collection named "users" in this case) and
make the current array element available in variable *u*. *u* is not modified in
this example but simply pushed into the result using the *RETURN* keyword.

Note: When iterating over collection-based arrays as shown here, the order of
documents is undefined unless an explicit sort order is defined using a *SORT*
statement.

The variable introduced by *FOR* is available until the scope the *FOR* is
placed in is closed.

Another example that uses a statically declared array of values to iterate over:

```
FOR year IN [ 2011, 2012, 2013 ]
  RETURN { "year" : year, "isLeapYear" : year % 4 == 0 && (year % 100 != 0 || year % 400 == 0) }
```

Nesting of multiple *FOR* statements is allowed, too. When *FOR* statements are
nested, a cross product of the array elements returned by the individual *FOR*
statements will be created.

```
FOR u IN users
  FOR l IN locations
    RETURN { "user" : u, "location" : l }
```

In this example, there are two array iterations: an outer iteration over the array
*users* plus an inner iteration over the array *locations*. The inner array is
traversed as many times as there are elements in the outer array.  For each
iteration, the current values of *users* and *locations* are made available for
further processing in the variable *u* and *l*.
