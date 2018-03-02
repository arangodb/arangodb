Advanced features
=================

### Subqueries

Wherever an expression is allowed in AQL, a subquery can be placed. A subquery
is a query part that can introduce its own local variables without affecting
variables and values in its outer scope(s).

It is required that subqueries be put inside parentheses *(* and *)* to
explicitly mark their start and end points:

```js
FOR p IN persons
  LET recommendations = (
    FOR r IN recommendations
      FILTER p.id == r.personId
      SORT p.rank DESC
      LIMIT 10
      RETURN r
  )
  RETURN { person : p, recommendations : recommendations }
```

```js
FOR p IN persons
  COLLECT city = p.city INTO g
  RETURN {
    city : city,
    numPersons : LENGTH(g),
    maxRating: MAX(
      FOR r IN g
      RETURN r.p.rating
    )}
```

Subqueries may also include other subqueries.

### Array expansion

In order to access a named attribute from all elements in an array easily, AQL
offers the shortcut operator <i>[\*]</i> for array variable expansion.

Using the <i>[\*]</i> operator with an array variable will iterate over all elements 
in the array, thus allowing to access a particular attribute of each element.  It is
required that the expanded variable is an array.  The result of the <i>[\*]</i>
operator is again an array.

To demonstrate the array expansion operator, let's go on with the following three 
example *users* documents:

```json
[
  {
    name: "john",
    age: 35,
    friends: [
      { name: "tina", age: 43 },
      { name: "helga", age: 52 },
      { name: "alfred", age: 34 }
    ]
  },
  {
    name: "yves",
    age: 24,
    friends: [
      { name: "sergei", age: 27 },
      { name: "tiffany", age: 25 }
    ]
  },
  {
    name: "sandra",
    age: 40,
    friends: [
      { name: "bob", age: 32 },
      { name: "elena", age: 48 }
    ]
  }
]
```

With the <i>[\*]</i> operator it becomes easy to query just the names of the
friends for each user:

```
FOR u IN users
  RETURN { name: u.name, friends: u.friends[*].name }
```

This will produce:

```json
[
  { "name" : "john", "friends" : [ "tina", "helga", "alfred" ] },
  { "name" : "yves", "friends" : [ "sergei", "tiffany" ] },
  { "name" : "sandra", "friends" : [ "bob", "elena" ] }
]
```

This a shortcut for the longer, semantically equivalent query:

```
FOR u IN users
  RETURN { name: u.name, friends: (FOR f IN u.friends RETURN f.name) }
```

While producing a result with the <i>[\*]</i> operator, it is also possible
to filter while iterating over the array, and to create a projection using the
current array element.

For example, to return only the names of friends that have an *age* value
higher than the user herself an inline *FILTER* can be used:

```
FOR u IN users
  RETURN { name: u.name, friends: u.friends[* FILTER CURRENT.age > u.age].name }
```

The pseudo-variable *CURRENT* can be used to access the current array element.
The *FILTER* condition can refer to *CURRENT* or any variables valid in the
outer scope.

To return a projection of the current element, use *RETURN*. If a *FILTER* is
also present, *RETURN* must come later.

```
FOR u IN users
  RETURN u.friends[* RETURN CONCAT(CURRENT.name, " is a friend of ", u.name)]
```

The above will return:

```json
[
  [
    "tina is a friend of john",
    "helga is a friend of john",
    "alfred is a friend of john"
  ],
  [
    "sergei is a friend of yves",
    "tiffany is a friend of yves"
  ],
  [
    "bob is a friend of sandra",
    "elena is a friend of sandra"
  ]
]
```

### Array contraction

In order to collapse (or flatten) results in nested arrays, AQL provides the <i>[\*\*]</i> 
operator. It works similar to the <i>[\*]</i> operator, but additionally collapses nested
arrays. How many levels are collapsed is determined by the amount of <i>\*</i> characters used.

For example, the following query produces an array of friend names per user:

```
FOR u IN users
  RETURN u.friends[*].name
```

As we have multiple users, the overall result is a nested array:

```json
[
  [
    "tina",
    "helga",
    "alfred"
  ],
  [
    "sergei",
    "tiffany"
  ],
  [
    "bob",
    "elena"
  ]
]
```

If the goal is to get rid of the nested array, we can apply the <i>[\*\*]</i> operator on the 
result. Simplying appending <i>[\*\*]</i> to the query won't help, because *u.friends*
is not a nested (multi-dimensional) array, but a simple (one-dimensional) array. Still, 
the <i>[\*\*]</i> can be used if it has access to a multi-dimensional nested result.

We can easily create a nested result like this:

```
RETURN (
  FOR u IN users RETURN u.friends[*].name
)
```

By now appending the <i>[\*\*]</i> operator the end of the query, the query result becomes:

```
RETURN (
  FOR u IN users RETURN u.friends[*].name
)[**]
```

```json
[
  [
    "tina",
    "helga",
    "alfred",
    "sergei",
    "tiffany",
    "bob",
    "elena"
  ]
]
```
