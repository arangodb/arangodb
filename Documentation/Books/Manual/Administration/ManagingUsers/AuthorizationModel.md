Internal representation
-----------------------

In general administrator will use the *@arangodb/users* module to
create users and administrate their rights. This is a description of
the internal format used to describe users and their privileges.  This
format is stored in the collection *_users* in the *_system* database.

Note that this format is subject to change without notice, therefore
you should never access this table directly. Always use the functions
supplied by the *users* module.

The new authorization model allows to add other types of rights.
Currently supported types are read and write permissions.  Every
database object has at least a permissions attribute and collections
attribute. The permissions sub-object holds the rights at dabase level
while the collections sub-object holds an object with collection
names. Every collection name object holds a permissions object which
holds permissions for collection level authorization.

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
