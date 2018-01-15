Show grants function
====================

Problem
-------

I'm looking for user database grants


Solution
--------
Create a global function in your _.arangosh.rc_ file like this:
```
global.show_grants = function () {
        let stmt;
        stmt=db._createStatement({"query": "FOR u in _users RETURN {\"user\": u.user, \"databases\": u.databases}"});
        console.log(stmt.execute().toString());
};
```
Now when you enter in arangosh, you can call **show_grants()** function.

#### Function out example
```
[object ArangoQueryCursor, count: 3, hasMore: false]


[
  {
    "user" : "foo",
    "databases" : {
      "_system" : "rw",
      "bar" : "rw"
    }
  },
  {
    "user" : "foo2",
    "databases" : {
      "bar" : "rw"
    }
  },
  {
    "user" : "root",
    "databases" : {
      "*" : "rw"
    }
  }
]
```
