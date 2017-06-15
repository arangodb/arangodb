Authorization Model
===================

Whith the release of ArangoDB 3.2 read only and collection level access was added.

    databases: {
        database-name: {
            permissions: {
                read: true,
                write: false,
            },
            collection-name: {
                *: {
                    permissions: {
                        read: true,
                        write: false
                    }
                }
            }
        }
    }

