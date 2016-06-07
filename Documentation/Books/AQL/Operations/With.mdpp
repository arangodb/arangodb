
!CHAPTER WITH

An AQL query can optionally start with a *WITH* statement and the list of 
collections used by the query. All collections specified in *WITH* will be
read-locked at query start, in addition to the other collections the query
uses and that are detected by the AQL query parser.

Specifying further collections in *WITH* can be useful for queries that 
dynamically access collections (e.g. via traversals or via dynamic 
document access functions such as `DOCUMENT()`). Such collections may be 
invisible to the AQL query parser at query compile time, and thus will not
be read-locked automatically at query start. In this case, the AQL execution 
engine will lazily lock these collections whenever they are used, which can 
lead to deadlock with other queries. In case such deadlock is detected, the 
query will automatically be aborted and changes will be rolled back. In this
case the client application can try sending the query again.
However, if client applications specify the list of used collections for all
their queries using *WITH*, then no deadlocks will happen and no queries will
be aborted due to deadlock situations.

Note that for queries that access only a single collection or that have all
collection names specified somewhere else in the query string, there is no
need to use *WITH*. *WITH* is only useful when the AQL query parser cannot
automatically figure out which collections are going to be used by the query.
*WITH* is only useful for queries that dynamically access collections, e.g.
via traversals, shortest path operations or the *DOCUMENT()* function.

```
WITH managers, usersHaveManagers
FOR v, e, p IN OUTBOUND 'users/1' GRAPH 'userGraph'
  RETURN { v, e, p }
```

Note that constant *WITH* is also a keyword that is used in other contexts,
for example in *UPDATE* statements. If *WITH* is used to specify the extra
list of collections, then it must be placed at the very start of the query
string.
