HTTP Interface for Databases
============================

### Address of a Database

Any operation triggered via ArangoDB's HTTP REST API is executed in the context of exactly
one database. To explicitly specify the database in a request, the request URI must contain
the [database name](../../Manual/Appendix/Glossary.html#database-name) in front of the actual path:

    http://localhost:8529/_db/mydb/...

where *...* is the actual path to the accessed resource. In the example, the resource will be
accessed in the context of the database *mydb*. Actual URLs in the context of *mydb* could look
like this:

    http://localhost:8529/_db/mydb/_api/version
    http://localhost:8529/_db/mydb/_api/document/test/12345
    http://localhost:8529/_db/mydb/myapp/get

