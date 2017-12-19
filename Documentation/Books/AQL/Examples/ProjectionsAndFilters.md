Projections and Filters
=======================

### Returning unaltered documents

To return three complete documents from collection *users*, the following query can be used:

```js
FOR u IN users 
  LIMIT 0, 3
  RETURN u
```

```json
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
```

Note that there is a *LIMIT* clause but no *SORT* clause. In this case it is not guaranteed
which of the user documents are returned. Effectively the document return order is unspecified
if no *SORT* clause is used, and you should not rely on the order in such queries.

### Projections


To return a projection from the collection *users* use a modified *RETURN* instruction:

```js
FOR u IN users 
  LIMIT 0, 3
  RETURN { 
    "user" : { 
      "isActive" : u.active ? "yes" : "no", 
      "name" : u.name 
    } 
  }
```

```json
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
```

### Filters

To return a filtered projection from collection *users*, you can use the
*FILTER* keyword. Additionally, a *SORT* clause is used to have the result
returned in a specific order:

```js
FOR u IN users 
  FILTER u.active == true && u.age >= 30
  SORT u.age DESC
  LIMIT 0, 5
  RETURN { 
    "age" : u.age, 
    "name" : u.name 
  }
```

```json
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
```