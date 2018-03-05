

`FoxxRepository#range(attribute, left, right)`

Returns all models in the repository such that the attribute is greater
than or equal to *left* and strictly less than *right*.

For range queries it is required that a skiplist index is present for the
queried attribute. If no skiplist index is present on the attribute, the
method will not be available.

*Parameter*

* *attribute*: attribute to query.
* *left*: lower bound of the value range (inclusive).
* *right*: upper bound of the value range (exclusive).

@EXAMPLES

```javascript
repository.range("age", 10, 13);
```

