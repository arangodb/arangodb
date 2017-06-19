Authorization Model
===================

Whith the release of ArangoDB 3.2 read only and collection level access was added.

The new authorization model allows us to add other types of rights in the feature. Currently supported are read and write permissions.
Every database object has a least a permissions and collections object. The permissions object holds the rights at dabase level while the collections object hold an object with collection names. Every collection name object holds a permissions object which holds permissions for collection level authorization.

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

| action   | collection | collection database | _system database |
|----------|------------|---------|------|
| create a document | rw |   |   |
| read a document | r |  |   |
| update a document | rw |   |   |
| drop a document | rw |   |   |
| create a collection |  | rw |   |
| drop a collection |  | rw |   |
| truncate a collection | rw |  |   |
| create a index |   | rw |   |
| drop a index |   | rw |   |
| create a database |   |   | rw |
| drop a database |   |   | rw |
| create a user |   |   | rw |
| update a user |   |   | rw |
| drop a user |   |   | rw |

In this table the collection is a document or ege collection; a collection database is the database in which the collection is stored. The _system database is the system database.

The table shows which rigths are needed for certain actions.

If we want to create a user that can only read all collections in database reports, the permissions json would look this way:

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

### The special collection '*'
The collection * is a wildcard and stands for all databases.