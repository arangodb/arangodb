---
layout: default
description: The versatile FOR keyword can be used to iterate over a collection or View, all elements of an array or to traverse a graph.
title: FOR Operations in AQL
---
FOR
===

The versatile `FOR` keyword can be used to iterate over a collection or View,
all elements of an array or to traverse a graph.

## General Syntax

```js
FOR variableName IN expression
```

There is also a special variant for [graph traversals](graphs-traversals.html):

```js
FOR vertexVariableName, edgeVariableName, pathVariableName IN traversalExpression
```

For Views, there is a special (optional) [`SEARCH` keyword](operations-search.html):

```js
FOR variableName IN viewName SEARCH searchExpression
```

Note that Views cannot be used as edge collections in traversals:

```js
FOR v IN 1..3 ANY startVertex viewName /* invalid! */
```

Each array element returned by *expression* is visited exactly once. It is
required that *expression* returns an array in all cases. The empty array is
allowed, too. The current array element is made available for further processing 
in the variable specified by *variableName*.

```js
FOR u IN users
  RETURN u
```

This will iterate over all elements from the array *users* (note: this array
consists of all documents from the collection named "users" in this case) and
make the current array element available in variable *u*. *u* is not modified in
this example but simply pushed into the result using the `RETURN` keyword.

Note: When iterating over collection-based arrays as shown here, the order of
documents is undefined unless an explicit sort order is defined using a `SORT`
statement.

The variable introduced by `FOR` is available until the scope the `FOR` is
placed in is closed.

Another example that uses a statically declared array of values to iterate over:

```js
FOR year IN [ 2011, 2012, 2013 ]
  RETURN { "year" : year, "isLeapYear" : year % 4 == 0 && (year % 100 != 0 || year % 400 == 0) }
```

Nesting of multiple `FOR` statements is allowed, too. When `FOR` statements are
nested, a cross product of the array elements returned by the individual `FOR`
statements will be created.

```js
FOR u IN users
  FOR l IN locations
    RETURN { "user" : u, "location" : l }
```

In this example, there are two array iterations: an outer iteration over the array
*users* plus an inner iteration over the array *locations*. The inner array is
traversed as many times as there are elements in the outer array.  For each
iteration, the current values of *users* and *locations* are made available for
further processing in the variable *u* and *l*.

## Options

For collections and views, the `FOR` construct supports an optional `OPTIONS`
suffix to modify behavior. The general syntax is:

```js
FOR variableName IN expression OPTIONS {option: value, ...}
```

### Index hints

For collections, index hints are provided though this inline options mechanism.
Hints can be specified in two different formats.

The first format option is the simplest, just a single index name. This should
be sufficient for many cases. Whenever there is a choice to potentially use an
index for this `FOR` loop, the optimizer will first check if the specified index
can be used. If so, it will use it, regardless of whether it would normally use
a different index. If it cannot use that index, then it will fall back to its
normal logic to select another index. If the optional `forceIndexHint: true` is
specified, then it will not fall back, and instead generate an error.

```js
OPTIONS {indexHint: 'byName'[, forceIndexHint: <boolean>]}
```

The second is an array of index names, in order of preference. When specified
this way, the optimizer will behave much in the same way as above, but will
check the feasibility of each of the specified indices, in the order they are
given, falling back to its normal logic or failing only if none of the specified
indices are feasible.

```js
OPTIONS {indexHint: ['byName', 'byColor'][, forceIndexHint: <boolean>]}
```
