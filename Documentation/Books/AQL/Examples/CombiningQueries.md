Combining queries
=================

Subqueries
----------

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

Note that subqueries always return a result **array**, even if there is only
a single return value:

```js
RETURN ( RETURN 1 )
```

```json
[ [ 1 ] ]
```

To avoid such a nested data structure, [FIRST()](../Functions/Array.md#first)
can be used for example:

```js
RETURN FIRST( RETURN 1 )
```

```json
[ 1 ]
```
