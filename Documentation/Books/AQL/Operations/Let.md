LET
===

The *LET* statement can be used to assign an arbitrary value to a variable.
The variable is then introduced in the scope the *LET* statement is placed in.

The general syntax is:

```
LET variableName = expression
```

Variables are immutable in AQL, which means they can not be re-assigned:

```js
LET a = [1, 2, 3]  // initial assignment

a = PUSH(a, 4)     // syntax error, unexpected identifier
LET a = PUSH(a, 4) // parsing error, variable 'a' is assigned multiple times
LET b = PUSH(a, 4) // allowed, result: [1, 2, 3, 4]
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
