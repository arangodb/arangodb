# Migration from ArangoDB 2.8 to 3.0

## Problem

I want to use ArangoDB 3.0 from now on but I still have data in ArangoDB 2.8.
I need to migrate my data. I am running an ArangoDB 3.0 cluster (and
possibly a cluster with ArangoDB 2.8 as well).

## Solution

The internal data format changed completely from ArangoDB 2.8 to 3.0,
therefore you have to dump all data using `arangodump` and then
restore it to the new ArangoDB instance using `arangorestore`.

General instructions for this procedure can be found 
[in the manual](https://docs.arangodb.com/3.0/Manual/Administration/Upgrading/Upgrading30.html).
Here, we cover some additional details about the cluster case.

## Dumping the data in ArangoDB 2.8

Basically, dumping the data works with the following command (use `arangodump`
from your ArangoDB 2.8 distribution!):

    arangodump --server.endpoint tcp://localhost:8530 --output-directory dump

or a variation of it, for details see the above mentioned manual page and
[this section](https://docs.arangodb.com/2.8/HttpBulkImports/Arangodump.html).
If your ArangoDB 2.8 instance is a cluster, simply use one of the
coordinator endpoints as the above `--server.endpoint`.

## Restoring the data in ArangoDB 3.0

The output consists of JSON files in the output directory, two for each
collection, one for the structure and one for the data. The data format
is 100% compatible with ArangoDB 3.0, except that ArangoDB 3.0 has
an additional option in the structure files for synchronous replication,
namely the attribute `replicationFactor`, which is used to specify,
how many copies of the data for each shard are kept in the cluster.

Therefore, you can simply use this command (use the `arangorestore` from
your ArangoDB 3.0 distribution!):

    arangorestore --server.endpoint tcp://localhost:8530 --input-directory dump

to import your data into your new ArangoDB 3.0 instance. See
[this page](https://docs.arangodb.com/3.0/Manual/Administration/Arangorestore.html)
for details on the available command line options. If your ArangoDB 3.0
instance is a cluster, then simply use one of the coordinators as
`--server.endpoint`.

That is it, your data is migrated.

## Controling the number of shards and the replication factor

This procedure works for all four combinations of single server and cluster
for source and destination respectively. If the target is a single server
all simply works.

So it remains to explain how one controls the number of shards and the
replication factor if the destination is a cluster.

If the source was a cluster, `arangorestore` will use the same number
of shards as before, if you do not tell it otherwise. Since ArangoDB 2.8
does not have synchronous replication, it does not produce dumps
with the `replicationFactor` attribute, and so `arangorestore` will
use replication factor 1 for all collections. If the source was a
single server, the same will happen, additionally, `arangorestore`
will always create collections with just a single shard.

There are essentially 3 ways to change this behaviour:

 1. The first is to create the collections explicitly on the
    ArangoDB 3.0 cluster, and then set the `--create-collection false` flag.
    In this case you can control the number of shards and the replication
    factor for each collection individually when you create them.
 2. The second is to use `arangorestore`'s options 
    `--default-number-of-shards` and `--default-replication-factor`
    (this option was introduced in Version 3.0.2)
    respectively to specify default values, which are taken if the 
    dump files do not specify numbers. This means that all such
    restored collections will have the same number of shards and
    replication factor.
 3. If you need more control you can simply edit the structure files
    in the dump. They are simply JSON files, you can even first
    use a JSON pretty printer to make editing easier. For the
    replication factor you simply have to add a `replicationFactor` 
    attribute to the `parameters` subobject with a numerical value. 
    For the number of shards, locate the `shards` subattribute of the
    `parameters` attribute and edit it, such that it has the right
    number of attributes. The actual names of the attributes as well
    as their values do not matter. Alternatively, add a `numberOfShards`
    attribute to the `parameters` subobject, this will override the
    `shards` attribute (this possibility was introduced in Version
    3.0.2).

Note that you can remove individual collections from your dump by
deleting their pair of structure and data file in the dump directory.
In this way you can restore your data in several steps or even
parallelise the restore operation by running multiple `arangorestore`
processes concurrently on different dump directories. You should
consider using different coordinators for the different `arangorestore`
processes in this case.

All these possibilities together give you full control over the sharding
layout of your data in the new ArangoDB 3.0 cluster.

