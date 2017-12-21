COLLECT
=======

The *COLLECT* keyword can be used to group an array by one or multiple group
criteria. 

The *COLLECT* statement will eliminate all local variables in the current
scope. After *COLLECT* only the variables introduced by *COLLECT* itself are
available.

The general syntaxes for *COLLECT* are:

```
COLLECT variableName = expression options
COLLECT variableName = expression INTO groupsVariable options
COLLECT variableName = expression INTO groupsVariable = projectionExpression options
COLLECT variableName = expression INTO groupsVariable KEEP keepVariable options
COLLECT variableName = expression WITH COUNT INTO countVariable options
COLLECT variableName = expression AGGREGATE variableName = aggregateExpression options
COLLECT AGGREGATE variableName = aggregateExpression options
COLLECT WITH COUNT INTO countVariable options
```

`options` is optional in all variants.

### Grouping syntaxes

The first syntax form of *COLLECT* only groups the result by the defined group 
criteria specified in *expression*. In order to further process the results 
produced by *COLLECT*, a new variable (specified by *variableName*) is introduced. 
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
variable (specified by *groupsVariable*) that contains all elements that fell into the
group. This works as follows: The *groupsVariable* variable is an array containing 
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


### Discarding obsolete variables

The third form of *COLLECT* allows rewriting the contents of the *groupsVariable* 
using an arbitrary *projectionExpression*:

```
FOR u IN users
  COLLECT country = u.country, city = u.city INTO groups = u.name
  RETURN { 
    "country" : country, 
    "city" : city, 
    "userNames" : groups 
  }
```

In the above example, only the *projectionExpression* is *u.name*. Therefore,
only this attribute is copied into the *groupsVariable* for each document. 
This is probably much more efficient than copying all variables from the scope into 
the *groupsVariable* as it would happen without a *projectionExpression*.

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
sub-attributes into the *groupsVariable*. 
This is safe but can have a negative impact on performance if there 
are many variables in scope or the variables contain massive amounts of data. 

The following example limits the variables that are copied into the *groupsVariable*
to just *name*. The variables *u* and *someCalculation* also present in the scope
will not be copied into *groupsVariable* because they are not listed in the *KEEP* clause:

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


### Group length calculation

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


### COLLECT variants

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


### Setting COLLECT options

*options* can be used in a *COLLECT* statement to inform the optimizer about the preferred *COLLECT*
method. When specifying the following appendix to a *COLLECT* statement, the optimizer will always use
the *sorted* variant of *COLLECT* and not even create a plan using the *hash* variant:

```
OPTIONS { method: "sorted" }
```

Note that specifying *hash* as method will not make the optimizer use the *hash* variant. This is
because the *hash* variant is not eligible for all queries. Instead, if no options or any other method
than *sorted* are specified in *OPTIONS*, the optimizer will use its regular cost estimations.


### COLLECT vs. RETURN DISTINCT

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



