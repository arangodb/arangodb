!CHAPTER Database Management

This is an introduction to ArangoDB's Http interface for managing databases.

The HTTP interface for databases provides operations to create and drop
individual databases. These are mapped to the standard HTTP methods `POST`
and `DELETE`. There is also the `GET` method to retrieve a list of existing
databases.

Please note that all database management operations can only be accessed via
the default database (`_system`) and none of the other databases.

