!CHAPTER Joins

So far we have only dealt with one collection (*users*) at a time. We also have a 
collection *relations* that stores relationships between users. We will now use
this extra collection to create a result from two collections.

First of all, we'll query a few users together with their friends' ids. For that,
we'll use all *relations* that have a value of *friend* in their *type* attribute.
Relationships are established by using the *friendOf* and *thisUser* attributes in the
*relations* collection, which point to the *userId* values in the *users* collection.

!SUBSECTION Join tuples

We'll start with a SQL-ish result set and return each tuple (user name, friends userId) 
separately. The AQL query to generate such result is:

```js
FOR u IN users
  FILTER u.active == true
  LIMIT 0, 4
  FOR f IN relations
    FILTER f.type == "friend" && f.friendOf == u.userId
    RETURN {
      "user" : u.name,
      "friendId" : f.thisUser
    }
```

```json
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
```

We iterate over the collection users. Only the 'active' users will be examined.
For each of these users we will search for up to 4 friends. We locate friends
by comparing the *userId* of our current user with the *friendOf* attribute of the
*relations* document. For each of those relations found we return the users name
and the userId of the friend.


!SUBSECTION Horizontal lists


Note that in the above result, a user can be returned multiple times. This is the
SQL way of returning data. If this is not desired, the friends' ids of each user
can be returned in a horizontal list. This will return each user at most once.

The AQL query for doing so is:

```js
FOR u IN users
  FILTER u.active == true LIMIT 0, 4
  RETURN {
    "user" : u.name,
    "friendIds" : (
      FOR f IN relations
        FILTER f.friendOf == u.userId && f.type == "friend"
        RETURN f.thisUser
    )
  }
```

```json
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
```

In this query we are still iterating over the users in the *users* collection
and for each matching user we are executing a subquery to create the matching
list of related users.

!SUBSECTION Self joins

To not only return friend ids but also the names of friends, we could "join" the
*users* collection once more (something like a "self join"):

```js
FOR u IN users
  FILTER u.active == true
  LIMIT 0, 4
  RETURN {
    "user" : u.name,
    "friendIds" : (
      FOR f IN relations
        FILTER f.friendOf == u.userId && f.type == "friend"
        FOR u2 IN users
          FILTER f.thisUser == u2.useId
          RETURN u2.name
    )
  }
```

```json
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
```

This query will then again in term fetch the clear text name of the
friend from the users collection. So here we iterate the users collection,
and for each hit the relations collection, and for each hit once more the
users collection.

!SUBSECTION Outer joins

Lets find the lonely people in our database - those without friends.

```js

FOR user IN users
  LET friendList = (
    FOR f IN relations
      FILTER f.friendOf == u.userId
      RETURN 1
  )
  FILTER LENGTH(friendList) == 0
  RETURN { "user" : user.name }
```

```json
[
  {
    "user" : "Abigail"
  },
  {
    "user" : "Fred"
  }
]
```

So, for each user we pick the list of her friends and count them. The ones where
count equals zero are the lonely people. Using *RETURN 1* in the subquery
saves even more precious CPU cycles and gives the optimizer more alternatives.

!SUBSECTION Pitfalls

Since we're free of schemata, there is by default no way to tell the format of the
documents. So, if your documents don't contain an attribute, it defaults to
null. We can however check our data for accuracy like this:

```js
RETURN LENGTH(FOR u IN users FILTER u.userId == null RETURN 1)
```

```json
[
  10000
]
```

```js
RETURN LENGTH(FOR f IN relations FILTER f.friendOf == null RETURN 1)
```

```json
[
  10000
]
```

So that the above queries return 10k matches each, the result of i.e. the Join
tuples query will become 100.000.000 items large and will use much memory plus
computation time. So it is generally a good idea to revalidate that the criteria
for your join conditions exist.

Using indices on the properties can speed up the operation significantly.
You can use the explain helper to revalidate your query actually uses them.

If you work with joins on edge collections you would typically aggregate over
the internal fields *_id*, *_from* and *_to* (where *_id* equals *userId*,
*_from* *friendOf* and *_to* would be *thisUser* in our examples). ArangoDB
implicitly creates indices on them.
