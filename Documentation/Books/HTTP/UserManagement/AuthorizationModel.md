Authorization Model
===================

Whith the release of ArangoDB 3.2 read only and collection level access was added.

This is the new authorization model. It allows us in the future to add other types of rights.
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

