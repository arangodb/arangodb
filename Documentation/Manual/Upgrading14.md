Upgrading to ArangoDB 1.4 {#Upgrading14}
========================================

@NAVIGATE_Upgrading14
@EMBEDTOC{Upgrading14TOC}

Upgrading {#Upgrading14Introduction}
====================================

1.4 is currently beta, please do not use in production.

Filesystem layout changes {#Upgrading14FileSystem}
--------------------------------------------------

The introduction of the "multiple databases" feature in ArangoDB 1.4
has caused some changes in how ArangoDB organises data in the filesystem.
The changes may be relevant when upgrading from an earlier version of
ArangoDB to 1.4.

If you upgrade from an existing ArangoDB 1.3 to 1.4, or from an older
version of 1.4 to the beta or stable 1.4 version, ArangoDB will move some 
files and directories in the database and the applications directory around
so the result matches the required new layout.

As usual when upgrading an existing ArangoDB instance, it may be necessary 
to force the upgrade by starting the server once with the `--upgrade` option.

This will automatically make the server run the upgrade tasks, which will
move files around if required.

It is good practice to do a full backup of your data and applications directories 
before upgrading.

Please note that if you have your own scripts or procedures in place that directly
work on ArangoDB files or directories, you might need to ajdust them so they
use the new file and directory locations of ArangoDB 1.4.

Changes in the data directory
-----------------------------

If you only access the database and collection files via ArangoDB commands and 
don't ever access the files in it separately, you may want to skip this section. 
You should read on if you work on/with the files in the database directory with 
tools other than ArangoDB (e.g. Unix commands, backup tools).

Before 1.4, there was only one database in an ArangoDB instance, and ArangoDB 
stored the data of all collections in directories directly underneath the main 
data directory.

This "old" filesystem layout looked like this:

    <data directory>/
      <collection-abc>/
      <collection-xyz>/
      ...

With multiple databases in 1.4, the filesystem layout is now:

    <data directory>/
      databases/
        <database-123>/
          <collection-abc>/
          <collection-xyz>/
        <database-456>/
          <collection-abc>/
          <collection-xyz>/

As you can see above, there is now an intermediate directory named _databases_ 
inside the data directory, and each database resides in its own sub-directory 
underneath the _databases_ directory.

Database directory names use a numeric id part, e.g. `database-12345`. 
To tell which database directory belongs to which database, you can check the file 
`parameter.json` inside a database directory (by the way: collections work the same,
and you can determine the name of a collection by looking into the `parameter.json`
file in a collection's directory).

Changes in the apps directory
-----------------------------
                                                                                            
Due to the multiple databases feature, there are also changes with respect to where
Foxx applications need to be organised in the filesystem. If you have not yet used
Foxx in ArangoDB already, you can safely skip this section.

Foxx applications in previous versions of ArangoDB resided in the application directory,
with each Foxx application being contained in a directory directly underneath the main
application directory like this:

    apps/
      <app name>/
      <app name>/
      ...

With multiple databases in ArangoDB 1.4, Foxx applications can be installed per database.
This requires Foxx applications to reside in database-specific directories.

    apps/
      databases/
        <database name>/
          <app name>/
          <app name>/
          ...
        <database name>/
          <app name>/
          ...
        ...
      system/
        <app name>/
        ...

The directories of individual applications will thus be moved underneath the correct 
database sub-directory in the directory `apps/databases`. 
There is also the option of using so-called *system applications*. These are not
database-specific but shared between all databases. ArangoDB is shipped with one
system application named *aardvark*, which is ArangoDB's web-based admin interface.

Addressing specific Databases in Requests {#Upgrading14Databases}
=================================================================

In pre-1.4, an ArangoDB instance only managed a single database, so there was no need
to specify a database name in any of the incoming HTTP requests. In ArangoDB 1.4, the 
server can manage multiple datbases, and it may be necessary in a request to specify
which database the request should be executed in.

A specific ArangoDB database can be explicitly addressed by putting the database name
into the URL. If the first part of the URL path is `/_db/...`, ArangoDB will interpret
the `...` as the database name, and strip off the database name from the URL path for
further processing. This allows any actions that use URL paths to remain fully functional.

If no database name is specified in the URL path, ArangoDB will use the algorithm 
described in @ref HttpDatabaseMapping to determine the database context for the request.
If no extra endpoints are used, the algorithm will default to the `_system` database.
A just upgraded ArangoDB instance will have all its collections and applications be 
mapped to the `_system` database too, meaning an upgraded ArangoDB instance should remain
fully functional.

ArangoDB clients and drivers are not forced to supplydatabase names as part of the ArangoDB
URLs they call because of this compatibility functionaltiy. However, if clients need to 
access a specific database in a multiple database context, they will be required to supply
the database name as part of the URLs. Most clients will use just one database most of the
time, making it sufficient to set the database name when the connection to the server is
established and then prefixing all requests with the database name initially set.

Troubleshooting {#Upgrading14Troubleshooting}
=============================================

If you cannot find a solution here, please ask the Google-Group at 
http://groups.google.com/group/arangodb

Problem: Server does not start
------------------------------

The server will try to bind to all defined endpoints at startup. The
list of endpoints to connect to is read from the command-line, ArangoDB's
configuration file, and a file named `ENDPOINTS` in the database directory.
The `ENDPOINTS` file contains all endpoints which were added at runtime.

In case a previously defined endpoint is invalid at server start, ArangoDB
will fail on startup with a message like this:

    2013-09-24T07:09:00Z [4318] INFO using endpoint 'tcp://127.0.0.1:8529' for non-encrypted requests
    2013-09-24T07:09:00Z [4318] INFO using endpoint 'tcp://127.0.0.1:8531' for non-encrypted requests
    2013-09-24T07:09:00Z [4318] INFO using endpoint 'tcp://localhost:8529' for non-encrypted requests
    ...
    2013-09-24T07:09:01Z [4318] ERROR bind() failed with 98 (Address already in use)
    2013-09-24T07:09:01Z [4318] FATAL failed to bind to endpoint 'tcp://localhost:8529'. Please review your endpoints configuration.

The problem above may be obvious: in a typical setup, `127.0.0.1` will be 
the same IP address, so ArangoDB will try to bind to the same address twice
(which will fail).
Other obvious bind problems at startup may be caused by ports being used by
other programs, or IP addresses changing.

Problem: Find out the storage location of a database / collection
-----------------------------------------------------------------

To tell which directory belongs to which named database and which collection, you 
can use a Bash script like this:

    cd /path/to/data/directroy
    for db in `find databases -mindepth 1 -maxdepth 1 -type d`;
      do
        echo -n "- database directory \"$db\", name ";
        grep -o -E "name\":\"[^\"]+\"" "$db"/parameter.json | cut -d ":" -f 2;

        for c in `find $db/ -mindepth 1 -maxdepth 1 -type d`;
          do
            echo -n "  - collection directory \"$c\", name ";
            grep -E -o "name\":\"[^\"]+\"" "$c/parameter.json" | cut -d ":" -f 2;
          done;

        echo;
      done

The above script should print out the names of all databases and collections 
with their corresponding directory names.

Problem: AQL user-function does not work anymore
------------------------------------------------

The namespace resolution operator for AQL user-defined functions has changed from `:` 
to `::`. Names of user-defined function names need to be adjusted in AQL queries.
Please refer to @ref Upgrading14ChangedBehavior for details.

Changed Behavior {#Upgrading14ChangedBehavior}
==============================================

The namespace resolution operator for AQL user-defined functions has been changed from 
`:` to `::`.

AQL user-defined functions were introduced in ArangoDB 1.3, and the namespace resolution
perator for them has been the single colon (`:`) in 1.3. A call to a user-defined function
in an AQL query looked like this:

    RETURN mygroup:myfunc()

The single colon caused an ambiguity in the AQL grammar, making it indistinguishable from
named attributes or the ternary operator in some cases, e.g.

    { mygroup:myfunc ? mygroup:myfunc }

To fix this ambiguity, the namespace resolution operator in 1.4 is changed from `:` to `::`, 
so the above call will in 1.4 look like this:

    RETURN mygroup::myfunc()

Names of existing user-defined AQL functions in the database will automatically be fixed 
when starting ArangoDB 1.4 with the `--upgrade` option. 

Still any AQL query strings assembled on the client side must be adjusted for use with 1.4 
if they refer to AQL user-defined functions. If AQL queries stored in Foxx applications or
other server-side actions use the "old" function name sytanx, they must be adjusted manually,
too. These change should be simple to carry out (replacing the `:` in names of user-defined 
functions with `::`) but cannot be done automatically by ArangoDB.

If function names are not changed in AQL queries, referring to a function using the old (`:`)
namespace operator is likely to cause a query parse error in 1.4.

The return value of the AQL `DOCUMENT` function is also changed in 1.4 when called with a
single argument (a document id or key) in case the sought document cannot be found. 
In pre-1.4, the function returned `undefined` in this case. As `undefined` is not part of 
the JSON type system, 1.4 now returns `null` for the same case. The return value for other
cases has not changed.

Removed Features {#Upgrading14RemovedFeatures}
==============================================

Removed or Renamed Configuration options {#Upgrading14RemovedConfiguration}
---------------------------------------------------------------------------

The following changes have been made in ArangoDB 1.4 with respect to
configuration / command-line options:

- The options `--server.admin-directory` and `--server.disable-admin-interface`
  have been removed. 
  
  In previous versions of ArangoDB, these options controlled where the static 
  files of the web admin interface were located and if the web admin interface 
  was accessible via HTTP. 
  In ArangoDB 1.4, the web admin interface has become a Foxx application and
  does not require an extra location. 

- The option `--log.filter` was renamed to `--log.source-filter`.

  This is a debugging option that should rarely be used by non-developers.

Other removed Features {#Upgrading14RemovedMisc}
------------------------------------------------

The action deployment tool available in ArangoDB 1.3 has been removed in 
version 1.4. Installing actions can now be achieved easier by packaging them
in a Foxx application and deploying them with the `foxx-manager` binary.

