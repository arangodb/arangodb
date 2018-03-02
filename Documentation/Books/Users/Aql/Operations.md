High-level operations
=====================

### FOR

The *FOR* keyword can be to iterate over all elements of an array.
The general syntax is:

```
FOR variable-name IN expression
```

There also is a special case for graph traversals:

```
FOR vertex-variable-name, edge-variable-name, path-variable-name IN traversal-expression
```

For this special case see [the graph traversals chapter](GraphTraversals.md).
For all other cases read on:

Each array element returned by *expression* is visited exactly once. It is
required that *expression* returns an array in all cases. The empty array is
allowed, too. The current array element is made available for further processing 
in the variable specified by *variable-name*.

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

### RETURN

The *RETURN* statement can be used to produce the result of a query.
It is mandatory to specify a *RETURN* statement at the end of each block in a
data-selection query, otherwise the query result would be undefined. Using 
*RETURN* on the main level in data-modification queries is optional.

The general syntax for *RETURN* is:

```
RETURN expression
```

The *expression* returned by *RETURN* is produced for each iteration in the block the
*RETURN* statement is placed in. That means the result of a *RETURN* statement
is always an array (this includes the empty array).  To return all elements from
the currently iterated array without modification, the following simple form can
be used:

```
FOR variable-name IN expression
  RETURN variable-name
```

As *RETURN* allows specifying an expression, arbitrary computations can be
performed to calculate the result elements. Any of the variables valid in the
scope the *RETURN* is placed in can be used for the computations.

Note: *RETURN* will close the current scope and eliminate all local variables in
it.

#### RETURN DISTINCT

Since ArangoDB 2.7, *RETURN* can optionally be followed by the *DISTINCT* keyword.
The *DISTINCT* keyword will ensure uniqueness of the values returned by the
*RETURN* statement:

```
FOR variable-name IN expression
  RETURN DISTINCT expression
```

If the *DISTINCT* is applied on an expression that itself is an array or a subquery, 
the *DISTINCT* will not make the values in each array or subquery result unique, but instead
ensure that the result contains only distinct arrays or subquery results. To make
the result of an array or a subquery unique, simply apply the *DISTINCT* for the
array or the subquery.

For example, the following query will apply *DISTINCT* on its subquery results,
but not inside the subquery:

```
FOR what IN 1..2
  RETURN DISTINCT (
    FOR i IN [ 1, 2, 3, 4, 1, 3 ] 
      RETURN i
  )
```

Here we'll have a *FOR* loop with two iterations that each execute a subquery. The
*DISTINCT* here is applied on the two subquery results. Both subqueries return the
same result value (that is [ 1, 2, 3, 4, 1, 3 ]), so after *DISTINCT* there will
only be one occurrence of the value [ 1, 2, 3, 4, 1, 3 ] left:

```
[
  [ 1, 2, 3, 4, 1, 3 ]
]
```

If the goal is to apply the *DISTINCT* inside the subquery, it needs to be moved
there:

```
FOR what IN 1..2
  LET sub = (
    FOR i IN [ 1, 2, 3, 4, 1, 3 ] 
      RETURN DISTINCT i
  ) 
  RETURN sub
```

In the above case, the *DISTINCT* will make the subquery results unique, so that
each subquery will return a unique array of values ([ 1, 2, 3, 4 ]). As the subquery
is executed twice and there is no *DISTINCT* on the top-level, that array will be
returned twice:

```
[
  [ 1, 2, 3, 4 ],
  [ 1, 2, 3, 4 ]
]
```

Note: the order of results is undefined for *RETURN DISTINCT*.

Note: *RETURN DISTINCT* is not allowed on the top-level of a query if there is no *FOR* 
loop in front of it.


### FILTER

The *FILTER* statement can be used to restrict the results to elements that
match an arbitrary logical condition. The general syntax is:

```
FILTER condition
```

*condition* must be a condition that evaluates to either *false* or *true*. If
the condition result is false, the current element is skipped, so it will not be
processed further and not be part of the result. If the condition is true, the
current element is not skipped and can be further processed.

```
FOR u IN users
  FILTER u.active == true && u.age < 39
  RETURN u
```

In the above example, all array elements from *users* will be included that have
an attribute *active* with value *true* and that have an attribute *age* with a
value less than *39* (including *null* ones). All other elements from *users* 
will be skipped and not be included the result produced by *RETURN*.

It is allowed to specify multiple *FILTER* statements in a query, and even in
the same block. If multiple *FILTER* statements are used, their results will be
combined with a logical and, meaning all filter conditions must be true to
include an element.

```
FOR u IN users
  FILTER u.active == true
  FILTER u.age < 39
  RETURN u
```

### SORT

The *SORT* statement will force a sort of the array of already produced
intermediate results in the current block. *SORT* allows specifying one or
multiple sort criteria and directions.  The general syntax is:

```
SORT expression direction
```

Specifying the *direction* is optional. The default (implicit) direction for a
sort is the ascending order. To explicitly specify the sort direction, the
keywords *ASC* (ascending) and *DESC* can be used. Multiple sort criteria can be
separated using commas.

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

### LIMIT

The *LIMIT* statement allows slicing the result array using an
offset and a count. It reduces the number of elements in the result to at most
the specified number.  Two general forms of *LIMIT* are followed:

```
LIMIT count
LIMIT offset, count
```

The first form allows specifying only the *count* value whereas the second form
allows specifying both *offset* and *count*. The first form is identical using
the second form with an *offset* value of *0*.

The *offset* value specifies how many elements from the result shall be
discarded.  It must be 0 or greater. The *count* value specifies how many
elements should be at most included in the result.

```
FOR u IN users
  SORT u.firstName, u.lastName, u.id DESC
  LIMIT 0, 5
  RETURN u
```

### LET

The *LET* statement can be used to assign an arbitrary value to a variable.  The
variable is then introduced in the scope the *LET* statement is placed in.  The
general syntax is:

```
LET variable-name = expression
```

*LET* statements are mostly used to declare complex computations and to avoid
repeated computations of the same value at multiple parts of a query.

```
FOR u IN users
  LET numRecommendations = LENGTH(u.recommendations)
  RETURN { 
    "user" : u, 
    "numRecommendations" : numRecommendations, 
    "isPowerUser" : numRecommendations >= 10 
  } 
```

In the above example, the computation of the number of recommendations is
factored out using a *LET* statement, thus avoiding computing the value twice in
the *RETURN* statement.

Another use case for *LET* is to declare a complex computation in a subquery,
making the whole query more readable.

```
FOR u IN users
  LET friends = (
  FOR f IN friends 
    FILTER u.id == f.userId
    RETURN f
  )
  LET memberships = (
  FOR m IN memberships
    FILTER u.id == m.userId
      RETURN m
  )
  RETURN { 
    "user" : u, 
    "friends" : friends, 
    "numFriends" : LENGTH(friends), 
    "memberShips" : memberships 
  }
```

### COLLECT

The *COLLECT* keyword can be used to group an array by one or multiple group
criteria. 

The *COLLECT* statement will eliminate all local variables in the current
scope. After *COLLECT* only the variables introduced by *COLLECT* itself are
available.

The general syntaxes for *COLLECT* are:

```
COLLECT variable-name = expression options
COLLECT variable-name = expression INTO groups-variable options
COLLECT variable-name = expression INTO groups-variable = projection-expression options
COLLECT variable-name = expression INTO groups-variable KEEP keep-variable options
COLLECT variable-name = expression WITH COUNT INTO count-variable options
COLLECT variable-name = expression AGGREGATE variable-name = aggregate-expression options
COLLECT AGGREGATE variable-name = aggregate-expression options
COLLECT WITH COUNT INTO count-variable options
```

#### Grouping syntaxes

The first syntax form of *COLLECT* only groups the result by the defined group 
criteria specified in *expression*. In order to further process the results 
produced by *COLLECT*, a new variable (specified by *variable-name*) is introduced. 
This variable contains the group value.

Here's an example query that find the distinct values in *u.city* and makes
them available in variable *city*:

```
FOR u IN users
  COLLECT city = u.city
  RETURN { 
    "city" : city 
  }
```

The second form does the same as the first form, but additionally introduces a
variable (specified by *groups-variable*) that contains all elements that fell into the
group. This works as follows: The *groups-variable* variable is an array containing 
as many elements as there are in the group. Each member of that array is
a JSON object in which the value of every variable that is defined in the 
AQL query is bound to the corresponding attribute. Note that this considers
all variables that are defined before the *COLLECT* statement, but not those on
the top level (outside of any *FOR*), unless the *COLLECT* statement is itself
on the top level, in which case all variables are taken. Furthermore note 
that it is possible that the optimizer moves *LET* statements out of *FOR*
statements to improve performance. 

```
FOR u IN users
  COLLECT city = u.city INTO groups
  RETURN { 
    "city" : city, 
    "usersInCity" : groups 
  }
```

In the above example, the array *users* will be grouped by the attribute
*city*. The result is a new array of documents, with one element per distinct
*u.city* value. The elements from the original array (here: *users*) per city are
made available in the variable *groups*. This is due to the *INTO* clause.

*COLLECT* also allows specifying multiple group criteria. Individual group
criteria can be separated by commas:

```
FOR u IN users
  COLLECT country = u.country, city = u.city INTO groups
  RETURN { 
    "country" : country, 
    "city" : city, 
    "usersInCity" : groups 
  }
```

In the above example, the array *users* is grouped by country first and then
by city, and for each distinct combination of country and city, the users
will be returned.


#### Discarding obsolete variables

The third form of *COLLECT* allows rewriting the contents of the *groups-variable* 
using an arbitrary *projection-expression*:

```
FOR u IN users
  COLLECT country = u.country, city = u.city INTO groups = u.name
  RETURN { 
    "country" : country, 
    "city" : city, 
    "userNames" : groups 
  }
```

In the above example, only the *projection-expression* is *u.name*. Therefore,
only this attribute is copied into the *groups-variable* for each document. 
This is probably much more efficient than copying all variables from the scope into 
the *groups-variable* as it would happen without a *projection-expression*.

The expression following *INTO* can also be used for arbitrary computations:

```
FOR u IN users
  COLLECT country = u.country, city = u.city INTO groups = { 
    "name" : u.name, 
    "isActive" : u.status == "active"
  }
  RETURN { 
    "country" : country, 
    "city" : city, 
    "usersInCity" : groups 
  }
```

*COLLECT* also provides an optional *KEEP* clause that can be used to control
which variables will be copied into the variable created by `INTO`. If no 
*KEEP* clause is specified, all variables from the scope will be copied as 
sub-attributes into the *groups-variable*. 
This is safe but can have a negative impact on performance if there 
are many variables in scope or the variables contain massive amounts of data. 

The following example limits the variables that are copied into the *groups-variable*
to just *name*. The variables *u* and *someCalculation* also present in the scope
will not be copied into *groups-variable* because they are not listed in the *KEEP* clause:

```
FOR u IN users
  LET name = u.name
  LET someCalculation = u.value1 + u.value2
  COLLECT city = u.city INTO groups KEEP name 
  RETURN { 
    "city" : city, 
    "userNames" : groups[*].name 
  }
```

*KEEP* is only valid in combination with *INTO*. Only valid variable names can
be used in the *KEEP* clause. *KEEP* supports the specification of multiple 
variable names.


#### Group length calculation

*COLLECT* also provides a special *WITH COUNT* clause that can be used to 
determine the number of group members efficiently.

The simplest form just returns the number of items that made it into the
*COLLECT*:

```
FOR u IN users
  COLLECT WITH COUNT INTO length
  RETURN length
```

The above is equivalent to, but more efficient than:

```
RETURN LENGTH(
  FOR u IN users
    RETURN length
)
```

The *WITH COUNT* clause can also be used to efficiently count the number
of items in each group:

```
FOR u IN users
  COLLECT age = u.age WITH COUNT INTO length
  RETURN { 
    "age" : age, 
    "count" : length 
  }
```

Note: the *WITH COUNT* clause can only be used together with an *INTO* clause.


### Aggregation

A `COLLECT` statement can be used to perform aggregation of data per group. To
only determine group lengths, the `WITH COUNT INTO` variant of `COLLECT` can be
used as described before.

For other aggregations, it is possible to run aggregate functions on the `COLLECT`
results:

```
FOR u IN users
  COLLECT ageGroup = FLOOR(u.age / 5) * 5 INTO g
  RETURN { 
    "ageGroup" : ageGroup,
    "minAge" : MIN(g[*].u.age),
    "maxAge" : MAX(g[*].u.age)
  }
```

The above however requires storing all group values during the collect operation for 
all groups, which can be inefficient. 

The special `AGGREGATE` variant of `COLLECT` allows building the aggregate values 
incrementally during the collect operation, and is therefore often more efficient.

With the `AGGREGATE` variant the above query becomes:

```
FOR u IN users
  COLLECT ageGroup = FLOOR(u.age / 5) * 5 
  AGGREGATE minAge = MIN(u.age), maxAge = MAX(u.age)
  RETURN {
    ageGroup, 
    minAge, 
    maxAge 
  }
```

The `AGGREGATE` keyword can only be used after the `COLLECT` keyword. If used, it 
must directly follow the declaration of the grouping keys. If no grouping keys 
are used, it must follow the `COLLECT` keyword directly:

```
FOR u IN users
  COLLECT AGGREGATE minAge = MIN(u.age), maxAge = MAX(u.age)
  RETURN {
    minAge, 
    maxAge 
  }
```
      
Only specific expressions are allowed on the right-hand side of each `AGGREGATE` 
assignment:

- on the top level, an aggregate expression must be a call to one of the supported 
  aggregation functions `LENGTH`, `MIN`, `MAX`, `SUM`, `AVERAGE`, `STDDEV_POPULATION`, 
  `STDDEV_SAMPLE`, `VARIANCE_POPULATION`, or `VARIANCE_SAMPLE`

- an aggregate expression must not refer to variables introduced by the `COLLECT` itself


#### COLLECT variants

Since ArangoDB 2.6, there are two variants of *COLLECT* that the optimizer can
choose from: the *sorted* variant and the *hash* variant. The *hash* variant only becomes a
candidate for *COLLECT* statements that do not use an *INTO* clause.

The optimizer will always generate a plan that employs the *sorted* method. The *sorted* method 
requires its input to be sorted by the group criteria specified in the *COLLECT* clause. 
To ensure correctness of the result, the AQL optimizer will automatically insert a *SORT* 
statement into the query in front of the *COLLECT* statement. The optimizer may be able to 
optimize away that *SORT* statement later if a sorted index is present on the group criteria. 

In case a *COLLECT* qualifies for using the *hash* variant, the optimizer will create an extra 
plan for it at the beginning of the planning phase. In this plan, no extra *SORT* statement will be
added in front of the *COLLECT*. This is because the *hash* variant of *COLLECT* does not require
sorted input. Instead, a *SORT* statement will be added after the *COLLECT* to sort its output. 
This *SORT* statement may be optimized away again in later stages. 
If the sort order of the *COLLECT* is irrelevant to the user, adding the extra instruction *SORT null* 
after the *COLLECT* will allow the optimizer to remove the sorts altogether:

```
FOR u IN users
  COLLECT age = u.age
  SORT null  /* note: will be optimized away */
  RETURN age
```
  
Which *COLLECT* variant is used by the optimizer depends on the optimizer's cost estimations. The 
created plans with the different *COLLECT* variants will be shipped through the regular optimization 
pipeline. In the end, the optimizer will pick the plan with the lowest estimated total cost as usual. 

In general, the *sorted* variant of *COLLECT* should be preferred in cases when there is a sorted index
present on the group criteria. In this case the optimizer can eliminate the *SORT* statement in front
of the *COLLECT*, so that no *SORT* will be left. 

If there is no sorted index available on the group criteria, the up-front sort required by the *sorted* 
variant can be expensive. In this case it is likely that the optimizer will prefer the *hash* variant
of *COLLECT*, which does not require its input to be sorted. 

Which variant of *COLLECT* was actually used can be figured out by looking into the execution plan of
a query, specifically the *AggregateNode* and its *aggregationOptions* attribute.


#### Setting COLLECT options

*options* can be used in a *COLLECT* statement to inform the optimizer about the preferred *COLLECT*
method. When specifying the following appendix to a *COLLECT* statement, the optimizer will always use
the *sorted* variant of *COLLECT* and not even create a plan using the *hash* variant:

```
OPTIONS { method: "sorted" }
```

Note that specifying *hash* as method will not make the optimizer use the *hash* variant. This is
because the *hash* variant is not eligible for all queries. Instead, if no options or any other method
than *sorted* are specified in *OPTIONS*, the optimizer will use its regular cost estimations.


#### COLLECT vs. RETURN DISTINCT

In order to make a result set unique, one can either use *COLLECT* or *RETURN DISTINCT*. Behind the
scenes, both variants will work by creating an *AggregateNode*. For both variants, the optimizer
may try the sorted and the hashed variant of *COLLECT*. The difference is therefore mainly syntactical,
with *RETURN DISTINCT* saving a bit of typing when compared to an equivalent *COLLECT*:

```
FOR u IN users
  RETURN DISTINCT u.age
```

```
FOR u IN users
  COLLECT age = u.age
  RETURN age
```

However, *COLLECT* is vastly more flexible than *RETURN DISTINCT*. Additionally, the order of results is 
undefined for a *RETURN DISTINCT*, whereas for a *COLLECT* the results will be sorted.


### REMOVE

The *REMOVE* keyword can be used to remove documents from a collection. On a
single server, the document removal is executed transactionally in an 
all-or-nothing fashion. For sharded collections, the entire remove operation
is not transactional.

Each *REMOVE* operation is restricted to a single collection, and the 
[collection name](../Glossary/README.md#collection-name) must not be dynamic.
Only a single *REMOVE* statement per collection is allowed per AQL query, and 
it cannot be followed by read operations that access the same collection, by
traversal operations, or AQL functions that can read documents.

The syntax for a remove operation is:

```
REMOVE key-expression IN collection options
```

*collection* must contain the name of the collection to remove the documents 
from. *key-expression* must be an expression that contains the document identification.
This can either be a string (which must then contain the [document key](../Glossary/README.md#document-key)) or a
document, which must contain a *_key* attribute.

The following queries are thus equivalent:

```
FOR u IN users
  REMOVE { _key: u._key } IN users

FOR u IN users
  REMOVE u._key IN users

FOR u IN users
  REMOVE u IN users
```

**Note**: A remove operation can remove arbitrary documents, and the documents
do not need to be identical to the ones produced by a preceding *FOR* statement:

```
FOR i IN 1..1000
  REMOVE { _key: CONCAT('test', i) } IN users

FOR u IN users
  FILTER u.active == false
  REMOVE { _key: u._key } IN backup
```

#### Setting query options

*options* can be used to suppress query errors that may occur when trying to
remove non-existing documents. For example, the following query will fail if one
of the to-be-deleted documents does not exist:

```
FOR i IN 1..1000
  REMOVE { _key: CONCAT('test', i) } IN users
```

By specifying the *ignoreErrors* query option, these errors can be suppressed so 
the query completes:

```
FOR i IN 1..1000
  REMOVE { _key: CONCAT('test', i) } IN users OPTIONS { ignoreErrors: true }
```

To make sure data has been written to disk when a query returns, there is the *waitForSync* 
query option:

```
FOR i IN 1..1000
  REMOVE { _key: CONCAT('test', i) } IN users OPTIONS { waitForSync: true }
```

#### Returning the removed documents

The removed documents can also be returned by the query. In this case, the `REMOVE` 
statement must be followed by a `RETURN` statement (intermediate `LET` statements
are allowed, too).`REMOVE` introduces the pseudo-value `OLD` to refer to the removed
documents:

```
REMOVE key-expression IN collection options RETURN OLD
```

Following is an example using a variable named `removed` for capturing the removed
documents. For each removed document, the document key will be returned.

```
FOR u IN users
  REMOVE u IN users 
  LET removed = OLD 
  RETURN removed._key
```

### UPDATE

The *UPDATE* keyword can be used to partially update documents in a collection. On a 
single server, updates are executed transactionally in an all-or-nothing fashion. 
For sharded collections, the entire update operation is not transactional.

Each *UPDATE* operation is restricted to a single collection, and the 
[collection name](../Glossary/README.md#collection-name) must not be dynamic.
Only a single *UPDATE* statement per collection is allowed per AQL query, and 
it cannot be followed by read operations that access the same collection, by
traversal operations, or AQL functions that can read documents.
The system attributes of documents (*_key*, *_id*, *_from*, *_to*, *_rev*) cannot be
updated.

The two syntaxes for an update operation are:

```
UPDATE document IN collection options
UPDATE key-expression WITH document IN collection options
```

*collection* must contain the name of the collection in which the documents should
be updated. *document* must be a document that contains the attributes and values 
to be updated. When using the first syntax, *document* must also contain the *_key*
attribute to identify the document to be updated. 

```
FOR u IN users
  UPDATE { _key: u._key, name: CONCAT(u.firstName, u.lastName) } IN users
```

The following query is invalid because it does not contain a *_key* attribute and
thus it is not possible to determine the documents to be updated:

```
FOR u IN users
  UPDATE { name: CONCAT(u.firstName, u.lastName) } IN users
```

When using the second syntax, *key-expression* provides the document identification.
This can either be a string (which must then contain the document key) or a
document, which must contain a *_key* attribute.

The following queries are equivalent:

```
FOR u IN users
  UPDATE u._key WITH { name: CONCAT(u.firstName, u.lastName) } IN users

FOR u IN users
  UPDATE { _key: u._key } WITH { name: CONCAT(u.firstName, u.lastName) } IN users

FOR u IN users
  UPDATE u WITH { name: CONCAT(u.firstName, u.lastName) } IN users
```

An update operation may update arbitrary documents which do not need to be identical
to the ones produced by a preceding *FOR* statement:

```
FOR i IN 1..1000
  UPDATE CONCAT('test', i) WITH { foobar: true } IN users

FOR u IN users
  FILTER u.active == false
  UPDATE u WITH { status: 'inactive' } IN backup
```

#### Setting query options

*options* can be used to suppress query errors that may occur when trying to
update non-existing documents or violating unique key constraints:

```
FOR i IN 1..1000
  UPDATE { _key: CONCAT('test', i) } WITH { foobar: true } IN users OPTIONS { ignoreErrors: true }
```

An update operation will only update the attributes specified in *document* and
leave other attributes untouched. Internal attributes (such as *_id*, *_key*, *_rev*,
*_from* and *_to*) cannot be updated and are ignored when specified in *document*.
Updating a document will modify the document's revision number with a server-generated value.

When updating an attribute with a null value, ArangoDB will not remove the attribute 
from the document but store a null value for it. To get rid of attributes in an update
operation, set them to null and provide the *keepNull* option:

```
FOR u IN users
  UPDATE u WITH { foobar: true, notNeeded: null } IN users OPTIONS { keepNull: false }
```

The above query will remove the *notNeeded* attribute from the documents and update
the *foobar* attribute normally.

There is also the option *mergeObjects* that controls whether object contents will be
merged if an object attribute is present in both the *UPDATE* query and in the 
to-be-updated document.

The following query will set the updated document's *name* attribute to the exact
same value that is specified in the query. This is due to the *mergeObjects* option
being set to *false*:

```
FOR u IN users
  UPDATE u WITH { name: { first: "foo", middle: "b.", last: "baz" } } IN users OPTIONS { mergeObjects: false }
```

Contrary, the following query will merge the contents of the *name* attribute in the
original document with the value specified in the query:

```
FOR u IN users
  UPDATE u WITH { name: { first: "foo", middle: "b.", last: "baz" } } IN users OPTIONS { mergeObjects: true }
```

Attributes in *name* that are present in the to-be-updated document but not in the
query will now be preserved. Attributes that are present in both will be overwritten
with the values specified in the query.

Note: the default value for *mergeObjects* is *true*, so there is no need to specify it
explicitly.

To make sure data are durable when an update query returns, there is the *waitForSync* 
query option:

```
FOR u IN users
  UPDATE u WITH { foobar: true } IN users OPTIONS { waitForSync: true }
```

#### Returning the modified documents

The modified documents can also be returned by the query. In this case, the `UPDATE` 
statement needs to be followed a `RETURN` statement (intermediate `LET` statements
are allowed, too). These statements can refer to the pseudo-values `OLD` and `NEW`.
The `OLD` pseudo-value refers to the document revisions before the update, and `NEW` 
refers to document revisions after the update.

Both `OLD` and `NEW` will contain all document attributes, even those not specified 
in the update expression.

```
UPDATE document IN collection options RETURN OLD
UPDATE document IN collection options RETURN NEW
UPDATE key-expression WITH document IN collection options RETURN OLD
UPDATE key-expression WITH document IN collection options RETURN NEW
```

Following is an example using a variable named `previous` to capture the original
documents before modification. For each modified document, the document key is returned.

```
FOR u IN users
  UPDATE u WITH { value: "test" } 
  LET previous = OLD 
  RETURN previous._key
```

The following query uses the `NEW` pseudo-value to return the updated documents,
without some of the system attributes:

```
FOR u IN users
  UPDATE u WITH { value: "test" } 
  LET updated = NEW 
  RETURN UNSET(updated, "_key", "_id", "_rev")
```

It is also possible to return both `OLD` and `NEW`:

```
FOR u IN users
  UPDATE u WITH { value: "test" } 
  RETURN { before: OLD, after: NEW }
```


### REPLACE

The *REPLACE* keyword can be used to completely replace documents in a collection. On a
single server, the replace operation is executed transactionally in an all-or-nothing 
fashion. For sharded collections, the entire replace operation is not transactional.

Each *REPLACE* operation is restricted to a single collection, and the 
[collection name](../Glossary/README.md#collection-name) must not be dynamic.
Only a single *REPLACE* statement per collection is allowed per AQL query, and 
it cannot be followed by read operations that access the same collection, by
traversal operations, or AQL functions that can read documents.
The system attributes of documents (*_key*, *_id*, *_from*, *_to*, *_rev*) cannot be
replaced.

The two syntaxes for a replace operation are:

```
REPLACE document IN collection options
REPLACE key-expression WITH document IN collection options
```

*collection* must contain the name of the collection in which the documents should
be replaced. *document* is the replacement document. When using the first syntax, *document* 
must also contain the *_key* attribute to identify the document to be replaced. 

```
FOR u IN users
  REPLACE { _key: u._key, name: CONCAT(u.firstName, u.lastName), status: u.status } IN users
```

The following query is invalid because it does not contain a *_key* attribute and
thus it is not possible to determine the documents to be replaced:

```
FOR u IN users
  REPLACE { name: CONCAT(u.firstName, u.lastName, status: u.status) } IN users
```

When using the second syntax, *key-expression* provides the document identification.
This can either be a string (which must then contain the document key) or a
document, which must contain a *_key* attribute.

The following queries are equivalent:

```
FOR u IN users
  REPLACE { _key: u._key, name: CONCAT(u.firstName, u.lastName) } IN users

FOR u IN users
  REPLACE u._key WITH { name: CONCAT(u.firstName, u.lastName) } IN users

FOR u IN users
  REPLACE { _key: u._key } WITH { name: CONCAT(u.firstName, u.lastName) } IN users

FOR u IN users
  REPLACE u WITH { name: CONCAT(u.firstName, u.lastName) } IN users
```

A replace will fully replace an existing document, but it will not modify the values
of internal attributes (such as *_id*, *_key*, *_from* and *_to*). Replacing a document
will modify a document's revision number with a server-generated value.

A replace operation may update arbitrary documents which do not need to be identical
to the ones produced by a preceding *FOR* statement:

```
FOR i IN 1..1000
  REPLACE CONCAT('test', i) WITH { foobar: true } IN users

FOR u IN users
  FILTER u.active == false
  REPLACE u WITH { status: 'inactive', name: u.name } IN backup
```

#### Setting query options

*options* can be used to suppress query errors that may occur when trying to
replace non-existing documents or when violating unique key constraints:

```
FOR i IN 1..1000
  REPLACE { _key: CONCAT('test', i) } WITH { foobar: true } IN users OPTIONS { ignoreErrors: true }
```

To make sure data are durable when a replace query returns, there is the *waitForSync* 
query option:

```
FOR i IN 1..1000
  REPLACE { _key: CONCAT('test', i) } WITH { foobar: true } IN users OPTIONS { waitForSync: true }
```

#### Returning the modified documents

The modified documents can also be returned by the query. In this case, the `REPLACE` 
statement must be followed by a `RETURN` statement (intermediate `LET` statements are
allowed, too). The `OLD` pseudo-value can be used to refer to document revisions before 
the replace, and `NEW` refers to document revisions after the replace.

Both `OLD` and `NEW` will contain all document attributes, even those not specified 
in the replace expression.


```
REPLACE document IN collection options RETURN OLD
REPLACE document IN collection options RETURN NEW
REPLACE key-expression WITH document IN collection options RETURN OLD
REPLACE key-expression WITH document IN collection options RETURN NEW
```

Following is an example using a variable named `previous` to return the original
documents before modification. For each replaced document, the document key will be
returned:

```
FOR u IN users
  REPLACE u WITH { value: "test" } 
  LET previous = OLD 
  RETURN previous._key
```

The following query uses the `NEW` pseudo-value to return the replaced
documents (without some of their system attributes):

```
FOR u IN users
  REPLACE u WITH { value: "test" } 
  LET replaced = NEW 
  RETURN UNSET(replaced, '_key', '_id', '_rev')
```


### INSERT

The *INSERT* keyword can be used to insert new documents into a collection. On a 
single server, an insert operation is executed transactionally in an all-or-nothing 
fashion. For sharded collections, the entire insert operation is not transactional.

Each *INSERT* operation is restricted to a single collection, and the 
[collection name](../Glossary/README.md#collection-name) must not be dynamic.
Only a single *INSERT* statement per collection is allowed per AQL query, and 
it cannot be followed by read operations that access the same collection, by
traversal operations, or AQL functions that can read documents.

The syntax for an insert operation is:

```
INSERT document IN collection options
```

**Note**: The *INTO* keyword is also allowed in the place of *IN*.

*collection* must contain the name of the collection into which the documents should
be inserted. *document* is the document to be inserted, and it may or may not contain
a *_key* attribute. If no *_key* attribute is provided, ArangoDB will auto-generate
a value for *_key* value. Inserting a document will also auto-generate a document
revision number for the document.

```
FOR i IN 1..100
  INSERT { value: i } IN numbers
```

When inserting into an [edge collection](../Glossary/README.md#edge-collection), it is mandatory to specify the attributes
*_from* and *_to* in document:

```
FOR u IN users
  FOR p IN products
    FILTER u._key == p.recommendedBy
    INSERT { _from: u._id, _to: p._id } IN recommendations
```

#### Setting query options

*options* can be used to suppress query errors that may occur when violating unique
key constraints:

```
FOR i IN 1..1000
  INSERT { _key: CONCAT('test', i), name: "test" } WITH { foobar: true } IN users OPTIONS { ignoreErrors: true }
```

To make sure data are durable when an insert query returns, there is the *waitForSync* 
query option:

```
FOR i IN 1..1000
  INSERT { _key: CONCAT('test', i), name: "test" } WITH { foobar: true } IN users OPTIONS { waitForSync: true }
```

#### Returning the inserted documents

The inserted documents can also be returned by the query. In this case, the `INSERT` 
statement can be a `RETURN` statement (intermediate `LET` statements are allowed, too).
To refer to the inserted documents, the `INSERT` statement introduces a pseudo-value
named `NEW`. 

The documents contained in `NEW` will contain all attributes, even those auto-generated by
the database (e.g. `_id`, `_key`, `_rev`, `_from`, and `_to`).


```
INSERT document IN collection options RETURN NEW
```

Following is an example using a variable named `inserted` to return the inserted
documents. For each inserted document, the document key is returned:

```
FOR i IN 1..100
  INSERT { value: i } 
  LET inserted = NEW 
  RETURN inserted._key
```


### UPSERT

The *UPSERT* keyword can be used for checking whether certain documents exist,
and to update them in case they exist, or create them in case they do not exist.
On a single server, upserts are executed transactionally in an all-or-nothing fashion. 
For sharded collections, the entire update operation is not transactional.

Each *UPSERT* operation is restricted to a single collection, and the 
[collection name](../Glossary/README.md#collection-name) must not be dynamic.
Only a single *UPSERT* statement per collection is allowed per AQL query, and 
it cannot be followed by read operations that access the same collection, by
traversal operations, or AQL functions that can read documents.

The syntax for an upsert operation is:

```
UPSERT search-expression INSERT insert-expression UPDATE update-expression IN collection options
UPSERT search-expression INSERT insert-expression REPLACE update-expression IN collection options
```

When using the *UPDATE* variant of the upsert operation, the found document will be 
partially updated, meaning only the attributes specified in *update-expression* will be 
updated or added. When using the *REPLACE* variant of upsert, existing documents will 
be replaced with the contexts of *update-expression*.

Updating a document will modify the document's revision number with a server-generated value.
The system attributes of documents (*_key*, *_id*, *_from*, *_to*, *_rev*) cannot be
updated.

The *search-expression* contains the document to be looked for. It must be an object 
literal without dynamic attribute names. In case no such document can be found in
*collection*, a new document will be inserted into the collection as specified in the
*insert-expression*. 

In case at least one document in *collection* matches the *search-expression*, it will
be updated using the *update-expression*. When more than one document in the collection
matches the *search-expression*, it is undefined which of the matching documents will
be updated. It is therefore often sensible to make sure by other means (such as unique 
indexes, application logic etc.) that at most one document matches *search-expression*.

The following query will look in the *users* collection for a document with a specific
*name* attribute value. If the document exists, its *logins* attribute will be increased
by one. If it does not exist, a new document will be inserted, consisting of the
attributes *name*, *logins*, and *dateCreated*:

```
UPSERT { name: 'superuser' } 
INSERT { name: 'superuser', logins: 1, dateCreated: DATE_NOW() } 
UPDATE { logins: OLD.logins + 1 } IN users
```

Note that in the *UPDATE* case it is possible to refer to the previous version of the
document using the *OLD* pseudo-value.


#### Setting query options

As in several above examples, the *ignoreErrors* option can be used to suppress query 
errors that may occur when trying to violate unique key constraints.

When updating or replacing an attribute with a null value, ArangoDB will not remove the 
attribute from the document but store a null value for it. To get rid of attributes in 
an upsert operation, set them to null and provide the *keepNull* option.

There is also the option *mergeObjects* that controls whether object contents will be
merged if an object attribute is present in both the *UPDATE* query and in the 
to-be-updated document.

Note: the default value for *mergeObjects* is *true*, so there is no need to specify it
explicitly.

To make sure data are durable when an update query returns, there is the *waitForSync* 
query option.


#### Returning documents

`UPSERT` statements can optionally return data. To do so, they need to be followed
by a `RETURN` statement (intermediate `LET` statements are allowed, too). These statements
can optionally perform calculations and refer to the pseudo-values `OLD` and `NEW`.
In case the upsert performed an insert operation, `OLD` will have a value of *null*.
In case the upsert performed an update or replace operation, `OLD` will contain the
previous version of the document, before update/replace.

`NEW` will always be populated. It will contain the inserted document in case the
upsert performed an insert, or the updated/replaced document in case it performed an
update/replace.

This can also be used to check whether the upsert has performed an insert or an update 
internally:

```
UPSERT { name: 'superuser' } 
INSERT { name: 'superuser', logins: 1, dateCreated: DATE_NOW() } 
UPDATE { logins: OLD.logins + 1 } IN users
RETURN { doc: NEW, type: OLD ? 'update' : 'insert' }
```

