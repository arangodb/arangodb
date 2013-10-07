Database Organisation {#GlossaryDatabaseOrganisation}
=====================================================

@GE{Database Organisation}: A single ArangoDB instance can handle multiple
databases in parallel. By default, there will be at least one database, which
is named `_system`.

Databases are physically stored in separate sub-directories underneath the
`database` directory, which itself resides in the instance's data directory. 

Each database has its own sub-directory, named `database-<database id>`. The
database directory contains sub-directories for the collections of the 
database, and a file named `parameter.json`. This file contains the database
id and name.

In an example ArangoDB instance which has two databases, the filesystem 
layout could look like this:

    data/                     # the instance's data directory
      databases/              # sub-directory containing all databases' data
        database-<id>/        # sub-directory for a single database
          parameter.json      # file containing database id and name
          collection-<id>/    # directory containg data about a collection
        database-<id>/        # sub-directory for another database
          parameter.json      # file containing database id and name
          collection-<id>/    # directory containg data about a collection
          collection-<id>/    # directory containg data about a collection

Foxx applications are also organised in database-specific directories inside
the application path. The filesystem layout could look like this:

    apps/                   # the instance's application directory
      system/               # system applications (can be ignored)
      databases/            # sub-directory containing database-specific applications
        <database-name>/    # sub-directory for a single database
          <app-name>        # sub-directory for a single application
          <app-name>        # sub-directory for a single application
        <database-name>/    # sub-directory for another database
          <app-name>        # sub-directory for a single application

