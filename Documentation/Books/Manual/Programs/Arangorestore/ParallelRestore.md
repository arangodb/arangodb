Parallel Restore Procedure
==========================

The following parallel restore procedure is recommended to speed up the
performance of [_arangorestore_](../Arangorestore/README.md) in a
cluster environment.
It is assumed that a cluster environment is running and a logical backup
with [_arangodump_](../Arangodump/README.md) has already been created.

### Allocate dump Directory

The first step is to copy the `dump` directory to all machines where
coordinators are running. 

{% hint 'info' %}
This step is not strictly required as the backup can be restored over the
network. However, if the restore is executed locally the restore speed is
significantly improved.
{% endhint %}
	
### Restore Collection Structure

The collection structure has to be restored from exactly one coordinator (any
coordinator can be used) with the following command:

```
arangorestore
  --server.endpoint <endpoint-of-a-coordinator>
  --server.database <database-name> 
  --server.password <password> 
  --import-data false 
  --input-directory <dump-directory>
```


The option `--import-data false`  tells _arangorestore_ to restore only the
collection structure and no data.

### Set Replication Factor to 1

It is necessary to set the the replication factor to 1 before importing any
data. Run the following command from exactly one coordinator (any coordinator
can be used):

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

### Create parallel restore scripts

Now that the cluster is prepared, the parallel restore scripts have to be
created. Therefore we will use a script that is provided in the following.

_parallelRestore.js_:

```
#!/bin/sh
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
for (let i = 0; i < coordinators.length; ++i) {
  scripts.push([]);
}

var cnum = 0;
for (let i = 0; i < dataFiles.length; ++i) {
  var f = files[dataFiles[i]];
  scripts[cnum].push(`${arangorestore} --collection ${f.collName} --input-directory ${dumpDir} --server.endpoint ${coordinators[cnum]} ` + otherArgs.join(" "));
  cnum += 1;
  if (cnum >= coordinators.length) {
    cnum = 0;
  }
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

This time the option `--create-collection false` is used since we already
created the document structure before. The above command will create 3 scripts,
whereas 3 corresponds to the amount of listed coordinators. The resulting
scripts are named `coordinator_<number-of-coordinator>.sh` (e.g.
coordinator_0.sh, coordinator_1.sh, coordinator_2.sh).

### Execute parallel restore scripts

The `coordinator_<number-of-coordinator>.sh` scripts, that were created in the
previous script, now have to be executed on each machine where a coordinator
is running. This will start a parallel restore of the dump.

### Revert to the initial Replication Factor

Once the _arangorestore_ process on every coordinator is completed, the
replication factor has to be set to its initial value.

Run the following command from exactly one coordinator (any coordinator can be
used) - please adjust the `replicationFactor` value accordingly.:

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
