AQL Examples {#AqlExamples}
===========================

@NAVIGATE_AqlExamples
@EMBEDTOC{AqlExamplesTOC}

Simple queries {#AqlExamplesSimple}
===================================

This page contains some examples for how to write queries in AQL. For better
understandability, the query results are also included directly below each query.

A query that returns a string value. the result string is contained in a list
because the result of every valid query is a list:

    RETURN "this will be returned"

    [ 
      "this will be returned" 
    ]


Here's a query that creates the cross products of two lists, and runs a projection 
on it, using a few of AQL's built-in functions:

    FOR year in [ 2011, 2012, 2013 ]
      FOR quarter IN [ 1, 2, 3, 4 ]
        RETURN { 
          "y" : "year", 
          "q" : quarter, 
          "nice" : CONCAT(TO_STRING(quarter), "/", TO_STRING(year)) 
        }

    [ 
      { "y" : "year", "q" : 1, "nice" : "1/2011" }, 
      { "y" : "year", "q" : 2, "nice" : "2/2011" }, 
      { "y" : "year", "q" : 3, "nice" : "3/2011" }, 
      { "y" : "year", "q" : 4, "nice" : "4/2011" }, 
      { "y" : "year", "q" : 1, "nice" : "1/2012" }, 
      { "y" : "year", "q" : 2, "nice" : "2/2012" }, 
      { "y" : "year", "q" : 3, "nice" : "3/2012" }, 
      { "y" : "year", "q" : 4, "nice" : "4/2012" }, 
      { "y" : "year", "q" : 1, "nice" : "1/2013" }, 
      { "y" : "year", "q" : 2, "nice" : "2/2013" }, 
      { "y" : "year", "q" : 3, "nice" : "3/2013" }, 
      { "y" : "year", "q" : 4, "nice" : "4/2013" } 
    ]

Collection-based queries {#AqlExamplesCollection}
=================================================

Normally you would want to run queries on data stored in collections. This section
will provide several examples for that.


Example data
------------

Some of the following example queries are executed on a collection `users`
with the following initial data:

    [ 
      { "id" : 100, "name" : "John", "age" : 37, "active" : true, "gender" : "m" },
      { "id" : 101, "name" : "Fred", "age" : 36, "active" : true, "gender" : "m" },
      { "id" : 102, "name" : "Jacob", "age" : 35, "active" : false, "gender" : "m" },
      { "id" : 103, "name" : "Ethan", "age" : 34, "active" : false, "gender" : "m" },
      { "id" : 104, "name" : "Michael", "age" : 33, "active" : true, "gender" : "m" },
      { "id" : 105, "name" : "Alexander", "age" : 32, "active" : true, "gender" : "m" },
      { "id" : 106, "name" : "Daniel", "age" : 31, "active" : true, "gender" : "m" },
      { "id" : 107, "name" : "Anthony", "age" : 30, "active" : true, "gender" : "m" },
      { "id" : 108, "name" : "Jim", "age" : 29, "active" : true, "gender" : "m" },
      { "id" : 109, "name" : "Diego", "age" : 28, "active" : true, "gender" : "m" },
      { "id" : 200, "name" : "Sophia", "age" : 37, "active" : true, "gender" : "f" },
      { "id" : 201, "name" : "Emma", "age" : 36,  "active" : true, "gender" : "f" },
      { "id" : 202, "name" : "Olivia", "age" : 35, "active" : false, "gender" : "f" },
      { "id" : 203, "name" : "Madison", "age" : 34, "active" : true, "gender": "f" },
      { "id" : 204, "name" : "Chloe", "age" : 33, "active" : true, "gender" : "f" },
      { "id" : 205, "name" : "Eva", "age" : 32, "active" : false, "gender" : "f" },
      { "id" : 206, "name" : "Abigail", "age" : 31, "active" : true, "gender" : "f" },
      { "id" : 207, "name" : "Isabella", "age" : 30, "active" : true, "gender" : "f" },
      { "id" : 208, "name" : "Mary", "age" : 29, "active" : true, "gender" : "f" },
      { "id" : 209, "name" : "Mariah", "age" : 28, "active" : true, "gender" : "f" }
    ]

For some of the examples, we'll also use a collection `relations` to store
relationships between users. The example data for `relations` are as follows:

    [
      { "from" : 209, "to" : 205, "type" : "friend" },
      { "from" : 206, "to" : 108, "type" : "friend" },
      { "from" : 202, "to" : 204, "type" : "friend" },
      { "from" : 200, "to" : 100, "type" : "friend" },
      { "from" : 205, "to" : 101, "type" : "friend" },
      { "from" : 209, "to" : 203, "type" : "friend" },
      { "from" : 200, "to" : 203, "type" : "friend" },
      { "from" : 100, "to" : 208, "type" : "friend" },
      { "from" : 101, "to" : 209, "type" : "friend" },
      { "from" : 206, "to" : 102, "type" : "friend" },
      { "from" : 104, "to" : 100, "type" : "friend" },
      { "from" : 104, "to" : 108, "type" : "friend" },
      { "from" : 108, "to" : 209, "type" : "friend" },
      { "from" : 206, "to" : 106, "type" : "friend" },
      { "from" : 204, "to" : 105, "type" : "friend" },
      { "from" : 208, "to" : 207, "type" : "friend" },
      { "from" : 102, "to" : 108, "type" : "friend" },
      { "from" : 207, "to" : 203, "type" : "friend" },
      { "from" : 203, "to" : 106, "type" : "friend" },
      { "from" : 202, "to" : 108, "type" : "friend" },
      { "from" : 201, "to" : 203, "type" : "friend" },
      { "from" : 105, "to" : 100, "type" : "friend" },
      { "from" : 100, "to" : 109, "type" : "friend" },
      { "from" : 207, "to" : 109, "type" : "friend" },
      { "from" : 103, "to" : 203, "type" : "friend" },
      { "from" : 208, "to" : 104, "type" : "friend" },
      { "from" : 105, "to" : 104, "type" : "friend" },
      { "from" : 103, "to" : 208, "type" : "friend" },
      { "from" : 203, "to" : 107, "type" : "boyfriend" },
      { "from" : 107, "to" : 203, "type" : "girlfriend" },
      { "from" : 208, "to" : 109, "type" : "boyfriend" },
      { "from" : 109, "to" : 208, "type" : "girlfriend" },
      { "from" : 106, "to" : 205, "type" : "girlfriend" },
      { "from" : 205, "to" : 106, "type" : "boyfriend" },
      { "from" : 103, "to" : 209, "type" : "girlfriend" },
      { "from" : 209, "to" : 103, "type" : "boyfriend" },
      { "from" : 201, "to" : 102, "type" : "boyfriend" },
      { "from" : 102, "to" : 201, "type" : "girlfriend" },
      { "from" : 206, "to" : 100, "type" : "boyfriend" },
      { "from" : 100, "to" : 206, "type" : "girlfriend" }
    ]

Things to consider when running queries on collections
------------------------------------------------------

Note that all documents created in the two collections will automatically get the
following server-generated attributes:

`_id`: a unique id, consisting of collection name and a server-side sequence value

`_key`: the server sequence value

`_rev`: the document's revision id


Whenever you run queries on the documents in the two collections, don't be surprised if
these additional attributes are returned as well.

Please also note that with real-world data, you might want to create additional
indexes on the data (left out here for brevity). Adding indexes on attributes that are
used in `FILTER` statements may considerably speed up queries. Furthermore, instead of
using attributes such as `id`, `from`, and `to`, you might want to use the built-in
`_id`, `_from` and `to` attributes. Finally, edge collections provide a nice way of
establishing references / links between documents. These features have been left out here 
for brevity as well.


Projections and Filters {#AqlExamplesProjection}
================================================

Returning unaltered documents
-----------------------------

To return 3 complete documents from collection `users`, the following query can be used:

    FOR u IN users 
      LIMIT 0, 3
      RETURN u

    [ 
      { 
        "_id" : "users/229886047207520", 
        "_rev" : "229886047207520", 
        "_key" : "229886047207520", 
        "active" : true, 
        "id" : 206, 
        "age" : 31, 
        "gender" : "f", 
        "name" : "Abigail" 
      }, 
      { 
        "_id" : "users/229886045175904", 
        "_rev" : "229886045175904", 
        "_key" : "229886045175904", 
        "active" : true, 
        "id" : 101, 
        "age" : 36, 
        "name" : "Fred", 
        "gender" : "m" 
      }, 
      { 
        "_id" : "users/229886047469664", 
        "_rev" : "229886047469664", 
        "_key" : "229886047469664", 
        "active" : true, 
        "id" : 208, 
        "age" : 29, 
        "name" : "Mary", 
        "gender" : "f" 
      }
    ]

Note that there is a `LIMIT` clause but no `SORT` clause. In this case it is not guaranteed
which of the user documents are returned. Effectively the document return order is unspecified
if no `SORT` clause is used, and you should not rely on the order in such queries.

Projections
-----------

To return a projection from the collection `users`, use a modified `RETURN` instruction:

    FOR u IN users 
      LIMIT 0, 3
      RETURN { 
        "user" : { 
          "isActive" : u.active ? "yes" : "no", 
          "name" : u.name 
        } 
      }

    [ 
      { 
        "user" : { 
          "isActive" : "yes", 
          "name" : "John" 
        } 
      }, 
      { 
        "user" : { 
          "isActive" : "yes", 
          "name" : "Anthony" 
        } 
      }, 
      { 
        "user" : { 
          "isActive" : "yes", 
          "name" : "Fred" 
        } 
      }
    ]

Filters
-------

To return a filtered projection from collection `users`, you can use the
`FILTER` keyword. Additionally, a `SORT` clause is used to have the result
returned in a specific order:

    FOR u IN users 
      FILTER u.active == true && u.age >= 30
      SORT u.age DESC
      LIMIT 0, 5
      RETURN { 
        "age" : u.age, 
        "name" : u.name 
      }

    [ 
      { 
        "age" : 37, 
          "name" : "Sophia" 
      }, 
      { 
        "age" : 37, 
        "name" : "John" 
      }, 
      { 
        "age" : 36, 
        "name" : "Emma" 
      }, 
      { 
        "age" : 36, 
        "name" : "Fred" 
      }, 
      { 
        "age" : 34, 
        "name" : "Madison" 
      } 
    ]

Joins {#AqlExamplesJoins}
=========================

So far we have only dealt with one collection (`users`) at a time. We also have a 
collection `relations` that stores relationships between users. We will now use
this extra collection to create a result from two collections.

First of all, we'll query a few users together with their friends' ids. For that,
we'll use all `relations` that have a value of `friend` in their `type` attribute.
Relationships are established by using the `from` and `to` attributes in the 
`relations` collection, which point to the `id` values in the `users` collection.

Join tuples
-----------

We'll start with an SQL-ish result set and return each tuple (user name, friend id) 
separately. The AQL query to generate such result is:

    FOR u IN users 
      FILTER u.active == true 
      LIMIT 0, 4 
      FOR f IN relations 
        FILTER f.type == "friend" && f.from == u.id 
        RETURN { 
          "user" : u.name, 
          "friendId" : f.to 
        }

    [ 
      { 
        "user" : "Abigail", 
        "friendId" : 108 
      }, 
      { 
        "user" : "Abigail", 
        "friendId" : 102 
      }, 
      { 
        "user" : "Abigail", 
        "friendId" : 106 
      }, 
      { 
        "user" : "Fred", 
        "friendId" : 209 
      }, 
      { 
        "user" : "Mary", 
        "friendId" : 207 
      }, 
      { 
        "user" : "Mary", 
        "friendId" : 104 
      }, 
      { 
        "user" : "Mariah", 
        "friendId" : 203 
      }, 
      { 
        "user" : "Mariah", 
        "friendId" : 205 
      } 
    ]

Horizontal lists
----------------

Note that in the above result, a user might be returned multiple times. This is the
SQL way of returning data. If this is not desired, the friends' ids of each user
can be returned in a horizontal list. This will each user at most once.

The AQL query for doing so is:

    FOR u IN users 
      FILTER u.active == true LIMIT 0, 4 
      RETURN { 
        "user" : u.name, 
        "friendIds" : (
          FOR f IN relations 
            FILTER f.from == u.id && f.type == "friend"
            RETURN f.to
        )
      }

    [ 
      { 
        "user" : "Abigail", 
        "friendIds" : [ 
          108, 
          102, 
          106 
        ] 
      }, 
      { 
        "user" : "Fred", 
        "friendIds" : [ 
          209 
        ] 
      }, 
      { 
        "user" : "Mary", 
        "friendIds" : [ 
          207, 
          104 
        ] 
      }, 
      { 
        "user" : "Mariah", 
        "friendIds" : [ 
          203, 
          205 
        ] 
      } 
    ]

In this query, we're still iterating over the users in the `users` collection, 
and for each matching user we are executing a sub-query to create the matching
list of related users.

Self joins
----------

To not only return friend ids but also the names of friends, we could "join" the
`users` collection once more (something like a "self join"):
   
    FOR u IN users 
      FILTER u.active == true 
      LIMIT 0, 4 
      RETURN { 
        "user" : u.name, 
        "friendIds" : (
          FOR f IN relations 
            FILTER f.from == u.id && f.type == "friend" 
            FOR u2 IN users 
              FILTER f.to == u2.id 
              RETURN u2.name
        ) 
      }    

    [ 
      { 
        "user" : "Abigail", 
        "friendIds" : [ 
          "Jim", 
          "Jacob", 
          "Daniel" 
        ] 
      }, 
      { 
        "user" : "Fred", 
        "friendIds" : [ 
          "Mariah" 
        ] 
      }, 
      { 
        "user" : "Mary", 
        "friendIds" : [ 
          "Isabella", 
          "Michael" 
        ] 
      }, 
      { 
        "user" : "Mariah", 
        "friendIds" : [ 
          "Madison", 
          "Eva" 
        ] 
      } 
    ]

Grouping {#AqlExamplesGrouping}
===============================

To group results by arbitrary criteria, AQL provides the `COLLECT` keyword.
`COLLECT` will perform a grouping, but no aggregation. Aggregation can still be
added in the query if required.

Grouping by criteria
--------------------

To group users by age, and result the names of the users with the highest ages,
we'll issue a query like this:

    FOR u IN users 
      FILTER u.active == true 
      COLLECT age = u.age INTO usersByAge 
      SORT age DESC LIMIT 0, 5 
      RETURN { 
        "age" : age, 
        "users" : usersByAge[*].u.name 
      }

    [ 
      { 
        "age" : 37, 
        "users" : [ 
          "John", 
          "Sophia" 
        ] 
      }, 
      { 
        "age" : 36, 
        "users" : [ 
          "Fred", 
          "Emma" 
        ] 
      }, 
      { 
        "age" : 34, 
        "users" : [ 
          "Madison" 
        ] 
      }, 
      { 
        "age" : 33, 
        "users" : [ 
          "Chloe", 
          "Michael" 
        ] 
      }, 
      { 
        "age" : 32, 
        "users" : [ 
          "Alexander" 
        ] 
      } 
    ]

The query will put all users together by their `age` attribute. There will be one
result document per distinct `age` value (let aside the `LIMIT`). For each group,
we have access to the matching document via the `usersByAge` variable introduced in
the `COLLECT` statement. 

[*] list expander
-----------------

The `usersByAge` variable contains the full documents found, and as we're only 
interested in user names, we'll use the list expander (`[*]`) to extract just the 
`name` attribute of all user documents in each group.

The `[*]` expander is just a handy short-cut. Instead of `usersByAge[*].u.name` we 
could also write:

    FOR temp IN usersByAge
      RETURN temp.u.name

Grouping by multiple criteria
-----------------------------

To group by multiple criteria, we'll use multiple arguments in the `COLLECT` clause.
For example, to group users by `ageGroup` (a derived value we need to calculate first)
and then by `gender`, we'll do:
    
    FOR u IN users 
      FILTER u.active == true
      COLLECT ageGroup = FLOOR(u.age / 5) * 5, 
              gender = u.gender INTO group
      SORT ageGroup DESC
      RETURN { 
        "ageGroup" : ageGroup, 
        "gender" : gender 
      }

    [ 
      { 
        "ageGroup" : 35, 
        "gender" : "f" 
      }, 
      { 
        "ageGroup" : 35, 
        "gender" : "m" 
      }, 
      { 
        "ageGroup" : 30, 
        "gender" : "f" 
      }, 
      { 
        "ageGroup" : 30, 
        "gender" : "m" 
      }, 
      { 
        "ageGroup" : 25, 
        "gender" : "f" 
      }, 
      { 
        "ageGroup" : 25, 
        "gender" : "m" 
      } 
    ]

Aggregation
-----------

So far we only grouped data without aggregation. Adding aggregation is simple in AQL,
as all that needs to be done is to run an aggregate function on the list created by
the `INTO` clause of a `COLLECT` statement:

    FOR u IN users 
      FILTER u.active == true
      COLLECT ageGroup = FLOOR(u.age / 5) * 5, 
              gender = u.gender INTO group
      SORT ageGroup DESC
      RETURN { 
        "ageGroup" : ageGroup, 
        "gender" : gender, 
        "numUsers" : LENGTH(group) 
      }

    [ 
      { 
        "ageGroup" : 35, 
        "gender" : "f", 
        "numUsers" : 2 
      }, 
      { 
        "ageGroup" : 35, 
        "gender" : "m", 
        "numUsers" : 2 
      }, 
      { 
        "ageGroup" : 30, 
        "gender" : "f", 
        "numUsers" : 4 
      }, 
      { 
        "ageGroup" : 30, 
        "gender" : "m", 
        "numUsers" : 4 
      }, 
      { 
        "ageGroup" : 25, 
        "gender" : "f", 
        "numUsers" : 2 
      }, 
      { 
        "ageGroup" : 25, 
        "gender" : "m", 
        "numUsers" : 2 
      } 
    ]

We have used the function `LENGTH` (returns the length of a list) here. This is the
equivalent to SQL's `SELECT g, COUNT(*) FROM ... GROUP BY g`.
In addition to `LENGTH`, AQL also provides `MAX`, `MIN`, `SUM`, and `AVERAGE` as 
basic aggregation functions.

In AQL, all aggregation functions can be run on lists only. If an aggregation function
is run on anything that is not a list, an error will be thrown and the query will fail.

Post-filtering aggregated data ("Having")
-----------------------------------------

To filter on the results of a grouping or aggregation operation (i.e. something
similar to `HAVING` in SQL), simply add another `FILTER` clause after the `COLLECT` 
statement. 

For example, to get the 3 `ageGroup`s with the most users in them:

    FOR u IN users 
      FILTER u.active == true 
      COLLECT ageGroup = FLOOR(u.age / 5) * 5 INTO group 
      LET numUsers = LENGTH(group) 
      FILTER numUsers > 2 /* group must contain at least 3 users in order to qualify */
      SORT numUsers DESC 
      LIMIT 0, 3 
      RETURN { 
        "ageGroup" : ageGroup, 
        "numUsers" : numUsers, 
        "users" : group[*].u.name 
      }

    [ 
      { 
        "ageGroup" : 30, 
        "numUsers" : 8, 
        "users" : [ 
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
        "ageGroup" : 25, 
        "numUsers" : 4, 
        "users" : [ 
          "Mary", 
          "Mariah", 
          "Jim", 
          "Diego" 
        ] 
      }, 
      { 
        "ageGroup" : 35, 
        "numUsers" : 4, 
        "users" : [ 
          "Fred", 
          "John", 
          "Emma", 
          "Sophia" 
        ] 
      } 
    ]

To increase readability, the repeated expression `LENGTH(group)` was put into a variable
`numUsers`. The `FILTER` on `numUsers` is the SQL HAVING equivalent.

