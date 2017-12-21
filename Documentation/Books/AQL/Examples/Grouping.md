Grouping
========

To group results by arbitrary criteria, AQL provides the *COLLECT* keyword.
*COLLECT* will perform a grouping, but no aggregation. Aggregation can still be
added in the query if required.

### Ensuring uniqueness

*COLLECT* can be used to make a result set unique. The following query will return each distinct
`age` attribute value only once:

```js
FOR u IN users
    COLLECT age = u.age
    RETURN age
```

This is grouping without tracking the group values, but just the group criterion (*age*) value.

Grouping can also be done on multiple levels using *COLLECT*:

```js
FOR u IN users
    COLLECT status = u.status, age = u.age
    RETURN { status, age }
```


Alternatively *RETURN DISTINCT* can be used to make a result set unique. *RETURN DISTINCT* supports a
single criterion only:

```js
FOR u IN users
    RETURN DISTINCT u.age
```

Note: the order of results is undefined for *RETURN DISTINCT*.

### Fetching group values

To group users by age, and return the names of the users with the highest ages,
we'll issue a query like this:

```js
FOR u IN users
    FILTER u.active == true
    COLLECT age = u.age INTO usersByAge
    SORT age DESC LIMIT 0, 5
    RETURN {
        age,
        users: usersByAge[*].u.name
    }
```

```json
[
  { "age": 37, "users": [ "John", "Sophia" ] },
  { "age": 36, "users": [ "Fred", "Emma" ] },
  { "age": 34, "users": [ "Madison" ] },
  { "age": 33, "users": [ "Chloe", "Michael" ] },
  { "age": 32, "users": [ "Alexander" ] }
]
```

The query will put all users together by their *age* attribute. There will be one
result document per distinct *age* value (let aside the *LIMIT*). For each group,
we have access to the matching document via the *usersByAge* variable introduced in
the *COLLECT* statement.

### Variable Expansion

The *usersByAge* variable contains the full documents found, and as we're only
interested in user names, we'll use the expansion operator <i>[\*]</i> to extract just the
*name* attribute of all user documents in each group:

```js
usersByAge[*].u.name
```

The <i>[\*]</i> expansion operator is just a handy short-cut. We could also write
a subquery:

```js
( FOR temp IN usersByAge RETURN temp.u.name )
```

### Grouping by multiple criteria

To group by multiple criteria, we'll use multiple arguments in the *COLLECT* clause.
For example, to group users by *ageGroup* (a derived value we need to calculate first)
and then by *gender*, we'll do:

```js
FOR u IN users
    FILTER u.active == true
    COLLECT ageGroup = FLOOR(u.age / 5) * 5,
            gender = u.gender INTO group
    SORT ageGroup DESC
    RETURN {
        ageGroup,
        gender
    }
```

```json
[
  { "ageGroup": 35, "gender": "f" },
  { "ageGroup": 35, "gender": "m" },
  { "ageGroup": 30, "gender": "f" },
  { "ageGroup": 30, "gender": "m" },
  { "ageGroup": 25, "gender": "f" },
  { "ageGroup": 25, "gender": "m" }
]
```

### Counting group values

If the goal is to count the number of values in each group, AQL provides the special
*COLLECT WITH COUNT INTO* syntax. This is a simple variant for grouping with an additional
group length calculation:

```js
FOR u IN users
    FILTER u.active == true
    COLLECT ageGroup = FLOOR(u.age / 5) * 5,
            gender = u.gender WITH COUNT INTO numUsers
    SORT ageGroup DESC
    RETURN {
        ageGroup,
        gender,
        numUsers
    }
```

```json
[
  { "ageGroup": 35, "gender": "f", "numUsers": 2 },
  { "ageGroup": 35, "gender": "m", "numUsers": 2 },
  { "ageGroup": 30, "gender": "f", "numUsers": 4 },
  { "ageGroup": 30, "gender": "m", "numUsers": 4 },
  { "ageGroup": 25, "gender": "f", "numUsers": 2 },
  { "ageGroup": 25, "gender": "m", "numUsers": 2 }
]
```

### Aggregation

Adding further aggregation is also simple in AQL by using an *AGGREGATE* clause
in the *COLLECT*:

```js
FOR u IN users
    FILTER u.active == true
    COLLECT ageGroup = FLOOR(u.age / 5) * 5,
            gender = u.gender
    AGGREGATE numUsers = LENGTH(1),
              minAge = MIN(u.age),
              maxAge = MAX(u.age)
    SORT ageGroup DESC
    RETURN {
        ageGroup,
        gender,
        numUsers,
        minAge,
        maxAge
    }
```

```json
[
  {
    "ageGroup": 35,
    "gender": "f",
    "numUsers": 2,
    "minAge": 36,
    "maxAge": 39,
  },
  {
    "ageGroup": 35,
    "gender": "m",
    "numUsers": 2,
    "minAge": 35,
    "maxAge": 39,
  },
  ...
]
```

We have used the aggregate functions *LENGTH* here (it returns the length of a array).
This is the equivalent to SQL's `SELECT g, COUNT(*) FROM ... GROUP BY g`. In addition to
*LENGTH* AQL also provides *MAX*, *MIN*, *SUM* and *AVERAGE*, *VARIANCE_POPULATION*,
*VARIANCE_SAMPLE*, *STDDEV_POPULATION* and *STDDEV_SAMPLE* as basic aggregation functions.

In AQL all aggregation functions can be run on arrays only. If an aggregation function
is run on anything that is not an array, a warning will be produced and the result will
be *null*.

Using an *AGGREGATE* clause will ensure the aggregation is run while the groups are built
in the collect operation. This is normally more efficient than collecting all group values
for all groups and then doing a post-aggregation.


### Post-aggregation

Aggregation can also be performed after a *COLLECT* operation using other AQL constructs,
though performance-wise this is often inferior to using *COLLECT* with *AGGREGATE*.

The same query as before can be turned into a post-aggregation query as shown below. Note
that this query will build and pass on all group values for all groups inside the variable
*g*, and perform the aggregation at the latest possible stage:

```js
FOR u IN users
    FILTER u.active == true
    COLLECT ageGroup = FLOOR(u.age / 5) * 5,
            gender = u.gender INTO g
    SORT ageGroup DESC
    RETURN {
        ageGroup,
        gender,
        numUsers: LENGTH(g[*]),
        minAge: MIN(g[*].u.age),
        maxAge: MAX(g[*].u.age)
    }
```

```json
[
  {
    "ageGroup": 35,
    "gender": "f",
    "numUsers": 2,
    "minAge": 36,
    "maxAge": 39,
  },
  {
    "ageGroup": 35,
    "gender": "m",
    "numUsers": 2,
    "minAge": 35,
    "maxAge": 39,
  },
  ...
]
```

This is in constrast to the previous query that used an *AGGREGATE* clause to perform
the aggregation during the collect operation, at the earliest possible stage.


### Post-filtering aggregated data

To filter the results of a grouping or aggregation operation (i.e. something
similar to *HAVING* in SQL), simply add another *FILTER* clause after the *COLLECT*
statement.

For example, to get the 3 *ageGroup*s with the most users in them:

```js
FOR u IN users
    FILTER u.active == true
    COLLECT ageGroup = FLOOR(u.age / 5) * 5 INTO group
    LET numUsers = LENGTH(group)
    FILTER numUsers > 2 /* group must contain at least 3 users in order to qualify */
    SORT numUsers DESC
    LIMIT 0, 3
    RETURN {
        "ageGroup": ageGroup,
        "numUsers": numUsers,
        "users": group[*].u.name
    }
```

```json
[
  {
    "ageGroup": 30,
    "numUsers": 8,
    "users": [
      "Abigail",
      "Madison",
      "Anthony",
      "Alexander",
      "Isabella",
      "Chloe",
      "Daniel",
      "Michael"
    ]
  },
  {
    "ageGroup": 25,
    "numUsers": 4,
    "users": [
      "Mary",
      "Mariah",
      "Jim",
      "Diego"
    ]
  },
  {
    "ageGroup": 35,
    "numUsers": 4,
    "users": [
      "Fred",
      "John",
      "Emma",
      "Sophia"
    ]
  }
]
```

To increase readability, the repeated expression *LENGTH(group)* was put into a variable
*numUsers*. The *FILTER* on *numUsers* is the equivalent an SQL *HAVING* clause.
