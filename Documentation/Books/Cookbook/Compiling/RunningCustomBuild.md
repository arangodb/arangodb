# Running a custom build

## Problem

You've already built a custom version of ArangoDB and want to run it. Possibly in isolation from an existing installation or you may want to re-use the data.

## Solution

First, you need to build your own version of ArangoDB. If you haven't done so already, have a look at any of the [Compiling](README.md) recipes.

This recipe assumes you're in the root directory of the ArangoDB distribution and compiling has successfully finished.

### Running in isolation

This part shows how to run your custom build with an empty database directory

```bash
# create data directory
mkdir /tmp/arangodb

# run
bin/arangod \
    --configuration etc/relative/arangod.conf\
     --database.directory /tmp/arangodb
```

### Running with data

This part shows how to run your custom build with the config and data from a pre-existing stable installation.

**BEWARE** ArangoDB's developers may change the db file format and after running with a changed file format, there may be no way back. Alternatively you can run your build in isolation and [dump](https://docs.arangodb.com/2.8/HttpBulkImports/Arangodump.html) and [restore](https://docs.arangodb.com/2.8/HttpBulkImports/Arangorestore.html) the data from the stable to your custom build.

When running like this, you must run the db as the arangod user (the default installed by the package) in order to have write access to the log, database directory etc. Running as root will likely mess up the file permissions - good luck fixing that!

```bash
# become root first
su

# now switch to arangod and run
su - arangod
bin/arangod --configuration /etc/arangodb/arangod.conf
```

**Author:** [Patrick Huber](https://github.com/stackmagic)

**Tags:** #build
