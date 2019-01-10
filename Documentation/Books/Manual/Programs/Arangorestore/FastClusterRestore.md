Fast Cluster Restore
====================

The _Fast Cluster Restore_ procedure documented in this page is recommended
to speed up the performance of [_arangorestore_](../Arangorestore/README.md)
in a Cluster environment.

It is assumed that a Cluster environment is running and a _logical_ backup
with [_arangodump_](../Arangodump/README.md) has already been taken.

{% hint 'info' %}
The procedure described in this page is particularly useful for ArangoDB
version 3.3, but can be used in 3.4 and later versions as well. Note that
from v3.4, _arangorestore_ includes the option _threads_ which can be a first
good step already in achieving restore parallelization and its speed benefit.
However, the the procedure below allows for even further parallelization (making
using of different _Coordinators_), and the part regarding temporarily setting
_replication factor_ to 1 is still useful in 3.4 and later versions.
{% endhint %}

The speed improvement obtained by the procedure below is achieved by:

1. Restoring into a Cluster that has _replication factor_ 1, thus reducing
   number of network hops needed during the restore operation (_replication factor_
   is reverted to initial value at the end of the procedure - steps #2, #3 and #6).
2. Restoring in parallel multiple collections on different _Coordinators_
   (steps #4 and #5).

{% hint 'info' %}
Please refer to
[this](Examples.md#factors-affecting-speed-of-arangorestore-in-a-cluster)
section for further context on the factors affecting restore speed when restoring
using _arangorestore_ in a Cluster.
{% endhint %}

Step 1: Copy the _dump_ directory to all _Coordinators_
-------------------------------------------------------

The first step is to copy the directory that contains the _dump_ to all machines
where _Coordinators_ are running.

{% hint 'info' %}
This step is not strictly required as the backup can be restored over the
network. However, if the restore is executed locally the restore speed is
significantly improved.
{% endhint %}

Step 2: Restore collection structures
-------------------------------------

The collection structures have to be restored from exactly one _Coordinator_ (any
_Coordinator_ can be used) with the following command:

```
arangorestore
  --server.endpoint <endpoint-of-a-coordinator>
  --server.database <database-name>
  --server.password <password>
  --import-data false
  --input-directory <dump-directory>
```

The option `--import-data false`  tells _arangorestore_ to restore only the
collection structure and no data. Reason why the structure is restored before
the data is that this allows to change _replication factor_ before restoring
the data.

Step 3: Set _Replication Factor_ to 1
--------------------------------------

To speed up restore, it is possible to set the _replication factor_ to 1 before
importing any data. Run the following command from exactly one _Coordinator_ (any
_Coordinator_ can be used):

```
echo 'db._collections().filter(function(c) { return c.name()[0] !== "_"; })
.forEach(function(c) { print("collection:", c.name(), "replicationFactor:",
c.properties().replicationFactor); c.properties({ replicationFactor: 1 }); });'
| arangosh
  --server.endpoint <endpoint-of-a-coordinator>
  --server.database <database-name>
  --server.username <user-name>
  --server.password <password>
```

Step 4: Create parallel restore scripts
---------------------------------------

Now that the Cluster is prepared, the parallel restore scripts have to be
created. Therefore we will use a script that is provided below:

_parallelRestore.js_:

```
#!/bin/sh
#
# Version: 0.2
#
# Release Notes:
# - v0.3: ....... TODO .... fixed a bug when _ is used in the collection name
# - v0.2: compatibility with version 3.4: now each coordinator_<number-of-coordinator>.sh
#         includes a single restore command (instead of one for each collection)
#         which allows making using of the --threads option in v.3.4.0 and later
# - v0.1: initial version

if test -z "$ARANGOSH" ; then
  export ARANGOSH=arangosh
fi
cat > /tmp/parallelRestore$$.js <<'EOF'
var fs = require("fs");
var print = require("internal").print;
var exit = require("internal").exit;
var arangorestore = "arangorestore";
var env = require("internal").env;
if (env.hasOwnProperty("ARANGORESTORE")) {
  arangorestore = env["ARANGORESTORE"];
}

// Check ARGUMENTS: dumpDir coordinator1 coordinator2 ...

if (ARGUMENTS.length < 2) {
  print("Need at least two arguments DUMPDIR and COORDINATOR_ENDPOINTS!");
  exit(1);
}

var dumpDir = ARGUMENTS[0];
var coordinators = ARGUMENTS[1].split(",");
var otherArgs = ARGUMENTS.slice(2);

// Quickly check the dump dir:
var files = fs.list(dumpDir).filter(f => !fs.isDirectory(f));
var found = files.indexOf("ENCRYPTION");
if (found === -1) {
  print("This directory does not have an ENCRYPTION entry.");
  exit(2);
}
// Remove ENCRYPTION entry:
files = files.slice(0, found).concat(files.slice(found+1));

for (let i = 0; i < files.length; ++i) {
  if (files[i].slice(-5) !== ".json") {
    print("This directory has files which do not end in '.json'!");
    exit(3);
  }
}
files = files.map(function(f) {
  var fullName = fs.join(dumpDir, f);
  var collName = "";
  if (f.slice(-10) === ".data.json") {
    var pos;
    if (f.slice(0, 1) === "_") {  // system collection
      pos = f.slice(1).indexOf("_") + 1;
      collName = "_" + f.slice(1, pos);
    } else {
      pos = f.indexOf("_");
      collName = f.slice(0, pos);
    }
  }
  return {name: fullName, collName, size: fs.size(fullName)};
});
files = files.sort(function(a, b) { return b.size - a.size; });
var dataFiles = [];
for (let i = 0; i < files.length; ++i) {
  if (files[i].name.slice(-10) === ".data.json") {
    dataFiles.push(i);
  }
}

// Produce the scripts, one for each coordinator:
var scripts = [];
var collections = [];
for (let i = 0; i < coordinators.length; ++i) {
  scripts.push([]);
  collections.push([]);
}

var cnum = 0;
var temp = '';
var collections = [];
for (let i = 0; i < dataFiles.length; ++i) {
  var f = files[dataFiles[i]];
  if (typeof collections[cnum] == 'undefined') {
    collections[cnum] = (`--collection ${f.collName}`);
  } else {
    collections[cnum] += (` --collection ${f.collName}`);
  }
  cnum += 1;
  if (cnum >= coordinators.length) {
    cnum = 0;
  }
}

var cnum = 0;
// for (let i = 0; i < dataFiles.length; ++i) {
for (let i = 0; i < coordinators.length; ++i) {
  // var f = files[dataFiles[i]];
  // scripts[cnum].push(`${arangorestore} --collection ${f.collName} --input-directory ${dumpDir} --server.endpoint ${coordinators[cnum]} ` + otherArgs.join(" "));
  scripts[i].push(`${arangorestore} --input-directory ${dumpDir} --server.endpoint ${coordinators[i]} ` + collections[i] + ' ' + otherArgs.join(" "));
  // cnum += 1;
  // if (cnum >= coordinators.length) {
  //   cnum = 0;
  // }
}

for (let i = 0; i < coordinators.length; ++i) {
  let f = "coordinator_" + i + ".sh";
  print("Writing file", f, "...");
  fs.writeFileSync(f, scripts[i].join("\n"));
}
EOF

${ARANGOSH} --javascript.execute /tmp/parallelRestore$$.js -- "$@"
rm /tmp/parallelRestore$$.js
```

To run this script, all coordinator endpoints of the cluster have to be
provided. The example below is a template for a cluster with 3 coordinators:

```
./parallelRestore.js <dump-directory>
  tcp://<ip-of-coordinator1>:<port of coordinator1>,
  tcp://<ip-of-coordinator2>:<port of coordinator2>,
  tcp://<ip-of-coordinator3>:<port of coordinator3>
  --server.username <username>
  --server.password <password>
  --server.database <database_name>
  --create-collection false
```

Note the use of the option `--create-collection false` (since we already
created the collection structure before). The above command will create 3 scripts,
whereas 3 corresponds to the amount of listed coordinators. The resulting
scripts are named `coordinator_<number-of-coordinator>.sh` (e.g.
`coordinator_0.sh`, `coordinator_1.sh`, `coordinator_2.sh`).

**Note:** starting from v3.4.0 the _arangorestore_ option *--threads N* can be
passed to the command above, where _N_ is an integer, to further parallelize
(so there will be a parallelization also in each _Coordinator_). *--threads 2*
is used by default (from v3.4.0 on) if the option *--threads* is not passed.

Step 5: Execute parallel restore scripts
----------------------------------------

The `coordinator_<number-of-coordinator>.sh` scripts, that were created in the
previous step, now have to be executed on each machine where a _Coordinator_
is running. This will start a parallel restore of the dump.

Step 6: Revert to the initial _Replication Factor_
--------------------------------------------------

Once the _arangorestore_ process on every _Coordinator_ is completed, the
_replication factor_ has to be set to its initial value.

Run the following command from exactly one _Coordinator_ (any _Coordinator_ can be
used) - please adjust the `replicationFactor` value to your specific case (2 in the
example below):

```
echo 'db._collections().filter(function(c) { return c.name()[0] !== "_"; })
.forEach(function(c) { print("collection:", c.name(), "replicationFactor:",
c.properties().replicationFactor); c.properties({ replicationFactor: 2 }); });'
| arangosh
  --server.endpoint <endpoint-of-a-coordinator>
  --server.database <database-name>
  --server.username <user-name>
  --server.password <password>
```
