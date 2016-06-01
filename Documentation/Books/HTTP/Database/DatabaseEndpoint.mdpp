!CHAPTER Database-to-Endpoint Mapping

If a [database name](../../Manual/Appendix/Glossary.html#database-name) is present in the
URI as above, ArangoDB will consult the database-to-endpoint mapping for the current
endpoint, and validate if access to the database is allowed on the endpoint. 
If the endpoint is not restricted to an array of databases, ArangoDB will continue with the 
regular authentication procedure. If the endpoint is restricted to an array of specified databases,
ArangoDB will check if the requested database is in the array. If not, the request will be turned
down instantly. If yes, then ArangoDB will continue with the regular authentication procedure.

If the request URI was *http:// localhost:8529/_db/mydb/...*, then the request to *mydb* will be 
allowed (or disallowed) in the following situations: 

```
Endpoint-to-database mapping           Access to *mydb* allowed?
----------------------------           -------------------------
[ ]                                    yes
[ "_system" ]                          no 
[ "_system", "mydb" ]                  yes
[ "mydb" ]                             yes
[ "mydb", "_system" ]                  yes
[ "test1", "test2" ]                   no
```

In case no database name is specified in the request URI, ArangoDB will derive the database
name from the endpoint-to-database mapping of the endpoint 
the connection was coming in on. 

If the endpoint is not restricted to an array of databases, ArangoDB will assume the *_system*
database. If the endpoint is restricted to one or multiple databases, ArangoDB will assume
the first name from the array.

Following is an overview of which database name will be assumed for different endpoint-to-database
mappings in case no database name is specified in the URI:

```
Endpoint-to-database mapping           Database
----------------------------           --------
[ ]                                    _system
[ "_system" ]                          _system
[ "_system", "mydb" ]                  _system
[ "mydb" ]                             mydb
[ "mydb", "_system" ]                  mydb
```
