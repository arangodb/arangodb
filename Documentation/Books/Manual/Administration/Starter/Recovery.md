<!-- don't edit here, it's from https://@github.com/arangodb-helper/arangodb.git / docs/Manual/ -->
# ArangoDB Starter Recovery Procedure

This procedure is intended to recover a cluster (that was started with the ArangoDB
_Starter_) when a machine of that cluster is broken without the possibility to recover
it (e.g. complete HD failure). In the procedure is does not matter if a replacement
machine uses the old or a new IP address.

To recover from this scenario, you must:

- Create a new (replacement) machine with ArangoDB (including _Starter_) installed.
- Create a file called `RECOVERY` in the directory you are going to use as data
  directory of the _Starter_ (the one that is passed via the option `--starter.data-dir`).
  This file must contain the IP address and port of the _Starter_ that has been
  broken (and will be replaced with this new machine).

E.g.

```bash
echo "192.168.1.25:8528" > $DATADIR/RECOVERY
```

After creating the `RECOVERY` file, start the _Starter_ using all the normal command
line arguments.

The _Starter_ will now:

1. Talk to the remaining _Starters_ to find the ID of the _Starter_ it replaces and
   use that ID to join the remaining _Starters_.
1. Talk to the remaining _Agents_ to find the ID of the _Agent_ it replaces and
   adjust the command-line arguments of the _Agent_ (it will start) to use that ID.
   This is skipped if the _Starter_ was not running an _Agent_.
1. Remove the `RECOVERY` file from the data directory.

The cluster will now recover automatically. It will however have one more _Coordinators_
and _DBServers_ than expected. Exactly one _Coordinator_ and one _DBServer_ will
be listed "red" in the web UI of the database. They will have to be removed manually
using the ArangoDB Web UI.
