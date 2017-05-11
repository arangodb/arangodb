
SORT
====

The *SORT* statement will force a sort of the array of already produced
intermediate results in the current block. *SORT* allows specifying one or
multiple sort criteria and directions.  The general syntax is:

```
SORT expression direction
```

Specifying the *direction* is optional. The default (implicit) direction for a
sort is the ascending order. To explicitly specify the sort direction, the
keywords *ASC* (ascending) and *DESC* can be used. Multiple sort criteria can be
separated using commas. In this case the direction is specified for each
expression sperately. For example

```
SORT doc.last_name, doc.first_name
```

will first sort documents by last name in ascending order and then by
first_name in ascending order.

```
SORT doc.last_name DESC, doc.first_name
```

will first sort documents by last name in descending order and then by
first_name in ascending order.

```
SORT doc.last_name, doc.first_name DESC
```

will first sort documents by last name in ascending order and then by
first_name in descending order.


Note: when iterating over collection-based arrays, the order of documents is
always undefined unless an explicit sort order is defined using *SORT*.

```
FOR u IN users
  SORT u.lastName, u.firstName, u.id DESC
  RETURN u
```

Note that constant *SORT* expressions can be used to indicate that no particular
sort order is desired. Constant *SORT* expressions will be optimized away by the AQL
optimizer during optimization, but specifying them explicitly may enable further
optimizations if the optimizer does not need to take into account any particular
sort order.

