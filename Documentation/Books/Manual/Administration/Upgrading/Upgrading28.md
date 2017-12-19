!CHAPTER Upgrading to ArangoDB 2.8

Please read the following sections if you upgrade from a previous version to
ArangoDB 2.8. Please be sure that you have checked the list of [changes in 2.8](../../ReleaseNotes/UpgradingChanges28.md)
before upgrading.

Please note first that a database directory used with ArangoDB 2.8
cannot be used with earlier versions (e.g. ArangoDB 2.7) any
more. Upgrading a database directory cannot be reverted. Therefore
please make sure to create a full backup of your existing ArangoDB
installation before performing an upgrade.

!SECTION Database Directory Version Check and Upgrade

ArangoDB will perform a database version check at startup. When ArangoDB 2.8
encounters a database created with earlier versions of ArangoDB, it will refuse
to start. This is intentional.

The output will then look like this:

```
2015-12-04T17:11:17Z [31432] ERROR In database '_system': Database directory version (20702) is lower than current version (20800).
2015-12-04T17:11:17Z [31432] ERROR In database '_system': ----------------------------------------------------------------------
2015-12-04T17:11:17Z [31432] ERROR In database '_system': It seems like you have upgraded the ArangoDB binary.
2015-12-04T17:11:17Z [31432] ERROR In database '_system': If this is what you wanted to do, please restart with the
2015-12-04T17:11:17Z [31432] ERROR In database '_system':   --upgrade
2015-12-04T17:11:17Z [31432] ERROR In database '_system': option to upgrade the data in the database directory.
2015-12-04T17:11:17Z [31432] ERROR In database '_system': Normally you can use the control script to upgrade your database
2015-12-04T17:11:17Z [31432] ERROR In database '_system':   /etc/init.d/arangodb stop
2015-12-04T17:11:17Z [31432] ERROR In database '_system':   /etc/init.d/arangodb upgrade
2015-12-04T17:11:17Z [31432] ERROR In database '_system':   /etc/init.d/arangodb start
2015-12-04T17:11:17Z [31432] ERROR In database '_system': ----------------------------------------------------------------------
2015-12-04T17:11:17Z [31432] FATAL Database '_system' needs upgrade. Please start the server with the --upgrade option
```

To make ArangoDB 2.8 start with a database directory created with an earlier
ArangoDB version, you may need to invoke the upgrade procedure once.  This can
be done by running ArangoDB from the command line and supplying the `--upgrade`
option.

Note: here the same database should be specified that is also specified when
arangod is started regularly. Please do not run the `--upgrade` command on each
individual database subfolder (named `database-<some number>`).
 
For example, if you regularly start your ArangoDB server with

    unix> arangod mydatabasefolder

then running

    unix> arangod mydatabasefolder --upgrade

will perform the upgrade for the whole ArangoDB instance, including all of its
databases.

Starting with `--upgrade` will run a database version check and perform any
necessary migrations. As usual, you should create a backup of your database
directory before performing the upgrade.

The last line of the output should look like this:
```
2015-12-04T17:12:15Z [31558] INFO database upgrade passed
```

Please check the full output the `--upgrade` run. Upgrading may produce errors, which need
to be fixed before ArangoDB can be used properly. If no errors are present or
they have been resolved manually, you can start ArangoDB 2.8 regularly.

!SECTION Upgrading a cluster planned in the web interface

A cluster of ArangoDB instances has to be upgraded as well. This
involves upgrading all ArangoDB instances in the cluster, as well as
running the version check on the whole running cluster in the end.

We have tried to make this procedure as painless and convenient for you.
We assume that you planned, launched and administrated a cluster using the
graphical front end in your browser. The upgrade procedure is then as
follows:

  1. First shut down your cluster using the graphical front end as
     usual.

  2. Then upgrade all dispatcher instances on all machines in your
     cluster using the version check as described above and restart them.

  3. Now open the cluster dash board in your browser by pointing it to
     the same dispatcher that you used to plan and launch the cluster in 
     the graphical front end. In addition to the usual buttons
     "Relaunch", "Edit cluster plan" and "Delete cluster plan" you will
     see another button marked "Upgrade and relaunch cluster".

  4. Hit this button, your cluster will be upgraded and launched and
     all is done for you behind the scenes. If all goes well, you will
     see the usual cluster dash board after a few seconds. If there is 
     an error, you have to inspect the log files of your cluster
     ArangoDB instances. Please let us know if you run into problems.

There is an alternative way using the `ArangoDB` shell. Instead of
steps 3. and 4. above you can launch `arangosh`, point it to the dispatcher
that you have used to plan and launch the cluster using the option
``--server.endpoint``, and execute

    arangosh> require("org/arangodb/cluster").Upgrade("root","");

This upgrades the cluster and launches it, exactly as with the button 
above in the graphical front end. You have to replace `"root"` with
a user name and `""` with a password that is valid for authentication
with the cluster.

!SECTION Upgrading Foxx apps generated by ArangoDB 2.7 and earlier

The implementation of the `require` function used to import modules in
ArangoDB and Foxx [has changed](../../ReleaseNotes/UpgradingChanges28.md#module-resolution)
in order to improve compatibility with Node.js modules.

Given an app/service with the following layout:

* manifest.json
* controllers/
  * todos.js
* models/
  * todo.js
* repositories/
  * todos.js
* node_modules/
  * models/
    * todo.js

The file `controllers/todos.js` would previously contain the following
`require` calls:

```js
var _ = require('underscore');
var joi = require('joi');
var Foxx = require('org/arangodb/foxx');
var ArangoError = require('org/arangodb').ArangoError;
var Todos = require('repositories/todos'); // <-- !
var Todo = require('models/todo'); // <-- !
```

The require paths `repositories/todos` and `models/todo` were previously
resolved locally as relative to the app root.

Starting with 2.8 these paths would instead be resolved as relative to
the `node_modules` folder or the global ArangoDB module paths before being
resolved locally as a fallback.

In the given example layout the app would break in 2.8 because the module
name `models/todo` would always resolve to `node_modules/models/todo.js`
(which previously would have been ignored) instead of the local `models/todo.js`.

In order to make sure the app still works in 2.8, the require calls in
`controllers/todos.js` would need to be adjusted to look like this:

```js
var _ = require('underscore');
var joi = require('joi');
var Foxx = require('org/arangodb/foxx');
var ArangoError = require('org/arangodb').ArangoError;
var Todos = require('../repositories/todos'); // <-- !
var Todo = require('../models/todo'); // <-- !
```

Note that the old "global" style require calls may still work in 2.8 but
may break unexpectedly if modules with matching names are installed globally.
