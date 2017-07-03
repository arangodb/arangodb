Authorization Model
===================

With the release of ArangoDB 3.2 read only and collection level access
was added. Each user can have *rw* (right/write), *ro* (read-one) or
*none* access to

- a collection
- the database containing the collection
- the *_system* database

These rights control which operations a user is allowed to execute.

| action                | collection | collection database | _system database |
|-----------------------|------------|---------------------|------------------|
| create a document     | rw         |                     |                  |
| read a document       | r          |                     |                  |
| update a document     | rw         |                     |                  |
| drop a document       | rw         |                     |                  |
| create a collection   |            | rw                  |                  |
| drop a collection     |            | rw                  |                  |
| truncate a collection | rw         |                     |                  |
| create an index       |            | rw                  |                  |
| drop an index         |            | rw                  |                  |
| create a database     |            |                     | rw               |
| drop a database       |            |                     | rw               |
| create a user         |            |                     | rw               |
| update a user         |            |                     | rw               |
| drop a user           |            |                     | rw               |


Internal representation
-----------------------

In general administrator will use the *@arangodb/users* module to create users
and administrate their rights.

These rights are stored in the *_users* table in the *_system* database.

The new authorization model allows to add other types of rights.  Currently
supported types are read and write permissions.  Every database object has at
least a permissions attribute and collections attribute. The permissions
sub-object holds the rights at dabase level while the collections sub-object
holds an object with collection names. Every collection name object holds a
permissions object which holds permissions for collection level authorization.

    databases: {
        database-name: {
            permissions: {
                read: true,
                write: false,
            },
            collections: {
                collection-name: {
                    permissions: {
                        read: true,
                        write: false
                    }
                }
            }
        }
    }


The *collection-name* is the name a document or ege collection; a collection
database is the database in which the collection is stored. The *_system* database
is the system database.

If we want to create a user that can only read all collections in database
reports, the permissions json would look this way:

    databases: {
        reports: {
            permissions: {
                read: true,
                write: false,
            },
            collections: {
                *: {
                    permissions: {
                        read: true,
                        write: false
                    }
                }
            }
        }
    }

### The special collection *

The collection *\** is a wildcard and stands for all collections in a
database. The permission lookup works as follows: First look for the exact
collection name requested. If the collection could not be found, look for the
*\** collection.  If the *\** collection is found check the permissions on the
collection *\**. If the collection *\** is not found no permissions are granted.

### The special database *

The database *\** is a wildcard and stands for all database in a ArangoDB single
instance or cluster. The permission lookup works as follows: First look for the
exact databse name requested. If the database could not be found, look for the
*\** database.  If the *\** database is found check the permissions on the
database *\**. If the database *\** is not found no permissions are granted.

    databases: {
        reports: {
            permissions: {
                read: true,
                write: false,
            },
            collections: {
                *: {
                    permissions: {
                        read: true,
                        write: false
                    }
                }
            }
        }
    }

If we request a document from the collection *daily* in the database *reports*
we get access because a read permission is defined for the wildcard collection
*\**.

    databases: {
        reports: {
            permissions: {
                read: true,
                write: false,
            },
            collections: {
                daily: {
                    permissions: {
                        read: true,
                        write: false
                    }
                }
            }
        }
    }

If we request a document from the collection *weekly* on the database reports we
get no access because the collection *weekly* does not exist in the permissions
object and there is also no wildcard collection *\**.
