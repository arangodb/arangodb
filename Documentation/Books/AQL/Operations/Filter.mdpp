!CHAPTER FILTER

The *FILTER* statement can be used to restrict the results to elements that
match an arbitrary logical condition.

!SECTION General syntax

```
FILTER condition
```

*condition* must be a condition that evaluates to either *false* or *true*. If
the condition result is false, the current element is skipped, so it will not be
processed further and not be part of the result. If the condition is true, the
current element is not skipped and can be further processed.
See [Operators](../Operators.md) for a list of comparison operators, logical
operators etc. that you can use in conditions.

```
FOR u IN users
  FILTER u.active == true && u.age < 39
  RETURN u
```

It is allowed to specify multiple *FILTER* statements in a query, even in
the same block. If multiple *FILTER* statements are used, their results will be
combined with a logical AND, meaning all filter conditions must be true to
include an element.

```
FOR u IN users
  FILTER u.active == true
  FILTER u.age < 39
  RETURN u
```

In the above example, all array elements of *users*  that have an attribute
*active* with value *true* and that have an attribute *age* with a value less
than *39* (including *null* ones) will be included in the result. All other
elements of *users* will be skipped and not be included in the result produced
by *RETURN*. You may refer to the chapter [Accessing Data from Collections](../Fundamentals/DocumentData.md)
for a description of the impact of non-existent or null attributes.

!SECTION Order of operations

Note that the positions of *FILTER* statements can influence the result of a query.
There are 16 active users in the [test data](../Examples/README.md#example-data)
for instance:

```js
FOR u IN users
  FILTER u.active == true
  RETURN u
```

We can limit the result set to 5 users at most:

```js
FOR u IN users
  FILTER u.active == true
  LIMIT 5
  RETURN u
```

This may return the user documents of Jim, Diego, Anthony, Michael and Chloe for
instance. Which ones are returned is undefined, since there is no *SORT* statement
to ensure a particular order. If we add a second *FILTER* statement to only return
women...

```js
FOR u IN users
  FILTER u.active == true
  LIMIT 5
  FILTER u.gender == "f"
  RETURN u
```

... it might just return the Chloe document, because the *LIMIT* is applied before
the second *FILTER*. No more than 5 documents arrive at the second *FILTER* block,
and not all of them fulfill the gender criterion, eventhough there are more than
5 active female users in the collection. A more deterministic result can be achieved
by adding a *SORT* block:

```js
FOR u IN users
  FILTER u.active == true
  SORT u.age ASC
  LIMIT 5
  FILTER u.gender == "f"
  RETURN u
```

This will return the users Mariah and Mary. If sorted by age in *DESC* order,
then the Sophia, Emma and Madison documents are returned. A *FILTER* after a
*LIMIT* is not very common however, and you probably want such a query instead:

```js
FOR u IN users
  FILTER u.active == true AND u.gender == "f"
  SORT u.age ASC
  LIMIT 5
  RETURN u
```

The significance of where *FILTER* blocks are placed allows that this single
keyword can assume the roles of two SQL keywords, *WHERE* as well as *HAVING*.
AQL's *FILTER* thus works with *COLLECT* aggregates the same as with any other
intermediate result, document attribute etc.
