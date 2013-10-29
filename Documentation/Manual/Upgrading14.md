Upgrading to ArangoDB 1.4 {#Upgrading14}
========================================

@NAVIGATE_Upgrading14
@EMBEDTOC{Upgrading14TOC}

Upgrading {#Upgrading14Introduction}
====================================

Please read the following sections if you upgrade from a pre-1.4 version of ArangoDB
to ArangoDB 1.4.

ArangoDB 1.4 comes with a few changes, some of which are not 100% compatible to
ArangoDB 1.3. The incompatibilies are mainly due to the introduction of the multiple
databases feature and to some changes inside Foxx. One change also affects AQL user-
defined functions, so if you use them in ArangoDB, please make sure to read @ref
Upgrade14AqlFunctions "this" too.

Following is a list of incompatible changes with workarounds. Please read the list
carefully and adjust any client programs or processes that work with ArangoDB 
appropriately. Please also make sure to create a full backup of your existing ArangoDB
installation before performing an update.

The upgrading section is closed with an instruction summary and a list of potential 
problems and @ref Upgrading14Troubleshooting "troubleshooting options" for them. 
Please consult that section if you encounter any problems during or after the upgrade.

Database Directory Version Check and Upgrade {#Upgrading14VersionCheck}
-----------------------------------------------------------------------

ArangoDB will perform a database version check at startup. This has not changed in
ArangoDB 1.4. When ArangoDB 1.4 encounters a database created with earlier 
versions of ArangoDB, it will refuse to start. This is intentional.
The output will then look like this:

    2013-10-29T16:34:02Z [3927] INFO ArangoDB 1.4.0 -- ICU 49.1.2, V8 version 3.16.14.1, SSL engine OpenSSL 1.0.1e 11 Feb 2013
    2013-10-29T16:34:02Z [3927] ERROR no databases found. Please start the server with the --upgrade option
    2013-10-29T16:34:02Z [3927] ERROR unable to initialise databases: invalid database directory
    2013-10-29T16:34:02Z [3927] FATAL cannot start server: invalid database directory

To make ArangoDB 1.4 start with a database directory created with an
earlier ArangoDB version, you may need to invoke the upgrade procedure once.
This can be done by running ArangoDB from the command line and supplying
the `--upgrade` option:

    unix> arangod data --upgrade

where `data` is the database directory. This will run a database version and
any necessary migrations. As usual, you should create a backup of your database
directory before performing the upgrade.

The output should be something like this:

    ....
    2013-10-29T16:35:01Z [3959] INFO In database '_system': starting upgrade from version 1.3 to 1.4.0
    2013-10-29T16:35:01Z [3959] INFO In database '_system': Found 18 defined task(s), 5 task(s) to run
    2013-10-29T16:35:01Z [3959] INFO In database '_system': Executing task #1 (moveProductionApps): move Foxx apps into per-database directory
    2013-10-29T16:35:01Z [3959] INFO In database '_system': Task successful
    ...
    2013-10-29T16:35:01Z [3959] INFO In database '_system': Task successful
    2013-10-29T16:35:01Z [3959] INFO In database '_system': upgrade successfully finished
    2013-10-29T16:35:01Z [3959] INFO database upgrade passed

Please check the output the `--upgrade` run. It may produce errors, which need to be
fixed before ArangoDB can be used properly. If no errors are present or they have been
resolved, you can start ArangoDB 1.4 regularly.

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
further processing. This allows any existing actions that use URL paths without database
names to remain fully functional.

For example, if the request URL is `http://127.0.0.1:8529/_db/mydb/some-method`, then
ArangoDB will extract the database name `mydb` from the URL, and pass `/some-method` as 
the URL path to any internal or user-defined functions.

If no database name is specified in the URL path via the `_db` prefix, ArangoDB will use 
the algorithm described in @ref HttpDatabaseMapping to determine the database context for 
the request. If no extra endpoints are used, the algorithm will default to the `_system` 
database. A just upgraded ArangoDB instance will have all its collections and applications be 
mapped to the `_system` database too, meaning an upgraded ArangoDB instance should remain
fully functional.

ArangoDB clients and drivers are not forced to supply database names as part of the ArangoDB
URLs they call because of this compatibility functionaltiy. However, if clients need to 
access a specific database in a multiple database context, they will be required to supply
the database name as part of the URLs. Most clients will use just one database most of the
time, making it sufficient to set the database name when the connection to the server is
established and then prefixing all requests with the database name initially set.

Changed Behavior {#Upgrading14ChangedBehavior}
==============================================

Changed Namespace Separator for AQL user-defined Functions {#Upgrade14AqlFunctions}
-----------------------------------------------------------------------------------

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

Changed Return Value of REST API method GET `/_api/collection/figures` {#Upgrade14ChangedFigures}
-------------------------------------------------------------------------------------------------

The value returned by the REST API method GET `/_api/collection/<collection-name>/figures` has
been extended in version 1.4 to also include the following attributes:

- `compactors`.`count`
- `compactors`.`fileSize`
- `shapefiles`.`count`
- `shapefiles`.`fileSize`

These attributes were not present in the return value in ArangoDB 1.3. Clients which rely on the
return value structure may need to be adjusted.

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

Upgrade Checklist {#Upgrading14Checklist}
=========================================

Here is a checklist for the required upgrade steps. For more details please 
consult the more detailed topics and the @ref Upgrading14Troubleshooting sections.

- Upgrade ArangoDB
  - Stop _arangod_
  - Create a full backup of your database directory
  - Upgrade ArangoDB to 1.4 using package manager, homebrew, `git pull` etc.
  - Adjust configuration file `arangod.conf`:
    - Add option `--javascript.app-path` if not yet present (otherwise _arangod_ 
      will refuse to start)
    - Check if you want to set `--server.default-api-compatibility` to `10300` for 
      "old" client drivers
    - Remove now superfluous configuration options `--server.admin-directory` and
      `--server.disable-admin-interface`
  - If required, start _arangod_ with the `--upgrade` option once and check the 
    output/logfile for any errors
- Upgrade clients/drivers
  - Upgrade client drivers to a 1.4-compatible version if needed and if available
  - When using AQL user-defined functions, adjust AQL function namespace separator 
    in queries/client code
- Upgrade Foxx applications
  - When using Foxx application from ArangoDB 1.3, make sure to adjust the manifest
    and the app/controller JavaScript files
- Restart _arangod_
- Check the output of _arangod_ and the server logfile for potential errors.

Troubleshooting {#Upgrading14Troubleshooting}
=============================================

If you cannot find a solution here, please ask the Google-Group at 
http://groups.google.com/group/arangodb

Please make sure that you include the currently used ArangoDB version number plus the 
version from which you upgrade in your post.
You can retrieve the current version number as follows:

    unix> arangod --version

Problem: Server does not start
------------------------------

If the server complains about no databases being found at startup like this

    2013-10-29T16:34:02Z [3927] ERROR no databases found. Please start the server with the --upgrade option
    2013-10-29T16:34:02Z [3927] ERROR unable to initialise databases: invalid database directory
    2013-10-29T16:34:02Z [3927] FATAL cannot start server: invalid database directory

the problem might be that an ArangoDB 1.4 server was started with database directory
created with an older version of ArangoDB. In this case it is necessary to start 
ArangoDB with the `--upgrade` option once. If it still does not start, it may be necessary to 
specifiy the `--javascript.app-path` option (see next item) when running `--upgrade`.
In case of errors, please also check ArangoDB's output or logfile for further details about
the problem.

Problem: Server does not start
------------------------------

Contrary to previous versions, ArangoDB 1.4 requires the startup option `--javascript.app-path` 
to be set when the server is started. If the option is not set, the server will print a message 
like this:

    2013-10-09T19:11:47Z [7121] FATAL no value has been specified for --javascript.app-path.

To fix this, you can either specify the value for `--javascript.app-path` on the command-line
or in a configuration file. You then need to start the server with the app path specified and
the `--upgrade` option. This will make ArangoDB create the directory automatically.

If you want to use the development mode for Foxx applications, it is necessary to also specify
the parameter `--javascript.dev-app-path` when starting with `--upgrade`. This will initialise
the directories for development mode apps, too.

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

Problem: Where can Example Configuration files for ArangoDB be found?
---------------------------------------------------------------------

Updating ArangoDB will likely not overwrite an existing ArangoDB configuration
file. If you have a configuration file from a previous ArangoDB version in use
already, you may need to adjust this file locally.

The most current configuration files for ArangoDB 1.4 can be found on Github
(look for files with file extension *.conf):

* for Linux & MacOS: @S_EXTREF{https://github.com/triAGENS/ArangoDB/tree/1.4/etc/relative,https://github.com/triAGENS/ArangoDB/tree/1.4/etc/relative}

* for Windows: @S_EXTREF{https://github.com/triAGENS/ArangoDB/tree/1.4/VS2012/Installer,https://github.com/triAGENS/ArangoDB/tree/1.4/VS2012/Installer}


Please note that all these configuration files use relative paths that may
need to be adjusted to the absolute paths on your system.

Problem: Server returns different `location` headers than in 1.3
-----------------------------------------------------------------

ArangoDB 1.4 by default will return `location` HTTP headers that contain the
database name too. This is a consequence of potentially having multiple databases 
in the same server instance.

For example, when creating a new document, ArangoDB 1.3 returned an HTTP
response with a `location` header like this:

    location: /_api/document/<collection name>/<document key>

Contrary, ArangoDB 1.4 will return a `location` header like this by default:
    
    location: /_db/<database name>/_api/document/<collection name>/<document key>

This may not be compatible to pre-1.4 clients that rely on the old format
of the `location` header.

Obviously one workaround is to upgrade the used client driver to the newest 
version. If that cannot be done or if the newest version of the client driver is
not ready for ArangoDB 1.4, the server provides a startup option that can be
used to increase compatibility with old clients:

    --server.default-api-compatibility

Not setting this option will make ArangoDB set it to the current server version, and 
assume all clients are compatible. This will also make it send the new-style
location headers.

Setting this value to an older version number will make the server try to keep
the API compatible to older versions where possible. For example, to send the
old (pre-1.4) style location headers, set the value to `10300` (1.3) as follows:
    
    --server.default-api-compatibility 10300

The server will then return the old-style `location` headers.

Another way to fix the `location` header issue is to make the client send API
compatibility information itself. This can be achieved by sending an extra HTTP
header `x-arango-version` along with a client request. For example, sending the 
following header in a request will make ArangoDB return the old style `location`
headers too:

    x-arango-version: 10300

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

Problem: AQL user-functions from 1.3 do not work anymore
--------------------------------------------------------

The namespace resolution operator for AQL user-defined functions has changed from `:` 
to `::`. Names of user-defined function names need to be adjusted in AQL queries.
Please refer to @ref Upgrading14ChangedBehavior for details.

Problem: Foxx Applications from 1.3 are missing in 1.4
------------------------------------------------------

The directory layout for Foxx applications got changed between ArangoDB 1.3 and 1.4.
In ArangoDB 1.4, Foxx applications need to placed in per-database directories which
were not present in 1.3. ArangoDB 1.4 will automatically move existing Foxx applications
into the required new structure if you start it with the `--upgrade` option and the
Foxx application directory `--javascript.app-path`. Please note that specifiying 
`--javascript.app-path` is required in 1.4 anyway, so it is good to add this option
to your configuration file if not already done.

Please read more about the 1.4 Foxx application directory layout @ref 
Upgrading14FileSystem "here".

Problem: Foxx Applications from 1.3 do not work in 1.4
------------------------------------------------------

@ref UserManualFoxx "Foxx" was released in an alpha state with ArangoDB 1.3. It got more 
stable over time, but some its internal APIs changed during the development of ArangoDB 1.4.

Foxx applications written for ArangoDB 1.3 need some modifications in order to work
properly with ArangoDB 1.4:
- in the manifest file of a Foxx application, please rename the `apps` attribute to
  `controllers`.
- the `require` directive for the Foxx framework components changed between 1.3 and 1.4.
  Whereas in 1.3 you could require `Foxx.Application`, in 1.4 you will need to require
  `Foxx.Controller`:

  Please look out for all places like this in your 1.3 Foxx controller/application 
  files:

      var FoxxApplication = require("org/arangodb/foxx").Application;
      var app = new FoxxApplication();

  and adjust them as follows for 1.4:

      var FoxxController = require("org/arangodb/foxx").Controller;
      controller = new FoxxController(applicationContext);

  You may also need to adjust the `setup` and `teardown` scripts if used. To check
  for any errors after adjusting your Foxx application, you can start ArangoDB 1.4
  in development mode using the `--javascript.dev-app-path` option. This will 
  either print or log (depending on configuration) errors occurring in Foxx 
  applications.
