!CHAPTER LIMIT

The *LIMIT* statement allows slicing the result array using an
offset and a count. It reduces the number of elements in the result to at most
the specified number. Two general forms of *LIMIT* are followed:

```js
LIMIT count
LIMIT offset, count
```

The first form allows specifying only the *count* value whereas the second form
allows specifying both *offset* and *count*. The first form is identical using
the second form with an *offset* value of *0*.

The *offset* value specifies how many elements from the result shall be
skipped. It must be 0 or greater. The *count* value specifies how many
elements should be at most included in the result.

```js
FOR u IN users
  SORT u.firstName, u.lastName, u.id DESC
  LIMIT 0, 5
  RETURN u
```

Note that variables and expressions can not be used for *offset* and *count*.
Their values must be known at query compile time, which means that you can
use number literals and bind parameters only.

Where a *LIMIT* is used in relation to other operations in a query has meaning.
*LIMIT* operations before *FILTER*s in particular can change the result
significantly, because the operations are executed in the order in which they
are written in the query. See [FILTER](Filter.md#order-of-operations) for a
detailed example.
 